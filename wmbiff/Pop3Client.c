/* $Id: Pop3Client.c,v 1.5 2001/11/16 07:11:04 bluehal Exp $ */
/* Author : Scott Holden ( scotth@thezone.net )
   Modified : Yong-iL Joh ( tolkien@mizi.com )
   Modified : Jorge García ( Jorge.Garcia@uv.es )
   Modified ; Mark Hurley ( debian4tux@telocity.com )
   Modified : Neil Spring ( nspring@cs.washington.edu )
 * 
 * Pop3 Email checker.
 *
 * Last Updated : Tue Nov 13 13:45:23 PST 2001
 *
 */

#include "Client.h"
#include "charutil.h"
#include <regex.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#define	PCU	(pc->u).pop

/* emulates what 'variadic macros' are great for */
#ifdef DEBUG_POP3
#define DM printf
#else
#define DM nullie
static void nullie(const char *format, ...)
{
	return;
}
#endif

static FILE *authenticate_md5(Pop3 pc, FILE * fp, char *unused);
static FILE *authenticate_apop(Pop3 pc, FILE * fp, char *apop_str);
static FILE *authenticate_plaintext(Pop3 pc, FILE * fp, char *unused);

static struct authentication_method {
	const char *name;
	/* callback returns the filehandle if successful, 
	   NULL if failed */
	FILE *(*auth_callback) (Pop3 pc, FILE * fp, char *apop_str);
} auth_methods[] = {
	{
	"cram-md5", authenticate_md5}, {
	"apop", authenticate_apop}, {
	"plaintext", authenticate_plaintext}, {
	NULL, NULL}
};

FILE *pop3Login(Pop3 pc)
{
	int fd;
	FILE *fp;
	char buf[BUF_SIZE];
	char apop_str[BUF_SIZE];
	char *ptr1, *ptr2;
	struct authentication_method *a;

	apop_str[0] = '\0';			/* if defined, server supports apop */

	if ((fd = sock_connect(PCU.serverName, PCU.serverPort)) == -1) {
		fprintf(stderr, "Not Connected To Server '%s:%d'\n",
				PCU.serverName, PCU.serverPort);
		return NULL;
	}

	fp = fdopen(fd, "r+");
	fgets(buf, BUF_SIZE, fp);
	fflush(fp);
	DM("%s", buf);

	/* Detect APOP, copy challenge into apop_str */
	for (ptr1 = buf + strlen(buf), ptr2 = NULL; ptr1 > buf; --ptr1) {
		if (*ptr1 == '>') {
			ptr2 = ptr1;
		} else if (*ptr1 == '<') {
			if (ptr2) {
				*(ptr2 + 1) = 0;
				strncpy(apop_str, ptr1, BUF_SIZE);
			}
			break;
		}
	}

	/* try each authentication method in turn. */
	for (a = auth_methods; a->name != NULL; a++) {
		/* was it specified or did the user leave it up to us? */
		if (PCU.authList[0] == '\0' || strstr(PCU.authList, a->name))
			/* did it work? */
			if ((a->auth_callback(pc, fp, apop_str)) != NULL)
				return (fp);
	}

	/* if authentication worked, we won't get here */
	fprintf(stderr,
			"All Pop3 authentication methods failed for '%s@%s:%d'\n",
			PCU.userName, PCU.serverName, PCU.serverPort);
	fprintf(fp, "QUIT\r\n");
	fclose(fp);
	return NULL;
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

int pop3Create(Pop3 pc, const char *str)
{
	/* POP3 format: pop3:user:password@server[:port] */
	/* new POP3 format: pop3:user password server [port] */
	/* If 'str' line is badly formatted, wmbiff won't display the mailbox. */
	int i;
	int matchedchars;
	struct re_registers regs;
	/* ([^: ]+) user
	   ([^@]+) or ([^ ]+) password 
	   ([^: ]+) server 
	   ([: ][0-9]+)? optional port 
	   ' *' gobbles trailing whitespace before authentication types.
	   use separate regexes for old and new types to permit
	   use of '@' in passwords
	 */
	const char *regexes[] = {
		"pop3:([^: ]{1,32}) ([^ ]{1,32}) ([^: ]+)( [0-9]+)? *",
		"pop3:([^: ]{1,32}):([^@]{1,32})@([^: ]+)(:[0-9]+)? *",
		NULL
	};

	for (matchedchars = 0, i = 0;
		 regexes[i] != NULL && matchedchars <= 0; i++) {
		matchedchars = compile_and_match_regex(regexes[i], str, &regs);
	}

	/* failed to match either regex */
	if (matchedchars <= 0) {
		pc->label[0] = '\0';
		fprintf(stderr, "Couldn't parse line %s (%d)\n", str,
				matchedchars);
		return -1;
	}

	/* copy matches where they belong */
	copy_substring(PCU.userName, regs.start[1], regs.end[1], str);
	copy_substring(PCU.password, regs.start[2], regs.end[2], str);
	copy_substring(PCU.serverName, regs.start[3], regs.end[3], str);
	if (regs.start[4] != -1)
		PCU.serverPort = atoi(str + regs.start[4] + 1);
	else
		PCU.serverPort = 110;

	grab_authList(str + regs.end[0], PCU.authList);

	DM("pop3: userName= '%s'\n", PCU.userName);
	DM("pop3: password= '%s'\n", PCU.password);
	DM("pop3: serverName= '%s'\n", PCU.serverName);
	DM("pop3: serverPort= '%d'\n", PCU.serverPort);
	DM("pop3: authList= '%s'\n", PCU.authList);

	pc->open = pop3Login;
	pc->checkMail = pop3CheckMail;
	pc->TotalMsgs = 0;
	pc->UnreadMsgs = 0;
	pc->OldMsgs = -1;
	pc->OldUnreadMsgs = -1;
	return 0;
}


static FILE *authenticate_md5(Pop3 pc, FILE * fp, char *apop_str)
{
#ifdef WITH_GCRYPT
	char buf[BUF_SIZE];
	char buf2[BUF_SIZE];
	unsigned char *md5;
	GCRY_MD_HD gmh;

	/* See if MD5 is supported */
	fprintf(fp, "AUTH CRAM-MD5\r\n");
	fflush(fp);
	fgets(buf, BUF_SIZE, fp);
	DM("%s", buf);

	if (buf[0] != '+' || buf[1] != ' ') {
		/* nope, not supported. */
		return NULL;
	}

	Decode_Base64(buf + 2, buf2);
	DM("POP3 CRAM-MD5 challenge: %s\n", buf2);

	strcpy(buf, PCU.userName);
	strcat(buf, " ");


	gmh = gcry_md_open(GCRY_MD_MD5, GCRY_MD_FLAG_HMAC);
	gcry_md_setkey(gmh, PCU.password, strlen(PCU.password));
	gcry_md_write(gmh, (unsigned char *) buf2, strlen(buf2));
	gcry_md_final(gmh);
	md5 = gcry_md_read(gmh, 0);
	/* hmac_md5(buf2, strlen(buf2), PCU.password,
	   strlen(PCU.password), md5); */
	Bin2Hex(md5, 16, buf2);
	gcry_md_close(gmh);

	strcat(buf, buf2);
	DM("POP3 CRAM-MD5 response: %s\n", buf);
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
		return NULL;
	}
#else
	return NULL;
#endif
}

static FILE *authenticate_apop(Pop3 pc, FILE * fp, char *apop_str)
{
#ifdef WITH_GCRYPT
	GCRY_MD_HD gmh;
	char buf[BUF_SIZE];
	unsigned char *md5;

	if (apop_str[0] == '\0') {
		/* server doesn't support apop. */
		return (NULL);
	}
	DM("POP3 APOP challenge: %s\n", apop_str);
	strcat(apop_str, PCU.password);

	gmh = gcry_md_open(GCRY_MD_MD5, 0);
	gcry_md_write(gmh, (unsigned char *) apop_str, strlen(apop_str));
	gcry_md_final(gmh);
	md5 = gcry_md_read(gmh, 0);
	Bin2Hex(md5, 16, buf);
	gcry_md_close(gmh);

	DM("POP3 APOP response: %s %s\n", PCU.userName, buf);
	fprintf(fp, "APOP %s %s\r\n", PCU.userName, buf);
	fflush(fp);
	fgets(buf, BUF_SIZE, fp);

	if (!strncmp(buf, "+OK", 3))
		return fp;				/* AUTH successful */
	else {
		fprintf(stderr, "POP3 APOP AUTH failed for user '%s@%s:%d'\n",
				PCU.userName, PCU.serverName, PCU.serverPort);
		fprintf(stderr, "It said %s", buf);
		return NULL;
	}
#else
	return NULL;
#endif							/* WITH_GCRYPT */
}

static FILE *authenticate_plaintext(Pop3 pc, FILE * fp, char *apop_str)
{
	char buf[BUF_SIZE];

	fprintf(fp, "USER %s\r\n", PCU.userName);
	fflush(fp);
	fgets(buf, BUF_SIZE, fp);
	if (buf[0] != '+') {
		fprintf(stderr, "Invalid User Name '%s@%s:%d'\n",
				PCU.userName, PCU.serverName, PCU.serverPort);
		DM("%s\n", buf);
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
		return NULL;
	};

	return fp;
}

/* vim:set ts=4: */
