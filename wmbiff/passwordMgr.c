/* passwordMgr.c 
 * Author: Neil Spring 
 */
/* this module implements a password cache: the goal is to
   allow multiple wmbiff mailboxes that are otherwise
   independent get all their passwords while only asking the
   user for an account's password once. */
/* NOTE: it will fail if a user has different passwords for
   pop vs. imap on the same server; this seems too far
   fetched to be worth complexity */

/* NOTE: it verifies that the askpass program, which, if
   given with a full path, must be owned either by the user
   or by root.  There may be decent reasons not to do
   this. */

/* Intended properties: 1) exit()s if the user presses
   cancel from askpass - this is detected by no output from
   askpass.  2) allows the caller to remove a cached entry
   if it turns out to be wrong, and prompt the user
   again. This might be poor if the askpass program is
   replaced with something non-interactive. */

#include "passwordMgr.h"
#include "Client.h"
#include "charutil.h"			/* chomp */
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

typedef struct password_binding_struct {
	struct password_binding_struct *next;
	char user[32];
	char server[255];
	char password[32];
} *password_binding;

password_binding pass_list = NULL;

/* verifies that askpass_fname, if it has no spaces, exists as 
   a file, is owned by the user or by root, and is not world 
   writeable.   This is just a sanity check, and is not intended 
   to ensure the integrity of the password-asking program. */
static int permissions_ok(Pop3 pc, const char *askpass_fname)
{
	struct stat st;
	if (index(askpass_fname, ' ')) {
		DM(pc, DEBUG_INFO,
		   "askpass has a space in it; not verifying ownership/permissions on '%s'\n",
		   askpass_fname);
		return (1);
	}
	if (stat(askpass_fname, &st)) {
		DM(pc, DEBUG_ERROR, "Can't stat askpass program: '%s'\n",
		   askpass_fname);
		DM(pc, DEBUG_ERROR, "For your own good, use a full pathname.\n");
		return (0);
	}
	if (st.st_uid != 0 && st.st_uid != getuid()) {
		DM(pc, DEBUG_ERROR,
		   "askpass program isn't owned by you or root: '%s'\n",
		   askpass_fname);
		return (0);
	}
	if (st.st_mode & S_IWOTH) {
		DM(pc, DEBUG_ERROR,
		   "askpass program is world writable: '%s'\n", askpass_fname);
		return (0);
	}
	return (1);
}

const char *passwordFor(const char *username,
						const char *servername, Pop3 pc, int bFlushCache)
{

	password_binding p;

	/* find the binding */
	for (p = pass_list;
		 p != NULL
		 && (strcmp(username, p->user) != 0 ||
			 strcmp(servername, p->server) != 0); p = p->next);

	/* if so, return the password */
	if (p != NULL) {
		if (p->password[0] != '\0') {
			if (bFlushCache == 0)
				return (p->password);
			/* else fall through, overwrite */
		} else if (pc) {
			/* if we've asked, but received nothing, disable this box */
			pc->open = NULL;
			pc->checkMail = NULL;
			return (NULL);
		}
	} else {
		p = (password_binding)
			malloc(sizeof(struct password_binding_struct));
	}

	/* else, try to get it. */
	if (pc->askpass != NULL) {
		/* check that the executed file is a good one. */
		if (permissions_ok(pc, pc->askpass)) {
			char buf[255];
			FILE *fp;
			int exit_status;
			strcpy(p->user, username);
			strcpy(p->server, servername);
			sprintf(buf, "%s 'password for wmbiff: %s@%s'",
					pc->askpass, username, servername);

			DM(pc, DEBUG_INFO, "passmgr: invoking %s\n", buf);
			fp = popen(buf, "r");

			if (fgets(p->password, 32, fp) == NULL) {
				/* this likely means that the user cancelled, and doesn't
				   want us to keep asking about the password. */
				DM(pc, DEBUG_ERROR,
				   "passmgr: fgets password failed, exiting\n");
				DM(pc, DEBUG_ERROR,
				   "passmgr: (it looks like you pressed 'cancel')\n");
				/* this seems like the sanest thing to do, for now */
				exit(EXIT_FAILURE);
			}

			exit_status = pclose(fp);
			if (exit_status != 0) {
				if (exit_status == -1) {
					/* an expected case with the signal handler */
					DM(pc, DEBUG_INFO,
					   "passmgr: pclose from '%s' failed: %s\n", buf,
					   strerror(errno));
				} else {
					DM(pc, DEBUG_ERROR,
					   "passmgr: '%s' returne non-zero exit status %d\n",
					   buf, exit_status);
				}
			}

			chomp(p->password);
			p->next = pass_list;
			pass_list = p;
			return (p->password);
		}
	}

	return (NULL);
}

#ifdef TEST_PASS_MGR
int main(int argc, char *argv[])
{
	const char *b;
	mbox_t m;

	/* sh is almost certainly conforming; owned by root, etc. */
	if (!permissions_ok(NULL, "/bin/sh")) {
		printf("FAILURE: permission checker failed on /bin/sh.");
		exit(EXIT_FAILURE);
	}
	/* tmp is definitely bad; and better be og+w */
	if (permissions_ok(NULL, "/tmp")) {
		printf("FAILURE: permission checker failed on /tmp.");
		exit(EXIT_FAILURE);
	}
	/* TODO: also find some user-owned binary that shouldn't be g+w */
	printf("SUCCESS: permission checker sanity check went well.\n");

	/* *** */
	m.askpass = "echo xlnt; #";

	b = passwordFor("bill", "ted", &m, 0);
	if (strcmp(b, "xlnt") != 0) {
		printf("FAILURE: expected 'xlnt' got '%s'\n", b);
		exit(EXIT_FAILURE);
	}
	printf("SUCCESS: expected 'xlnt' got '%s'\n", b);

	/* *** */
	m.askpass = "should be cached";
	b = passwordFor("bill", "ted", &m, 0);
	if (strcmp(b, "xlnt") != 0) {
		printf("FAILURE: expected 'xlnt' got '%s'\n", b);
		exit(EXIT_FAILURE);
	}

	printf("SUCCESS: cached 'xlnt' correctly\n");

	/* *** */
	m.askpass = "echo abcdefghi1abcdefghi2abcdefghi3a; #";

	b = passwordFor("abbot", "costello", &m, 0);
	if (strcmp(b, "abcdefghi1abcdefghi2abcdefghi3a") != 0) {
		printf
			("FAILURE: expected 'abcdefghi1abcdefghi2abcdefghi3a' got '%s'\n",
			 b);
		exit(EXIT_FAILURE);
	}
	printf
		("SUCCESS: expected 'abcdefghi1abcdefghi2abcdefghi3ab' got '%s'\n",
		 b);

	/* try overflowing the buffer */
	m.askpass = "echo abcdefghi1abcdefghi2abcdefghi3ab; #";
	b = passwordFor("laverne", "shirley", &m, 0);
	/* should come back truncated to fill the buffer */
	if (strcmp(b, "abcdefghi1abcdefghi2abcdefghi3a") != 0) {
		printf
			("FAILURE: expected 'abcdefghi1abcdefghi2abcdefghi3a' got '%s'\n",
			 b);
		exit(EXIT_FAILURE);
	}
	printf
		("SUCCESS: expected 'abcdefghi1abcdefghi2abcdefghi3ab' got '%s'\n",
		 b);

	/* make sure we still have the old one */
	b = passwordFor("bill", "ted", &m, 0);
	if (strcmp(b, "xlnt") != 0) {
		printf("FAILURE: expected 'xlnt' got '%s'\n", b);
		exit(EXIT_FAILURE);
	}
	printf("SUCCESS: expected 'xlnt' got '%s'\n", b);

	/* what it's like if ssh-askpass is cancelled - should drop the mailbox */
	m.askpass = "echo -n ; #";
	b = passwordFor("abort", "me", &m, 0);
	if (strcmp(b, "") != 0) {
		printf("FAILURE: expected '' got '%s'\n", b);
		exit(EXIT_FAILURE);
	}
	printf("SUCCESS: expected '' got '%s'\n", b);

	/* what it's like if ssh-askpass is ok'd with an empty password. */
	m.askpass = "echo ; #";
	b = passwordFor("try", "again", &m, 0);
	if (strcmp(b, "") != 0) {
		printf("FAILURE: expected '' got '%s'\n", b);
		exit(EXIT_FAILURE);
	}
	printf("SUCCESS: expected '' got '%s'\n", b);

	exit(EXIT_SUCCESS);
}
#endif
