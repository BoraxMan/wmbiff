/* Author : Yong-iL Joh ( tolkien@mizi.com )
   Modified: Jorge García ( Jorge.Garcia@uv.es )
 * 
 * Imap4 Email checker.
 *
 * Last Updated : Mar 20, 05:32:35 CET 2001
 *
 */

#include "Client.h"

#define	PCU	(pc->u).pop

FILE *imap4Login(Pop3 pc)
{
	int fd;
	FILE *f;
	char buf[BUF_SIZE];

	if ((fd = sock_connect(PCU.serverName, PCU.serverPort)) == -1) {
		fprintf(stderr, "Not Connected To Server '%s:%d'\n",
				PCU.serverName, PCU.serverPort);
		return NULL;
	}

	f = fdopen(fd, "r+");
	fgets(buf, BUF_SIZE, f);

	/* Login to the server */
	fflush(f);
	fprintf(f, "a001 LOGIN %s %s\r\n", PCU.userName, PCU.password);

	/* Ensure that the buffer is not an informational line */
	do {
		fflush(f);
		fgets(buf, BUF_SIZE, f);
	}
	while (buf[0] == '*');

	if (buf[5] != 'O') {		/* Looking for "a001 OK" */
		fprintf(f, "a002 LOGOUT\r\n");
		fclose(f);
		return NULL;
	};

	return f;
}

int imap4CheckMail(Pop3 pc)
{
	FILE *f;

	f = pc->open(pc);
	if (f == NULL)
		return -1;

	fflush(f);

	fprintf(f, "a004 LOGOUT\r\n");
	fclose(f);

	return 0;
}

int imap4Create(Pop3 pc, char *str)
{
	/* IMAP4 format: imap:user:password@server/mailbox[:port] */
	/* If 'str' line is badly formatted, wmbiff won't display the mailbox. */
	char *tmp;
	char *p;

#ifdef DEBUG_IMAP4
	printf("imap4: str = '%s'\n", str);
#endif

	strcpy(PCU.password, "");
	strcpy(PCU.userName, "");
	strcpy(PCU.serverName, "");
	PCU.serverPort = 143;

	tmp = strdup(str);

	/* We start with imap:user:password@server[/mailbox][:port] */
	p = strtok(tmp, ":");		/* cut off ``imap:'' */
	/* Now, we have user:password@server[/mailbox][:port] */
	if ((p = strtok(NULL, ":"))) {	/* p pointed to username */
		strcpy(PCU.userName, p);
		/* Now, we have password@server[/mailbox][:port] */
		if ((p = strtok(NULL, "@"))) {	/* p -> password */
			strcpy(PCU.password, p);
			/* Now we have server[/mailbox][:port] */
			if ((p = strtok(NULL, "/"))) {	/* p -> server */
				strcpy(PCU.serverName, p);
				/* It should now be [mailbox][:port] */
				if ((p = strtok(NULL, ":"))) {	/* p -> mailbox */
					strcpy(pc->path, p);
				} else {
					strcpy(pc->path, "INBOX");
				}
				/* and finally [port] */
				if ((p = strtok(NULL, ":"))) {	/* port selected; p -> port */
					PCU.serverPort = atoi(p);
				}
				free(tmp);

#ifdef DEBUG_IMAP4
				printf("imap4: userName= '%s'\n", PCU.userName);
				printf("imap4: password= '%s'\n", PCU.password);
				printf("imap4: serverName= '%s'\n", PCU.serverName);
				printf("imap4: serverPath= '%s'\n", pc->path);
				printf("imap4: serverPort= '%d'\n", PCU.serverPort);
#endif
				pc->open = imap4Login;
				pc->checkMail = imap4CheckMail;
				pc->TotalMsgs = 0;
				pc->UnreadMsgs = 0;
				pc->OldMsgs = -1;
				pc->OldUnreadMsgs = -1;
				return 0;
			}
		}
	}
	/* The line is badly formatted, we're not creating the mailbox line */
	pc->label[0] = 0;
	fprintf(stderr, "config line with bad format '%s'.\n", str);
	return -1;
}

/* vim:set ts=4: */
