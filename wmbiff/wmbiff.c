/* $Id: wmbiff.c,v 1.6 2001/10/04 08:54:02 jordi Exp $ */

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
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>

#include "../wmgeneral/wmgeneral.h"
#include "../wmgeneral/misc.h"

#include "Client.h"
#include "charutil.h"

#include "wmbiff-master.xpm"
char wmbiff_mask_bits[64 * 64];
int wmbiff_mask_width = 64;
int wmbiff_mask_height = 64;

#define CHAR_WIDTH  5
#define CHAR_HEIGHT 7

#define BLINK_TIMES 8
#define DEFAULT_SLEEP_INTERVAL 1000
#define BLINK_SLEEP_INTERVAL    200
#define DEFAULT_LOOP 5

mbox_t mbox[5];

int ReadLine(FILE *, char *, char *, int *);
int Read_Config_File(char *, int *);
int count_mail(int);
void parse_cmd(int, char **, char *);
void init_biff(char *);
void displayMsgCounters(int, int, int *, int *);

void usage(void);
void printversion(void);
void do_biff(int argc, char **argv);
void parse_mbox_path(int item);
void BlitString(char *name, int x, int y, int new);
void BlitNum(int num, int x, int y, int new);
void ClearDigits(int i);
void XSleep(int millisec);
void sigchld_handler(int sig);


void init_biff(char *uconfig_file)
{
	int i, j, loopinterval = DEFAULT_LOOP;
	char config_file[256];
	char *m;

	for (i = 0; i < 5; i++) {
		mbox[i].label[0] = 0;
		mbox[i].path[0] = 0;
		mbox[i].notify[0] = 0;
		mbox[i].action[0] = 0;
		mbox[i].fetchcmd[0] = 0;
		mbox[i].loopinterval = 0;
	}

	/* Some defaults, if config file is unavailable */
	strcpy(mbox[0].label, "Spool");
	if ((m = getenv("MAIL")) != NULL) {
		strcpy(mbox[0].path, m);
	} else if ((m = getenv("USER")) != NULL) {
		strcpy(mbox[0].path, "/var/mail/");
		strcat(mbox[0].path, m);
	}

	/* Read config file */
	if (uconfig_file[0] != 0) {
		/* user-specified config file */
		fprintf(stderr, "Using user-specified config file '%s'.\n",
				uconfig_file);
		strcpy(config_file, uconfig_file);
	} else
		sprintf(config_file, "%s/.wmbiffrc", getenv("HOME"));

#ifdef DEBUG
	printf("config_file = %s.\n", config_file);
#endif

	if (!Read_Config_File(config_file, &loopinterval)) {
		if (m == NULL) {
			fprintf(stderr, "Cannot open '%s' nor use the "
					"MAIL environment var.\n", uconfig_file);
			exit(1);
		}
		/* we are using MAIL environment var. type mbox */
		fprintf(stderr, "Using MAIL environment var '%s'.\n", m);
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


	createXBMfromXPM(wmbiff_mask_bits, wmbiff_master_xpm,
					 wmbiff_mask_width, wmbiff_mask_height);

	openXwindow(argc, argv, wmbiff_master_xpm, wmbiff_mask_bits,
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
			displayMsgCounters(i, count_mail(i), &Sleep_Interval,
							   &Blink_Mode);
#ifdef DEBUG
			printf("[%d].label=>%s<\n[%d].path=>%s<\n\n", i,
				   mbox[i].label, i, mbox[i].path);
#endif
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
					mbox[i].prevtime = curtime;
					displayMsgCounters(i, count_mail(i), &Sleep_Interval,
									   &Blink_Mode);
#ifdef DEBUG
					printf("[%d].label=>%s<\n[%d].path=>%s<\n\n",
						   i, mbox[i].label, i, mbox[i].path);
					printf("curtime=%d, prevtime=%d, interval=%d\n",
						   (int) curtime,
						   (int) mbox[i].prevtime, mbox[i].loopinterval);
#endif
				}
			}
			if (mbox[i].blink_stat > 0) {
				if (--mbox[i].blink_stat <= 0) {
					Blink_Mode &= ~(1 << i);
					mbox[i].blink_stat = 0;
				}
				displayMsgCounters(i, 1, &Sleep_Interval, &Blink_Mode);
				NeedRedraw = 1;
#ifdef DEBUG
				/*printf("i= %d, Sleep_Interval= %d\n", i, Sleep_Interval); */
#endif
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

void displayMsgCounters(int i, int mail_stat, int *Sleep_Interval,
						int *Blink_Mode)
{
	switch (mail_stat) {
	case 2:					/* New mail has arrived */
		/* Enter blink-mode for digits */
		mbox[i].blink_stat = BLINK_TIMES * 2;
		*Sleep_Interval = BLINK_SLEEP_INTERVAL;
		*Blink_Mode |= (1 << i);	/* Global blink flag set for this mailbox */
		ClearDigits(i);			/* Clear digits */
		if ((mbox[i].blink_stat & 0x01) == 0) {
			BlitNum(mbox[i].UnreadMsgs, 45, (11 * i) + 5, 1);	/* Yellow digits */
		}
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
		ClearDigits(i);			/* Clear digits */
		if ((mbox[i].blink_stat & 0x01) == 0) {
			if (mbox[i].UnreadMsgs > 0) {	/* New mail arrived */
				BlitNum(mbox[i].UnreadMsgs, 45, (11 * i) + 5, 1);	/* Yellow digits */
			} else {
				BlitNum(mbox[i].TotalMsgs, 45, (11 * i) + 5, 0);	/* Cyan digits */
			}
		}
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
void BlitString(char *name, int x, int y, int new)
{
	int i, c, k = x;

	for (i = 0; name[i]; i++) {
		c = toupper(name[i]);
		if (c >= 'A' && c <= 'Z') {	/* it's a letter */
			c -= 'A';
			copyXPMArea(c * (CHAR_WIDTH + 1), 74,
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
	copyXPMArea(39, 84, (3 * (CHAR_WIDTH + 1)), (CHAR_HEIGHT + 1), 39, (11 * i) + 5);	/* Clear digits */
}

/* 	Read a line from a file to obtain a pair setting=value 
	skips # and leading spaces
	NOTE: if setting finish with 0, 1, 2, 3 or 4 last char are deleted and
	index takes its value... if not index will get -1 
	Returns -1 if no setting=value
*/
int ReadLine(FILE * fp, char *setting, char *value, int *index)
{
	char buf[BUF_SIZE];
	char *p, *q;
	int len, aux;

	*setting = 0;
	*value = 0;

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
			*index = aux;
		}
	} else
		*index = -1;

#ifdef DEBUG
	printf("@%s.%d=%s@\n", setting, *index, value);
#endif
	return 1;
}

void parse_mbox_path(int item)
{
	if (!strncasecmp(mbox[item].path, "pop3:", 5)) {	/* pop3 account */
		pop3Create((&mbox[item]), mbox[item].path);
	} else if (!strncasecmp(mbox[item].path, "licq:", 5)) {	/* licq history file */
		licqCreate((&mbox[item]), mbox[item].path);
	} else if (!strncasecmp(mbox[item].path, "imap:", 5)) {	/* imap4 account */
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
	int index;

	if (!(fp = fopen(filename, "r"))) {
		perror("Read_Config_File");
		fprintf(stderr, "Unable to open %s, no settings read.\n",
				filename);
		return 0;
	}
	while (!feof(fp)) {
		if (ReadLine(fp, setting, value, &index) == -1)
			continue;
		if (!strcmp(setting, "interval")) {
			*loopinterval = atoi(value);
		} else if (index == -1)
			continue;			/* Didn't read any setting.[0-5] value */
		if (!strcmp(setting, "label.")) {
			strcpy(mbox[index].label, value);
		} else if (!strcmp(setting, "path.")) {
			strcpy(mbox[index].path, value);
		} else if (!strcmp(setting, "notify.")) {
			strcpy(mbox[index].notify, value);
		} else if (!strcmp(setting, "action.")) {
			strcpy(mbox[index].action, value);
		} else if (!strcmp(setting, "interval.")) {
			mbox[index].loopinterval = atoi(value);
		} else if (!strcmp(setting, "fetchcmd.")) {
			strcpy(mbox[index].fetchcmd, value);
		} else if (!strcmp(setting, "fetchinterval.")) {
			mbox[index].fetchinterval = atoi(value);
		}
	}
	for (index = 0; index < 5; index++)
		if (mbox[index].label[0] != 0)
			parse_mbox_path(index);
	fclose(fp);
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

void sigchld_handler(int sig)
{
	waitpid(0, NULL, WNOHANG);
	signal(SIGCHLD, sigchld_handler);
}

int main(int argc, char *argv[])
{
	char uconfig_file[256];

	parse_cmd(argc, argv, uconfig_file);
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
				if (strcmp(arg + 1, "display")) {
					usage();
					exit(1);
				}
				break;
			case 'g':
				if (strcmp(arg + 1, "geometry")) {
					usage();
					exit(1);
				}
				break;
			case 'v':
				printversion();
				exit(0);
				break;
			case 'c':
				if (argc > (i + 1)) {
					strcpy(uconfig_file, argv[i + 1]);
					i++;
				}
				break;
			default:
				usage();
				exit(0);
				break;
			}
		}
	}
	strcpy(config_file, uconfig_file);
}

void usage(void)
{
	fprintf(stderr,
			"\nwmBiff v" WMBIFF_VERSION
			" - incoming mail checker\n"
			"Gennady Belyakov <gb@ccat.elect.ru> and others (see the README file)\n"
			"\n");
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "    -display <display name>\n");
	fprintf(stderr,
			"    -geometry +XPOS+YPOS      initial window position\n");
	fprintf(stderr,
			"    -c <filename>             use specified config file\n");
	fprintf(stderr, "    -h                        this help screen\n");
	fprintf(stderr,
			"    -v                        print the version number\n");
	fprintf(stderr, "\n");
}

void printversion(void)
{
	fprintf(stderr, "wmbiff v%s\n", WMBIFF_VERSION);
}

/* vim:set ts=4: */
