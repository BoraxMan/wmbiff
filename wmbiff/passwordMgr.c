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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "passwordMgr.h"
#include "Client.h"
#include "charutil.h"			/* chomp */
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>			/* index */
#include <sys/stat.h>
#include "assert.h"

typedef struct password_binding_struct {
	struct password_binding_struct *next;
	char user[32];
	char server[255];
	char password[32];			/* may be frobnicated */
} *password_binding;

static password_binding pass_list = NULL;

/* verifies that askpass_fname, if it has no spaces, exists as 
   a file, is owned by the user or by root, and is not world 
   writeable.   This is just a sanity check, and is not intended 
   to ensure the integrity of the password-asking program. */
/* would be static, but used in test_wmbiff */
int permissions_ok(Pop3 pc, const char *askpass_fname)
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

char *passwordFor(const char *username,
				  const char *servername, Pop3 pc, int bFlushCache)
{

	password_binding p;

	assert(username != NULL);
	assert(username[0] != '\0');

	/* find the binding */
	for (p = pass_list;
		 p != NULL
		 && (strcmp(username, p->user) != 0 ||
			 strcmp(servername, p->server) != 0); p = p->next);

	/* if so, return the password */
	if (p != NULL) {
		if (p->password[0] != '\0') {
			if (bFlushCache == 0) {
				char *ret = strdup(p->password);
#ifdef HAVE_MEMFROB
				memfrob(ret, strlen(ret));
#endif
				return (ret);
			}
			/* else fall through, overwrite */
		} else if (pc) {
			/* if we've asked, but received nothing, disable this box */
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
			char *command;
			char *password_ptr;
			int len =
				strlen(pc->askpass) + strlen(username) +
				strlen(servername) + 40;
			command = malloc(len);
			snprintf(command, len, "%s 'password for wmbiff: %s@%s'",
					 pc->askpass, username, servername);

			(void) grabCommandOutput(pc, command, &password_ptr);
			/* it's not clear what to do with the exit
			   status, though we can get it from
			   grabCommandOutput if needed to deal with some
			   programs that will print a message but exit
			   non-zero on error */
			free(command);

			if (password_ptr == NULL) {
				/* this likely means that the user cancelled, and doesn't
				   want us to keep asking about the password. */
				DM(pc, DEBUG_ERROR,
				   "passmgr: fgets password failed, exiting\n");
				DM(pc, DEBUG_ERROR,
				   "passmgr: (it looks like you pressed 'cancel')\n");
				/* this seems like the sanest thing to do, for now */
				exit(EXIT_FAILURE);
			}

			strcpy(p->user, username);
			strcpy(p->server, servername);
			password_ptr[31] = '\0';
			strncpy(p->password, password_ptr, 31);
			p->password[31] = '\0';	/* force a null termination */
			// caller is responsible for freeing plaintext version free(password_ptr);
#ifdef HAVE_MEMFROB
			memfrob(p->password, strlen(p->password));
#endif
			p->next = pass_list;
			pass_list = p;
			return (password_ptr);
		}
	}

	return (NULL);
}

/* vim:set ts=4: */
/*
 * Local Variables:
 * tab-width: 4
 * c-indent-level: 4
 * c-basic-offset: 4
 * End:
 */
