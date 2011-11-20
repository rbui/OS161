#ifndef _TEST_H_
#define _TEST_H_

#include "opt-A1.h"
<<<<<<< HEAD
#include "opt-A2.h"
=======
>>>>>>> fed23e5f1c87e30850e46c38ef4eb0b6c6737c5c
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

<<<<<<< HEAD
#if OPT_A2
int runprogram(char *progname, char** args, unsigned long nargs);
#else
/* Routine for running userlevel test code. */
int runprogram(char *progname);
#endif
=======
/* Routine for running userlevel test code. */
int runprogram(char *progname);

>>>>>>> fed23e5f1c87e30850e46c38ef4eb0b6c6737c5c
#endif /* _TEST_H_ */
