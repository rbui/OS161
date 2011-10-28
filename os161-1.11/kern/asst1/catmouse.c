/*
 * catmouse.c
 *
 *
*/


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>

/*
 * 
 * cat,mouse,bowl simulation functions defined in bowls.c
 *
 * For Assignment 1, you should use these functions to
 *  make your cats and mice eat from the bowls.
 * 
 * You may *not* modify these functions in any way.
 * They are implemented in a separate file (bowls.c) to remind
 * you that you should not change them.
 *
 * For information about the behaviour and return values
 *  of these functions, see bowls.c
 *
 */

/* this must be called before any calls to cat_eat or mouse_eat */
extern int initialize_bowls(unsigned int bowlcount);

extern void cat_eat(unsigned int bowlnumber);
extern void mouse_eat(unsigned int bowlnumber);
extern void cat_sleep(void);
extern void mouse_sleep(void);

/*
 *
 * Problem parameters
 *
 * Values for these parameters are set by the main driver
 *  function, catmouse(), based on the problem parameters
 *  that are passed in from the kernel menu command or
 *  kernel command line.
 *
 * Once they have been set, you probably shouldn't be
 *  changing them.
 *
 *
 */
int NumBowls;  // number of food bowls
int NumCats;   // number of cats
int NumMice;   // number of mice
int NumLoops;  // number of times each cat and mouse should eat

/*
 * Once the main driver function (catmouse()) has created the cat and mouse
 * simulation threads, it uses this semaphore to 
 *
 * numMiceEating = 0;
 * numCatEating = 0;lock until all of the
 * cat and mouse simulations are finished.
 */
struct semaphore *CatMouseWait;
struct cv * bowlEmpty;
struct lock * mutex;
/*
 * 
 * Function Definitions
 * 
 */


/*
 * cat_simulation()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds cat identifier from 0 to NumCats-1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Each cat simulation thread runs this function.
 *
 *      You need to finish implementing this function using 
 *      the OS161 synchronization primitives as indicated
 *      in the assignment description
 */
volatile int numMiceEating;
volatile int numCatEating;
volatile int numMiceWaiting, numCatWaiting;
volatile int *bowlInUse;
//0 - mouse, 1 -cat, 2 - not decided
volatile int turn;

static
void
cat_simulation(void * unusedpointer, 
	       unsigned long catnumber)
{
  int i, j;
  unsigned int bowl;

  /* avoid unused variable warnings. */
  (void) unusedpointer;
  (void) catnumber;

  numCatWaiting += 1;


  for(i=0;i<NumLoops;i++) {
    cat_sleep();

	lock_acquire(mutex);
	while( (turn != 1 && turn != 2 && (numMiceEating > 0 || numMiceWaiting > 0 ||  numCatEating == NumBowls)) ||
			( (turn == 1 || turn == 2) && numCatEating == NumBowls ) )	{

		cv_wait(bowlEmpty, mutex);
	}
	turn = 1;
	for(j = 0; j<NumBowls; j+=1 ) {
		if (bowlInUse[j] == 0) {
			bowlInUse[j] = 1;
			bowl = j+1;
			numCatEating += 1;
			break;
		}
	}
	lock_release(mutex);

    cat_eat(bowl);

	lock_acquire(mutex);
	numCatEating -= 1;
	bowlInUse[bowl-1] = 0;
	if (numCatEating == 0) {
		turn = 0;
		cv_broadcast(bowlEmpty, mutex);
	}
	lock_release(mutex);
  }

  numCatWaiting -= 1;

  /* indicate that this cat simulation is finished */
  V(CatMouseWait); 
}
	
struct cv * bowlEmpty;
struct lock * mutex;
/*
 * mouse_simulation()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds mouse identifier from 0 to NumMice-1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      each mouse simulation thread runs this function
 *
 *      You need to finish implementing this function using 
 *      the OS161 synchronization primitives as indicated
 *      in the assignment description
 *
 */

static
void
mouse_simulation(void * unusedpointer,
          unsigned long mousenumber)
{
  int i, j;
  unsigned int bowl;

  /* Avoid unused variable warnings. */
  (void) unusedpointer;
  (void) mousenumber;

  numMiceWaiting += 1;

  for(i=0;i<NumLoops;i++) {

    mouse_sleep();

	lock_acquire(mutex);
	while( (turn != 0 && turn != 2 && (numCatEating > 0 || numCatWaiting > 0 ||  numMiceEating == NumBowls)) ||
			( (turn == 0 || turn == 2) && numMiceEating == NumBowls ) )	{
		cv_wait(bowlEmpty, mutex);
	}
	turn = 0;
	for(j = 0; j<NumBowls; j+=1 ) {
		if (bowlInUse[j] == 0) {
			bowlInUse[j] = 1;
			bowl = j+1;
			numMiceEating += 1;
			break;
		}
	}
	lock_release(mutex);
    
    mouse_eat(bowl);

	lock_acquire(mutex);
	numMiceEating -= 1;
	bowlInUse[bowl-1] = 0;
	if (numMiceEating == 0) {
		turn = 1;
		cv_broadcast(bowlEmpty, mutex);
	}
	lock_release(mutex);
  }

  numMiceWaiting -= 1;
  /* indicate that this mouse is finished */
  V(CatMouseWait); 
}


/*
 * catmouse()
 *
 * Arguments:
 *      int nargs: should be 5
 *      char ** args: args[1] = number of food bowls
 *                    args[2] = number of cats
 *                    args[3] = number of mice
 *                    args[4] = number of loops
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up cat_simulation() and
 *      mouse_simulation() threads.
 *      You may need to modify this function, e.g., to
 *      initialize synchronization primitives used
 *      by the cat and mouse threads.
 *      
 *      However, you should should ensure that this function
 *      continues to create the appropriate numbers of
 *      cat and mouse threads, to initialize the simulation,
 *      and to wait for all cats and mice to finish.
 */

int
catmouse(int nargs,
	 char ** args)
{
  int index, error;
  int i;
  numMiceEating = 0;
  numCatEating = 0;
  numMiceWaiting = 0;
  numCatWaiting = 0;
  turn = 2;

  /* check and process command line arguments */
  if (nargs != 5) {
    kprintf("Usage: <command> NUM_BOWLS NUM_CATS NUM_MICE NUM_LOOPS\n");
    return 1;  // return failure indication
  }

  /* check the problem parameters, and set the global variables */
  NumBowls = atoi(args[1]);
  if (NumBowls <= 0) {
    kprintf("catmouse: invalid number of bowls: %d\n",NumBowls);
    return 1;
  }
  NumCats = atoi(args[2]);
  if (NumCats < 0) {
    kprintf("catmouse: invalid number of cats: %d\n",NumCats);
    return 1;
  }
  NumMice = atoi(args[3]);
  if (NumMice < 0) {
    kprintf("catmouse: invalid number of mice: %d\n",NumMice);
    return 1;
  }
  NumLoops = atoi(args[4]);
  if (NumLoops <= 0) {
    kprintf("catmouse: invalid number of loops: %d\n",NumLoops);
    return 1;
  }
  kprintf("Using %d bowls, %d cats, and %d mice. Looping %d times.\n",
          NumBowls,NumCats,NumMice,NumLoops);

  /* create the semaphore that is used to make the main thread
     wait for all of the cats and mice to finish */
  CatMouseWait = sem_create("CatMouseWait",0);
  if (CatMouseWait == NULL) {
    panic("catmouse: could not create semaphore\n");
  }

   bowlEmpty = cv_create("bowlEmpty");
   mutex = lock_create("mutex");
  /* 
   * initialize the bowls
   */
  if (initialize_bowls(NumBowls)) {
    panic("catmouse: error initializing bowls.\n");
  }
  bowlInUse = (int*)kmalloc(sizeof(int)*NumBowls);
  for(i = 0; i<NumBowls; i+=1 ) {
  	bowlInUse[i] = 0;
  }

  numMiceEating = 0;
  numCatEating = 0;
  /*
   * Start NumCats cat_simulation() threads.
   */
  for (index = 0; index < NumCats; index++) {
    error = thread_fork("cat_simulation thread",NULL,index,cat_simulation,NULL);
    if (error) {
      panic("cat_simulation: thread_fork failed: %s\n", strerror(error));
    }
  }

  /*
   * Start NumMice mouse_simulation() threads.
   */
  for (index = 0; index < NumMice; index++) {
    error = thread_fork("mouse_simulation thread",NULL,index,mouse_simulation,NULL);
    if (error) {
      panic("mouse_simulation: thread_fork failed: %s\n",strerror(error));
    }
  }

  /* wait for all of the cats and mice to finish before
     terminating */  
  for(i=0;i<(NumCats+NumMice);i++) {
    P(CatMouseWait);
  }

  /* clean up the semaphore the we created */
  sem_destroy(CatMouseWait);

  return 0;
}

/*
 * End of catmouse.c
 */
