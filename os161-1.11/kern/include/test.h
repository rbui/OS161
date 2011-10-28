#ifndef _TEST_H_
#define _TEST_H_

#include "opt-A1.h"
/*
 * Declarations for test code and other miscellaneous functions.
 */


/* These are only actually available if OPT_SYNCHPROBS is set. */
int catmouse(int, char **);
int createcars(int, char **);

/*
 * Test code.
 */

/* lib tests */
int arraytest(int, char **);
int bitmaptest(int, char **);
int queuetest(int, char **);

/* thread tests */
int threadtest(int, char **);
int threadtest2(int, char **);
int threadtest3(int, char **);
int semtest(int, char **);
int locktest(int, char **);
#if OPT_A1
int lockTest1(int, char **);
#endif

int cvtest(int, char **);

/* filesystem tests */
int fstest(int, char **);
int readstress(int, char **);
int writestress(int, char **);
int writestress2(int, char **);
int createstress(int, char **);
int printfile(int, char **);

/* other tests */
int malloctest(int, char **);
int mallocstress(int, char **);
int nettest(int, char **);

/* Kernel menu system */
void menu(char *argstr);

/* Routine for running userlevel test code. */
int runprogram(char *progname);

#endif /* _TEST_H_ */
