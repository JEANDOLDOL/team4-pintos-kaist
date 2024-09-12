#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
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
	lock_init(&filesys_lock);
}


int filesize(int fd)
{
	struct thread *curr = thread_current(); 
	struct file **fdt = curr -> fd_table;

	if (fd < 0 || fd >= FDT_COUNT_LIMIT || fdt[fd] == NULL)
		return -1;

	struct file *file = fdt[fd];

	int size = file_length(file);
	// printf("File size: %d\n", size);
	return size;
}

int add_file_to_fd_table(struct file *file)
{
	
	struct thread *curr = thread_current(); 
	struct file **fdt = curr -> fd_table; //현재 스레드의 파일 디스크립터 테이블

	//fd_table을 처음부터 순회하면 빈 곳이 있는 지 찾는다.
	//빈 곳을 찾으면 그 곳에 파일을 넣고 빈 곳이 없다면 가장 마지막에 넣는다.

	int max_fds = curr -> fdidx; // 파일 디스크립터 테이블의 최대 크기
	int fd;

	max_fds++;

	for(fd = 3; fd < max_fds; fd++){ //fd는 3부터 시작(0:stdin, 1:stdout을 제외
		if(fdt[fd] == NULL){ // 빈 자리를 찾으면 
			fdt[fd] = file; // 해당 파일을 테이블에 넣음
			return fd; // 해당 fd 반환
		}
	}

	if (fd == max_fds){ // 끝까지 다 돌았는데 빈 공간이 없으면 
		curr->fdidx += 1; // 테이블의 크기를 늘인다.
		fdt[fd] = file; // 파일을 마지막에 추가
		return fd; // 새로운 fd반환 
	}

	return -1; // 실패 시 -1 반환 (메모리 부족 혹은 테이블 문제)

}
void exit(int status){
	struct thread *curr = thread_current();
	curr -> exit_status = status;
	printf ("%s: exit(%d)\n", curr->name, status);
	thread_exit();
}

void check_address(void *addr)
{
	struct thread *curr = thread_current();
	if(!is_user_vaddr(addr)||addr==NULL||pml4_get_page(curr->pml4, addr)==NULL)
	//주소가 유저 영역이 아니거나, 주소가 NULL이거나, 그 주소에 대응하는 물리 페이지가 있는지를 확인한다.
	{
		exit(-1);
	}
}


bool create (const char *file, unsigned initial_size)
{
	check_address(file);
	if(filesys_create(file, initial_size))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool remove(const char *file) {
	check_address(file);
	if(filesys_remove(file))
	{
		return true;
	}
	else
	{
		return false;
	}
}

int open(const char *file)
{
	check_address(file);
	// 진짜 시이이발 욕밖에 안나오네 시이이이이이이이바아아아아아아아아아알
	// 뭐 어쩌라는거야 진짜 이 시이이이바아아아알럼
	struct thread *curr = thread_current();
	struct file *file_object = filesys_open(file);
	if(file_object == NULL) {
		return -1;
	}
	int fd = add_file_to_fd_table(file_object);

	if(fd == -1)
	{
		file_close(file_object);
	}
	return fd;
}


int write (int fd, const void *buffer, unsigned size) {
	// putbuf(buffer, size);
	// return size;
	check_address(buffer);

	struct thread *curr = thread_current();
	struct file **fdt = curr -> fd_table;

	if(fd < 0 || fd >= FDT_COUNT_LIMIT)
		return -1;

	if(fd == 1)
	{
		// 쓴것을 버퍼에 저장해놓는다.
		putbuf(buffer, size);
		return size;	
	}
	else if (fd >= 3) {
		// printf("fd normal\n");
		//이 코드 조각은 파일 디스크립터 테이블(fdt)에서 특정 파일 디스크립터(fd)에 해당하는 파일을 찾아오는 과정에서 사용됩니다. 파일 디스크립터 테이블은 각 프로세스가 열고 있는 파일들을 관리하는 구조입니다.
		struct file *file_to_write = fdt[fd]; 
		if (file_to_write == NULL) {
			return -1;
		}

		lock_acquire(&filesys_lock);
		int bytes_written = file_write(file_to_write, buffer, size);
		// printf("Bytes read: %d\n", bytes_read);
		lock_release(&filesys_lock);
		
		if (bytes_written < 0){
			return -1;
		}
		return bytes_written;
	}
	return -1;
}


void close(int fd)
{
	struct thread *curr = thread_current();
	struct file **fdt = curr -> fd_table;

	if (fd < 0 || fd >= FDT_COUNT_LIMIT || fdt[fd] == NULL)
		return;

	file_close(fdt[fd]);
	fdt[fd] = NULL;
}

int read (int fd, void *buffer, unsigned size) {
	check_address(buffer);

	struct thread *curr = thread_current();
	struct file **fdt = curr -> fd_table;
	// int bytes;

	if(fd < 0 || fd >= FDT_COUNT_LIMIT)
		return -1;
	// printf("fd : %d\n", fd);
	// printf("fd : %d\n", fd);
	// 표준 입력(fd가 0인 경우)
	if(fd == 0)
	{	//input_getc함수가 데이터를 반환하고 1바이트씩 읽기 때문에 한바이트씩 읽은 것들을 저장하고 나중에 한번에 그것들은 반환한다.
		// printf("if in\n");
		unsigned i;
		for(i = 0; i < size; i++){
			((char*)buffer)[i] = input_getc();
		}
		return size;
	}
	else if (fd >= 3) {
		// printf("fd normal\n");
		struct file *file_to_read = fdt[fd]; 
		if (file_to_read == NULL) {
			return -1;
		}
		lock_acquire(&filesys_lock);
		int bytes_read = file_read(file_to_read, buffer, size);
		// printf("Bytes read: %d\n", bytes_read);
		lock_release(&filesys_lock);
		
		if (bytes_read < 0){
			return -1;
		}
		return bytes_read;
	}
	return -1;
}

pid_t fork (const char *thread_name){
	
	// 엄마랑 똑같은 자식을 만든다.
	// 자 그래서 이거는 어떻게 해야하나
	// 포크를 하면 어미와 똑같은 자식새끼를 하나 만든다.

}	

// unsigned tell (int fd) {
// 	if(fd < 3);
// 		return ;
	
// }

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	// thread_exit ();

	switch (f->R.rax)
	{
		case SYS_HALT:
			power_off();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
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
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;		
		case SYS_REMOVE:
			remove(f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		// case SYS_SEEK:
		// 	halt();
		// 	break;
		// case SYS_TELL:
		// 	tell();
		// 	break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
	}
}