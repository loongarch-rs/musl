#include "pthread_impl.h"
#include "stdio_impl.h"

static void dummy_0()
{
}
weak_alias(dummy_0, __synccall_lock);
weak_alias(dummy_0, __synccall_unlock);
weak_alias(dummy_0, __pthread_tsd_run_dtors);

#ifdef __pthread_unwind_next
#undef __pthread_unwind_next
#define __pthread_unwind_next __pthread_unwind_next_3
#endif

void __pthread_unwind_next(struct __ptcb *cb)
{
	pthread_t self = pthread_self();
	int n;

	if (cb->__next) {
		self->cancelbuf = cb->__next->__next;
		longjmp((void *)cb->__next->__jb, 1);
	}

	__pthread_tsd_run_dtors();

	__lock(&self->exitlock);

	/* Mark this thread dead before decrementing count */
	__lock(&self->killlock);
	self->dead = 1;
	a_store(&self->killlock, 0);

	do n = libc.threads_minus_1;
	while (n && a_cas(&libc.threads_minus_1, n, n-1)!=n);
	if (!n) exit(0);

	if (self->detached && self->map_base) {
		__syscall(SYS_rt_sigprocmask, SIG_BLOCK, (uint64_t[]){-1},0,8);
		__unmapself(self->map_base, self->map_size);
	}

	__syscall(SYS_exit, 0);
}

static int start(void *p)
{
	struct pthread *self = p;
	if (self->unblock_cancel)
		__syscall(SYS_rt_sigprocmask, SIG_UNBLOCK, SIGPT_SET, 0, 8);
	pthread_exit(self->start(self->start_arg));
	return 0;
}

int __uniclone(void *, int (*)(), void *);

#define ROUND(x) (((x)+PAGE_SIZE-1)&-PAGE_SIZE)

/* pthread_key_create.c overrides this */
static const size_t dummy = 0;
weak_alias(dummy, __pthread_tsd_size);

static FILE *const dummy_file = 0;
weak_alias(dummy_file, __stdin_used);
weak_alias(dummy_file, __stdout_used);
weak_alias(dummy_file, __stderr_used);

static void init_file_lock(FILE *f)
{
	if (f && f->lock<0) f->lock = 0;
}

int pthread_create(pthread_t *res, const pthread_attr_t *attr, void *(*entry)(void *), void *arg)
{
	int ret;
	size_t size = DEFAULT_STACK_SIZE + DEFAULT_GUARD_SIZE;
	size_t guard = DEFAULT_GUARD_SIZE;
	struct pthread *self = pthread_self(), *new;
	unsigned char *map, *stack, *tsd;

	if (!self) return ENOSYS;
	if (!libc.threaded) {
		for (FILE *f=libc.ofl_head; f; f=f->next)
			init_file_lock(f);
		init_file_lock(__stdin_used);
		init_file_lock(__stdout_used);
		init_file_lock(__stderr_used);
		__syscall(SYS_rt_sigprocmask, SIG_UNBLOCK, SIGPT_SET, 0, 8);
		libc.threaded = 1;
	}

	if (attr) {
		guard = ROUND(attr->_a_guardsize + DEFAULT_GUARD_SIZE);
		size = guard + ROUND(attr->_a_stacksize + DEFAULT_STACK_SIZE);
	}
	size += __pthread_tsd_size;
	map = mmap(0, size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANON, -1, 0);
	if (!map) return EAGAIN;
	if (guard) mprotect(map, guard, PROT_NONE);

	tsd = map + size - __pthread_tsd_size;
	new = (void *)(tsd - sizeof *new - PAGE_SIZE%sizeof *new);
	new->map_base = map;
	new->map_size = size;
	new->pid = self->pid;
	new->errno_ptr = &new->errno_val;
	new->start = entry;
	new->start_arg = arg;
	new->self = new;
	new->tsd = (void *)tsd;
	if (attr) new->detached = attr->_a_detach;
	new->unblock_cancel = self->cancel;
	memcpy(new->tlsdesc, self->tlsdesc, sizeof new->tlsdesc);
	new->tlsdesc[1] = (uintptr_t)new;
	stack = (void *)((uintptr_t)new-1 & ~(uintptr_t)15);

	__synccall_lock();

	a_inc(&libc.threads_minus_1);
	ret = __uniclone(stack, start, new);

	__synccall_unlock();

	if (ret < 0) {
		a_dec(&libc.threads_minus_1);
		munmap(map, size);
		return EAGAIN;
	}
	*res = new;
	return 0;
}

void pthread_exit(void *result)
{
	struct pthread *self = pthread_self();
	struct __ptcb cb = { .__next = self->cancelbuf };
	self->result = result;
	__pthread_unwind_next(&cb);
}
