#ifndef _MOTOR_H
#define _MOTOR_H
#include "sys.h"


/* PWM 上限必须等于 TIM4 的 ARR(1000-1)。写 10000 时 speed=5000 直接贴死在满速，
   左右轮的差速会被完全吃掉，表现就是「要么直冲、要么一把甩出去、来回摇头」 */
#define Speed_Max 999


void motor_init(u16 arr,u16 psc);                //PWM初始化
void motor_control(int16_t leftpwm,int16_t rightpwm);    //电机速度设置
void stop(void); //停车



#endif
