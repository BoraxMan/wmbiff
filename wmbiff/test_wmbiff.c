#include "Client.h"

int debug_default = DEBUG_INFO;

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

int main(int argc __attribute__((unused)), 
         char *argv[] __attribute__((unused))) {
  exit( test_backtickExpand() );
}

/* vim:set ts=4: */
/*
 * Local Variables:
 * tab-width: 4
 * c-indent-level: 4
 * c-basic-offset: 4
 * End:
 */


