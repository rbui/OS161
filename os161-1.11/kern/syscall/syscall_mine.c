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
	int refCount;
	int offset;
};

struct lock *threadLock =  NULL;

/*struct pidInfo {
	cv *isDone;
	int exitcode;
	int done;
	array *children;
};*/

int numOpen = 0;
struct lock *mutex =  NULL;
struct lock *forkLock = NULL;
struct lock *childLock = NULL;
struct lock *waitLock = NULL;
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

	if(fd<0 || fd>=array_getnum(vnodes) || array_getguy(vnodes, fd) == NULL ) {
		//kprintf("FILE OPEND, %d\n", fd);
		*err = EBADF;
		return -1;
	}
	
	struct fileHandle *handle = array_getguy(vnodes, fd);
	struct uio u;
	//kprintf("writing, %d\n", fd);
	void *kbuf = kmalloc(nbytes + 1);
	int ret = copyin((const_userptr_t)buf, kbuf, nbytes + 1);
	mk_kuio(&u, kbuf, nbytes, handle->offset, UIO_WRITE);
	
	if (ret) {
		*err = ret;
		return -1;
	}

	lock_acquire(mutex);
	int result = VOP_WRITE(handle->file, &u);
	lock_release(mutex);

	kfree(kbuf);
	if (result) {
		*err = result;
		return -1;
	}

	handle->offset = u.uio_offset;
	return nbytes - u.uio_resid;
}

int sys_read(int fd, void *buf, size_t nbytes, int *err) {

	setup();

	if(fd<0 || fd>=array_getnum(vnodes) || array_getguy(vnodes, fd) == NULL ) {
		*err = EBADF;
		return -1;
	}
	
	struct fileHandle *handle = array_getguy(vnodes, fd);
	struct uio u;
	void *kbuf = kmalloc(nbytes);
	mk_kuio(&u, kbuf, nbytes, handle->offset, UIO_READ);
	
	lock_acquire(mutex);
	int result = VOP_READ(handle->file, &u);
	lock_release(mutex);

	if (result) {
		kfree(kbuf);
		return -1;
	}
	handle->offset = u.uio_offset;
	result = copyout(kbuf, buf, nbytes - u.uio_resid + 1);
	kfree(kbuf);
	if (result) {
		*err = result;
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
		struct fileHandle *handle = kmalloc(sizeof( struct fileHandle) );
		char *fname= NULL;
		int *index = kmalloc(sizeof(int));
		fname = kstrdup(filename);

		lock_acquire(mutex);
		int result = vfs_open(fname, flags, &(handle->file));
		lock_release(mutex);
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
		handle->refCount = 1;
		if (result) {
			kfree(handle);
			array_setguy(vnodes, *index, NULL);
			array_add(freeArray, index);
			*err = result;
			return -1;
		}
		
		numOpen += 1;
		int ind = *index;
		kfree(index);
		return ind;
	}
	else {
		*err = EFAULT;
		return -1;
	}
}

int sys_close(int fd, int *err) {
	setup();

	if(fd<3 || fd>=array_getnum(vnodes) || array_getguy(vnodes, fd) == NULL ) {
		*err = EBADF;
		return -1;
	}

	//attempt to  close
	struct fileHandle *handle = array_getguy(vnodes, fd);
	handle->refCount -= 1;
	numOpen -=1;
	if(handle->refCount > 0) {
		return 0;
	}
	int * index = kmalloc(sizeof(int*));
	*index = fd;
	lock_acquire(mutex);
	vfs_close(handle->file);
	lock_release(mutex);
	kfree(handle);
	array_setguy(vnodes, fd, NULL);
	array_add(freeArray, index);
	
	return 0;
}

void sys__exit(int exitcode) {
	int i = 0;
	
	//killchildren
	if(threadLock == NULL) {
		threadLock = lock_create("threadLock");
	}
	
	if (vnodes != NULL) {
		for(i = 0; i<array_getnum(vnodes); i+=1) {
			struct fileHandle *file = array_getguy(vnodes, i);
			if (file != NULL) {
				file->refCount -= 1;
				if (file->refCount <= 0) {
					vfs_close(file->file);
				}
			}
		}
		array_destroy(vnodes);
	}

	if(freeArray != NULL) {
		for(i = 0; i<array_getnum(freeArray); i+=1) {
			int *index  = array_getguy(freeArray, i);
			kfree(index);
		}
		array_destroy(freeArray);
	}
	
	if (waitLock == NULL) {
		waitLock = lock_create("lock");
	}
	
	lock_acquire(pidLock);
	struct pidInfo *curPid = array_getguy(pids, curthread->pid);
	curPid->done = 1;
	curPid->exitcode = exitcode;
	lock_release(pidLock);
	
	lock_acquire(threadLock);
	cv_broadcast(curPid->isDone, threadLock);
	lock_release(threadLock);


	lock_acquire(pidLock);
	struct array *childArray = curPid->children;
	for(i = 0; i<array_getnum(childArray); i += 1) {
		pid_t *childPid = array_getguy(childArray, i);
		struct pidInfo *childInfo = array_getguy(pids, *childPid);
		if(childInfo->deleteable) {	
			pid_t *tmpPid = kmalloc(sizeof(pid_t));
			array_setguy(pids, *childPid, NULL);
			kfree(childInfo);
			*tmpPid = *childPid;
			array_add(freePids, tmpPid);
			kfree(childPid);
		}
		else {
			childInfo->deleteable = 1;
		}
	}
	array_destroy(childArray);
	childArray = NULL;	

	if (curPid->deleteable) {
		pid_t *tmpPid = kmalloc(sizeof(pid_t));
		struct pidInfo *curPidInfo = array_getguy(pids, curthread->pid);
		//kfree(curPidInfo);
		array_setguy(pids, curthread->pid, NULL);
		*tmpPid = curthread->pid;
		array_add(freePids, tmpPid);
	}
	else {
		curPid->deleteable = 1;
	}
	cv_destroy(curPid->isDone);
	lock_release(pidLock);

	thread_exit();
}

void newThread(void *data, unsigned long numData) {
	int s = splhigh();
	int *pointers = data;
	struct trapframe *tf = pointers[0];
	struct array *fileHandles = pointers[1];
	struct semaphore *sem = pointers[2];
	struct addrspace *addr = pointers[3];
	struct array *freeFiles = pointers[4];

	as_copy(addr, &(curthread->t_vmspace));


	int i =0;
	if (fileHandles != NULL) {
		struct array * files=array_create();
		for(i = 0; i <array_getnum(fileHandles); i += 1) {
			struct fileHandle *handle = array_getguy(fileHandles, i);
			handle->refCount += 1;
			array_add(files, handle);
		}
		curthread->fileHandles = files;
	}
	
	if (freeFiles != NULL) {
		freeArray = array_create();
		for(i = 0; i < array_getnum(freeFiles); i +=1 ) {
			int *index = array_getguy(freeFiles, i);
			int *a = kmalloc(sizeof(int));
			*a = *index;
			array_add(freeArray, a);
		}
	}

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
		*err = EAGAIN;
		return -1;
	}
	
	struct semaphore *sem = sem_create("sem", 0);
	int *data = kmalloc(sizeof(int*) *5);

	data[0] = tf;
	data[1] = curthread->fileHandles;
	data[2] = sem;
	data[3] = curthread->t_vmspace;
	data[4] = freeArray;

	struct thread *child;

	int result = thread_fork("child", data, 3, newThread, &child); 

	if (result) {
		*err = result;
		return -1;
	}

	P(sem);
	pid_t returnPid = child->pid;
	kfree(data);

	if (waitLock == NULL) {
		waitLock = lock_create("fuckit");
	}

	lock_acquire(pidLock);
	pid_t *toAdd  = kmalloc(sizeof(pid_t));
	struct pidInfo *curPid = array_getguy(pids, curthread->pid);
	*toAdd = returnPid;	
	array_add(curPid->children, toAdd);
	lock_release(pidLock);

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
		*err = EINVAL;
		return -1;
	}

	if (status == NULL) {
		*err = EFAULT;
		return -1;
	}
	int one = 1;
	int tmp = copyin(status, &one, sizeof(int));
	if (tmp) {
		*err = tmp;
		return -1;
	}

	if (pid >=0 && pid < array_getnum(pids) && array_getguy(pids, pid) != NULL) {
		lock_acquire(pidLock);
		struct pidInfo *curPid = array_getguy(pids, pid);
		struct pidInfo *myPidInfo = array_getguy(pids, curthread->pid);
		struct array *childArray = myPidInfo->children;
		int i, found = 0;
		for(i = 0; i<array_getnum(childArray); i += 1) {
			pid_t *childPid = array_getguy(childArray, i);
			if (*childPid == pid) {
				found = 1;
				break;
			}
		}
		lock_release(pidLock);
		if (found == 0) {
			*status = 0;
			return 0;
		}

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
	*err = EINVAL;	
	return -1;
}

int sys_execv(const char *program, char **args, int *err) {
	int nargs = 0, i = 0;
	int result = 0, len;
	
	if (program == NULL) {
		*err = EFAULT;
		return -1;
	}
	
	char path[1024];
	result = copyinstr(program, path, 1024, &len);
	if(result) {
		*err = EFAULT;
		return -1;
	}
	if (strlen(path)==0) {
		*err = EINVAL;
		return -1;
	}

	if (args == NULL) {
		*err = EFAULT;
		return -1;
	}
	

	int test;
	result = copyin(args, &test, sizeof(int));
	if (result) {
		*err = EFAULT;
		return -1;
	}

	while(args[nargs] != NULL) {
		result = copyin(args + nargs*4, &test, sizeof(int));
		if (result) {
			*err = EFAULT;
			return -1;
		}

		nargs += 1;
	}
	char **run_args = kmalloc(sizeof(char)*nargs);
	for(i =0; i<nargs; i+=1) {
		run_args[i] = kmalloc(sizeof(char)*1024);
		result = copyinstr(args[i], run_args[i], 1024, &len);
		if(result) {
			*err = EFAULT;
			return -1;
		}
	}
	if (result) {
		*err = EFAULT;
		return -1;
	}
	
	as_destroy(curthread->t_vmspace);
	curthread->t_vmspace = NULL;
	

	result = runprogram(path, run_args, nargs);
	*err = result;
	return -1;
}

