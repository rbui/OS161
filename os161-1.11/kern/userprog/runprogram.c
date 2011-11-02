/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>
#include "opt-A2.h"

#if OP_A2
#include <types.h>
#endif

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
#if OPT_A2
int runprogram(char* progname, char **args, unsigned long nargs)
#else
int
runprogram(char *progname)
#endif
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);
	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		return result;
	}
	
	#if OPT_A2
	stackptr -= sizeof(char*) * (nargs + 1);
	
	userptr_t *usr_argv = (userptr_t*)stackptr;
	unsigned int argIndex = 0;
	copyout(args, usr_argv, sizeof(char*)*nargs + 1);

	for(argIndex = 0; argIndex < nargs; argIndex += 1) {
		stackptr -= sizeof(char) * (strlen(args[argIndex]) + 1);
		//kprintf("Stack is :%x\n", stackptr);
		usr_argv[argIndex] = stackptr;
		/copyout(args[argIndex], usr_argv[argIndex], sizeof(char) * (strlen(args[argIndex]) + 1) );

	}
	usr_argv[nargs] = 0;
	stackptr -= stackptr%8;
	//stackptr -= stackptr%sizeof(char*);

	//char *nul = NULL;
	//copyout(args[argIndex], usr_arg, sizeof(char) * (strlen(args[argIndex]) + 1) );
	//copyin(usr_arg, &addr, sizeof(char*) );
	//int t =0;
	//copyout(t, usr_argv - nargs*sizeof(char*), sizeof(char*) );
	//kprintf("DONE\n");
	//stackptr = stackptr - stackptr%8;
	//usr_argv[nargs] = 0;
	#endif
	#if OPT_A2
	//md_usermode(nargs, usr_argv,
	md_usermode(nargs /*argc*/, usr_argv/*userspace addr of argv*/,
		    stackptr, entrypoint);
	#else
	/* Warp to user mode. */
	md_usermode(0 /*argc*/, NULL /*userspace addr of argv*/,
		    stackptr, entrypoint);
	#endif
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

