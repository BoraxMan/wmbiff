/* Author : Yong-iL Joh ( tolkien@mizi.com )
   Modified: Jorge García ( Jorge.Garcia@uv.es )
   shellcmd support : Benoît Rouits ( brouits@free.fr ) thanks to Neil Spring.
 * 
 * generic Shell command support
 *
 * Last Updated : Tue Mar  5 15:23:35 CET 2002
 *
 */

#include "Client.h"
#include <errno.h>
#ifdef USE_DMALLOC
#include <dmalloc.h>
#include <string.h>
#endif

int shellCmdCheck(Pop3 pc)
{
	FILE *F;
	int count_status = 0;

	DM(pc, DEBUG_INFO, ">Mailbox: '%s'\n", pc->path);

	if ((F = popen(pc->path, "r")) == NULL) {
		DM(pc, DEBUG_ERROR, "popen: error while reading '%s': %s\n",
		   pc->path, strerror(errno));
		pc->TotalMsgs = pc->UnreadMsgs = -1;
		return 1;
	}

	/* doesn't really need to be handled separately, but it
	   seems worth an error message */
	if (fscanf(F, "%d\n", &(count_status)) != 1) {
		DM(pc, DEBUG_ERROR,
		   "'%s' returned something other than an integer message count.\n",
		   pc->path);
		pc->TotalMsgs = pc->UnreadMsgs = -1;
		pclose(F);
		return 1;
	}

	pc->TotalMsgs = pc->UnreadMsgs + count_status;
	pc->UnreadMsgs = count_status;
	DM(pc, DEBUG_INFO, "from: %d status: %d\n", pc->TotalMsgs,
	   pc->UnreadMsgs);

	pclose(F);
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
	pc->open = openMailbox;
	pc->checkMail = shellCmdCheck;
	reserved1 = str + 6;		/* shell:>:: */
	reserved2 = index(reserved1, ':') + 1;	/* shell::>: */
	if (reserved2 == NULL) {
		DM(pc, DEBUG_ERROR,
		   "shell method, unable to parse '%s', expecting ':'", str);
		return 0;
	}
	commandline = index(reserved2, ':') + 1;	/* shell:::> */
	if (commandline == NULL) {
		DM(pc, DEBUG_ERROR,
		   "shell method, unable to parse '%s', expecting another ':'",
		   str);
		return 0;
	}
	strcpy(pc->path, commandline);
	DM(pc, DEBUG_INFO, "shell: path= '%s'\n", commandline);
	return 0;
}

/* vim:set ts=4: */
