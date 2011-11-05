#include <syscall.h>
#include <synch.h>
#include <array.h>
#include <kern/unistd.h>
#include <uio.h>
#include <thread.h>
#include <curthread.h>
#include <vnode.h>
#include <types.h>

struct fileHandle {
	struct vnode *file;
	int offset;
};


struct lock *mutex =  NULL;
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
		//kprintf("reading fail, %d\n", fd);
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
	thread_exit();
}
