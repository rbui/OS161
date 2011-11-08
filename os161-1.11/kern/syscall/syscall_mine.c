#include <syscall.h>
#include <synch.h>
#include <array.h>
#include <kern/unistd.h>
#include <uio.h>
#include <thread.h>
#include <curthread.h>
#include <vnode.h>
#include <types.h>
#include <kern/errno.h>
#include <machine/trapframe.h>

struct fileHandle {
	struct vnode *file;
	int offset;
};

struct lock *threadLock =  NULL;

/*struct pidInfo {
	cv *isDone;
	int exitcode;
	int done;
	array *children;
};*/


struct lock *mutex =  NULL;
struct lock *forkLock = NULL;
struct lock *childLock = NULL;
//struct array *vnodes = NULL, *freeArray = NULL;
#define vnodes curthread->fileHandles
#define freeArray curthread->freeArray

void setup() {
	//vnodes = curthread->fileHandles;
	///freeArray = curthread->freeArray;
	if (mutex==NULL) {
		mutex = lock_create("mylock");
	}
	
	if(vnodes == NULL) {
		vnodes = array_create();
		//curthread->fileHandles = vnodes;
		
	}
	
	if(freeArray == NULL) {
		freeArray = array_create();	
		//curthread->freeArray = freeArray;
		int err;
		sys_open("con:", O_RDONLY, &err);
		sys_open("con:", O_WRONLY, &err);
		sys_open("con:", O_WRONLY, &err);
	}
}

int sys_write(int fd, const void *buf, size_t nbytes, int *err) {

	setup();

	if(fd>=array_getnum(vnodes) || array_getguy(vnodes, fd) == NULL ) {
		//kprintf("FILE OPEND, %d\n", fd);
		*err = EBADF;
		return -1;
	}
	
	struct fileHandle *handle = array_getguy(vnodes, fd);
	struct uio u;
	//kprintf("writing, %d\n", fd);
	void *kbuf = kmalloc(nbytes);
	copyin((const_userptr_t)buf, kbuf, nbytes);
	mk_kuio(&u, kbuf, nbytes, handle->offset, UIO_WRITE);
	
	lock_acquire(mutex);
	int result = VOP_WRITE(handle->file, &u);
	lock_release(mutex);

	kfree(kbuf);
	if (result) {
		*err = EIO;
		return -1;
	}

	handle->offset = u.uio_offset;
	return nbytes - u.uio_resid;
}

int sys_read(int fd, void *buf, size_t nbytes, int *err) {

	setup();

	if(fd>=array_getnum(vnodes) || array_getguy(vnodes, fd) == NULL ) {
		//kprintf("FILE OPEND, %d\n", fd);
		return -1;
	}
	
	struct fileHandle *handle = array_getguy(vnodes, fd);
	struct uio u;
	//kprintf("reading, %d\n", fd);
	void *kbuf = kmalloc(nbytes);
	//copyin((const_userptr_t)buf, kbuf, nbytes);
	mk_kuio(&u, kbuf, nbytes, handle->offset, UIO_READ);
	
	lock_acquire(mutex);
	int result = VOP_READ(handle->file, &u);
	lock_release(mutex);

	//kprintf("%s %d %d\n", (char*)kbuf, nbytes, u.uio_resid);
	if (result) {
		//kprintf("reading fail, %d\n", fd);
		kfree(kbuf);
		lock_release(mutex);
		return -1;
	}
	handle->offset = u.uio_offset;
	result = copyout(kbuf, buf, nbytes - u.uio_resid);
	kfree(kbuf);
	if (result) {
		//kprintf("reading fail, %d\n", fd);../../arch/mips/mips/syscall.c:152: error: invalid type argument of \ufffd\ufffd\ufffd->\ufffd\ufffd\ufffd

		return -1;
	}
	return nbytes - u.uio_resid;
}

int sys_open(const char * filename, int flags, int *err) {

	//create mutex

	setup();
	if (filename != NULL) {

		//create files array and freeArray if they aren't already
		if(vnodes == NULL) {
			vnodes = array_create();	
		}
		
		if(freeArray == NULL) {
			freeArray = array_create();	
		}
		
		//error check
		/*if (flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR) {
				return -1;
		}*/
	
		//attempt to open
		lock_acquire(mutex);
		struct fileHandle *handle = kmalloc(sizeof( struct fileHandle) );
		char *fname= NULL;
		int *index = kmalloc(sizeof(int));
		fname = kstrdup(filename);

		int result = vfs_open(fname, flags, &(handle->file));
		//if there is a free node
		if (array_getnum(freeArray) > 0) {
			int *tmp = (int*)array_getguy(freeArray, 0);
			*index = *tmp;
			kfree(tmp);
			array_setguy(vnodes, *index, handle);
			array_remove(freeArray, 0);
		}
		else {
			array_add(vnodes, handle);
			*index = array_getnum(vnodes) -1;
		}

		kfree(fname);
		handle->offset = 0;
		if (result) {
			kfree(handle);
			array_setguy(vnodes, *index, NULL);
			array_add(freeArray, index);
			lock_release(mutex);
			*err = result;
			//kprintf("STUFFING  failed%d\n", *index);
			return -1;
		}

		lock_release(mutex);
		int ind = *index;
		//kprintf("STUFFING %d\n", *index);
		kfree(index);
		return ind;
	}
	else {
		return -1;
	}
}

int sys_close(int fd, int *err) {
	setup();

	if(fd>=array_getnum(vnodes) || array_getguy(vnodes, fd) == NULL ) {
		return -1;
	}

	//attempt to  close
	lock_acquire(mutex);
	struct fileHandle *handle = array_getguy(vnodes, fd);
	int * index = kmalloc(sizeof(int*));
	*index = fd;
	vfs_close(handle->file);
	kfree(handle);
	array_setguy(vnodes, fd, NULL);
	array_add(freeArray, index);
	lock_release(mutex);

	return 0;
}

void sys__exit(int exitcode) {
	
	//killchildren
	if(threadLock == NULL) {
		threadLock = lock_create("threadLock");
	}
	lock_acquire(threadLock);
	struct pidInfo *curPid = array_getguy(pids, curthread->pid);
	//array_setguy(pids, curthread->pid, NULL);
	curPid->done = 1;
	curPid->exitcode = exitcode;
	cv_broadcast(curPid->isDone, threadLock);
	lock_release(threadLock);
	thread_exit();
}

void newThread(void *data, unsigned long numData) {
	int s = splhigh();
	int *pointers = data;
	struct trapframe *tf = pointers[0];
	struct array *fileHandles = pointers[1];
	struct semaphore *sem = pointers[2];
	struct addrspace *addr = pointers[3];
	as_copy(addr, &(curthread->t_vmspace));

	struct array * files=array_create();

	int i =0;
	for(i = 0; i <array_getnum(files); i += 1) {
		array_add(files, array_getguy(fileHandles, i));
	}
	curthread->fileHandles = files;

	struct trapframe trap;
	trap = *tf;
	trap.tf_v0 = 0;
	trap.tf_a3 = 0;
	trap.tf_epc += 4;
	
	V(sem);

	splx(s);
	md_forkentry(&trap);
}

int sys_fork(void *tf, int *err) {

	if (forkLock == NULL) {
		forkLock = lock_create("fork");
	}
	
	if (procs >= MAX_PROC) {
		return -1;
	}
	
	lock_acquire(forkLock);
	struct semaphore *sem = sem_create("sem", 0);
	int *data = kmalloc(sizeof(int*) *4);

	data[0] = tf;
	data[1] = curthread->fileHandles;
	data[2] = sem;
	data[3] = curthread->t_vmspace;

	struct thread *child;

	thread_fork("child", data, 3, newThread, &child); 
	P(sem);
	pid_t returnPid = child->pid;
	kfree(data);
	lock_release(forkLock);

	sem_destroy(sem);
	return returnPid;
}

int sys_getpid() {
	return curthread->pid;
}

pid_t sys_waitpid(pid_t pid, int *status, int options, int *err) {
	
	if(threadLock == NULL) {
		threadLock = lock_create("threadLock");
	}

	if (options != 0) {
		return -1;
	}

	if (pid >=0 && pid < array_getnum(pids) && array_getguy(pids, pid) != NULL) {
		struct pidInfo *curPid = array_getguy(pids, pid);
		if (curPid->done) {
			*status = curPid->exitcode;
		}
		else {
			lock_acquire(threadLock);
			cv_wait(curPid->isDone, threadLock);
			*status = curPid->exitcode;
			lock_release(threadLock);
		}
		return pid;
	}
	
	return -1;
}

int sys_execv(const char *program, char **args, int *err) {
	int s = splhigh();
	int nargs = 0, i = 0;
	while(args[nargs] != NULL) {
		nargs += 1;
	}
	char **run_args = kmalloc(sizeof(char)*nargs);
	for(i =0; i<nargs; i+=1) {
		run_args[i] = kstrdup(args[i]);
	}
	/*for(i =0; i<nargs; i+=1) {
		kprintf("%s\n", run_args[i]);
	}*/

	char *path = kstrdup(program), *prog = kstrdup(args[0]);
	
	//thread_exit();
	as_destroy(curthread->t_vmspace);
	curthread->t_vmspace = NULL;

	char *exec = kmalloc(sizeof(char)*(strlen("../os161-1.11") + strlen(path) + strlen(prog) +strlen("/") + 4));
	strcpy(exec, "../os161-1.11"); 
	strcat(exec, path);
	strcat(exec, "/"); 
	strcat(exec, prog); 
	//kprintf("%s\n", exec);
	splx(s);
	int result = runprogram(path, run_args, nargs);
	*err = result;
	return -1;
}

