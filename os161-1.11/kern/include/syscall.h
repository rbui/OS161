#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#include "opt-A2.h"
#include <types.h>
/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);

#if OPT_A2
int sys_write(int fd, const void * buf, size_t nbytes );

void sys__exit(int exitcode);
#endif


#endif /* _SYSCALL_H_ */
