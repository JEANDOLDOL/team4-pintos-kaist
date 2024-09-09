#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H
// #define USERPROG
#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

// 초기화에 필요한 값
#define PRI_MAX 63
#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int priority;			   /* Priority. */
	int64_t wake_time;		   /* 기상나팔 울리는 시간 */
	int original_priority;	   /* 원래 우선순위 */
	struct list donators;	   /* 기부자들 명단 */
	struct lock *waiting_lock; /* 기다리고 있는 락 */
	int nice;				   /* 나이스값 */
	int recent_cpu;			   /* recent_cpu */
	struct list_elem all_elem; /* for advanced scheduler*/
	int exit_status;
	struct file **fd_table;
	int max_fd;

	/* Shared between thread.c and synch.c. */
	struct list_elem elem; /* List element. */
	struct list_elem donation_elem;

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);

// 깨우고 재우는 함수 추가
void thread_sleep(int64_t ticks);
void thread_wake(int64_t ticks);
extern struct list ready_list;

bool compare_thread(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool thread_compare_donate_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

void remove_with_lock(struct lock *lock);
void refresh_priority(void);

void thread_change(void);

// 소수 연산 매크로 생성
#define F (1 << 14) // 고정 소수점 비율 정의

#define FLOAT(n) ((n) * F) // 정수를 고정 소수점으로 변환
#define INT(n) ((n) / F)   // 고정 소수점을 정수로 변환

#define ROUNDINT(x) (((x) >= 0) ? (((x) + F / 2) / F) : (((x) - F / 2) / F)) // 반올림하여 정수로 변환

#define ADDFI(x, i) ((x) + (i) * F) // 고정 소수점에 정수 추가
#define SUBIF(i, f) ((i) * F - (f)) // 정수에서 고정 소수점 뺌

#define MUL(x, y) (((int64_t)(x)) * (y) / F) // 두 고정 소수점 수를 곱함
#define MULFI(x, n) ((x) * (n))				 // 고정 소수점을 정수와 곱함

#define DIV(x, y) (((int64_t)(x)) * F / (y)) // 두 고정 소수점 수를 나눔
#define DIVFI(x, n) ((x) / (n))				 // 고정 소수점을 정수로 나눔

void mlfqs_priority(struct thread *t);
void mlfqs_recent_cpu(struct thread *t);
void mlfqs_load_avg(void);
void mlfqs_increment(void);
void mlfqs_recalc_recent_cpu(void);
void mlfqs_recalc_priority(void);

#endif /* threads/thread.h */
