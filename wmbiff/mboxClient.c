/* $Id: mboxClient.c,v 1.7 2002/04/07 05:08:23 bluehal Exp $ */
/* Author:		Yong-iL Joh <tolkien@mizi.com>
   Modified:	Jorge García <Jorge.Garcia@uv.es>
   			 	Rob Funk <rfunk@funknet.net>
 * 
 * MBOX checker.
 *
 * Last Updated : Thu Apr 26 03:09:40 CEST 2001
 *
 */

#include "Client.h"
#include <sys/stat.h>
#include <errno.h>
#include <utime.h>
#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#define PCM	(pc->u).mbox
#define FROM_STR   "From "
#define STATUS_STR "Status: "

FILE *openMailbox(Pop3 pc)
{
	FILE *mailbox;

	if ((mailbox = fopen(pc->path, "r")) == NULL) {
		DM(pc, DEBUG_ERROR, "Error opening mailbox '%s': %s\n",
		   pc->path, strerror(errno));
	}
	return (mailbox);
}

int mboxCheckHistory(Pop3 pc)
{
	struct stat st;
	struct utimbuf ut;
	FILE *F;
	char buf[BUF_SIZE];


	int is_header = 0;
	int next_from_is_start_of_header = 1;
	int count_from = 0, count_status = 0;
	int len_from = strlen(FROM_STR), len_status = strlen(STATUS_STR);

	DM(pc, DEBUG_INFO, ">Mailbox: '%s'\n", pc->path);

	/* mbox file */
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
			if (buf[0] == '\n') {
				/* a newline by itself terminates the header */
				if (is_header)
					is_header = 0;
				else
					next_from_is_start_of_header = 1;
			} else if (!strncmp(buf, FROM_STR, len_from)) {
				/* A line starting with "From" is the beginning of a new header.
				   "From" in the text of the mail should get escaped by the MDA.
				   If your MDA doesn't do that, it is broken.
				 */
				if (next_from_is_start_of_header)
					is_header = 1;
				if (is_header)
					count_from++;
			} else {
				next_from_is_start_of_header = 0;
				if (!strncmp(buf, STATUS_STR, len_status)) {
					if (strrchr(buf, 'R')) {
						if (is_header)
							count_status++;
					}
				}
			}
		}

		DM(pc, DEBUG_INFO, "from: %d status: %d\n", count_from,
		   count_status);
		pc->TotalMsgs = count_from;
		pc->UnreadMsgs = count_from - count_status;
		fclose(F);

		utime(pc->path, &ut);
		/* Reset atime for MUTT and something others correctly work */
		PCM.mtime = st.st_mtime;	/* Store new mtime */
		PCM.size = st.st_size;	/* Store new size */
	}

	return 0;
}

int mboxCreate(Pop3 pc, char *str)
{
	/* MBOX format: mbox:fullpathname */

	pc->TotalMsgs = 0;
	pc->UnreadMsgs = 0;
	pc->OldMsgs = -1;
	pc->OldUnreadMsgs = -1;
	pc->open = openMailbox;
	pc->checkMail = mboxCheckHistory;

	/* default boxes are mbox... cut mbox: if it exists */
	if (!strncasecmp(pc->path, "mbox:", 5))
		strcpy(pc->path, str + 5);	/* cut off ``mbox:'' */

	DM(pc, DEBUG_INFO, "mbox: str = '%s'\n", str);
	DM(pc, DEBUG_INFO, "mbox: path= '%s'\n", pc->path);

	return 0;
}

/* vim:set ts=4: */
