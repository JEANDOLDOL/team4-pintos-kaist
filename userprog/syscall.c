#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
// 추가
#include "filesys/filesys.h"
#include "threads/init.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "devices/input.h"
struct lock filesys_lock; // 파일 읽기/쓰기 용 lock

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
int open(const char *file);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	// lock초기화
	lock_init(&filesys_lock);
}

void check_address(const uint64_t *addr)
{
	struct thread *curr = thread_current();
	if (addr == NULL || !(is_user_vaddr(addr)) || pml4_get_page(curr->pml4, addr) == NULL)
		exit(-1);
}

// project2
// halt 구현
void halt(void)
{ // power_off()를 호출해 핀토스를 종료함.
	power_off();
}

// exit 구현
void exit(int status)
{ // 현재 동작중인 유저 프로그램을 종료함. 커널에 상태를 리턴하면서 종료. 0은 성공, 나머지는 fail.

	thread_current()->exit_num = status;
	struct thread *curr = thread_current();
	char cpyname[16]; // cpyname의 크기를 적절하게 설정하세요
	strlcpy(cpyname, curr->name, 16);
	char *save_ptr;
	char *first_word;
	first_word = strtok_r(cpyname, " ", &save_ptr);
	if (first_word != NULL)
	{
		printf("%s: exit(%d)\n", first_word, curr->exit_num);
	}

	thread_exit();
}

// // fork 구현
// pid_t fork(const char *thread_name)
// { // thread_name이라는 이름을 가진 현재 프로세스의 복제본인 새 프로세스를 만듬. 자식 프로세스의 pid를 반환해야함. 자식프로세스의 반환값은 0.
// 	return;
// }

bool create(const char *file, unsigned initial_size)
{ // 첫 번째 인자를 이름으로 하는 파일을 생성. 두 번쨰 인자는 크기이다. 성공시 true 반환. open 하지는 않는다.
	check_address(file);
	return filesys_create(file, initial_size);
}

int open(const char *file)
{ // 첫번째 인자인 이름을 가진 파일을 연다. 성공적으로 열렸다면, 0또는 양수인 fd를 반환, 실패했다면 -1을 반환.
	check_address(file);
	struct file *open_file = filesys_open(file);
	if (open_file == NULL)
	{
		return -1; // 담피하고 할일 파일 모음에 이름이. 있나 검사
	}
	else
	{
		struct thread *curr = thread_current();
		int fd;
		for (fd = 3; fd <= 30; fd++)
		{
			// if (curr->fdt[fd] == open_file)
			if (curr->fdt[fd] == NULL)
			{
				curr->fdt[fd] = open_file;
				curr->max_fd += 1;
				return fd;
			}
		}
		file_close(open_file);
		return -1;
	}
}

void close(int fd)
{
	struct thread *curr = thread_current();
	if (fd < 0 || fd >= 30 || curr->fdt[fd] == NULL)
		return;
	if (curr->fdt[fd])
	{
		file_close(curr->fdt[fd]);
		curr->fdt[fd] = NULL;

		return;
	}
}

int filesize(int fd)
{
	struct thread *curr = thread_current();
	struct file *file = curr->fdt[fd];

	if (file == NULL)
		return -1;

	return file_length(file);
}

int write(int fd, const void *buffer, unsigned size)
{ // buffer에 있는 size만큼의 바이트를 fd에 작성한다. 실제로 작성된 바이트의 수를 반환한다(버퍼의 size를 다 채우지 않을수도).
  // write는 eof에 도달할 때 까지 최대한 많은 바이트를 작성해야 함. 그리고 실제로 작성된 바이트를 반환함.
  // 아무것도 작성되지 않았다면, 0을 반환.
  // fd 1은 콘솔에 작성하는 기능.

	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}
	else
	{
		return 0;
	}
}

int read(int fd, void *buffer, unsigned size)
{
	// fd로 연 `size`바이트를 버퍼로 읽어들인다.
	// fd 0 은 input_getc()의 키보드입력으로 부터 읽힘.

	if (fd >= 30 || fd < 0)
	{
		return -1;
	}
	check_address(buffer);
	if (fd == 0)
	{
		unsigned i;
		char c;
		unsigned char *buf = buffer;

		for (i = 0; i < size; i++)
		{
			c = input_getc();
			*buf++ = c;
			if (c == '\0')
				break;
		}

		return i;
	}
	struct thread *curr = thread_current();
	struct file *file = curr->fdt[fd];
	// 파일이 read될 수 없는 경우(eof때문이 아닌 컨디션 때문에) 실제로 read된 바이트의 수를 리턴.
	if (fd == 1 || fd == 2)
		return -1;
	if (file == NULL)
	{
		return -1;
	}
	off_t bytes = -1;
	lock_acquire(&filesys_lock);
	bytes = file_read(file, buffer, size);
	lock_release(&filesys_lock);

	return bytes;
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// register uint64_t *num asm("rax") = (uint64_t *)num_;
	// register uint64_t *a1 asm("rdi") = (uint64_t *)a1_;
	// register uint64_t *a2 asm("rsi") = (uint64_t *)a2_;
	// register uint64_t *a3 asm("rdx") = (uint64_t *)a3_;
	// register uint64_t *a4 asm("r10") = (uint64_t *)a4_;
	// register uint64_t *a5 asm("r8") = (uint64_t *)a5_;
	// register uint64_t *a6 asm("r9") = (uint64_t *)a6_;
	// TODO: Your implementation goes here.
	int number = f->R.rax;
	switch (number)
	{
	case SYS_HALT:
		halt();
		break;

	case SYS_EXIT:
		exit(f->R.rdi);
		break;

	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;

		// case SYS_FORK:
		// 	fork();
		// 	break;

		// case SYS_EXEC:
		// 	halt();
		// 	break;

		// case SYS_WAIT:
		// 	halt();
		// 	break;

	case SYS_CREATE:
		f->R.rax = create((char *)f->R.rdi, f->R.rsi);
		break;

	// case SYS_REMOVE:
	// 	halt();
	// 	break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;

	case SYS_OPEN:
		f->R.rax = open((char *)f->R.rdi);
		break;

	case SYS_WRITE:
		write(f->R.rdi, (void *)f->R.rsi, f->R.rdx);
		break;

	case SYS_READ:
		f->R.rax = read((struct file *)f->R.rdi, (void *)f->R.rsi, f->R.rdx);
		break;

	default:
		// printf("system call!\n");
		thread_exit();
		break;
	}
}
