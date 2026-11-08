#ifndef _TIMER_H
#define _TIMER_H
#include "sys.h"

void TIM3_isr_init(u16 arr,u16 psc);

extern int Timer_Stop;

#endif

