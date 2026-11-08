#ifndef __ADC_H
#define	__ADC_H
#include "sys.h"

extern __IO u16 ADC_ConvertedValue[4]; 

extern int ADC_right_aclinic;
extern int ADC_right_vertical;
extern int ADC_left_vertical;
extern int ADC_left_aclinic;

/* 差比和偏差 + 开机自动零点：不需要标定量程 */
extern int track_err;        /* -1000(偏左) ~ +1000(偏右) */
extern int err_zero;         /* 开机自校准得到的固定偏置 */
extern int adc_sum;          /* 左右读数之和，判断有没有信号 */

int get_track_error(void);   /* 返回 -1000(偏左) ~ +1000(偏右) */
u8  zero_ready(void);
extern u8 adc_lost;          /* 1 = 当前判为丢线（两路信号都太弱） */


void ADC1_init(void);          //adc1初始化
void get_ADC_values(void);     //获取电感数值
float first_order_low_pass_filter(char *type, float k, float number);//一阶低通滤波器

#endif 

