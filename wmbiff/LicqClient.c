/* $Id: LicqClient.c,v 1.3 2001/10/04 09:50:59 jordi Exp $ */
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

#ifdef DEBUG_MAIL_COUNT
	printf(">Mailbox: '%s'\n", pc->path);
#endif

	/* licq file */
	if (stat(pc->path, &st)) {
		fprintf(stderr, "wmbiff: Can't stat mailbox '%s': %s\n",
				pc->path, strerror(errno));
		return -1;				/* Error stating mailbox */
	}

	if (st.st_mtime != PCM.mtime || st.st_size != PCM.size
		|| pc->OldMsgs < 0) {
		/* file was changed OR initially read */
#ifdef DEBUG_MAIL_COUNT
		printf("  was changed,"
			   " TIME: old %lu, new %lu"
			   " SIZE: old %lu, new %lu\n",
			   PCM.mtime, st.st_mtime, (unsigned long) PCM.size,
			   st.st_size);
#endif
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
#ifdef DEBUG_MAIL_COUNT
		printf("from: %d status: %d\n", pc->TotalMsgs, pc->UnreadMsgs);
#endif

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

#ifdef DEBUG_LICQ
	printf("licq: str = '%s'\n", str);
	printf("licq: path= '%s'\n", pc->path);
#endif

	return 0;
}

/* vim:set ts=4: */
