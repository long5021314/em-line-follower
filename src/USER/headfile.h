#ifndef _HEADFILE_H
#define _HEADFILE_H
#include "stm32f10x.h"

//--------产品驱动文件
#include "adc.h"
#include "led.c"
#include "motor.h"
#include "display.h"
#include "oled.h"
#include "oledfont.h"
#include "timer.h"
#include "init.h"

extern int speed;   //速度
extern float p;      //转向差值比例

void control(void);//控制函数

#endif

