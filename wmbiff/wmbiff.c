#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <utime.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>

#include "../wmgeneral/wmgeneral.h"
#include "../wmgeneral/misc.h"

#include "Pop3Client.h"

#include "wmbiff-master.xpm"
char wmbiff_mask_bits[64*64];
int  wmbiff_mask_width = 64;
int  wmbiff_mask_height = 64;

#define WMBIFF_VERSION "0.2"

#define CHAR_WIDTH  5
#define CHAR_HEIGHT 7

#define BUFFER_SIZE 1024

#define BLINK_TIMES 8
#define DEFAULT_SLEEP_INTERVAL 1000
#define BLINK_SLEEP_INTERVAL    200

#define FROM_STR   "From "
#define STATUS_STR "Status: R"

#undef DEBUG
#undef DEBUG_MAIL_COUNT
#undef DEBUG_POP3

int loopinterval=5;	/* Default rescan interval, in seconds */
int Sleep_Interval = DEFAULT_SLEEP_INTERVAL;	/* Default sleep time, in milliseconds */
int Blink_Mode = 0;	/* Bit mask, digits are ib blinking mode or not. Each bit for separate mailbox */

typedef struct {
  char label[32];	/* Printed at left; max 5 chars */
  char path[256];	/* Path to mailbox */
  char notify[256];	/* Program to notify mail arrivation */
  char action[256];	/* Action to execute on mouse click */
  char fetchcmd[256];	/* Action for mail fetching for pop3/imap */
  int  fetchinterval;
  int  TotalMsgs;	/* Total messages in mailbox */
  int  UnReadMsgs;	/* New (unread) messages in mailbox */
  int  OldMsgs;
  int  OldUnReadMsgs;
  int  blink_stat;	/* blink digits flag-counter */
  time_t ctime;
  time_t mtime;
  size_t size;
  Pop3 pc;
  time_t prevtime;
  time_t prevfetch_time;
  int  loopinterval;	/* loop interval for this mailbox */
} mbox_t;

mbox_t mbox[5];

char uconfig_file[256];

void usage(void);
void printversion(void);
void parse_cmd(int argc, char **argv);
void init_biff(int argc, char **argv);
void do_biff(int argc, char **argv);
void displayMsgCounters(int i, int mail_stat);
int  ReadConfigInt(FILE *fp, char *setting, int *value);
int  ReadConfigString(FILE *fp, char *setting, char *value);
int  Read_Config_File( char *filename );
void parse_mbox_path(int item);
FILE *open_mailbox_file(char *file);
int  count_mail(int item, int init);
void count_messages(FILE *file, int item);
void BlitString(char *name, int x, int y, int new);
void BlitNum(int num, int x, int y, int new);
void ClearDigits(int i);
void BlinkOn(int i);
void BlinkOff(int i);
void BlinkToggle(int i);
void XSleep(int millisec);
void sigchld_handler(int sig);


void init_biff(int argc, char **argv)
{
  int	i,j;
  char	config_file[512];
  char  *m;

  /* Some defaults, if config file unavailable */
  strcpy(mbox[0].label,"Spool");
  if( (m=getenv("MAIL")) != NULL ) {
    strcpy(mbox[0].path, m);
  }
  else if( (m=getenv("USER")) != NULL ) {
    strcpy(mbox[0].path, "/var/spool/mail/");
    strcat(mbox[0].path, m);
  }
  mbox[0].notify[0]=0;
  mbox[0].action[0]=0;
  mbox[0].fetchcmd[0]=0;
  mbox[0].pc=NULL;
  mbox[0].loopinterval=loopinterval;
  mbox[0].OldMsgs = mbox[0].OldUnReadMsgs = -1;
  for(i=1; i<5; i++) {
    mbox[i].label[0]=0;
    mbox[i].path[0]=0;
    mbox[i].notify[0]=0;
    mbox[i].action[0]=0;
    mbox[i].fetchcmd[0]=0;
    mbox[i].pc = NULL;
    mbox[i].loopinterval=loopinterval;
    mbox[i].OldMsgs = mbox[i].OldUnReadMsgs = -1;
  }

  /* Read config file */
  if (uconfig_file[0] != 0) {
    /* user-specified config file */
    fprintf(stderr, "Using user-specified config file '%s'.\n", uconfig_file);
    Read_Config_File(uconfig_file);
  }
  else {
    sprintf(config_file, "%s/.wmbiffrc", getenv("HOME"));
    if (!Read_Config_File(config_file) || m == NULL) {
      fprintf(stderr, "Cannot open '%s' nor use the MAIL environment var.\n", uconfig_file);
      exit(1);
    }
  }

  /* Make labels looks correctly */
  for(i=0; i<5; i++) {
    if(mbox[i].label[0] != 0) {
      for(j=0; j<5; j++)
	if(mbox[i].label[j] == 0)
	  mbox[i].label[j] = ' ';
      mbox[i].label[5]=':';
      mbox[i].label[6]=0;
    }
  }
}

void do_biff(int argc, char **argv)
{
  int	i;
  XEvent Event;
  int	but_stat = -1;
  time_t curtime;
  int	NeedRedraw = 0;
  
  createXBMfromXPM(wmbiff_mask_bits, wmbiff_master_xpm, wmbiff_mask_width, wmbiff_mask_height);
  
  openXwindow(argc, argv, wmbiff_master_xpm, wmbiff_mask_bits, wmbiff_mask_width, wmbiff_mask_height);

  AddMouseRegion(0, 5,  6, 58, 16);
  AddMouseRegion(1, 5, 16, 58, 26);
  AddMouseRegion(2, 5, 26, 58, 36);
  AddMouseRegion(3, 5, 36, 58, 46);
  AddMouseRegion(4, 5, 46, 58, 56);

/*  copyXPMArea(39, 84, (3*CHAR_WIDTH), 8, 39, 5);  */
/*  copyXPMArea(39, 84, (3*CHAR_WIDTH), 8, 39, 16); */
/*  copyXPMArea(39, 84, (3*CHAR_WIDTH), 8, 39, 27); */
/*  copyXPMArea(39, 84, (3*CHAR_WIDTH), 8, 39, 38); */
/*  copyXPMArea(39, 84, (3*CHAR_WIDTH), 8, 39, 49); */
	      
/*  BlitString("XX", 45, 5,  0); */
/*  BlitString("XX", 45, 16, 0); */
/*  BlitString("XX", 45, 27, 0); */
/*  BlitString("XX", 45, 38, 0); */
/*  BlitString("XX", 45, 49, 0); */

  /* Initially read mail counters and resets; and initially draw labels and counters */
  curtime = time(0);
  for (i=0; i<5; i++) {
    if(mbox[i].label[0] != 0) {
      mbox[i].prevtime = mbox[i].prevfetch_time = curtime;
      BlitString(mbox[i].label, 5, (11*i) + 5, 0); 
      displayMsgCounters( i, count_mail(i, 1));
#ifdef DEBUG
    printf ("[%d].label=>%s<\n[%d].path=>%s<\n\n",i,mbox[i].label,i,mbox[i].path);
#endif
    }		  
  }
  
  RedrawWindow();
  
  NeedRedraw = 0;
  while (1) {
    /* waitpid(0, NULL, WNOHANG); */
  
    for (i=0; i<5; i++) {
      if(mbox[i].label[0] != 0) {
	curtime = time(0);
	if( curtime >= mbox[i].prevtime+mbox[i].loopinterval ) {
	  NeedRedraw = 1;
	  mbox[i].prevtime = curtime;
	  displayMsgCounters(i, count_mail(i, 0));
#ifdef DEBUG
	  printf("[%d].label=>%s<\n[%d].path=>%s<\n\n",i,mbox[i].label,i,mbox[i].path);
	  printf("curtime=%d, prevtime=%d, interval=%d\n",curtime, mbox[i].prevtime,mbox[i].loopinterval);
#endif
	}
      }
      if(mbox[i].blink_stat > 0) {
	BlinkToggle(i);
	displayMsgCounters(i, 1);
	NeedRedraw = 1;
	/**/printf("i= %d, Sleep_Interval= %d\n", i, Sleep_Interval);
      }
      if(Blink_Mode == 0) {
	BlinkOff(i);
      }
      if( mbox[i].fetchinterval > 0 && mbox[i].fetchcmd[0] != 0 &&
	  curtime >= mbox[i].prevfetch_time+mbox[i].fetchinterval ) {
	execCommand(mbox[i].fetchcmd);
	mbox[i].prevfetch_time = curtime;
      }
    }
    if( NeedRedraw ) {
      NeedRedraw = 0;
      RedrawWindow();
    }
    
    /* X Events */
    while(XPending(display)) {
      XNextEvent(display, &Event);
      switch(Event.type) {
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
	  if ( but_stat == i && but_stat >= 0 ) {
	    switch(Event.xbutton.button) {
	    case 1:					/* Left-mouse click */
	      if(mbox[but_stat].action[0] != 0)
		execCommand(mbox[but_stat].action);
	      break;
	    case 3:					/* right-mouse click */
	      if(mbox[but_stat].fetchcmd[0] != 0)
		execCommand(mbox[but_stat].fetchcmd);
	      break;
	    }
	  }
	  but_stat = -1;
/*	    RedrawWindow(); */
	  break;
      }
    }
    XSleep(Sleep_Interval);
  }
}

void displayMsgCounters(int i, int mail_stat)
{
  switch( mail_stat) {
  case 2:						/* New mail has arrived */
    BlinkOn(i);						/* Enter blink-mode for digits */
    ClearDigits(i);					/* Clear digits */
    if((mbox[i].blink_stat&0x01) == 0)
      BlitNum(mbox[i].UnReadMsgs, 45, (11*i) + 5, 1);	/* Yellow digits */
    if(mbox[i].notify[0] != 0) {			/* need to call notify ? */
      if(!strcasecmp(mbox[i].notify,"beep"))		/* Internal keyword ? */
	XBell(display,100);				/* Yes, bell */
      else
	execCommand(mbox[i].notify);			/* Else call external notifyer */
    }
    if(mbox[i].fetchinterval == -1 && mbox[i].fetchcmd[0] != 0)	/* Autofetch on new mail arrival ? */
      execCommand(mbox[i].fetchcmd);			/* yes */
    break;
  case 1:						/* mailbox has been rescanned/changed */
    ClearDigits(i);					/* Clear digits */
    if((mbox[i].blink_stat&0x01) == 0) {
      if(mbox[i].UnReadMsgs > 0)			/* New mail arrived */
	BlitNum(mbox[i].UnReadMsgs, 45, (11*i) + 5, 1);	/* Yellow digits */
      else
	BlitNum(mbox[i].TotalMsgs,  45, (11*i) + 5, 0);	/* Cyan digits */
    }
    break;
  case 0:
    break;
  case -1:						/* Error was detected */
    ClearDigits(i);					/* Clear digits */
    BlitString("XX", 45, (11*i) + 5, 0);
    break;
  }
}

FILE *open_mailbox_file (char *file)
{
  FILE *mailbox;

  if ( (mailbox = fopen(file, "r")) == NULL ) {
    fprintf(stderr,"wmbiff: Error opening mailbox '%s': %s\n", file, strerror(errno));
  }
  return (mailbox);
}
                             
/** counts mail in spool-file
   if "init" is >0, messages counted even if mailbox isn't changed (for unix-style)
   Returned value:
   -1 : Error was encountered
   0  : mailbox status wasn't changed
   1  : mailbox was changed (NO new mail)
   2  : mailbox was changed AND new mail has arrived
**/
int count_mail(int item, int init)
{
  struct stat	  st;
  struct utimbuf  ut;
  FILE  *F;
  int    rc = 0;

#ifdef DEBUG_MAIL_COUNT
  printf(">Mailbox: '%s'\n",mbox[item].path);
#endif
  if(mbox[item].pc == NULL) {		/* ubix-style mailbox */
    if(stat(mbox[item].path, &st)) {
/*      if( errno != ENOENT )  */
	fprintf(stderr,"wmbiff: Can't stat mailbox '%s': %s\n", mbox[item].path, strerror(errno));
      return -1;			/* Error stating mailbox */
    }

    if(st.st_mtime != mbox[item].mtime || st.st_size != mbox[item].size || init > 0)  {  /* file was changed OR initially read */
#ifdef DEBUG_MAIL_COUNT
      printf("  was changed,"
	     " TIME: old %lu, new %lu"
	     " SIZE: old %lu, new %lu\n",
	     mbox[item].mtime,st.st_mtime,
	     mbox[item].size,st.st_size);
#endif
      ut.actime  = st.st_atime;
      ut.modtime = st.st_mtime;
      F = open_mailbox_file(mbox[item].path);
      count_messages(F, item);					/* No comments :) */
      fclose(F);
      utime(mbox[item].path, &ut);				/* Reset atime for MUTT and something others correctly work */
      mbox[item].mtime = st.st_mtime;				/* Store new mtime */
      mbox[item].size  = st.st_size;				/* Store new size */
    }
  }
  else {							/* pop3 */
    if( pop3MakeConnection(mbox[item].pc) == -1 ) {
      return -1;
    }
    if( pop3Login(mbox[item].pc) == -1 ) {
      pop3Quit(mbox[item].pc);
      return -1;
    }
    if( (pop3CheckMail(mbox[item].pc)) == -1){
      pop3Quit(mbox[item].pc);
      return -1;						/* Error connecting to pop3 server */
    }
    else{
      mbox[item].UnReadMsgs = pop3GetNumberOfUnreadMessages(mbox[item].pc);
      mbox[item].TotalMsgs  = pop3GetTotalNumberOfMessages(mbox[item].pc);
#ifdef DEBUG_POP3
      printf("OLD: %d, TOTAL: %d, NEW: %d\n",mbox[item].OldMsgs,mbox[item].TotalMsgs,mbox[item].UnReadMsgs);
#endif
      pop3Quit(mbox[item].pc);
    }
  }
  if(mbox[item].UnReadMsgs > mbox[item].OldUnReadMsgs && mbox[item].UnReadMsgs > 0)
    rc = 2;							/* New mail detected */
  else if(mbox[item].UnReadMsgs < mbox[item].OldUnReadMsgs ||
	  mbox[item].TotalMsgs != mbox[item].OldMsgs)
    rc = 1;							/* mailbox was changed - NO new mail */
  else
    rc = 0;							/* mailbox wasn't changed */
  mbox[item].OldMsgs 	   = mbox[item].TotalMsgs;
  mbox[item].OldUnReadMsgs = mbox[item].UnReadMsgs;
  return rc;
}

void count_messages(FILE *file, int item)
{
  int is_header = 0;
  int next_from_is_start_of_header = 1;
  int count_from = 0, count_status = 0;
  char buf[BUFFER_SIZE];
  int len_from = strlen(FROM_STR), len_status = strlen(STATUS_STR);

  while(fgets(buf, BUFFER_SIZE, file)) {
    if (buf[0] == '\n') {	/* a newline on itself terminates the header */
      if (is_header) is_header = 0;
      else next_from_is_start_of_header = 1;
    }
    else if(!strncmp(buf, FROM_STR, len_from))  {
      /* a line starting with "From" is the beginning of a new header 
         "From" in the text of the mail should get escaped by the MDA.
         If your MDA doesn't he is broken.
	*/
      if (next_from_is_start_of_header) is_header = 1;
      if (is_header) count_from++;
    } 
    else {
      next_from_is_start_of_header = 0;
      if(!strncmp(buf, STATUS_STR, len_status)) {
#ifdef DEBUG_MAIL_COUNT
/*	printf ("Got a status: %s",buf); */
#endif
	if (is_header) count_status++;
      }
    }
  }
#ifdef DEBUG_MAIL_COUNT
  printf ("from: %d status: %d\n", count_from, count_status);
#endif
  mbox[item].TotalMsgs = count_from;
  mbox[item].UnReadMsgs = count_from-count_status;
  return;
}

/* Blits a string at given co-ordinates
   If a ``new'' parameter is given, all digits will be yellow
*/
void BlitString(char *name, int x, int y, int new)
{
  int	i, c, k = x;

  for(i=0; name[i]; i++) {
    c = toupper(name[i]); 
    if(c >= 'A' && c <= 'Z') {   /* it's a letter */
      c -= 'A';
      copyXPMArea(c * (CHAR_WIDTH+1), 74, (CHAR_WIDTH+1), (CHAR_HEIGHT+1), k, y);
      k += (CHAR_WIDTH+1);
    }
    else {   /* it's a number or symbol */
      c -= '0';
      if ( new )
	copyXPMArea((c*(CHAR_WIDTH+1))+65, 0, (CHAR_WIDTH+1), (CHAR_HEIGHT+1), k, y);
      else
	copyXPMArea((c*(CHAR_WIDTH+1)),   64, (CHAR_WIDTH+1), (CHAR_HEIGHT+1), k, y);
      k += (CHAR_WIDTH+1);
    }
  }
}

/* Blits number to give coordinates.. two 0's, right justified */
void BlitNum(int num, int x, int y, int new)
{
  char buf[32];
  int newx=x;

  if (num >  99) newx -= (CHAR_WIDTH+1);
  if (num > 999) newx -= (CHAR_WIDTH+1);

  sprintf(buf, "%02i", num);

  BlitString(buf, newx, y, new);
}

void ClearDigits(int i)
{
  copyXPMArea(39, 84, (3*(CHAR_WIDTH+1)), (CHAR_HEIGHT+1), 39, (11*i) + 5); /* Clear digits */
}

void BlinkOn(int i)
{
  mbox[i].blink_stat = BLINK_TIMES*2;
  Sleep_Interval = BLINK_SLEEP_INTERVAL;
  Blink_Mode |= (1<<i);		/* Global blink flag set for this mailbox */
}

void BlinkOff(int i)
{
  mbox[i].blink_stat = 0;
  Sleep_Interval = DEFAULT_SLEEP_INTERVAL;
}

void BlinkToggle(int i)
{
  if(--mbox[i].blink_stat <= 0) {
    Blink_Mode &= ~(1<<i);
    mbox[i].blink_stat = 0;
  }
  
}

/* ReadConfigSetting */
int ReadConfigString(FILE *fp, char *setting, char *value)
{
  char buf[BUFFER_SIZE];
  char *p=NULL;
  int  len, slen;

  if (!fp) return 0;
  
  fseek(fp, 0, SEEK_SET);

  while( !feof(fp) ) {
    if( !fgets(buf, BUFFER_SIZE-1, fp) )
      break;
    len=strlen(buf);
    if( buf[len-1] == '\n' ) buf[len-1] = 0;			/* strip linefeed */
    for( p=(char*)buf; *p != '#' && *p; p++ ) ; *p = 0;		/* Strip comments */
    for( p=(char*)buf; (*p == ' ' || *p == '\t') && *p; p++) ;	/* Skip leading spaces */
    if( !strncmp(p, setting, slen=strlen(setting)) ) {		/* found our setting */
      p += slen;						/* point to end of keyword+1 */
      while( (*p == ' ' || *p == '=' || *p == '\t') && *p ) p++;/* Skip spaces and equal sign */
      strcpy(value, p);
      return 1;
    }
  }
  return 0;
}

int ReadConfigInt(FILE *fp, char *setting, int *value)
{
  char buf[BUFFER_SIZE];

  if (ReadConfigString(fp, setting, (char *)buf)) {
    *value = atoi(buf);
    return 1;
  }

  return 0;
}

void parse_mbox_path(int item)
{
  if(!strncasecmp(mbox[item].path,"pop3:",5)) {			/* pop3 account */
    mbox[item].pc = pop3Create(mbox[item].path);
    *strchr(mbox[item].path, ':')=0;				/* special keyword ``pop3'' */
  }
  else if(!strncasecmp(mbox[item].path,"imap:",5)) {
    fprintf(stderr, "IMAP protocol does not realized yet.\n");
    *strchr(mbox[item].path, ':')=0;				/* special keyword ``imap'' */
  }
}

int Read_Config_File( char *filename )
{
  FILE *fp;

  fp = fopen(filename, "r");
  if (fp) {
    ReadConfigInt(   fp,    "interval", &loopinterval);

    ReadConfigString(fp, "label.0",     	mbox[0].label);
    ReadConfigString(fp, "path.0",      	mbox[0].path);
    ReadConfigString(fp, "notify.0",    	mbox[0].notify);
    ReadConfigString(fp, "action.0",    	mbox[0].action);
    ReadConfigInt(   fp, "interval.0", 	       &mbox[0].loopinterval);
    ReadConfigString(fp, "fetchcmd.0",  	mbox[0].fetchcmd);
    ReadConfigInt(   fp, "fetchinterval.0",    &mbox[0].fetchinterval);
    parse_mbox_path(0);

    ReadConfigString(fp, "label.1",		mbox[1].label);
    ReadConfigString(fp, "path.1",		mbox[1].path);
    ReadConfigString(fp, "notify.1",		mbox[1].notify);
    ReadConfigString(fp, "action.1",		mbox[1].action);
    ReadConfigInt(   fp, "interval.1", 	       &mbox[1].loopinterval);
    ReadConfigString(fp, "fetchcmd.1",		mbox[1].fetchcmd);
    ReadConfigInt(   fp, "fetchinterval.1",    &mbox[1].fetchinterval);
    parse_mbox_path(1);

    ReadConfigString(fp, "label.2",		mbox[2].label);
    ReadConfigString(fp, "path.2",		mbox[2].path);
    ReadConfigString(fp, "notify.2",		mbox[2].notify);
    ReadConfigString(fp, "action.2",		mbox[2].action);
    ReadConfigInt(   fp, "interval.2",	       &mbox[2].loopinterval);
    ReadConfigString(fp, "fetchcmd.2",		mbox[2].fetchcmd);
    ReadConfigInt(   fp, "fetchinterval.2",    &mbox[2].fetchinterval);
    parse_mbox_path(2);

    ReadConfigString(fp, "label.3",		mbox[3].label);
    ReadConfigString(fp, "path.3",		mbox[3].path);
    ReadConfigString(fp, "notify.3",		mbox[3].notify);
    ReadConfigString(fp, "action.3",		mbox[3].action);
    ReadConfigInt(   fp, "interval.3",	       &mbox[3].loopinterval);
    ReadConfigString(fp, "fetchcmd.3",		mbox[3].fetchcmd);
    ReadConfigInt(   fp, "fetchinterval.3",    &mbox[3].fetchinterval);
    parse_mbox_path(3);
    
    ReadConfigString(fp, "label.4",		mbox[4].label);
    ReadConfigString(fp, "path.4",		mbox[4].path);
    ReadConfigString(fp, "notify.4",		mbox[4].notify);
    ReadConfigString(fp, "action.4",		mbox[4].action);
    ReadConfigInt(   fp, "interval.4",	       &mbox[4].loopinterval);
    ReadConfigString(fp, "fetchcmd.4",		mbox[4].fetchcmd);
    ReadConfigInt(   fp, "fetchinterval.4",    &mbox[4].fetchinterval);
    parse_mbox_path(4);

    fclose(fp);
    return 1;
  }
  else {
    perror("Read_Config_File");
    fprintf(stderr, "Unable to open %s, no settings read.\n", filename);
    return 0;
  }
}
/*
 * NOTE: this function assumes that the ConnectionNumber() macro
 *       will return the file descriptor of the Display struct
 *       (it does under XFree86 and solaris' openwin X)
 */
void XSleep(int millisec)
{
  struct timeval to;
  struct timeval *timeout = NULL;
  fd_set readfds;
  int max_fd;

  if(millisec >= 0) {
    timeout = &to;
    to.tv_sec = millisec / 1000;
    to.tv_usec = (millisec % 1000) * 1000;
  }
  FD_ZERO(&readfds);
  FD_SET(ConnectionNumber(display), &readfds);
  max_fd = ConnectionNumber(display);

  select(max_fd+1, &readfds, NULL, NULL, timeout);
}

void sigchld_handler(int sig)
{
  waitpid(0, NULL, WNOHANG);
  signal(SIGCHLD, sigchld_handler);
}

int main(int argc, char *argv[])
{
    
  parse_cmd(argc, argv);

  init_biff(argc, argv);

  signal(SIGCHLD, sigchld_handler);

  do_biff(argc, argv);

  return 0;
}

void parse_cmd(int argc, char **argv)
{
  int	i;

  uconfig_file[0] = 0;
  
  /* Parse Command Line */

  for (i=1; i<argc; i++) {
    char *arg = argv[i];

    if (*arg=='-') {
      switch (arg[1]) {
	case 'd' :
	  if (strcmp(arg+1, "display")) {
	    usage();
	    exit(1);
	  }
	  break;
	case 'g' :
	  if (strcmp(arg+1, "geometry")) {
	    usage();
	    exit(1);
	  }
	  break;
	case 'v' :
	  printversion();
	  exit(0);
	  break;
        case 'c' :
          if (argc > (i+1)) {
	    strcpy(uconfig_file, argv[i+1]);
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
}

void usage(void)
{
  fprintf(stderr, "\nwmBiff v"WMBIFF_VERSION"- incoming mail checker\nGennady Belyakov <gb@ccat.elect.ru>\n\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "    -display <display name>\n");
  fprintf(stderr, "    -geometry +XPOS+YPOS      initial window position\n");
  fprintf(stderr, "    -c <filename>             use specified config file\n");
  fprintf(stderr, "    -h                        this help screen\n");
  fprintf(stderr, "    -v                        print the version number\n");
  fprintf(stderr, "\n");
}

void printversion(void)
{
  fprintf(stderr, "wmbiff v%s\n", WMBIFF_VERSION);
}

