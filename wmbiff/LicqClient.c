/* $Id: LicqClient.c,v 1.8 2002/04/20 07:53:20 bluehal Exp $ */
/* Author : Yong-iL Joh ( tolkien@mizi.com )
   Modified: Jorge García ( Jorge.Garcia@uv.es )
 * 
 * LICQ checker.
 *
 * Last Updated : $Date: 2002/04/20 07:53:20 $
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
	struct utimbuf ut;

	DM(pc, DEBUG_INFO, ">Mailbox: '%s'\n", pc->path);

    if(fileHasChanged(pc->path, &ut.actime, &PCM.mtime, &PCM.size) 
       || pc->OldMsgs < 0) {
        FILE *F;
        char buf[1024];
        int count_status = 0;

		/* file was changed OR initially read */
		DM(pc, DEBUG_INFO, "  was changed,"
		   " TIME: new %lu SIZE: new %lu\n",
		   PCM.mtime, (unsigned long) PCM.size);

        F = openMailbox(pc, pc->path);
		/* count message */
		while (fgets(buf, BUF_SIZE, F)) {
			if ((buf[0] == '[') || (buf[0] == '-')) {	/* new, or old licq */
				count_status++;
			}
		}
		fclose(F);

		pc->TotalMsgs = count_status * 2;
		pc->UnreadMsgs = pc->TotalMsgs - count_status;
		DM(pc, DEBUG_INFO, "from: %d status: %d\n", pc->TotalMsgs,
		   pc->UnreadMsgs);

		/* Not clear that resetting the mtime is useful, as mutt
           is not involved.  Unfortunately, I can't tell whether
           this cut-and-pasted code is useful */
		ut.modtime = PCM.mtime;
		utime(pc->path, &ut);
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
	pc->checkMail = licqCheckHistory;

	strcpy(pc->path, str + 5);	/* cut off ``licq:'' */

	DM(pc, DEBUG_INFO, "licq: str = '%s'\n", str);
	DM(pc, DEBUG_INFO, "licq: path= '%s'\n", pc->path);

	return 0;
}

/* vim:set ts=4: */
/*
 * Local Variables:
 * tab-width: 4
 * c-indent-level: 4
 * c-basic-offset: 4
 * End:
 */
