/* $Id: Pop3Client.c,v 1.2 2001/06/19 03:38:58 dwonis Exp $ */
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

#define	PCU	(pc->u).pop

FILE *pop3Login(Pop3 pc)
{
	int fd;
	FILE *fp;
	char buf[BUF_SIZE];

	if ((fd = sock_connect(PCU.serverName, PCU.serverPort)) == -1) {
		fprintf(stderr, "Not Connected To Server '%s:%d'\n",
				PCU.serverName, PCU.serverPort);
		return NULL;
	}

	fp = fdopen(fd, "r+");
	fgets(buf, BUF_SIZE, fp);

	fflush(fp);
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


/* vim:set ts=4: */
