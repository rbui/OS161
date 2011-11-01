#include <syscall.h>
#include <synch.h>

struct lock *mutex;

int sys_write(int fd, const void *buf, size_t nbytes) {

	if (mutex==NULL) {
		mutex = lock_create("mylock");
	}

	lock_acquire(mutex);
	char *str = (char*) buf;
	size_t i = 0;
	for(i = 0; i<nbytes; i+=1) {
		putch(str[i]);
	}
	lock_release(mutex);
}

void sys__exit(int exitcode) {
	thread_exit();
}
