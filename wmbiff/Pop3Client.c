/* $Id: Pop3Client.c,v 1.3 2001/10/04 09:50:59 jordi Exp $ */
/* Author : Scott Holden ( scotth@thezone.net )
   Modified : Yong-iL Joh ( tolkien@mizi.com )
   Modified : Jorge García ( Jorge.Garcia@uv.es )
   Modified ; Mark Hurley ( debian4tux@telocity.com )
 * 
 * Pop3 Email checker.
 *
 * Last Updated : Apr 29, 23:04:57 EDT 2001     
 *
 */

#include "Client.h"
#include "charutil.h"
#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#define	PCU	(pc->u).pop

FILE *authenticate_md5(Pop3 pc, FILE * fp, char *buf);
FILE *authenticate_apop(Pop3 pc, FILE * fp, char *apop_str);

FILE *pop3Login(Pop3 pc)
{
	int fd;
	FILE *fp;
	char buf[BUF_SIZE];
	char apop_str[BUF_SIZE];
	char *ptr1, *ptr2;
	int has_apop = 0;

	if ((fd = sock_connect(PCU.serverName, PCU.serverPort)) == -1) {
		fprintf(stderr, "Not Connected To Server '%s:%d'\n",
				PCU.serverName, PCU.serverPort);
		return NULL;
	}

	fp = fdopen(fd, "r+");
	fgets(buf, BUF_SIZE, fp);
	fflush(fp);
#ifdef DEBUG_POP3
	fprintf(stderr, "%s", buf);
#endif
	/* Detect APOP */
	for (ptr1 = buf + strlen(buf), ptr2 = NULL; ptr1 > buf; --ptr1) {
		if (*ptr1 == '>') {
			ptr2 = ptr1;
		} else if (*ptr1 == '<') {
			if (ptr2) {
				has_apop = 1;
				*(ptr2 + 1) = 0;
				strncpy(apop_str, ptr1, BUF_SIZE);
			}
			break;
		}
	}

#ifdef WITH_GCRYPT
	/* Try to authenticate using AUTH CRAM-MD5 first */
	fprintf(fp, "AUTH CRAM-MD5\r\n");
	fflush(fp);
	fgets(buf, BUF_SIZE, fp);
#ifdef DEBUG_POP3
	fprintf(stderr, "%s", buf);
#endif

	if (buf[0] == '+' && buf[1] == ' ') {
		return (authenticate_md5(pc, fp, buf));
	}

	if (has_apop) {				/* APOP is better than nothing */
		return (authenticate_apop(pc, fp, apop_str));
	}
#endif
	/* A brave man has nothing to hide */

	fprintf(fp, "USER %s\r\n", PCU.userName);
	fflush(fp);
	fgets(buf, BUF_SIZE, fp);
	if (buf[0] != '+') {
		fprintf(stderr, "Invalid User Name '%s@%s:%d'\n",
				PCU.userName, PCU.serverName, PCU.serverPort);
#ifdef DEBUG_POP3
		fprintf(stderr, "%s\n", buf);
#endif
		fprintf(fp, "QUIT\r\n");
		fclose(fp);
		return NULL;
	};

	fflush(fp);
	fprintf(fp, "PASS %s\r\n", PCU.password);
	fflush(fp);
	fgets(buf, BUF_SIZE, fp);
	if (buf[0] != '+') {
		fprintf(stderr, "Incorrect Password for user '%s@%s:%d'\n",
				PCU.userName, PCU.serverName, PCU.serverPort);
		fprintf(stderr, "It said %s", buf);
		fprintf(fp, "QUIT\r\n");
		fclose(fp);
		return NULL;
	};

	return fp;
}

int pop3CheckMail(Pop3 pc)
{
	FILE *f;
	int read;
	char buf[BUF_SIZE];

	f = pc->open(pc);
	if (f == NULL)
		return -1;

	fflush(f);
	fprintf(f, "STAT\r\n");
	fflush(f);
	fgets(buf, 256, f);
	if (buf[0] != '+') {
		fprintf(stderr, "Error Receiving Stats '%s@%s:%d'\n",
				PCU.userName, PCU.serverName, PCU.serverPort);
		return -1;
	} else {
		sscanf(buf, "+OK %d", &(pc->TotalMsgs));
	}

	/*  - Updated - Mark Hurley - debian4tux@telocity.com
	 *  In compliance with RFC 1725
	 *  which removed the LAST command, any servers
	 *  which follow this spec will return:
	 *      -ERR unimplimented
	 *  We will leave it here for those servers which haven't
	 *  caught up with the spec.
	 */
	fflush(f);
	fprintf(f, "LAST\r\n");
	fflush(f);
	fgets(buf, 256, f);
	if (buf[0] != '+') {
		/* it is not an error to receive this according to RFC 1725 */
		/* no error should be returned */
		pc->UnreadMsgs = pc->TotalMsgs;
	} else {
		sscanf(buf, "+OK %d", &read);
		pc->UnreadMsgs = pc->TotalMsgs - read;
	}

	fprintf(f, "QUIT\r\n");
	fclose(f);

	return 0;
}

int pop3Create(Pop3 pc, char *str)
{
	/* POP3 format: pop3:user:password@server[:port] */
	/* new POP3 format: pop3:user password server [port] */
	/* If 'str' line is badly formatted, wmbiff won't display the mailbox. */

	/* old config file style... */
	if (parse_old_pop3_path(pc, str) != 0)

		/* new config file style... */
		if (parse_new_pop3_path(pc, str) != 0) {

			/* The line is badly formatted,
			 * we're not creating the mailbox line */
			pc->label[0] = 0;
			fprintf(stderr, "config line with bad format '%s'.\n", str);
			return -1;
		}

	return 0;
}

/* parse the pop3 config line
 * one simple function with a delimiter won't work,
 * because of the '@' before server in old style!
 */
int parse_old_pop3_path(Pop3 pc, char *str)
{
	char *tmp;
	char *p;

#ifdef DEBUG_POP3
	printf("pop3: str = '%s'\n", str);
#endif

	strcpy(PCU.password, "");
	strcpy(PCU.userName, "");
	strcpy(PCU.serverName, "");
	PCU.serverPort = 110;

	tmp = strdup(str);

	p = strtok(tmp, ":");		/* cut off 'pop3:' */

	if ((p = strtok(NULL, ":"))) {	/* p -> username */
		strcpy(PCU.userName, p);
		if ((p = strtok(NULL, "@"))) {	/* p -> password */
			strcpy(PCU.password, p);
			if ((p = strtok(NULL, ":"))) {	/* p -> server */
				strcpy(PCU.serverName, p);
				strcpy(pc->path, "");	/* p -> mailbox */
				if ((p = strtok(NULL, ":"))) {	/* p -> port */
					PCU.serverPort = atoi(p);
				}
				free(tmp);

#ifdef DEBUG_POP3
				printf("pop3: userName= '%s'\n", PCU.userName);
				printf("pop3: password= '%s'\n", PCU.password);
				printf("pop3: serverName= '%s'\n", PCU.serverName);
				printf("pop3: serverPort= '%d'\n", PCU.serverPort);
#endif
				pc->open = pop3Login;
				pc->checkMail = pop3CheckMail;
				pc->TotalMsgs = 0;
				pc->UnreadMsgs = 0;
				pc->OldMsgs = -1;
				pc->OldUnreadMsgs = -1;
				return 0;
			}
		}
	}

	return 1;
}

/* delimiter is a "space" */
int parse_new_pop3_path(Pop3 pc, char *str)
{
	char *tmp;
	char *p;
	char *delim = " ";

#ifdef DEBUG_POP3
	printf("pop3: str = '%s'\n", str);
#endif

	strcpy(PCU.password, "");
	strcpy(PCU.userName, "");
	strcpy(PCU.serverName, "");
	PCU.serverPort = 110;

	tmp = strdup(str);

	p = strtok(tmp, ":");		/* cut off 'pop3:' */

	if ((p = strtok(NULL, delim))) {	/* p -> username */
		strcpy(PCU.userName, p);
		if ((p = strtok(NULL, delim))) {	/* p -> password */
			strcpy(PCU.password, p);
			if ((p = strtok(NULL, delim))) {	/* p -> server */
				strcpy(PCU.serverName, p);
				strcpy(pc->path, "");	/* p -> mailbox */
				if ((p = strtok(NULL, delim))) {	/* p -> port */
					PCU.serverPort = atoi(p);
				}
				free(tmp);

#ifdef DEBUG_POP3
				printf("pop3: userName= '%s'\n", PCU.userName);
				printf("pop3: password= '%s'\n", PCU.password);
				printf("pop3: serverName= '%s'\n", PCU.serverName);
				printf("pop3: serverPort= '%d'\n", PCU.serverPort);
#endif
				pc->open = pop3Login;
				pc->checkMail = pop3CheckMail;
				pc->TotalMsgs = 0;
				pc->UnreadMsgs = 0;
				pc->OldMsgs = -1;
				pc->OldUnreadMsgs = -1;
				return 0;
			}
		}
	}

	return 1;
}

#ifdef WITH_GCRYPT

FILE *authenticate_md5(Pop3 pc, FILE * fp, char *buf)
{
	char buf2[BUF_SIZE];
	unsigned char *md5;
	GCRY_MD_HD gmh;

	Decode_Base64(buf + 2, buf2);
#ifdef DEBUG_POP3
	fprintf(stderr, "POP3 CRAM-MD5 challenge: %s\n", buf2);
#endif

	strcpy(buf, PCU.userName);
	strcat(buf, " ");

	gmh = gcry_md_open(GCRY_MD_MD5, GCRY_MD_FLAG_HMAC);
	gcry_md_setkey(gmh, PCU.password, strlen(PCU.password));
	gcry_md_write(gmh, (unsigned char *) buf2, strlen(buf2));
	gcry_md_final(gmh);
	md5 = gcry_md_read(gmh, 0);
	//hmac_md5(buf2, strlen(buf2), PCU.password,
	//      strlen(PCU.password), md5);
	Bin2Hex(md5, 16, buf2);
	gcry_md_close(gmh);

	strcat(buf, buf2);
#ifdef DEBUG_POP3
	fprintf(stderr, "POP3 CRAM-MD5 response: %s\n", buf);
#endif
	Encode_Base64(buf, buf2);

	fprintf(fp, "%s\r\n", buf2);
	fflush(fp);
	fgets(buf, BUF_SIZE, fp);

	if (!strncmp(buf, "+OK", 3))
		return fp;				/* AUTH successful */
	else {
		fprintf(stderr,
				"POP3 CRAM-MD5 AUTH failed for user '%s@%s:%d'\n",
				PCU.userName, PCU.serverName, PCU.serverPort);
		fprintf(stderr, "It said %s", buf);
		fprintf(fp, "QUIT\r\n");
		fclose(fp);
		return NULL;
	}
}

FILE *authenticate_apop(Pop3 pc, FILE * fp, char *apop_str)
{
	GCRY_MD_HD gmh;
	char buf[BUF_SIZE];
	unsigned char *md5;

#ifdef DEBUG_POP3
	fprintf(stderr, "POP3 APOP challenge: %s\n", apop_str);
#endif
	strcat(apop_str, PCU.password);

	gmh = gcry_md_open(GCRY_MD_MD5, 0);
	gcry_md_write(gmh, (unsigned char *) apop_str, strlen(apop_str));
	gcry_md_final(gmh);
	md5 = gcry_md_read(gmh, 0);
	Bin2Hex(md5, 16, buf);
	gcry_md_close(gmh);

#ifdef DEBUG_POP3
	fprintf(stderr, "POP3 APOP response: %s %s\n", PCU.userName, buf);
#endif
	fprintf(fp, "APOP %s %s\r\n", PCU.userName, buf);
	fflush(fp);
	fgets(buf, BUF_SIZE, fp);

	if (!strncmp(buf, "+OK", 3))
		return fp;				/* AUTH successful */
	else {
		fprintf(stderr, "POP3 APOP AUTH failed for user '%s@%s:%d'\n",
				PCU.userName, PCU.serverName, PCU.serverPort);
		fprintf(stderr, "It said %s", buf);
		fprintf(fp, "QUIT\r\n");
		fclose(fp);
		return NULL;
	}
}
#endif

/* vim:set ts=4: */
