/* Author : NAKAYAMA Takao ( hoehoe@wakaba.jp )
 * 
 * Apop Email checker.
 *
 * Last Updated : Mon Sep  3 21:47:25 JST 2001
 *
 */

#include "Client.h"
#include <sys/types.h>
#include <md5.h>

#define	PCU	(pc->u).pop

FILE *apopLogin(Pop3 pc)
{
	int fd;
	FILE *fp;
	char buf[BUF_SIZE];
	char md5con[128], md5key[128], md5tmp[128];
	char chksum[64];

	if ((fd = sock_connect(PCU.serverName, PCU.serverPort)) == -1) {
		fprintf(stderr, "Not Connected To Server '%s:%d'\n",
				PCU.serverName, PCU.serverPort);
		return NULL;
	}

	fp = fdopen(fd, "r+");
	fgets(buf, BUF_SIZE, fp);


	/* cut md5 keyword from pop server */

	bzero(md5key, 128);
	bzero(md5tmp, 128);
	bzero(md5con, 128);
	strncpy(md5tmp, strchr(buf, '<'), 128);
	strncpy(md5key, md5tmp,
			strlen(md5tmp) - strlen(strchr(md5tmp, '>')) + 1);

#ifdef DEBUG_APOP
	fprintf(stderr, "md5key=%s\n", md5key);
#endif

	/* calculate MD5-chkcksum for APOP */
	strncpy(md5con, md5key, strlen(md5key));
	strncat(md5con, PCU.password, strlen(PCU.password));
	MD5Data(md5con, strlen(md5con), chksum);

#ifdef DEBUG_APOP
	fprintf(stderr, "md5con=%s\n", md5con);
	fprintf(stderr, "chksum=%s\n", chksum);
	fflush(stderr);
#endif

	fflush(fp);
	fprintf(fp, "APOP %s %s\r\n", PCU.userName, chksum);
	fflush(fp);
	fgets(buf, BUF_SIZE, fp);
	if (buf[0] != '+') {
		fprintf(stderr, "Invalid checksum '%s@%s:%d'\n",
				PCU.userName, PCU.serverName, PCU.serverPort);
#ifdef DEBUG_APOP
		fprintf(stderr, "%s\n", buf);
#endif
		fprintf(fp, "QUIT\r\n");
		fclose(fp);
		return NULL;
	};

	fflush(fp);

	return fp;
}

int apopCheckMail(Pop3 pc)
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

int apopCreate(Pop3 pc, char *str)
{
	/* APOP format: apop:user:password@server[:port] */
	/* new APOP format: apop:user password server [port] */
	/* If 'str' line is badly formatted, wmbiff won't display the mailbox. */

	/* old config file style... */
	if (parse_old_apop_path(pc, str) != 0)

		/* new config file style... */
		if (parse_new_apop_path(pc, str) != 0) {

			/* The line is badly formatted,
			 * we're not creating the mailbox line */
			pc->label[0] = 0;
			fprintf(stderr, "config line with bad format '%s'.\n", str);
			return -1;
		}

	return 0;
}

/* parse the apop config line
 * one simple function with a delimiter won't work,
 * because of the '@' before server in old style!
 */
int parse_old_apop_path(Pop3 pc, char *str)
{
	char *tmp;
	char *p;

#ifdef DEBUG_APOP
	printf("apop: str = '%s'\n", str);
#endif

	strcpy(PCU.password, "");
	strcpy(PCU.userName, "");
	strcpy(PCU.serverName, "");
	PCU.serverPort = 110;

	tmp = strdup(str);

	p = strtok(tmp, ":");		/* cut off 'apop:' */

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

#ifdef DEBUG_APOP
				printf("apop: userName= '%s'\n", PCU.userName);
				printf("apop: password= '%s'\n", PCU.password);
				printf("apop: serverName= '%s'\n", PCU.serverName);
				printf("apop: serverPort= '%d'\n", PCU.serverPort);
#endif
				pc->open = apopLogin;
				pc->checkMail = apopCheckMail;
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
int parse_new_apop_path(Pop3 pc, char *str)
{
	char *tmp;
	char *p;
	char *delim = " ";

#ifdef DEBUG_APOP
	printf("apop: str = '%s'\n", str);
#endif

	strcpy(PCU.password, "");
	strcpy(PCU.userName, "");
	strcpy(PCU.serverName, "");
	PCU.serverPort = 110;

	tmp = strdup(str);

	p = strtok(tmp, ":");		/* cut off 'apop:' */

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

#ifdef DEBUG_APOP
				printf("apop: userName= '%s'\n", PCU.userName);
				printf("apop: password= '%s'\n", PCU.password);
				printf("apop: serverName= '%s'\n", PCU.serverName);
				printf("apop: serverPort= '%d'\n", PCU.serverPort);
#endif
				pc->open = apopLogin;
				pc->checkMail = apopCheckMail;
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


/* vim:set ts=4: */
