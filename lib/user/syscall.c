#include <syscall.h>
#include <stdint.h>
#include "../syscall-nr.h"
#include "filesys/filesys.h"

__attribute__((always_inline)) static __inline int64_t syscall(uint64_t num_, uint64_t a1_, uint64_t a2_,
															   uint64_t a3_, uint64_t a4_, uint64_t a5_, uint64_t a6_)
{
	int64_t ret;
	register uint64_t *num asm("rax") = (uint64_t *)num_;
	register uint64_t *a1 asm("rdi") = (uint64_t *)a1_;
	register uint64_t *a2 asm("rsi") = (uint64_t *)a2_;
	register uint64_t *a3 asm("rdx") = (uint64_t *)a3_;
	register uint64_t *a4 asm("r10") = (uint64_t *)a4_;
	register uint64_t *a5 asm("r8") = (uint64_t *)a5_;
	register uint64_t *a6 asm("r9") = (uint64_t *)a6_;

	__asm __volatile(
		"mov %1, %%rax\n"
		"mov %2, %%rdi\n"
		"mov %3, %%rsi\n"
		"mov %4, %%rdx\n"
		"mov %5, %%r10\n"
		"mov %6, %%r8\n"
		"mov %7, %%r9\n"
		"syscall\n"
		: "=a"(ret)
		: "g"(num), "g"(a1), "g"(a2), "g"(a3), "g"(a4), "g"(a5), "g"(a6)
		: "cc", "memory");
	return ret;
}

/* Invokes syscall NUMBER, passing no arguments, and returns the
   return value as an `int'. */
#define syscall0(NUMBER) ( \
	syscall(((uint64_t)NUMBER), 0, 0, 0, 0, 0, 0))

/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0) ( \
	syscall(((uint64_t)NUMBER),  \
			((uint64_t)ARG0), 0, 0, 0, 0, 0))
/* Invokes syscall NUMBER, passing arguments ARG0 and ARG1, and
   returns the return value as an `int'. */
#define syscall2(NUMBER, ARG0, ARG1) ( \
	syscall(((uint64_t)NUMBER),        \
			((uint64_t)ARG0),          \
			((uint64_t)ARG1),          \
			0, 0, 0, 0))

#define syscall3(NUMBER, ARG0, ARG1, ARG2) ( \
	syscall(((uint64_t)NUMBER),              \
			((uint64_t)ARG0),                \
			((uint64_t)ARG1),                \
			((uint64_t)ARG2), 0, 0, 0))

#define syscall4(NUMBER, ARG0, ARG1, ARG2, ARG3) ( \
	syscall(((uint64_t *)NUMBER),                  \
			((uint64_t)ARG0),                      \
			((uint64_t)ARG1),                      \
			((uint64_t)ARG2),                      \
			((uint64_t)ARG3), 0, 0))

#define syscall5(NUMBER, ARG0, ARG1, ARG2, ARG3, ARG4) ( \
	syscall(((uint64_t)NUMBER),                          \
			((uint64_t)ARG0),                            \
			((uint64_t)ARG1),                            \
			((uint64_t)ARG2),                            \
			((uint64_t)ARG3),                            \
			((uint64_t)ARG4),                            \
			0))
void halt(void)
{ // power_off()를 호출해 핀토스를 종료함.
	syscall0(SYS_HALT);
	// power_off();
	NOT_REACHED();
}

void exit(int status)
{ // 현재 동작중인 유저 프로그램을 종료함. 커널에 상태를 리턴하면서 종료. 0은 성공, 나머지는 fail.
	syscall1(SYS_EXIT, status);
	NOT_REACHED();
}

pid_t fork(const char *thread_name)
{ // thread_name이라는 이름을 가진 현재 프로세스의 복제본인 새 프로세스를 만듬. 자식 프로세스의 pid를 반환해야함. 자식프로세스의 반환값은 0.
	return (pid_t)syscall1(SYS_FORK, thread_name);
}

int exec(const char *file)
{ // 현재의 프로세스가 cmd_line에서 이름이 주어지는 실행가능한 프로세스로 변경됨. 이때 주어진 인자를 전달함. 성공하면 리턴하지 않음.
	return (pid_t)syscall1(SYS_EXEC, file);
}

int wait(pid_t pid)
{ /* 자식 프로세스 (pid) 를 기다려서 자식의 종료 상태(exit status)를 가져옴. 자식이 살아있다면, 종료될 때까지 기다림.
	 종료가 되면 그 프로세스가 exit 함수로 전달해준 상태(exit status)를 반환함.*/
	return syscall1(SYS_WAIT, pid);
}

bool create(const char *file, unsigned initial_size)
{
	return syscall2(SYS_CREATE, file, initial_size);
}

bool remove(const char *file)
{
	return syscall1(SYS_REMOVE, file);
}

int open(const char *file)
{ // 첫번째 인자인 이름을 가진 파일을 연다. 성공적으로 열렸다면, 0또는 양수인 fd를 반환, 실패했다면 -1을 반환.
	return syscall1(SYS_OPEN, file);
}

int filesize(int fd)
{
	return syscall1(SYS_FILESIZE, fd);
}

int read(int fd, void *buffer, unsigned size)
{
	return syscall3(SYS_READ, fd, buffer, size);
}

int write(int fd, const void *buffer, unsigned size)
{ // buffer에 있는 size만큼의 바이트를 fd에 작성한다. 실제로 작성된 바이트의 수를 반환한다.
	return syscall3(SYS_WRITE, fd, buffer, size);
}

void seek(int fd, unsigned position)
{
	syscall2(SYS_SEEK, fd, position);
}

unsigned
tell(int fd)
{
	return syscall1(SYS_TELL, fd);
}

void close(int fd)
{
	syscall1(SYS_CLOSE, fd);
}

int dup2(int oldfd, int newfd)
{
	return syscall2(SYS_DUP2, oldfd, newfd);
}

void *
mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
	return (void *)syscall5(SYS_MMAP, addr, length, writable, fd, offset);
}

void munmap(void *addr)
{
	syscall1(SYS_MUNMAP, addr);
}

bool chdir(const char *dir)
{
	return syscall1(SYS_CHDIR, dir);
}

bool mkdir(const char *dir)
{
	return syscall1(SYS_MKDIR, dir);
}

bool readdir(int fd, char name[READDIR_MAX_LEN + 1])
{
	return syscall2(SYS_READDIR, fd, name);
}

bool isdir(int fd)
{
	return syscall1(SYS_ISDIR, fd);
}

int inumber(int fd)
{
	return syscall1(SYS_INUMBER, fd);
}

int symlink(const char *target, const char *linkpath)
{
	return syscall2(SYS_SYMLINK, target, linkpath);
}

int mount(const char *path, int chan_no, int dev_no)
{
	return syscall3(SYS_MOUNT, path, chan_no, dev_no);
}

int umount(const char *path)
{
	return syscall1(SYS_UMOUNT, path);
}
