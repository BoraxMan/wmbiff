/* rewrite of the IMAP code by Neil Spring
 * (nspring@cs.washington.edu) to support gnutls and
 * persistent connections to servers.  */

/* Originally written by Yong-iL Joh (tolkien@mizi.com),
 * modified by Jorge Garcia (Jorge.Garcia@uv.es), and
 * modified by Jay Francis (jtf@u880.org) to support
 * CRAM-MD5 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Client.h"
#include "charutil.h"
#include "tlsComm.h"
#include "passwordMgr.h"

#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#define	PCU	(pc->u).pop_imap

#ifdef __LCLINT__
void asprintf( /*@out@ */ char **out, char *fmt, ...);
#endif

#define IMAP_DM(pc, lvl, args...) DM(pc, lvl, "imap4: " args)

/* this array maps server:port pairs to file descriptors, so
   that when more than one mailbox is queried from a server,
   we only use one socket.  It's limited in size by the
   number of different mailboxes displayed. */
#define FDMAP_SIZE 5
static struct fdmap_struct {
	char *user_password_server_port;	/* tuple, in string form */
	/*@owned@ */ struct connection_state *cs;
} fdmap[FDMAP_SIZE];

static void ask_user_for_password( /*@notnull@ */ Pop3 pc,
								  int bFlushCache);

/* authentication callbacks */
#ifdef HAVE_GCRYPT_H
static int authenticate_md5( /*@notnull@ */ Pop3 pc,
							struct connection_state *scs,
							const char *capabilities);
#endif
static int authenticate_plaintext( /*@notnull@ */ Pop3 pc,
								  struct connection_state *scs,
								  const char *capabilities);

/* the auth_methods array maps authentication identifiers
   to the callback that will attempt to authenticate */
static struct imap_authentication_method {
	const char *name;
	/* callback returns 1 if successful, 0 if failed */
	int (*auth_callback) ( /*@notnull@ */ Pop3 pc,
						  struct connection_state * scs,
						  const char *capabilities);
} auth_methods[] = {
	{
#ifdef HAVE_GCRYPT_H
	"cram-md5", authenticate_md5}, {
#endif
	"plaintext", authenticate_plaintext}, {
	NULL, NULL}
};


/* recover a socket from the connection cache */
/*@null@*/
/*@dependent@*/
static struct connection_state *state_for_pcu(Pop3 pc)
{
	char *connection_id;
	struct connection_state *retval = NULL;
	int i;
	asprintf(&connection_id, "%s|%s|%d", PCU.userName,
			 PCU.serverName, PCU.serverPort);
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
	asprintf(&connection_id, "%s|%s|%d", PCU.userName,
			 PCU.serverName, PCU.serverPort);
	for (i = 0; i < FDMAP_SIZE && fdmap[i].cs != NULL; i++);
	if (i == FDMAP_SIZE) {
		/* should never happen */
		IMAP_DM(pc, DEBUG_ERROR,
				"Tried to open too many IMAP connections. Sorry!\n");
		exit(EXIT_FAILURE);
	}
	fdmap[i].user_password_server_port = connection_id;
	fdmap[i].cs = scs;
}

/* remove from the connection cache */
static
/*@owned@*/
/*@null@*/
struct connection_state *unbind(
/*@returned@*/ struct connection_state
								   *scs)
{
	int i;
	struct connection_state *retval = NULL;
	assert(scs != NULL);

	for (i = 0; i < FDMAP_SIZE && fdmap[i].cs != scs; i++);
	if (i < FDMAP_SIZE) {
		free(fdmap[i].user_password_server_port);
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

	assert(pc != NULL);

	/* no cached connection */
	sd = sock_connect((const char *) PCU.serverName, PCU.serverPort);
	if (sd == -1) {
		IMAP_DM(pc, DEBUG_ERROR, "Couldn't connect to %s:%d: %s\n",
				PCU.serverName, PCU.serverPort, strerror(errno));
		if (errno == ETIMEDOUT) {
			/* only give up if it was a time-consuming error */
			/* try again later if it was just a connection refused */
			IMAP_DM(pc, DEBUG_ERROR,
					"Will not retry because this attempt to connect timed out.\n"
					" This is done so that other mailboxes can be updated in a timely manner.\n"
					" To try again to connect to %s:%d, restart wmbiff.\n",
					PCU.serverName, PCU.serverPort);
			bind_state_to_pcu(pc, initialize_blacklist(connection_name));
		}
		return NULL;
	}

	/* build the connection using STARTTLS */
	if (PCU.dossl != 0 && (PCU.serverPort == 143)) {
		/* setup an unencrypted binding long enough to invoke STARTTLS */
		scs = initialize_unencrypted(sd, connection_name, pc);

		/* can we? */
		tlscomm_printf(scs, "a000 CAPABILITY\r\n");
		if (tlscomm_expect(scs, "* CAPABILITY", capabilities, BUF_SIZE) ==
			0)
			goto communication_failure;

		if (!strstr(capabilities, "STARTTLS")) {
			IMAP_DM(pc, DEBUG_ERROR,
					"server doesn't support ssl imap on port 143.");
			goto communication_failure;
		}

		/* we sure can! */
		IMAP_DM(pc, DEBUG_INFO, "Negotiating TLS within IMAP");
		tlscomm_printf(scs, "a001 STARTTLS\r\n");

		if (tlscomm_expect(scs, "a001 ", buf, BUF_SIZE) == 0)
			goto communication_failure;

		if (strstr(buf, "a001 OK") == 0) {
			/* we didn't see the success message in the response */
			IMAP_DM(pc, DEBUG_ERROR, "couldn't negotiate tls. :(\n");
			goto communication_failure;
		}

		/* we don't need the unencrypted state anymore */
		/* note that communication_failure will close the 
		   socket and free via tls_close() */
		free(scs);				/* fall through will scs = initialize_gnutls(sd); */
	}

	/* either we've negotiated ssl from starttls, or
	   we're starting an encrypted connection now */
	if (PCU.dossl != 0) {
		scs = initialize_gnutls(sd, connection_name, pc);
		if (scs == NULL) {
			IMAP_DM(pc, DEBUG_ERROR, "Failed to initialize TLS\n");
			return NULL;
		}
	} else {
		scs = initialize_unencrypted(sd, connection_name, pc);
	}

	/* authenticate; first find out how */
	/* note that capabilities may have changed since last
	   time we may have asked, if we called STARTTLS, my 
	   server will allow plain password login within an 
	   encrypted session. */
	tlscomm_printf(scs, "a000 CAPABILITY\r\n");
	if (tlscomm_expect(scs, "* CAPABILITY", capabilities, BUF_SIZE) == 0) {
		IMAP_DM(pc, DEBUG_ERROR, "unable to query capability string");
		goto communication_failure;
	}

	/* try each authentication method in turn. */
	for (a = auth_methods; a->name != NULL; a++) {
		/* was it specified or did the user leave it up to us? */
		if (PCU.authList[0] == '\0'
			|| strstr(PCU.authList, a->name) != NULL)
			/* try the authentication method */
			if ((a->auth_callback(pc, scs, capabilities)) != 0) {
				/* store this well setup connection in the cache */
				bind_state_to_pcu(pc, scs);
				return NULL;
			}
	}

	/* if authentication worked, we won't get here */
	IMAP_DM(pc, DEBUG_ERROR,
			"All authentication methods failed for '%s@%s:%d'\n",
			PCU.userName, PCU.serverName, PCU.serverPort);
  communication_failure:
	tlscomm_printf(scs, "a002 LOGOUT\r\n");
	tlscomm_close(scs);
	return NULL;

}

int imap_checkmail( /*@notnull@ */ Pop3 pc)
{
	/* recover connection state from the cache */
	struct connection_state *scs = state_for_pcu(pc);
	char buf[BUF_SIZE];

	/* if it's not in the cache, try to open */
	if (scs == NULL) {
		IMAP_DM(pc, DEBUG_INFO, "Need new connection to %s@%s\n",
				PCU.userName, PCU.serverName);
		(void) imap_open(pc);
		scs = state_for_pcu(pc);
	}
	if (scs == NULL) {
		return -1;
	}

	if (tlscomm_is_blacklisted(scs) != 0) {
		/* unresponsive server, don't bother. */
		return -1;
	}

	/* if we've got it by now, try the status query */
	tlscomm_printf(scs, "a003 STATUS %s (MESSAGES UNSEEN)\r\n", pc->path);
	if (tlscomm_expect(scs, "* STATUS", buf, 127) != 0) {
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
int imap4Create( /*@notnull@ */ Pop3 pc, const char *const str)
{
	struct re_registers regs;
	int i, matchedchars;
	const char *regexes[] = {
		".*imaps?:([^: ]{1,32}):([^@]{0,32})@([^/: ]+)(/[^: ]+)?(:[0-9]+)? *",
		".*imaps?:([^: ]{1,32}) ([^ ]{1,32}) ([^/: ]+)(/[^: ]+)?( [0-9]+)? *",
		NULL
	};

	/* IMAP4 format: imap:user:password@server/mailbox[:port] */
	/* If 'str' line is badly formatted, wmbiff won't display the mailbox. */
	if (strncmp("sslimap:", str, 8) == 0 || strncmp("imaps:", str, 6) == 0) {
#ifdef HAVE_GNUTLS_H
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

	for (matchedchars = 0, i = 0;
		 regexes[i] != NULL && matchedchars <= 0; i++) {
		matchedchars = compile_and_match_regex(regexes[i], str, &regs);
	}

	/* failed to match either regex */
	if (matchedchars <= 0) {
		pc->label[0] = '\0';
		IMAP_DM(pc, DEBUG_ERROR, "Couldn't parse line %s (%d)\n", str,
				matchedchars);
		return -1;
	}

	/* copy matches where they belong */
	copy_substring(PCU.userName, regs.start[1], regs.end[1], str);
	copy_substring(PCU.password, regs.start[2], regs.end[2], str);
	if (PCU.password[0] == '\0')
		PCU.interactive_password = 1;
	copy_substring(PCU.serverName, regs.start[3], regs.end[3], str);
	if (regs.start[4] != -1)
		copy_substring(pc->path, regs.start[4] + 1, regs.end[4], str);
	else
		strcpy(pc->path, "INBOX");
	if (regs.start[5] != -1)
		PCU.serverPort = atoi(str + regs.start[5] + 1);
	else
		PCU.serverPort = (PCU.dossl != 0) ? 993 : 143;

	grab_authList(str + regs.end[0], PCU.authList);

	free(regs.end);				// added 3 jul 02, appeasing valgrind
	free(regs.start);			// added 3 jul 02, appeasing valgrind

	IMAP_DM(pc, DEBUG_INFO, "userName= '%s'\n", PCU.userName);
	IMAP_DM(pc, DEBUG_INFO, "password is %d characters long\n",
			(int) strlen(PCU.password));
	IMAP_DM(pc, DEBUG_INFO, "serverName= '%s'\n", PCU.serverName);
	IMAP_DM(pc, DEBUG_INFO, "serverPath= '%s'\n", pc->path);
	IMAP_DM(pc, DEBUG_INFO, "serverPort= '%d'\n", PCU.serverPort);
	IMAP_DM(pc, DEBUG_INFO, "authList= '%s'\n", PCU.authList);

	pc->checkMail = imap_checkmail;
	pc->TotalMsgs = 0;
	pc->UnreadMsgs = 0;
	pc->OldMsgs = -1;
	pc->OldUnreadMsgs = -1;
	return 0;
}

static int authenticate_plaintext( /*@notnull@ */ Pop3 pc,
								  struct connection_state *scs,
								  const char *capabilities)
{
	char buf[BUF_SIZE];
	/* is login prohibited? */
	/* "An IMAP client which complies with [rfc2525, section 3.2] 
	 *  MUST NOT issue the LOGIN command if this capability is present.
	 */
	if (strstr(capabilities, "LOGINDISABLED")) {
		IMAP_DM(pc, DEBUG_ERROR,
				"Plaintext auth prohibited by server: (LOGINDISABLED).\n");
		goto plaintext_failed;
	}

	ask_user_for_password(pc, 0);
	do {
		/* login */
		tlscomm_printf(scs, "a001 LOGIN %s \"%s\"\r\n", PCU.userName,
					   PCU.password);
		if (tlscomm_expect(scs, "a001 ", buf, BUF_SIZE) == 0) {
			IMAP_DM(pc, DEBUG_ERROR,
					"Did not get a response to the LOGIN command.\n");
			goto plaintext_failed;
		}

		if (buf[5] != 'O') {
			IMAP_DM(pc, DEBUG_ERROR, "IMAP Login failed.\n");
			/* if we're prompting the user, ask again, else fail */
			if (PCU.interactive_password) {
				PCU.password[0] = '\0';
				ask_user_for_password(pc, 1);	/* 1=overwrite the cache */
			} else {
				goto plaintext_failed;
			}
		} else {
			return (1);
		}
	} while (1);

  plaintext_failed:
	return (0);
}

#ifdef HAVE_GCRYPT_H
static int authenticate_md5(Pop3 pc,
							struct connection_state *scs,
							const char *capabilities)
{
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
	IMAP_DM(pc, DEBUG_INFO, "CRAM-MD5 challenge: %s\n", buf2);

	strcpy(buf, PCU.userName);
	strcat(buf, " ");
	ask_user_for_password(pc, 0);
	gmh = gcry_md_open(GCRY_MD_MD5, GCRY_MD_FLAG_HMAC);
	gcry_md_setkey(gmh, PCU.password, strlen(PCU.password));
	gcry_md_write(gmh, (unsigned char *) buf2, strlen(buf2));
	gcry_md_final(gmh);
	md5 = gcry_md_read(gmh, 0);
	Bin2Hex(md5, 16, buf2);
	gcry_md_close(gmh);

	strcat(buf, buf2);
	IMAP_DM(pc, DEBUG_INFO, "CRAM-MD5 response: %s\n", buf);
	Encode_Base64(buf, buf2);

	tlscomm_printf(scs, "%s\r\n", buf2);
	if (!tlscomm_expect(scs, "a007 ", buf, BUF_SIZE))
		goto expect_failure;

	if (!strncmp(buf, "a007 OK", 7))
		return 1;				/* AUTH successful */

	IMAP_DM(pc, DEBUG_ERROR,
			"CRAM-MD5 AUTH failed for user '%s@%s:%d'\n",
			PCU.userName, PCU.serverName, PCU.serverPort);
	IMAP_DM(pc, DEBUG_INFO, "It said %s", buf);
	return 0;

  expect_failure:
	IMAP_DM(pc, DEBUG_ERROR,
			"tlscomm_expect failed during cram-md5 auth: %s", buf);
	IMAP_DM(pc, DEBUG_ERROR, "failed to authenticate using cram-md5.");
	return 0;
}
#endif

static void ask_user_for_password( /*@notnull@ */ Pop3 pc, int bFlushCache)
{
	/* see if we already have a password, as provided in the config file, or
	   already requested from the user. */
	if (PCU.interactive_password) {
		if (strlen(PCU.password) == 0) {
			/* we need to grab the password from the user. */
			const char *password;
			IMAP_DM(pc, DEBUG_INFO, "asking for password %d\n",
					bFlushCache);
			password =
				passwordFor(PCU.userName, PCU.serverName, pc, bFlushCache);
			if (password != NULL) {
				strcpy(PCU.password, password);
			}
		}
	}
}

/* vim:set ts=4: */
/*
 * Local Variables:
 * tab-width: 4
 * c-indent-level: 4
 * c-basic-offset: 4
 * End:
 */
