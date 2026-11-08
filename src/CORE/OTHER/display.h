#ifndef _DISPLAY_H
#define _DISPLAY_H
#include "sys.h"

/* control() 里的故障记录（main.c 定义），记录页要读 */
extern int diag_max_e;
extern int diag_lost;
extern int diag_full;
extern int diag_lost_n;
extern int diag_lost_e;

void page1(void);
void page2(void);
void page3(void);
void page_ready(u8 sec);
void page_live(void);
void page_stop(u32 ms);


#endif

