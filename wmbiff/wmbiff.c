/* $Id: wmbiff.c,v 1.24 2002/04/15 21:06:29 bluehal Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define	USE_POLL

#include <time.h>
#include <ctype.h>

#ifdef USE_POLL
#include <poll.h>
#else
#include <sys/time.h>
#endif

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>

#include <errno.h>

#include "../wmgeneral/wmgeneral.h"
#include "../wmgeneral/misc.h"

#include "Client.h"
#include "charutil.h"

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#include "wmbiff-master-led.xpm"
char wmbiff_mask_bits[64 * 64];
const int wmbiff_mask_width = 64;
const int wmbiff_mask_height = 64;

#define CHAR_WIDTH  5
#define CHAR_HEIGHT 7

#define BLINK_TIMES 8
#define DEFAULT_SLEEP_INTERVAL 1000
#define BLINK_SLEEP_INTERVAL    200
#define DEFAULT_LOOP 5

mbox_t mbox[5];
/* this is the normal pixmap. */
const char *skin_filename = "wmbiff-master-led.xpm";
/* path to search for pixmaps */
/* /usr/share/wmbiff should have the default pixmap. */
/* /usr/local/share/wmbiff if compiled locally. */
/* / is there in case a user wants to specify a complete path */
/* . is there for development. */
const char *skin_search_path = DEFAULT_SKIN_PATH;

int ReadLine(FILE *, char *, char *, int *);
int Read_Config_File(char *, int *);
int count_mail(int);
void parse_cmd(int, char **, char *);
void init_biff(char *);
void displayMsgCounters(int, int, int *, int *);

void usage(void);
void printversion(void);
void do_biff(int argc, char **argv) __attribute__ ((noreturn));
void parse_mbox_path(int item);
static void BlitString(const char *name, int x, int y, int new);
void BlitNum(int num, int x, int y, int new);
void ClearDigits(int i);
void XSleep(int millisec);
void sigchld_handler(int sig);

int debug_default = DEBUG_ERROR;

void init_biff(char *config_file)
{
	int i, j, loopinterval = DEFAULT_LOOP;
	char *m;

	for (i = 0; i < 5; i++) {
		mbox[i].label[0] = 0;
		mbox[i].path[0] = 0;
		mbox[i].notify[0] = 0;
		mbox[i].action[0] = 0;
		mbox[i].fetchcmd[0] = 0;
		mbox[i].loopinterval = 0;
		mbox[i].debug = debug_default;
		mbox[i].askpass = DEFAULT_ASKPASS;
	}

	/* Some defaults, if config file is unavailable */
	strcpy(mbox[0].label, "Spool");
	if ((m = getenv("MAIL")) != NULL) {
		strcpy(mbox[0].path, m);
	} else if ((m = getenv("USER")) != NULL) {
		strcpy(mbox[0].path, "/var/mail/");
		strcat(mbox[0].path, m);
	}
#ifdef HAVE_GCRYPT_H
	/* gcrypt is a little strange, in that it doesn't 
	 * seem to initialize its memory pool by itself. 
	 * I believe we *expect* "Warning: using insecure memory!"
	 */
	if ((i = gcry_control(GCRYCTL_INIT_SECMEM, 16384, 0)) != 0) {
		DMA(DEBUG_ERROR,
			"Error: gcry_control() to initialize secure memory returned non-zero: %d\n"
			" Message: %s\n"
			" libgcrypt version: %s\n"
			" recovering: will fail later if using CRAM-MD5 or APOP authentication.\n",
			i, gcry_strerror(gcry_errno()), gcry_check_version(NULL));
	};
#endif

	DMA(DEBUG_INFO, "config_file = %s.\n", config_file);
	if (!Read_Config_File(config_file, &loopinterval)) {
		if (m == NULL) {
			DMA(DEBUG_ERROR, "Cannot open '%s' nor use the "
				"MAIL environment var.\n", config_file);
			exit(1);
		}
		/* we are using MAIL environment var. type mbox */
		DMA(DEBUG_INFO, "Using MAIL environment var '%s'.\n", m);
		mboxCreate((&mbox[0]), mbox[0].path);
	}

	/* Make labels look right */
	for (i = 0; i < 5; i++) {
		if (mbox[i].label[0] != 0) {
			j = strlen(mbox[i].label);
			if (j < 5) {
				memset(mbox[i].label + j, ' ', 5 - j);
			}
			mbox[i].label[5] = ':';
			mbox[i].label[6] = 0;
			/* set global loopinterval to boxes with 0 loop */
			if (!mbox[i].loopinterval) {
				mbox[i].loopinterval = loopinterval;
			}
		}
	}
}

char **LoadXPM(const char *pixmap_filename)
{
	char **xpm;
	int success;
	success = XpmReadFileToData((char *) pixmap_filename, &xpm);
	if (success == XpmOpenFailed) {
		DMA(DEBUG_ERROR, "Unable to open %s\n", pixmap_filename);
	} else if (success == XpmFileInvalid) {
		DMA(DEBUG_ERROR, "%s is not a valid pixmap\n", pixmap_filename);
	} else if (success == XpmNoMemory) {
		DMA(DEBUG_ERROR, "Insufficient memory to read %s\n",
			pixmap_filename);
	}
	return (xpm);
}

int exists(const char *filename)
{
	struct stat st_buf;
	DMA(DEBUG_INFO, "looking for %s\n", filename);
	if (stat(filename, &st_buf) == 0) {
		if (S_ISREG(st_buf.st_mode)) {
			DMA(DEBUG_INFO, "found %s\n", filename);
			return (1);
		}
	}
	return (0);
}

/* acts like execvp, with code inspired by it */
/* mustfree */
char *search_path(const char *path, const char *find_me)
{
	char *buf;
	const char *p;
	int len, pathlen;
	if (strchr(find_me, '/') != NULL) {
		return (strdup(find_me));
	}
	pathlen = strlen(path);
	len = strlen(find_me) + 1;
	buf = malloc(pathlen + len + 1);
	memcpy(buf + pathlen + 1, find_me, len);
	buf[pathlen] = '/';

	for (p = path; p != NULL; path = p, path++) {
		char *startp;
		p = strchr(path, ':');
		if (p == NULL) {
			/* not found; p should point to the null char at the end */
			startp =
				memcpy(buf + pathlen - strlen(path), path, strlen(path));
		} else if (p == path) {
			/* double colon apparently means try here */
			startp = &buf[pathlen + 1];
		} else {
			/* copy the part between the colons to the buffer */
			startp = memcpy(buf + pathlen - (p - path), path, p - path);
		}
		if (exists(startp)) {
			char *ret = strdup(startp);
			free(buf);
			return (ret);
		}
	}
	free(buf);
	return (NULL);
}

/* verifies that .wmbiffrc, is a file, is owned by the user,
   is not world writeable, and is not world readable.  This
   is just to help keep passwords secure */
static int wmbiffrc_permissions_check(const char *wmbiffrc_fname)
{
	struct stat st;
	if (stat(wmbiffrc_fname, &st)) {
		DMA(DEBUG_ERROR, "Can't stat wmbiffrc: '%s'\n", wmbiffrc_fname);
		return (1);				/* well, it's not a bad permission
								   problem: if you can't find it,
								   neither can the bad guys.. */
	}
	if (st.st_uid != getuid()) {
		char *user = getenv("USER");
		DMA(DEBUG_ERROR,
			".wmbiffrc '%s' isn't owned by you.\n"
			"Verify its contents, then 'chown %s %s'\n",
			wmbiffrc_fname, ((user != NULL) ? user : "(your username)"),
			wmbiffrc_fname);
		return (0);
	}
	if (st.st_mode & S_IWOTH) {
		DMA(DEBUG_ERROR, ".wmbiffrc '%s' is world writable.\n"
			"Verify its contents, then 'chmod 0600 %s'\n",
			wmbiffrc_fname, wmbiffrc_fname);
		return (0);
	}
	if (st.st_mode & S_IROTH) {
		DMA(DEBUG_ERROR, ".wmbiffrc '%s' is world readable.\n"
			"Please run 'chmod 0600 %s' and consider changing your passwords.\n",
			wmbiffrc_fname, wmbiffrc_fname);
		return (0);
	}
	return (1);
}



void do_biff(int argc, char **argv)
{
	int i;
	XEvent Event;
	int but_stat = -1;
	time_t curtime;
	int NeedRedraw = 0;
	int Sleep_Interval = DEFAULT_SLEEP_INTERVAL;	/* Default sleep time (in msec) */
	int Blink_Mode = 0;			/* Bit mask, digits are in blinking mode or not.
								   Each bit for separate mailbox */
	const char **skin_xpm = NULL;
	char *skin_file_path = search_path(skin_search_path, skin_filename);

	if (skin_file_path != NULL) {
		skin_xpm = (const char **) LoadXPM(skin_file_path);
	}
	if (skin_xpm == NULL) {
		DMA(DEBUG_ERROR, "using built-in xpm; %s wasn't found in %s\n",
			skin_filename, skin_search_path);
		skin_xpm = wmbiff_master_xpm;
	}

	createXBMfromXPM(wmbiff_mask_bits, skin_xpm,
					 wmbiff_mask_width, wmbiff_mask_height);

	openXwindow(argc, argv, skin_xpm, wmbiff_mask_bits,
				wmbiff_mask_width, wmbiff_mask_height);

	AddMouseRegion(0, 5, 6, 58, 16);
	AddMouseRegion(1, 5, 16, 58, 26);
	AddMouseRegion(2, 5, 26, 58, 36);
	AddMouseRegion(3, 5, 36, 58, 46);
	AddMouseRegion(4, 5, 46, 58, 56);

#if 0
	copyXPMArea(39, 84, (3 * CHAR_WIDTH), 8, 39, 5);
	copyXPMArea(39, 84, (3 * CHAR_WIDTH), 8, 39, 16);
	copyXPMArea(39, 84, (3 * CHAR_WIDTH), 8, 39, 27);
	copyXPMArea(39, 84, (3 * CHAR_WIDTH), 8, 39, 38);
	copyXPMArea(39, 84, (3 * CHAR_WIDTH), 8, 39, 49);

	BlitString("XX", 45, 5, 0);
	BlitString("XX", 45, 16, 0);
	BlitString("XX", 45, 27, 0);
	BlitString("XX", 45, 38, 0);
	BlitString("XX", 45, 49, 0);
#endif

	/* Initially read mail counters and resets,
	   and initially draw labels and counters */
	curtime = time(0);
	for (i = 0; i < 5; i++) {
		if (mbox[i].label[0] != 0) {
			mbox[i].prevtime = mbox[i].prevfetch_time = curtime;
			BlitString(mbox[i].label, 5, (11 * i) + 5, 0);
			DM(&mbox[i], DEBUG_INFO,
			   "working on [%d].label=>%s< [%d].path=>%s<\n", i,
			   mbox[i].label, i, mbox[i].path);
			displayMsgCounters(i, count_mail(i), &Sleep_Interval,
							   &Blink_Mode);
		}
	}

	RedrawWindow();

	NeedRedraw = 0;
	while (1) {
		/* waitpid(0, NULL, WNOHANG); */

		for (i = 0; i < 5; i++) {
			if (mbox[i].label[0] != 0) {
				curtime = time(0);
				if (curtime >= mbox[i].prevtime + mbox[i].loopinterval) {
					NeedRedraw = 1;
					DM(&mbox[i], DEBUG_INFO,
					   "working on [%d].label=>%s< [%d].path=>%s<\n", i,
					   mbox[i].label, i, mbox[i].path);
					DM(&mbox[i], DEBUG_INFO,
					   "curtime=%d, prevtime=%d, interval=%d\n",
					   (int) curtime, (int) mbox[i].prevtime,
					   mbox[i].loopinterval);
					mbox[i].prevtime = curtime;
					displayMsgCounters(i, count_mail(i), &Sleep_Interval,
									   &Blink_Mode);
				}
			}
			if (mbox[i].blink_stat > 0) {
				if (--mbox[i].blink_stat <= 0) {
					Blink_Mode &= ~(1 << i);
					mbox[i].blink_stat = 0;
				}
				displayMsgCounters(i, 1, &Sleep_Interval, &Blink_Mode);
				NeedRedraw = 1;
			}
			if (Blink_Mode == 0) {
				mbox[i].blink_stat = 0;
				Sleep_Interval = DEFAULT_SLEEP_INTERVAL;
			}
			if (mbox[i].fetchinterval > 0 && mbox[i].fetchcmd[0] != 0
				&& curtime >=
				mbox[i].prevfetch_time + mbox[i].fetchinterval) {
				execCommand(mbox[i].fetchcmd);
				mbox[i].prevfetch_time = curtime;
			}
		}
		if (NeedRedraw) {
			NeedRedraw = 0;
			RedrawWindow();
		}

		/* X Events */
		while (XPending(display)) {
			XNextEvent(display, &Event);
			switch (Event.type) {
			case Expose:
				RedrawWindow();
				break;
			case DestroyNotify:
				XCloseDisplay(display);
				exit(0);
				break;
			case ButtonPress:
				i = CheckMouseRegion(Event.xbutton.x, Event.xbutton.y);
				but_stat = i;
				break;
			case ButtonRelease:
				i = CheckMouseRegion(Event.xbutton.x, Event.xbutton.y);
				if (but_stat == i && but_stat >= 0) {
					switch (Event.xbutton.button) {
					case 1:	/* Left mouse-click */
						if (mbox[but_stat].action[0] != 0) {
							execCommand(mbox[but_stat].action);
						}
						break;
					case 3:	/* Right mouse-click */
						if (mbox[but_stat].fetchcmd[0] != 0) {
							execCommand(mbox[but_stat].fetchcmd);
						}
						break;
					}
				}
				but_stat = -1;
				/* RedrawWindow(); */
				break;
			}
		}
		XSleep(Sleep_Interval);
	}
}

/* helper function for displayMsgCounters, which has outgrown its name */
static void blitMsgCounters(int i)
{
	int y_row = (11 * i) + 5;	/* constant for each mailbox */
	ClearDigits(i);				/* Clear digits */
	if ((mbox[i].blink_stat & 0x01) == 0) {
		int newmail = (mbox[i].UnreadMsgs > 0) ? 1 : 0;
		if (mbox[i].TextStatus[0] != '\0') {
			BlitString(mbox[i].TextStatus, 39, y_row, newmail);
		} else {
			int mailcount =
				(newmail) ? mbox[i].UnreadMsgs : mbox[i].TotalMsgs;
			BlitNum(mailcount, 45, y_row, newmail);
		}
	}
}

void displayMsgCounters(int i, int mail_stat, int *Sleep_Interval,
						int *Blink_Mode)
{
	switch (mail_stat) {
	case 2:					/* New mail has arrived */
		/* Enter blink-mode for digits */
		mbox[i].blink_stat = BLINK_TIMES * 2;
		*Sleep_Interval = BLINK_SLEEP_INTERVAL;
		*Blink_Mode |= (1 << i);	/* Global blink flag set for this mailbox */
		blitMsgCounters(i);
		if (mbox[i].notify[0] != 0) {	/* need to call notify() ? */
			if (!strcasecmp(mbox[i].notify, "beep")) {	/* Internal keyword ? */
				XBell(display, 100);	/* Yes, bell */
			} else {
				execCommand(mbox[i].notify);	/* Else call external notifyer */
			}
		}

		/* Autofetch on new mail arrival? */
		if (mbox[i].fetchinterval == -1 && mbox[i].fetchcmd[0] != 0) {
			execCommand(mbox[i].fetchcmd);	/* yes */
		}
		break;
	case 1:					/* mailbox has been rescanned/changed */
		blitMsgCounters(i);
		break;
	case 0:
		break;
	case -1:					/* Error was detected */
		ClearDigits(i);			/* Clear digits */
		BlitString("XX", 45, (11 * i) + 5, 0);
		break;
	}
}

/** counts mail in spool-file
   Returned value:
   -1 : Error was encountered
   0  : mailbox status wasn't changed
   1  : mailbox was changed (NO new mail)
   2  : mailbox was changed AND new mail has arrived
**/
int count_mail(int item)
{
	int rc = 0;

	if (!mbox[item].checkMail) {
		return -1;
	}

	if (mbox[item].checkMail(&(mbox[item])) < 0) {
		/* we failed to obtain any numbers
		 * therefore set them to -1's
		 * ensuring the next pass (even if zero)
		 * will be captured correctly
		 */
		mbox[item].TotalMsgs = -1;
		mbox[item].UnreadMsgs = -1;
		mbox[item].OldMsgs = -1;
		mbox[item].OldUnreadMsgs = -1;
		return -1;
	}

	if (mbox[item].UnreadMsgs > mbox[item].OldUnreadMsgs &&
		mbox[item].UnreadMsgs > 0) {
		rc = 2;					/* New mail detected */
	} else if (mbox[item].UnreadMsgs < mbox[item].OldUnreadMsgs ||
			   mbox[item].TotalMsgs != mbox[item].OldMsgs) {
		rc = 1;					/* mailbox was changed - NO new mail */
	} else {
		rc = 0;					/* mailbox wasn't changed */
	}
	mbox[item].OldMsgs = mbox[item].TotalMsgs;
	mbox[item].OldUnreadMsgs = mbox[item].UnreadMsgs;
	return rc;
}

/* Blits a string at given co-ordinates
   If a ``new'' parameter is given, all digits will be yellow
*/
static void BlitString(const char *name, int x, int y, int new)
{
	int i, c, k = x;

	for (i = 0; name[i]; i++) {
		c = toupper(name[i]);
		if (c >= 'A' && c <= 'Z') {	/* it's a letter */
			c -= 'A';
			copyXPMArea(c * (CHAR_WIDTH + 1), (new ? 95 : 74),
						(CHAR_WIDTH + 1), (CHAR_HEIGHT + 1), k, y);
			k += (CHAR_WIDTH + 1);
		} else {				/* it's a number or symbol */
			c -= '0';
			if (new) {
				copyXPMArea((c * (CHAR_WIDTH + 1)) + 65, 0,
							(CHAR_WIDTH + 1), (CHAR_HEIGHT + 1), k, y);
			} else {
				copyXPMArea((c * (CHAR_WIDTH + 1)), 64,
							(CHAR_WIDTH + 1), (CHAR_HEIGHT + 1), k, y);
			}
			k += (CHAR_WIDTH + 1);
		}
	}
}

/* Blits number to give coordinates.. two 0's, right justified */
void BlitNum(int num, int x, int y, int new)
{
	char buf[32];
	int newx = x;

	if (num > 99)
		newx -= (CHAR_WIDTH + 1);
	if (num > 999)
		newx -= (CHAR_WIDTH + 1);

	sprintf(buf, "%02i", num);

	BlitString(buf, newx, y, new);
}

void ClearDigits(int i)
{
	copyXPMArea((10 * (CHAR_WIDTH + 1)), 64, (CHAR_WIDTH + 1),
				(CHAR_HEIGHT + 1), 35, (11 * i) + 5);
	copyXPMArea(39, 84, (3 * (CHAR_WIDTH + 1)), (CHAR_HEIGHT + 1), 39,
				(11 * i) + 5);
}

/* 	Read a line from a file to obtain a pair setting=value 
	skips # and leading spaces
	NOTE: if setting finish with 0, 1, 2, 3 or 4 last char are deleted and
	index takes its value... if not index will get -1 
	Returns -1 if no setting=value
*/
int ReadLine(FILE * fp, char *setting, char *value, int *mbox_index)
{
	char buf[BUF_SIZE];
	char *p, *q;
	int len, aux;

	*setting = 0;
	*value = 0;
	*mbox_index = -1;

	if (!fp || feof(fp) || !fgets(buf, BUF_SIZE - 1, fp))
		return -1;

	len = strlen(buf);

	if (buf[len - 1] == '\n') {
		buf[len - 1] = 0;		/* strip linefeed */
	}
	for (p = (char *) buf; *p != '#' && *p; p++);
	*p = 0;						/* Strip comments */
	if (!(p = strtok(buf, "=")))
		return -1;
	if (!(q = strtok(NULL, "\n")))
		return -1;

	/* Chg - Mark Hurley
	 * Date: May 8, 2001
	 * Removed for loop (which removed leading spaces)
	 * Leading & Trailing spaces need to be removed
	 * to Fix Debian bug #95849
	 */
	FullTrim(p);
	FullTrim(q);

	strcpy(setting, p);
	strcpy(value, q);

	len = strlen(setting) - 1;
	if (len > 0) {
		aux = setting[len] - 48;
		if (aux > -1 && aux < 5) {
			setting[len] = 0;
			*mbox_index = aux;
		}
	} else
		*mbox_index = -1;

	DMA(DEBUG_INFO, "@%s.%d=%s@\n", setting, *mbox_index, value);
	return 1;
}

void parse_mbox_path(int item)
{
	if (!strncasecmp(mbox[item].path, "pop3:", 5)) {	/* pop3 account */
		pop3Create((&mbox[item]), mbox[item].path);
	} else if (!strncasecmp(mbox[item].path, "shell:", 6)) {	/* generic cmd */
		shellCreate((&mbox[item]), mbox[item].path);
	} else if (!strncasecmp(mbox[item].path, "gicu:", 5)) {	/* gnomeicu check */
		char buf[255];
		if (isdigit(mbox[item].path[5])) {
			sprintf(buf, "shell:::echo `gnomeicu-client -u%s msgcount` new",
					mbox[item].path + 5);
		} else {
			sprintf(buf, "shell:::echo `gnomeicu-client msgcount` new");
		}
		shellCreate((&mbox[item]), buf);
	} else if (!strncasecmp(mbox[item].path, "finger:", 7)) {
		char buf[255];
		sprintf(buf, "shell:::finger -lm %s | "
				"perl -ne '(/^new mail/i && print \"new\");' "
				"-e '(/^mail last read/i && print \"old\");' "
				"-e '(/^no( unread)? mail/i && print \"no\");'",
				mbox[item].path + 7);
		shellCreate((&mbox[item]), buf);
	} else if (!strncasecmp(mbox[item].path, "licq:", 5)) {	/* licq history file */
		licqCreate((&mbox[item]), mbox[item].path);
	} else if (!strncasecmp(mbox[item].path, "imap:", 5) ||	/* imap4 account */
			   !strncasecmp(mbox[item].path, "sslimap:", 8) ||	/* sslimap4 account */
			   !strncasecmp(mbox[item].path, "imaps:", 6)) {	/* sslimap4 account */
		imap4Create((&mbox[item]), mbox[item].path);
	} else if (!strncasecmp(mbox[item].path, "maildir:", 8)) {	/* maildir */
		maildirCreate((&mbox[item]), mbox[item].path);
	} else
		mboxCreate((&mbox[item]), mbox[item].path);	/* default are mbox */
}

int Read_Config_File(char *filename, int *loopinterval)
{
	FILE *fp;
	char setting[17], value[250];
	int mbox_index;

	if (!(fp = fopen(filename, "r"))) {
		DMA(DEBUG_ERROR, "Unable to open %s, no settings read: %s\n",
			filename, strerror(errno));
		return 0;
	}
	while (!feof(fp)) {
		if (ReadLine(fp, setting, value, &mbox_index) == -1)
			continue;
		/* settings that can be global go here. */
		if (!strcmp(setting, "interval")) {
			*loopinterval = atoi(value);
		} else if (!strcmp(setting, "askpass")) {
			const char *askpass = strdup(value);
			if (mbox_index == -1) {
				DMA(DEBUG_INFO, "setting all to askpass %s\n", askpass);
				for (mbox_index = 0; mbox_index < 5; mbox_index++)
					mbox[mbox_index].askpass = askpass;
			} else {
				mbox[mbox_index].askpass = askpass;
			}
		} else if (!strcmp(setting, "skinfile")) {
			skin_filename = strdup(value);
		} else if (mbox_index == -1)
			continue;			/* Didn't read any setting.[0-5] value */
		/* now only local settings */
		if (!strcmp(setting, "label.")) {
			strcpy(mbox[mbox_index].label, value);
		} else if (!strcmp(setting, "path.")) {
			strcpy(mbox[mbox_index].path, value);
		} else if (!strcmp(setting, "notify.")) {
			strcpy(mbox[mbox_index].notify, value);
		} else if (!strcmp(setting, "action.")) {
			strcpy(mbox[mbox_index].action, value);
		} else if (!strcmp(setting, "interval.")) {
			mbox[mbox_index].loopinterval = atoi(value);
		} else if (!strcmp(setting, "fetchcmd.")) {
			strcpy(mbox[mbox_index].fetchcmd, value);
		} else if (!strcmp(setting, "fetchinterval.")) {
			mbox[mbox_index].fetchinterval = atoi(value);
		} else if (!strcmp(setting, "debug.")) {
			int debug_value = debug_default;
			if (strcasecmp(value, "all") == 0) {
				debug_value = DEBUG_ALL;
			}
			/* could disable debugging, but I want the command
			   line argument to provide all information
			   possible. */
			mbox[mbox_index].debug = debug_value;
		}
	}
	fclose(fp);
	for (mbox_index = 0; mbox_index < 5; mbox_index++)
		if (mbox[mbox_index].label[0] != 0)
			parse_mbox_path(mbox_index);
	return 1;
}

/*
 * NOTE: this function assumes that the ConnectionNumber() macro
 *       will return the file descriptor of the Display struct
 *       (it does under XFree86 and solaris' openwin X)
 */
void XSleep(int millisec)
{
#ifdef USE_POLL
	struct pollfd timeout;

	timeout.fd = ConnectionNumber(display);
	timeout.events = POLLIN;

	poll(&timeout, 1, millisec);
#else
	struct timeval to;
	struct timeval *timeout = NULL;
	fd_set readfds;
	int max_fd;

	if (millisec >= 0) {
		timeout = &to;
		to.tv_sec = millisec / 1000;
		to.tv_usec = (millisec % 1000) * 1000;
	}
	FD_ZERO(&readfds);
	FD_SET(ConnectionNumber(display), &readfds);
	max_fd = ConnectionNumber(display);

	select(max_fd + 1, &readfds, NULL, NULL, timeout);
#endif
}

void sigchld_handler(int sig __attribute__ ((unused)))
{
	while (waitpid(0, NULL, WNOHANG) > 0);
	signal(SIGCHLD, sigchld_handler);
}

int main(int argc, char *argv[])
{
	char uconfig_file[256];

	parse_cmd(argc, argv, uconfig_file);

	/* decide what the config file is */
	if (uconfig_file[0] != 0) {	/* user-specified config file */
		DMA(DEBUG_INFO, "Using user-specified config file '%s'.\n",
			uconfig_file);
	} else {
		sprintf(uconfig_file, "%s/.wmbiffrc", getenv("HOME"));
	}

	if (wmbiffrc_permissions_check(uconfig_file) == 0) {
		DMA(DEBUG_ERROR,
			"WARNING: In future versions of WMBiff, .wmbiffrc MUST be\n"
			"owned by the user, and not readable or writable by others.\n\n");
	}
	init_biff(uconfig_file);
	signal(SIGCHLD, sigchld_handler);
	do_biff(argc, argv);
	return 0;
}

void parse_cmd(int argc, char **argv, char *config_file)
{
	int i;

	char uconfig_file[256];

	uconfig_file[0] = 0;
	/* Parse Command Line */

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (*arg == '-') {
			switch (arg[1]) {
			case 'd':
				if (strcmp(arg + 1, "debug") == 0) {
					debug_default = DEBUG_ALL;
				} else if (strcmp(arg + 1, "display")) {
					usage();
					exit(EXIT_FAILURE);
				}
				break;
			case 'g':
				if (strcmp(arg + 1, "geometry")) {
					usage();
					exit(EXIT_FAILURE);
				}
				break;
			case 'v':
				printversion();
				exit(EXIT_SUCCESS);
				break;
			case 'c':
				if (argc > (i + 1)) {
					strcpy(uconfig_file, argv[i + 1]);
					i++;
				}
				break;
			default:
				usage();
				exit(EXIT_SUCCESS);
				break;
			}
		}
	}
	strcpy(config_file, uconfig_file);
}

void usage(void)
{
	printf("\nwmBiff v" VERSION
		   " - incoming mail checker\n"
		   "Gennady Belyakov and others (see the README file)\n"
		   "Please report bugs to wmbiff-devel@lists.sourceforge.net\n"
		   "\n"
		   "usage:\n"
		   "    -debug                    enable debugging\n"
		   "    -display <display name>   use specified X display\n"
		   "    -geometry +XPOS+YPOS      initial window position\n"
		   "    -c <filename>             use specified config file\n"
		   "    -h                        this help screen\n"
		   "    -v                        print the version number\n"
		   "\n");
}

void printversion(void)
{
	printf("wmbiff v%s\n", VERSION);
}

/* vim:set ts=4: */
