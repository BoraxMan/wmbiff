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

	union {
		struct {
			time_t ctime;
			time_t mtime;
			size_t size;
		} mbox;
		struct {
			time_t ctime_new;
			time_t mtime_new;
			size_t size_new;
			time_t ctime_cur;
			time_t mtime_cur;
			size_t size_cur;
		} maildir;
		struct {
			char password[32];
			char userName[32];
			char serverName[256];
			int serverPort;
			int localPort;
		} pop;
	} u;

	FILE *(*open) (Pop3);
	int (*checkMail) (Pop3);

	time_t prevtime;
	time_t prevfetch_time;
	int loopinterval;			/* loop interval for this mailbox */
} mbox_t;

#define BUF_SIZE 1024

int sock_connect(char *hostname, int port);
int pop3Create(Pop3 pc, char *str);
int imap4Create(Pop3 pc, char *str);
int licqCreate(Pop3 pc, char *str);
int mboxCreate(Pop3 pc, char *str);
int maildirCreate(Pop3 pc, char *str);
FILE *openMailbox(Pop3 pc);
int parse_old_pop3_path(Pop3 pc, char *str);
int parse_new_pop3_path(Pop3 pc, char *str);

#endif
/* vim:set ts=4: */
