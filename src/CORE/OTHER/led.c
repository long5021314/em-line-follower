#include "led.h"

//led初始化
void led_init(void)
{
 
 GPIO_InitTypeDef  GPIO_InitStructure;                                          //个体结构声明
 	
 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOE, ENABLE);	    //使能PB端口时钟
	
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;				                              //IO口配置GPIOC13
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		                          //推挽输出
 GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		                          //IO口速度为50MHz
 GPIO_Init(GPIOC, &GPIO_InitStructure);					                                //根据设定参数初始化GPIOC13
 GPIO_ResetBits(GPIOC,GPIO_Pin_13);					                                 	    //拉高GPIOC13电平

}
 
