#include "init.h"
#include "delay.h"
#include "led.h"
#include "adc.h"
#include "motor.h"
#include "oled.h"
#include "timer.h"



//所有函数初始化
void all_init(void)
{
	delay_init();//延时函数初始化
	ADC1_init();//adc初始化
	led_init();//led初始化
	stop();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	motor_init(1000-1,36-1);//电机初始化
	OLED_Init();//初始化oled屏幕
	TIM3_isr_init(5000 - 1, 7200-1);//中断初始化
}

