/* rewrite of the IMAP code by Neil Spring
 * (nspring@cs.washington.edu) to support gnutls and
 * persistent connections to servers.  */

/* Originally written by Yong-iL Joh (tolkien@mizi.com),
 * modified by Jorge Garcia (Jorge.Garcia@uv.es), and
 * modified by Jay Francis (jtf@u880.org) to support
 * CRAM-MD5 */

/* get asprintf */
#define _GNU_SOURCE

#include "Client.h"
#include "charutil.h"
#include "tlsComm.h"

#include <sys/types.h>
#include <regex.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

/* emulates what 'variadic macros' are great for */
#ifdef DEBUG_IMAP4
#define DM printf
#else
#define DM nullie
/*@unused@*/
static void nullie( /*@unused@ */ const char *format, ...)
{
	return;
}
#endif

#define	PCU	(pc->u).imap

#ifdef __LCLINT__
void asprintf( /*@out@ */ char **out, char *fmt, ...);
#endif

/* this array maps server:port pairs to file descriptors, so
   that when more than one mailbox is queried from a server,
   we only use one socket.  It's limited in size by the
   number of different mailboxes displayed. */
#define FDMAP_SIZE 5
static struct fdmap_struct {
	const char *user_password_server_port;	/* tuple, in string form */
	/*@owned@ */ struct connection_state *cs;
} fdmap[FDMAP_SIZE];


/* authentication callbacks */
static int authenticate_md5(Pop3 pc, struct connection_state *scs,
							const char *capabilities);
static int authenticate_plaintext(Pop3 pc, struct connection_state *scs,
								  const char *capabilities);

/* the auth_methods array maps authentication identifiers
   to the callback that will attempt to authenticate */
static struct imap_authentication_method {
	const char *name;
	/* callback returns 1 if successful, 0 if failed */
	int (*auth_callback) (Pop3 pc, struct connection_state * scs,
						  const char *capabilities);
} auth_methods[] = {
	{
	"cram-md5", authenticate_md5}, {
	"plaintext", authenticate_plaintext}, {
	NULL, NULL}
};


/* recover a socket from the connection cache */
/*@null@*//*@dependent@ */
static struct connection_state *state_for_pcu(Pop3 pc)
{
	char *connection_id;
	struct connection_state *retval = NULL;
	int i;
	asprintf(&connection_id, "%s|%s|%s|%d", PCU.userName,
			 PCU.password, PCU.serverName, PCU.serverPort);
	for (i = 0; i < FDMAP_SIZE; i++)
		if (fdmap[i].user_password_server_port != NULL &&
			(strcmp(connection_id,
					fdmap[i].user_password_server_port) == 0)) {
			retval = fdmap[i].cs;
		}
	free(connection_id);
	return (retval);
}

/* bind to the connection cache */
static void bind_state_to_pcu(Pop3 pc,
							  /*@owned@ */ struct connection_state *scs)
{
	char *connection_id;
	int i;
	if (scs == NULL) {
		abort();
	}
	asprintf(&connection_id, "%s|%s|%s|%d", PCU.userName,
			 PCU.password, PCU.serverName, PCU.serverPort);
	for (i = 0; i < FDMAP_SIZE && fdmap[i].cs != NULL; i++);
	if (i == FDMAP_SIZE) {
		/* should never happen */
		printf
			("wmbiff: Tried to open too many IMAP connections. Sorry!\n");
		exit(EXIT_FAILURE);
	}
	fdmap[i].user_password_server_port = connection_id;
	fdmap[i].cs = scs;
}

/* remove from the connection cache */
static
/*@owned@*//*@null@ */
struct connection_state *unbind( /*@returned@ */ struct connection_state
								*scs)
{
	int i;
	struct connection_state *retval = NULL;
	assert(scs != NULL);

	for (i = 0; i < FDMAP_SIZE && fdmap[i].cs != scs; i++);
	if (i < FDMAP_SIZE) {
		free((char *) fdmap[i].user_password_server_port);
		retval = fdmap[i].cs;
		fdmap[i].cs = NULL;
	}
	return (retval);
}

/* creates a connection to the server, if a matching one doesn't exist. */
/* *always* returns null, just declared this wasy to match other protocols. */
/*@null@*/
FILE *imap_open(Pop3 pc)
{
	struct connection_state *scs;
	struct imap_authentication_method *a;
	char *connection_name;
	int sd;
	char capabilities[BUF_SIZE];
	char buf[BUF_SIZE];


	if (state_for_pcu(pc) != NULL) {
		/* don't need to open. */
		return NULL;
	}

	/* got this far; we're going to create a connection_state 
	   structure, although it might be a blacklist entry */
	asprintf(&connection_name, "%s:%d", PCU.serverName, PCU.serverPort);

	/* no cached connection */
	sd = sock_connect((const char *) PCU.serverName, PCU.serverPort);
	if (sd == -1) {
		fprintf(stderr, "Couldn't connect to %s:%d: %s\n",
				PCU.serverName, PCU.serverPort, strerror(errno));
		if (errno == ETIMEDOUT) {
			/* only give up if it was a time-consuming error */
			/* try again later if it was just a connection refused */
			fprintf(stderr,
					"Will not retry because this attempt to connect timed out.\n");
			fprintf(stderr,
					"This is done so that other mailboxes can be updated in a timely manner.\n");
			fprintf(stderr,
					"To try again to connect to %s:%d, restart wmbiff.\n",
					PCU.serverName, PCU.serverPort);
			bind_state_to_pcu(pc, initialize_blacklist(connection_name));
		}
		return NULL;
	}

	/* build the connection using STARTTLS */
	if (PCU.dossl && (PCU.serverPort == 143)) {
		/* setup an unencrypted binding long enough to invoke STARTTLS */
		scs = initialize_unencrypted(sd, connection_name);

		/* can we? */
		tlscomm_printf(scs, "a000 CAPABILITY\r\n");
		if (!tlscomm_expect(scs, "* CAPABILITY", capabilities, BUF_SIZE))
			goto communication_failure;

		if (!strstr(capabilities, "STARTTLS")) {
			printf("server doesn't support ssl imap on port 143.");
			goto communication_failure;
		}

		/* we sure can! */
		DM("Negotiating TLS within IMAP");
		tlscomm_printf(scs, "a001 STARTTLS\r\n");

		if (!tlscomm_expect(scs, "a001 ", buf, BUF_SIZE))
			goto communication_failure;

		if (!strstr(buf, "a001 OK")) {
			printf("couldn't negotiate tls. :(\n");
			goto communication_failure;
		}

		/* we don't need the unencrypted state anymore */
		/* note that communication_failure will close the 
		   socket and free via tls_close() */
		free(scs);				/* fall through will scs = initialize_gnutls(sd); */
	}

	/* either we've negotiated ssl from starttls, or
	   we're starting an encrypted connection now */
	if (PCU.dossl) {
		scs = initialize_gnutls(sd, connection_name);
		if (scs == NULL) {
			fprintf(stderr, "Failed to initialize TLS\n");
			return NULL;
		}
	} else {
		scs = initialize_unencrypted(sd, connection_name);
	}

	/* authenticate; first find out how */
	/* note that capabilities may have changed since last
	   time we may have asked, if we called STARTTLS, my 
	   server will allow plain password login within an 
	   encrypted session. */
	tlscomm_printf(scs, "a000 CAPABILITY\r\n");
	if (!tlscomm_expect(scs, "* CAPABILITY", capabilities, BUF_SIZE)) {
		printf("unable to query capability string");
		goto communication_failure;
	}

	/* try each authentication method in turn. */
	for (a = auth_methods; a->name != NULL; a++) {
		/* was it specified or did the user leave it up to us? */
		if (PCU.authList[0] == '\0' || strstr(PCU.authList, a->name))
			/* try the authentication method */
			if ((a->auth_callback(pc, scs, capabilities)) != 0) {
				/* store this well setup connection in the cache */
				bind_state_to_pcu(pc, scs);
				return NULL;
			}
	}

	/* if authentication worked, we won't get here */
	fprintf(stderr,
			"All Imap4 authentication methods failed for '%s@%s:%d'\n",
			PCU.userName, PCU.serverName, PCU.serverPort);
  communication_failure:
	tlscomm_printf(scs, "a002 LOGOUT\r\n");
	tlscomm_close(scs);
	return NULL;

}

int imap_checkmail(Pop3 pc)
{
	/* recover connection state from the cache */
	struct connection_state *scs = state_for_pcu(pc);
	char buf[BUF_SIZE];

	/* if it's not in the cache, try to open */
	if (scs == NULL) {
		(void) pc->open(pc);
		scs = state_for_pcu(pc);
	}
	if (scs == NULL) {
		return -1;
	}

	if (tlscomm_is_blacklisted(scs)) {
		/* unresponsive server, don't bother. */
		return -1;
	}

	/* if we've got it by now, try the status query */
	tlscomm_printf(scs, "a003 STATUS %s (MESSAGES UNSEEN)\r\n", pc->path);
	if (tlscomm_expect(scs, "* STATUS", buf, 127)) {
		/* a valid response? */
		(void) sscanf(buf, "* STATUS %*s (MESSAGES %d UNSEEN %d)",
					  &(pc->TotalMsgs), &(pc->UnreadMsgs));
	} else {
		/* something went wrong. bail. */
		tlscomm_close(unbind(scs));
		return -1;
	}
	return 0;
}

/* parse the config line to setup the Pop3 structure */
int imap4Create(Pop3 pc, const char *const str)
{
	struct re_registers regs;
	const char *regex =
		".*imaps?:([^:]{1,32}):([^@]{1,32})@([^/: ]+)(/[^: ]+)?(:[0-9]+)? *";

	/* IMAP4 format: imap:user:password@server/mailbox[:port] */
	/* If 'str' line is badly formatted, wmbiff won't display the mailbox. */
	if (strncmp("sslimap:", str, 8) == 0 || strncmp("imaps:", str, 6) == 0) {
#ifdef WITH_TLS
		static int haveBeenWarned;
		PCU.dossl = 1;
		if (!haveBeenWarned) {
			printf("wmbiff uses gnutls for TLS/SSL encryption support:\n"
				   "  If you distribute software that uses gnutls, don't forget\n"
				   "  to warn the users of your software that gnutls is at a\n"
				   "  testing phase and may be totally insecure.\n"
				   "\nConsider yourself warned.\n");
			haveBeenWarned = 1;
		}
#else
		printf("This copy of wmbiff was not compiled with gnutls;\n"
			   "imaps is unavailable.  Exiting to protect your\n"
			   "passwords and privacy.\n");
		exit(EXIT_FAILURE);
#endif
	} else
		PCU.dossl = 0;

	if (compile_and_match_regex(regex, str, &regs) <= 0) {
		pc->label[0] = '\0';
		fprintf(stderr, "Couldn't parse line %s\n", str);
		return -1;
	}

	/* copy matches where they belong */
	copy_substring(PCU.userName, regs.start[1], regs.end[1], str);
	copy_substring(PCU.password, regs.start[2], regs.end[2], str);
	copy_substring(PCU.serverName, regs.start[3], regs.end[3], str);
	if (regs.start[4] != -1)
		copy_substring(pc->path, regs.start[4] + 1, regs.end[4], str);
	else
		strcpy(pc->path, "INBOX");
	if (regs.start[5] != -1)
		PCU.serverPort = atoi(str + regs.start[5] + 1);
	else
		PCU.serverPort = (PCU.dossl) ? 993 : 143;

	grab_authList(str + regs.end[0], PCU.authList);

	DM("imap4: userName= '%s'\n", PCU.userName);
	DM("imap4: password= '%s'\n", PCU.password);
	DM("imap4: serverName= '%s'\n", PCU.serverName);
	DM("imap4: serverPath= '%s'\n", pc->path);
	DM("imap4: serverPort= '%d'\n", PCU.serverPort);
	DM("imap4: authList= '%s'\n", PCU.authList);

	pc->open = imap_open;
	pc->checkMail = imap_checkmail;
	pc->TotalMsgs = 0;
	pc->UnreadMsgs = 0;
	pc->OldMsgs = -1;
	pc->OldUnreadMsgs = -1;
	return 0;
}

static int authenticate_plaintext(Pop3 pc,
								  struct connection_state *scs,
								  const char *capabilities)
{
	char buf[BUF_SIZE];
	/* is login prohibited? */
	/* "An IMAP client which complies with [rfc2525, section 3.2] 
	 *  MUST NOT issue the LOGIN command if this capability is present.
	 */
	if (strstr(capabilities, "LOGINDISABLED")) {
		printf("Plaintext auth prohibited by server: (LOGINDISABLED).\n");
		goto plaintext_failed;
	}

	/* login */
	tlscomm_printf(scs, "a001 LOGIN %s \"%s\"\r\n", PCU.userName,
				   PCU.password);
	if (!tlscomm_expect(scs, "a001 ", buf, BUF_SIZE)) {
		printf("unable to login");
		goto plaintext_failed;
	}

	if (buf[5] != 'O') {
		printf("login failed\n");
		goto plaintext_failed;
	}
	return (1);
  plaintext_failed:
	return (0);
}

static int authenticate_md5(Pop3 pc,
							struct connection_state *scs,
							const char *capabilities)
{
#ifdef WITH_GCRYPT
	char buf[BUF_SIZE];
	char buf2[BUF_SIZE];
	unsigned char *md5;
	GCRY_MD_HD gmh;

	if (!strstr(capabilities, "AUTH=CRAM-MD5")) {
		/* server doesn't support cram-md5. */
		return 0;
	}

	tlscomm_printf(scs, "a007 AUTHENTICATE CRAM-MD5\r\n");
	if (!tlscomm_expect(scs, "+ ", buf, BUF_SIZE))
		goto expect_failure;

	Decode_Base64(buf + 2, buf2);
	DM("IMAP4 CRAM-MD5 challenge: %s\n", buf2);

	strcpy(buf, PCU.userName);
	strcat(buf, " ");
	gmh = gcry_md_open(GCRY_MD_MD5, GCRY_MD_FLAG_HMAC);
	gcry_md_setkey(gmh, PCU.password, strlen(PCU.password));
	gcry_md_write(gmh, (unsigned char *) buf2, strlen(buf2));
	gcry_md_final(gmh);
	md5 = gcry_md_read(gmh, 0);
	Bin2Hex(md5, 16, buf2);
	gcry_md_close(gmh);

	strcat(buf, buf2);
	DM("IMAP4 CRAM-MD5 response: %s\n", buf);
	Encode_Base64(buf, buf2);

	tlscomm_printf(scs, "%s\r\n", buf2);
	if (!tlscomm_expect(scs, "a007 ", buf, BUF_SIZE))
		goto expect_failure;

	if (!strncmp(buf, "a007 OK", 7))
		return 1;				/* AUTH successful */

	fprintf(stderr,
			"IMAP4 CRAM-MD5 AUTH failed for user '%s@%s:%d'\n",
			PCU.userName, PCU.serverName, PCU.serverPort);
	fprintf(stderr, "It said %s", buf);
	return 0;

  expect_failure:
	fprintf(stderr, "tlscomm_expect failed during cram-md5 auth: %s", buf);
	return 0;
#else
	/* not compiled with gcrypt. */
	return 0;
#endif
}
