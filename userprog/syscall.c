#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/init.h"
#include "devices/input.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

void
halt (void) {
	power_off();
}

void
exit (int status) {
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf ("%s: exit(%d)\n", curr->name, curr->exit_status);
	thread_exit();
}

// pid_t
// fork (const char *thread_name){
// 	process_fork();
// }

void check_address(const uint64_t *addr) {
	struct thread *curr = thread_current();
	if (addr == NULL || !(is_user_vaddr(addr)) || pml4_get_page(curr->pml4, addr) == NULL)
		exit(-1);
}

bool
create (const char *file, unsigned initial_size) {
	check_address(file);
	// 새로운 파일 생성
	return filesys_create(file, initial_size);
}

int
open (const char *file) {
	check_address(file);
	struct thread *curr = thread_current();
	struct file *f;

	if ((f = filesys_open(file))) {
		// fd 생성
		curr->max_fd++;

		// 스레드 구조체 속 파일 배열에 push
		*(curr->fd_table + curr->max_fd) = f;
		
		// fd 반환
		return curr->max_fd;
	}
	return -1;
}

int
filesize (int fd) {
	struct thread *curr = thread_current();
	// file 찾기
	struct file *file = *(curr->fd_table + fd);
	if (file == NULL)
		return -1;
	return file_length(file);
}

void check_fd(const int fd) {
	struct thread *curr = thread_current();
	if (fd < 0 || fd > curr->max_fd)
		exit(-1);
}

int
read (int fd, void *buffer, unsigned size) {
	check_fd(fd);
	check_address(buffer);
	
	struct thread *curr = thread_current();
	int bytes = 0;

	if (fd == 0) {
		for (unsigned i = 0; i< size; i++)
			*((char *)buffer + i) = input_getc();
		bytes = size;
	}
	else if (fd >=3) {
		// file 찾기
		struct file *file = *(curr->fd_table + fd);
		if (file == NULL)
			return -1;
		bytes = file_read(file, buffer, size);
	}
	
	return bytes;
}

void
close (int fd) {
	check_fd(fd);
	struct thread *curr = thread_current();
	// file 찾기
	struct file *file = *(curr->fd_table + fd);
	if (file == NULL)
		exit(-1);
	file_close(file);
	if (fd == curr->max_fd)
		curr->max_fd--;
	*(curr->fd_table + fd) = NULL;
}

int
write (int fd, const void *buffer, unsigned size) {
	check_fd(fd);
	check_address(buffer);
	// fd 활용하여 file 찾기
	if (fd == 1) {
		//콘솔에 작성
		putbuf(buffer, size);
		return size;
	}
	else if (fd >= 3) {
		struct thread *curr = thread_current();
		struct file *file = *(curr->fd_table + fd);

		if (file == NULL)
			return -1;

		// buffer에서 fd 파일로 size 바이트만큼 쓰기
		return file_write(file, buffer, size);
	}
	return 0;
	
	// 만약 몇 바이트가 안 읽혔다면 size보다 작을 실제로 쓴 바이트 수 반환
	// 파일 끝 부분까지 쓰고 더 이상 쓸 수 없는 경우 0 반환
	// 콘솔에 쓰는 코드는 수백 바이트보다 크지 않은 한 putbuf() 호출 한 번으로 다 쓰기
	// (큰 버퍼는 분할)
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// 시스템 콜 번호와 인수를 찾고 적절한 작업 수행 필요
	// %rax 는 시스템 콜 번호
	// 인수는 %rdi , %rsi, %rdx, %r10, %r8, %r9 순서로 넘겨짐
	// intr_frame 통해 레지스터 상태 접근
	// %rax 에 함수 리턴값 배치
	 
	switch (f->R.rax)
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		// case SYS_FORK:
		// 	fork(f->R.rdi);
		// 	break;
		// case SYS_EXEC:
		// 	exec();
		// 	break;
		// case SYS_WAIT:
		// 	wait();
		// 	break;
		case SYS_CREATE:
			f->R.rax = create((char *)f->R.rdi, f->R.rsi);
			break;
		// case SYS_REMOVE:
		// 	create(f->R.rdi, f->R.rsi);
		// 	break;
		case SYS_OPEN:
			f->R.rax = open((char *)f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read(f->R.rdi, (void *)f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, (void *)f->R.rsi, f->R.rdx);
			break;
		// case SYS_SEEK:
		// 	open(f->R.rdi);
		// 	break;
		// case SYS_TELL:
		// 	open(f->R.rdi);
		// 	break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		
		default:
			exit(-1);
			break;
	}

	// printf ("system call!\n");
	// thread_exit ();
}
