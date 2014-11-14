/* Author : Scott Holden ( scotth@thezone.net )
 * 
 * Pop3 Email checker.
 *
 * Last Updated : Mar 20, 1999
 *
 */


#include "Pop3Client.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#undef DEBUG_POP3
/* #define DEBUG_POP3 */

struct pop3_struct{
        struct sockaddr_in server;
        struct hostent *hp;
        enum   {CONNECTED, NOT_CONNECTED} connected;
        char   inBuf[1024];
        int    inBufSize;
        char   outBuf[1024];
        int    outBufSize;
        int    s;
        char   password[256];
        char   userName[256];
	char   serverName[256];
        int    serverPort;
        int    localPort;
        int    numOfMessages;
        int    numOfUnreadMessages;

};

Pop3 pop3Create(char *str)
{
  /* POP3 format: pop3:user:password@server[:port] */
  Pop3 pc;
  char *tmp;
  char *p;

#ifdef DEBUG_POP3
  printf("pop3: str = '%s'\n", str);
#endif

  pc = (Pop3)malloc( sizeof(*pc) );
  if( pc == 0)
        return 0;
  strcpy(pc->password , "");
  strcpy(pc->userName , "");
  strcpy(pc->serverName , "");
  pc->connected     = NOT_CONNECTED;
  pc->serverPort    = 110;
  pc->localPort     = 0;
  pc->numOfMessages = 0;
  pc->numOfUnreadMessages = 0;
  pc->inBufSize     = 0;
  pc->outBufSize    = 0;
  tmp = strdup(str);
  p = strtok(tmp, ":");				/* cut off ``pop3:'' */
  p = strtok(NULL, ":");			/* p pointed to username */
  strcpy(pc->userName, p);
  p = strtok(NULL, "@");			/* p -> password */
  strcpy(pc->password, p);
  p = strtok(NULL, ":");			/* p-> server */
  strcpy(pc->serverName, p);
  if( (p = strtok(NULL, ":")) != NULL ) {	/* port selected; p -> port */
    pc->serverPort = atoi(p);
  }
  free(tmp);
#ifdef DEBUG_POP3
/*  printf("pop3: mbox[%d].path= '%s'\n", item, mbox[item].path); */
  printf("pop3: userName= '%s'\n", pc->userName);
  printf("pop3: password= '%s'\n",pc->password);
  printf("pop3: serverName= '%s'\n", pc->serverName);
  printf("pop3: serverPort= '%d'\n", pc->serverPort);
#endif

  return pc;
}

int pop3MakeConnection(Pop3 pc)
{
    pc->s = socket(AF_INET, SOCK_STREAM, 0 );
    memset( &pc->server, 0 , sizeof(pc->server));
    pc->server.sin_family = AF_INET;
    pc->hp = gethostbyname(pc->serverName);
    if( pc->hp == 0)
        return -1;
    memcpy( &pc->server.sin_addr, pc->hp->h_addr, pc->hp->h_length);
    pc->server.sin_port = htons(pc->serverPort);
    if ( connect(pc->s, (struct sockaddr *)&pc->server
                               , sizeof(pc->server)) < 0 )
        return -1;
    pc->connected = CONNECTED;
   return 0;
}

int pop3IsConnected(Pop3 pc)
{
    if( pc->connected == CONNECTED)
        return 1;
    return 0;
}

int pop3Login(Pop3 pc)
{
      int size; 
      char temp[1024];

      if( pc->connected == NOT_CONNECTED ){
          fprintf(stderr, "Not Connected To Server '%s:%d'\n", pc->serverName, pc->serverPort);
          return -1;
      }

      size = recv(pc->s,&pc->inBuf,1024,0);
      memset(&temp,0,1024);
      memcpy(&temp,&pc->inBuf,size);
      if( temp[0] != '+' ){
          fprintf(stderr, "Error Logging in (server '%s:%d')\n", pc->serverName, pc->serverPort);
          return -1;
      }

      sprintf(pc->outBuf,"USER %s\r\n",pc->userName);
      send(pc->s, &pc->outBuf,strlen(pc->outBuf),0); 
      size =recv(pc->s,&pc->inBuf,1024,0);
      memset(&temp,0,1024);
      memcpy(&temp,&pc->inBuf,size);
      if( temp[0] != '+' ){
          fprintf(stderr, "Invalid User Name '%s@%s:%d'\n", pc->userName, pc->serverName, pc->serverPort);
          return -1;
      }

      memset(&pc->outBuf,0,1024);
      sprintf(pc->outBuf,"PASS %s\r\n",pc->password);
      send(pc->s, pc->outBuf, strlen(pc->outBuf),0 );
      size =recv(pc->s,&pc->inBuf,1024,0);
      memset(&temp,0,1024);
      memcpy(&temp,&pc->inBuf,size);
      if( temp[0] != '+' ){
          fprintf(stderr, "Incorrect Password for user '%s@%s:%d'\n", pc->userName, pc->serverName, pc->serverPort);
          return -1;
      }

      return 0;
}

int pop3CheckMail(Pop3 pc){
    int size;
    char temp[1024];
    char *ptr;
    if( pc->connected == NOT_CONNECTED )
        return -1;

     /* Find total number of messages in mail box */
     sprintf(pc->outBuf,"STAT\r\n");
     send(pc->s, pc->outBuf, strlen(pc->outBuf),0 );
     size =recv(pc->s,&pc->inBuf,1024,0);
     memset(&temp,0,1024);
     memcpy(&temp,&pc->inBuf,size);
     ptr = strtok(temp, " ");
     ptr = strtok( 0," ");
     pc->numOfMessages = atoi(ptr);

     if( temp[0] != '+' ){
         fprintf(stderr, "Error Receiving Stats '%s@%s:%d'\n", pc->userName, pc->serverName, pc->serverPort);
         return -1;
     }

     sprintf(pc->outBuf,"LAST\r\n");
     send(pc->s, pc->outBuf, strlen(pc->outBuf),0 );
     size =recv(pc->s,&pc->inBuf,1024,0);
     memset(&temp,0,1024);
     memcpy(&temp,&pc->inBuf,size);
     ptr = strtok(temp, " ");
     ptr = strtok( 0," ");
     pc->numOfUnreadMessages = pc->numOfMessages - atoi(ptr);

     if( temp[0] != '+' ){
         fprintf(stderr, "Error Receiving Stats '%s@%s:%d'\n", pc->userName, pc->serverName, pc->serverPort);
         return -1;
     }
     return 1;

}

int  pop3GetTotalNumberOfMessages( Pop3 pc ){
     if( pc != 0 )
         return pc->numOfMessages;
     return -1;    
}

int  pop3GetNumberOfUnreadMessages( Pop3 pc ){
    if( pc != 0)
        return pc->numOfUnreadMessages;
    return -1;
}

int pop3Quit(Pop3 pc){
    int size;
    if( pc->connected == NOT_CONNECTED )
        return -1;
    send(pc->s, "quit\r\n", 6,0 );
    size =recv(pc->s,&pc->inBuf,1024,0);
    pc->connected = NOT_CONNECTED;
    return 0;

}
