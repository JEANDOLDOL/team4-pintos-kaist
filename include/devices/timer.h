#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>

/* Number of timer interrupts per second. */
// 1초에 timer tick이 몇번
// 변경 금지 -> test fail 뜰 것
#define TIMER_FREQ 100

#define F (1 << 14)  // 고정 소수점 비율 정의
#define FLOAT(n) ((n) * F)  // 정수를 고정 소수점으로 변환
#define INT(n) ((n) / F)  // 고정 소수점을 정수로 변환

#define DIVFI(x, n) ((x) / (n))  // 고정 소수점을 정수로 나눔

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

void timer_print_stats (void);

#endif /* devices/timer.h */
