#ifndef __OLED_H
#define __OLED_H 

#include "sys.h"

//-----------------ÊµãËØïLEDÁ´ØÂè£ÂÆö‰πâ---------------- 

#define LED_ON GPIO_ResetBits(GPIOB,GPIO_Pin_8)
#define LED_OFF GPIO_SetBits(GPIOB,GPIO_Pin_8)

//-----------------OLEDÁ´ØÂè£ÂÆö‰πâ---------------- 

/* ---------- 0.96 ¥Á 7 ’Î SPI OLED£®SSD1306£©“˝Ω≈∂®“Â ----------
   D0/SCLK -> PA5        D1/MOSI -> PA7
   RES     -> PA4        DC      -> PA6
   CS      -> ÷±Ω”Ω” GND£®◊‹œﬂ…œ÷ª”–’‚“ªø≈∆˜º˛£¨≥£—°Õ®º¥ø…£©
   Àƒ∏˘–≈∫≈œﬂ»´‘⁄ PA4~PA7 ’‚Àƒ∏ˆœ‡¡⁄“˝Ω≈…œ£¨Ω”œﬂ◊Ó ° ¬°£
   ◊¢“‚£∫‘≠¿¥µƒ 4 ’Î I2C ∆¡◊˘◊”∏¯µƒ PA5 / PA7 ’˝∫√æÕ «’‚¡Ω∏˘ ˝æ›œﬂ£¨
   À˘“‘∆¡◊˘…œƒ«¡Ω∏˘œﬂø…“‘Ω”◊≈”√£¨÷ª–Ë‘Ÿ∑… RES / DC ¡Ω∏˘°£          */
#define OLED_PORT       GPIOA
#define OLED_SCLK_PIN   GPIO_Pin_5
#define OLED_SDIN_PIN   GPIO_Pin_7
#define OLED_RST_PIN    GPIO_Pin_4
#define OLED_DC_PIN     GPIO_Pin_6

#define OLED_SCLK_Clr() (OLED_PORT->BRR  = OLED_SCLK_PIN)
#define OLED_SCLK_Set() (OLED_PORT->BSRR = OLED_SCLK_PIN)
#define OLED_SDIN_Clr() (OLED_PORT->BRR  = OLED_SDIN_PIN)
#define OLED_SDIN_Set() (OLED_PORT->BSRR = OLED_SDIN_PIN)
#define OLED_RST_Clr()  (OLED_PORT->BRR  = OLED_RST_PIN)
#define OLED_RST_Set()  (OLED_PORT->BSRR = OLED_RST_PIN)
#define OLED_DC_Clr()   (OLED_PORT->BRR  = OLED_DC_PIN)
#define OLED_DC_Set()   (OLED_PORT->BSRR = OLED_DC_PIN)


#define OLED_CMD  0	//ÂÜôÂëΩ‰ª§
#define OLED_DATA 1	//ÂÜôÊï∞ÊçÆ
#define u8 unsigned char
#define u32 unsigned int

extern u8 OLED_GRAM[144][8];
void OLED_ClearPoint(u8 x,u8 y);
void OLED_ColorTurn(u8 i);
void OLED_DisplayTurn(u8 i);
void OLED_WR_Byte(u8 dat,u8 mode);
void OLED_DisPlay_On(void);
void OLED_DisPlay_Off(void);
void OLED_Refresh(void);
void OLED_Clear(void);
void OLED_DrawPoint(u8 x,u8 y);
void OLED_DrawLine(int x1,int y1,int x2,int y2);
void OLED_DrawCircle(u8 x,u8 y,u8 r);
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 size1);
void OLED_ShowString(u8 x,u8 y,u8 *chr,u8 size1);
void OLED_ShowNum(u8 x,u8 y,u32 num,u8 len,u8 size1);
void OLED_ShowChinese(u8 x,u8 y,u8 num,u8 size1);
void OLED_ScrollDisplay(u8 num,u8 space);
void OLED_WR_BP(u8 x,u8 y);
void OLED_ShowPicture(u8 x0,u8 y0,u8 x1,u8 y1,u8 BMP[]);
void OLED_Init(void);

#endif

