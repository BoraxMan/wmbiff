#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>

int debug_default = 2;

int indices[12];
char *sequence[][4] = { 
  { NULL, NULL, NULL, NULL },
  { "prefix", " hello", NULL },
  { "pre", "fix", " hello", NULL },
};

/* trick tlscomm into believing it can read. */
int read(int s, char *buf, int buflen) {
  int val = indices[s]++;

  if(sequence[s][val] == NULL) {
    indices[s]--; /* make it stay */
    return 0;
  } else {
    strncpy(buf, sequence[s][val], buflen);
    printf("read: %s\n", sequence[s][val]);
    return(strlen(sequence[s][val]));
  }
}

int select(int nfds, fd_set *r, fd_set *w, fd_set *x, struct timeval *tv) {
  int i;
  int ready;
  for(i=0;i<nfds;i++) {
    if(FD_ISSET(i,r) && sequence[i][indices[i]] != NULL) {
      ready++;
    } else {
      FD_CLR(i,r);
    }
  }
  if(ready == 0) {
    printf("botched.\n");
  }
}

#define BUF_SIZE 1024

struct connection_state {
	int sd;
	char *name;
	/*@null@ */ void *tls_state;
	/*@null@ */ void *xcred;
	char unprocessed[BUF_SIZE];
	void * pc;					/* mailbox handle for debugging messages */
};

int main(int argc, char **argv) {
  char buf[255];
  struct connection_state scs;
  scs.name = "test";
  scs.unprocessed[0] = '\0';
  scs.pc = NULL;
  // alarm(10);
  
  for(scs.sd = 1; scs.sd < 3; scs.sd++) {
    memset(scs.unprocessed, 0, BUF_SIZE);
    printf("%d\n", tlscomm_expect(&scs, "prefix", buf, 255));
  }
  
}
