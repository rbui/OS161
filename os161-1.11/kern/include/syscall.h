#ifndef _SYSCALL_H_
#define _SYSCALL_H_
<<<<<<< HEAD
#include "opt-A2.h"
#include <types.h>
=======

>>>>>>> fed23e5f1c87e30850e46c38ef4eb0b6c6737c5c
/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);

<<<<<<< HEAD
#if OPT_A2
int sys_write(int fd, const void * buf, size_t nbytes, int *err);

int sys_read(int fd, void * buf, size_t nbytes, int *err);

void sys__exit(int exitcode);

int sys_open(const char* filename, int flags, int *err);

pid_t fork(void *tf, int*err);

pid_t waitpid(pid_t pid, int *status, int options, int *err);

pid_t sys_getpid();

int sys_execv(const char *program, char **args, int *err);
#endif

=======
>>>>>>> fed23e5f1c87e30850e46c38ef4eb0b6c6737c5c

#endif /* _SYSCALL_H_ */
