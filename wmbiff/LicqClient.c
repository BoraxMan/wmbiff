/* $Id: LicqClient.c,v 1.5 2002/04/07 05:08:23 bluehal Exp $ */
/* Author : Yong-iL Joh ( tolkien@mizi.com )
   Modified: Jorge García ( Jorge.Garcia@uv.es )
 * 
 * LICQ checker.
 *
 * Last Updated : Mar 20, 05:32:35 CET 2001     
 *
 */

#include "Client.h"
#include <sys/stat.h>
#include <utime.h>
#include <errno.h>
#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#define PCM     (pc->u).mbox

int licqCheckHistory(Pop3 pc)
{
	struct stat st;
	struct utimbuf ut;
	FILE *F;
	int count_status = 0;
	char buf[1024];

	DM(pc, DEBUG_INFO, ">Mailbox: '%s'\n", pc->path);

	/* licq file */
	if (stat(pc->path, &st)) {
		DM(pc, DEBUG_ERROR, "Can't stat mailbox '%s': %s\n",
		   pc->path, strerror(errno));
		return -1;				/* Error stating mailbox */
	}

	if (st.st_mtime != PCM.mtime || st.st_size != PCM.size
		|| pc->OldMsgs < 0) {
		/* file was changed OR initially read */
		DM(pc, DEBUG_INFO,
		   "  was changed,"
		   " TIME: old %lu, new %lu"
		   " SIZE: old %lu, new %lu\n",
		   PCM.mtime, (unsigned long)st.st_mtime, 
           (unsigned long) PCM.size, (unsigned long)st.st_size);
		ut.actime = st.st_atime;
		ut.modtime = st.st_mtime;
		F = pc->open(pc);

		/* count message */
		while (fgets(buf, BUF_SIZE, F)) {
			if ((buf[0] == '[') || (buf[0] == '-')) {	/* new, or old licq */
				count_status++;
			}
		}
		pc->TotalMsgs = count_status * 2;
		pc->UnreadMsgs = pc->TotalMsgs - count_status;
		DM(pc, DEBUG_INFO, "from: %d status: %d\n", pc->TotalMsgs,
		   pc->UnreadMsgs);

		fclose(F);

		utime(pc->path, &ut);
		/* Reset atime for MUTT and something others correctly work */
		PCM.mtime = st.st_mtime;	/* Store new mtime */
		PCM.size = st.st_size;	/* Store new size */
	}

	return 0;
}

int licqCreate(Pop3 pc, char *str)
{
	/* LICQ format: licq:fullpathname */

	pc->TotalMsgs = 0;
	pc->UnreadMsgs = 0;
	pc->OldMsgs = -1;
	pc->OldUnreadMsgs = -1;
	pc->open = openMailbox;
	pc->checkMail = licqCheckHistory;

	strcpy(pc->path, str + 5);	/* cut off ``licq:'' */

	DM(pc, DEBUG_INFO, "licq: str = '%s'\n", str);
	DM(pc, DEBUG_INFO, "licq: path= '%s'\n", pc->path);

	return 0;
}

/* vim:set ts=4: */
