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
#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#define SH_DM(pc, lvl, args...) DM(pc, lvl, "shell: " args)

/* kind_pclose checks the return value from pclose and prints
   some nice error messages about it.  ordinarily, this would be 
   a good idea, but wmbiff has a sigchld handler that reaps 
   children immediately (needed when spawning other child processes),
   so no error checking can be done here until that's disabled */

/* TODO: block or unbind sigchld before popen, and reenable on pclose */
static void kind_pclose(FILE * F, const char *command, Pop3 pc)
{
	int exit_status = pclose(F);
	if (exit_status != 0) {
		if (exit_status == -1) {
			/* wmbiff has a sigchld handler already, so wait is likely 
			   to fail */
			if (errno != ECHILD) {
				SH_DM(pc, DEBUG_ERROR, "pclose '%s' failed: %s\n",
					  command, strerror(errno));
			}
		} else {
			SH_DM(pc, DEBUG_ERROR,
				  "'%s' exited with non-zero status %d\n", command,
				  exit_status);
		}
	}
}

int shellCmdCheck(Pop3 pc)
{
	FILE *F;
	int count_status = 0;

	if (pc == NULL)
		return -1;
	SH_DM(pc, DEBUG_INFO, ">Mailbox: '%s'\n", pc->path);

	if ((F = popen(pc->path, "r")) == NULL) {
		SH_DM(pc, DEBUG_ERROR, "popen: error while reading '%s': %s\n",
			  pc->path, strerror(errno));
		return -1;
	}

	/* doesn't really need to be handled separately, but it
	   seems worth an error message */
	if (fscanf(F, "%d\n", &(count_status)) != 1) {
		SH_DM(pc, DEBUG_ERROR,
			  "'%s' returned something other than an integer message count.\n",
			  pc->path);
		kind_pclose(F, pc->path, pc);
		return -1;
	}

	pc->TotalMsgs = pc->UnreadMsgs + count_status;
	pc->UnreadMsgs = count_status;
	SH_DM(pc, DEBUG_INFO, "from: %d status: %d\n", pc->TotalMsgs,
		  pc->UnreadMsgs);

	kind_pclose(F, pc->path, pc);
	return 0;
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
