#include "Client.h"
#include "passwordMgr.h"

int debug_default = DEBUG_INFO;

/* return 1 if fail, 0 if success */
int test_backtickExpand(void) {
  const char *tests[] = 
      { "prefix`echo 1`suffix", 
        "prefix`echo 1`", 
        "`echo 1`", 
        "`echo a b` c", 
        "`false`", 
        NULL };
  const char *solns[] = 
      { "prefix1suffix", 
        "prefix1", 
        "1", 
        "a b c", 
        "", 
        NULL };
  int i;
  int retval = 0;
  for(i=0; tests[i]!=NULL; i++) {
    char *x = backtickExpand(NULL, tests[i]);
    if(strcmp(x, solns[i]) != 0) {
      printf("test failed: [%s] != [%s]\n", 
             tests[i], solns[i]);
      retval = 1;
    }
    free(x);
  }
  printf("backtick tests complete\n");
  return(retval);
}

/* return 1 if fail, 0 if success */
int test_passwordMgr(void) {
	const char *b;
	mbox_t m;

	/* sh is almost certainly conforming; owned by root, etc. */
	if (!permissions_ok(NULL, "/bin/sh")) {
		printf("FAILURE: permission checker failed on /bin/sh.");
		return(1);
	}
	/* tmp is definitely bad; and better be og+w */
	if (permissions_ok(NULL, "/tmp")) {
		printf("FAILURE: permission checker failed on /tmp.");
		return(1);
	}
	/* TODO: also find some user-owned binary that shouldn't be g+w */
	printf("SUCCESS: permission checker sanity check went well.\n");

	/* *** */
	m.askpass = "echo xlnt; #";

	b = passwordFor("bill", "ted", &m, 0);
	if (strcmp(b, "xlnt") != 0) {
		printf("FAILURE: expected 'xlnt' got '%s'\n", b);
		return(1);
	}
	printf("SUCCESS: expected 'xlnt' got '%s'\n", b);

	/* *** */
	m.askpass = "should be cached";
	b = passwordFor("bill", "ted", &m, 0);
	if (strcmp(b, "xlnt") != 0) {
		printf("FAILURE: expected 'xlnt' got '%s'\n", b);
		return(1);
	}

	printf("SUCCESS: cached 'xlnt' correctly\n");

	/* *** */
	m.askpass = "echo abcdefghi1abcdefghi2abcdefghi3a; #";

	b = passwordFor("abbot", "costello", &m, 0);
	if (strcmp(b, "abcdefghi1abcdefghi2abcdefghi3a") != 0) {
		printf
			("FAILURE: expected 'abcdefghi1abcdefghi2abcdefghi3a' got '%s'\n",
			 b);
		return(1);
	}
	printf
		("SUCCESS: expected 'abcdefghi1abcdefghi2abcdefghi3ab' got '%s'\n",
		 b);

	/* try overflowing the buffer */
	m.askpass = "echo abcdefghi1abcdefghi2abcdefghi3ab; #";
	b = passwordFor("laverne", "shirley", &m, 0);
	/* should come back truncated to fill the buffer */
	if (strcmp(b, "abcdefghi1abcdefghi2abcdefghi3a") != 0) {
		printf
			("FAILURE: expected 'abcdefghi1abcdefghi2abcdefghi3a' got '%s'\n",
			 b);
		return(1);
	}
	printf
		("SUCCESS: expected 'abcdefghi1abcdefghi2abcdefghi3ab' got '%s'\n",
		 b);

	/* make sure we still have the old one */
	b = passwordFor("bill", "ted", &m, 0);
	if (strcmp(b, "xlnt") != 0) {
		printf("FAILURE: expected 'xlnt' got '%s'\n", b);
		return(1);
	}
	printf("SUCCESS: expected 'xlnt' got '%s'\n", b);

	/* what it's like if ssh-askpass is cancelled - should drop the mailbox */
#if 0
    /* will exit on our behalf; not so good for continued testing. */
	m.askpass = "echo -n ; #";
	b = passwordFor("abort", "me", &m, 0);
	if (strcmp(b, "") != 0) {
		printf("FAILURE: expected '' got '%s'\n", b);
		return(1);
	}
	printf("SUCCESS: expected '' got '%s'\n", b);
#endif

	/* what it's like if ssh-askpass is ok'd with an empty password. */
	m.askpass = "echo ; #";
	b = passwordFor("try", "again", &m, 0);
	if (strcmp(b, "") != 0) {
		printf("FAILURE: expected '' got '%s'\n", b);
		return(1);
	}
	printf("SUCCESS: expected '' got '%s'\n", b);
    return(0);
}

int main(int argc __attribute__((unused)), 
         char *argv[] __attribute__((unused))) {
    if( test_backtickExpand() || 
        test_passwordMgr() ) {
        printf("SOME TESTS FAILED!\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Success! on all tests.\n");
        exit(EXIT_SUCCESS);
    }
}

/* vim:set ts=4: */
/*
 * Local Variables:
 * tab-width: 4
 * c-indent-level: 4
 * c-basic-offset: 4
 * End:
 */


