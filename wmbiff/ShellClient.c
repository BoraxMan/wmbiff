/* Author: Benoît Rouits ( brouits@free.fr ) thanks to Neil Spring.
   from LicqClient by Yong-iL Joh ( tolkien@mizi.com ) 
   and Jorge García ( Jorge.Garcia@uv.es )
 * 
 * generic Shell command support
 *
 * Last Updated : Tue Mar  5 15:23:35 CET 2002
 *
 */

#include "Client.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <assert.h>
#include "charutil.h"
#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#define SH_DM(pc, lvl, args...) DM(pc, lvl, "shell: " args)

/* kind_popen bumps off the sigchld handler - we care whether
   a checking program fails. */

sighandler_t old_signal_handler;

FILE *kind_popen(const char *command, const char *type)
{
	FILE *ret;
	assert(strcmp(type, "r") == 0);
	assert(old_signal_handler == NULL);
	old_signal_handler = signal(SIGCHLD, SIG_DFL);
	ret = popen(command, type);
	if (ret == NULL) {
		DMA(DEBUG_ERROR, "popen: error while reading '%s': %s\n",
			command, strerror(errno));
		signal(SIGCHLD, old_signal_handler);
		old_signal_handler = NULL;
	}
	return (ret);
}

/* kind_pclose checks the return value from pclose and prints
   some nice error messages about it.  ordinarily, this would be 
   a good idea, but wmbiff has a sigchld handler that reaps 
   children immediately (needed when spawning other child processes),
   so no error checking can be done here until that's disabled */

/* returns as a mailcheck function does: -1 on fail, 0 on success */
static int kind_pclose(FILE * F, const char *command, Pop3 pc)
{
	int exit_status = pclose(F);

	if (old_signal_handler != NULL) {
		signal(SIGCHLD, old_signal_handler);
		old_signal_handler = NULL;
	}

	if (exit_status != 0) {
		if (exit_status == -1) {
			/* wmbiff has a sigchld handler already, so wait is likely 
			   to fail */
			SH_DM(pc, DEBUG_ERROR, "pclose '%s' failed: %s\n",
				  command, strerror(errno));
			return (-1);
		} else {
			SH_DM(pc, DEBUG_ERROR,
				  "'%s' exited with non-zero status %d\n", command,
				  exit_status);
			return (-1);
		}
	}
	return (0);
}

int shellCmdCheck(Pop3 pc)
{
	FILE *F;
	int count_status = 0;
	char linebuf[256];

	if (pc == NULL)
		return -1;
	SH_DM(pc, DEBUG_INFO, ">Mailbox: '%s'\n", pc->path);

	/* run the program and disable the signal handler (if successful) */
	if ((F = kind_popen(pc->path, "r")) == NULL) {
		return -1;
	}

	/* fetch the first line of input */
	pc->TextStatus[0] = '\0';
	if (fgets(linebuf, 256, F) == NULL) {
		SH_DM(pc, DEBUG_ERROR,
			  "fgets: unable to read the output of '%s': %s\n", pc->path,
			  strerror(errno));
		kind_pclose(F, pc->path, pc);
		return -1;
	}
	chomp(linebuf);
	SH_DM(pc, DEBUG_INFO, "'%s' returned '%s'\n", pc->path, linebuf);

	/* see if it's numeric; the numeric check is somewhat 
	   useful, as wmbiff renders 4-digit numbers, but not
	   4-character strings. */
	if (sscanf(linebuf, "%d", &(count_status)) == 1) {
		if (strstr(linebuf, "new")) {
			pc->UnreadMsgs = count_status;
			pc->TotalMsgs = 0;
		} else if (strstr(linebuf, "old")) {
			pc->UnreadMsgs = 0;
			pc->TotalMsgs = count_status;
		} else {
			/* this default should be configurable. */
			pc->UnreadMsgs = 0;
			pc->TotalMsgs = count_status;
		}
	} else if (sscanf(linebuf, "%9s\n", pc->TextStatus) == 1) {
		/* validate the string input */
		int i;
		for (i = 0; pc->TextStatus[i] != '\0' && isalnum(pc->TextStatus[i])
			 && i < 10; i++);
		if (pc->TextStatus[i] != '\0') {
			SH_DM(pc, DEBUG_ERROR,
				  "sorry, wmbiff supports only alphanumeric (isalnum()) strings ('%s' is not ok)\n",
				  pc->TextStatus);
			pc->TextStatus[i] = '\0';	/* null terminate it at the first bad char. */
		}
		/* see if we should print as new or not */
		pc->UnreadMsgs = (strstr(linebuf, "new")) ? 1 : 0;
	} else {
		SH_DM(pc, DEBUG_ERROR,
			  "'%s' returned something other than an integer message count or short string.\n",
			  pc->path);
		kind_pclose(F, pc->path, pc);
		return -1;
	}

	SH_DM(pc, DEBUG_INFO, "from: %s status: %s %d %d\n",
		  pc->path, pc->TextStatus, pc->TotalMsgs, pc->UnreadMsgs);

	return (kind_pclose(F, pc->path, pc));
}

int shellCreate(Pop3 pc, const char *str)
{
	/* SHELL format: shell:::/path/to/script */
	const char *reserved1, *reserved2, *commandline;

	pc->TotalMsgs = 0;
	pc->UnreadMsgs = 0;
	pc->OldMsgs = -1;
	pc->OldUnreadMsgs = -1;
	pc->checkMail = shellCmdCheck;
	reserved1 = str + 6;		/* shell:>:: */

	reserved2 = index(reserved1, ':');
	if (reserved2 == NULL) {
		SH_DM(pc, DEBUG_ERROR, "unable to parse '%s', expecting ':'", str);
		return 0;
	}
	reserved2++;				/* shell::>: */

	commandline = index(reserved2, ':');
	if (commandline == NULL) {
		SH_DM(pc, DEBUG_ERROR,
			  "unable to parse '%s', expecting another ':'", str);
		return 0;
	}
	commandline++;				/* shell:::> */

	/* good thing strcpy handles overlapping regions */
	strcpy(pc->path, commandline);
	SH_DM(pc, DEBUG_INFO, "path= '%s'\n", commandline);
	return 0;
}

/* vim:set ts=4: */
