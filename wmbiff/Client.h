/* $Id: Client.h,v 1.9 2002/03/02 05:58:16 bluehal Exp $ */
/* Author : Scott Holden ( scotth@thezone.net )
   Modified : Yong-iL Joh ( tolkien@mizi.com )
   Modified : Jorge García ( Jorge.Garcia@uv.es )
 *
 * Email Checker Pop3/Imap4/Licq/mbox/maildir
 *
 * Last Updated : Mar 20, 05:32:35 CET 2001     
 *
 */

#ifndef CLIENT
#define CLIENT

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef WITH_GCRYPT
#include <gcrypt.h>
#endif

typedef struct _mbox_t *Pop3;
typedef struct _mbox_t {
	char label[32];				/* Printed at left; max 5 chars */
	char path[256];				/* Path to mailbox */
	char notify[256];			/* Program to notify mail arrivation */
	char action[256];			/* Action to execute on mouse click */
	char fetchcmd[256];			/* Action for mail fetching for pop3/imap */
	int fetchinterval;
	int TotalMsgs;				/* Total messages in mailbox */
	int UnreadMsgs;				/* New (unread) messages in mailbox */
	int OldMsgs;
	int OldUnreadMsgs;
	int blink_stat;				/* blink digits flag-counter */
	int debug;					/* debugging status */

	union {
		struct {
			time_t ctime;
			time_t mtime;
			off_t size;
		} mbox;
		struct {
			time_t ctime_new;
			time_t mtime_new;
			off_t size_new;
			time_t ctime_cur;
			time_t mtime_cur;
			off_t size_cur;
		} maildir;
		struct {
			char password[32];
			char userName[32];
			char serverName[256];
			int serverPort;
			int localPort;
			char authList[100];
		} pop;
		struct {
			char password[32];
			char userName[32];
			char serverName[256];
			int serverPort;
			int localPort;
			char authList[100];
			unsigned int dossl:1;
		} imap;
	} u;

	FILE *(*open) (Pop3);
	int (*checkMail) (Pop3);

	time_t prevtime;
	time_t prevfetch_time;
	int loopinterval;			/* loop interval for this mailbox */
} mbox_t;

#define BUF_SIZE 1024

int sock_connect(const char *hostname, int port);
int pop3Create(Pop3 pc, const char *str);
int imap4Create(Pop3 pc, const char *str);
int licqCreate(Pop3 pc, char *str);
int mboxCreate(Pop3 pc, char *str);
int maildirCreate(Pop3 pc, char *str);
FILE *openMailbox(Pop3 pc);

/* _NONE is for silent operation.  _ERROR is for things that should
   be printed assuming that the user might possibly see them. _INFO is
   for reasonably useless but possibly interesting messages. _ALL is
   for everything.  Restated, _ERROR will always be printed, _INFO only
   if debugging messages were requested. */
#define DEBUG_NONE  0
#define DEBUG_ERROR 1
#define DEBUG_INFO  2
#define DEBUG_ALL   2
/* inspired by ksymoops-2.3.4/ksymoops.h */
#define DM(mbox, msglevel, X...) \
do { \
  if (mbox == NULL || (mbox)->debug >= msglevel) { \
     printf("wmbiff: " X); \
(void)fflush(NULL); \
  } \
} while(0)

extern int debug_default;
#define DMA(msglevel, X...) \
do { \
  if (debug_default >= msglevel) { \
     printf("wmbiff: " ##X); \
(void)fflush(NULL); \
  } \
} while(0)

#endif
/* vim:set ts=4: */
