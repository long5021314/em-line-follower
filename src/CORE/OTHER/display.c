#include "display.h"
#include "oled.h"
#include "adc.h"
#include "timer.h"

/* ================== 调电位器专用页面 ==================
   屏幕上显示的是四路 ADC 的原始值（0~4095），不是欧姆数 ——
   电位器的阻值只有万用表能量，但"拧电位器到底改变了多少"完全等价于这个读数，
   所以调平就是看这四个数，不用拆板子。

        L 1834      R 1801        <- 左水平(PA0) / 右水平(PA3)，差比和只用这两路
        A1  612     A2  588       <- PA1 / PA2（竖直两路，暂未启用，但拧电位器时也会动）
        S 3635      B  +1%        <- 左右之和 / 左右不平衡度
        Z   +8      OK            <- 零点偏置 / 状态

   冒号变成感叹号（例如 "L!3990"）表示这一路超程：
      大于 3800 是运放削顶，必须把电位器调小；
      小于  300 是信号太弱，先查虚焊再去拧。

   状态位含义：
      SAT  有通道 >3800，饱和削顶，差速会被吃掉
      LOW  有通道 <300，信号太弱
      ADJ  左右相差超过 15%，需要调平
      OK   四路都在正常区间且左右平衡
   ====================================================== */

static u32 p10(u8 n)
{
	u32 r = 1;
	while(n--) r *= 10;
	return r;
}

static void show_signed(u8 x, u8 y, int v, u8 len, u8 size1)
{
	u8 i;
	u32 a;
	if(v < 0) { OLED_ShowChar(x, y, '-', size1); a = (u32)(-v); }
	else      { OLED_ShowChar(x, y, ' ', size1); a = (u32)v; }
	for(i = 0; i < len; i++)
		OLED_ShowChar(x + (size1/2)*(i+1), y, (u8)((a / p10(len-i-1)) % 10) + '0', size1);
}

/* 画 "名字 + 正负号 + 3 位数值"，专给偏差/转向量这类 ±1000 的量用。
   名字最多 1 个字符，整体占 5 个字符宽（40 像素），左右两栏都放得下。 */
static void show_schan(u8 x, u8 y, const char *name, int v)
{
	u8 n = 0, i;
	u32 a = (v < 0) ? (u32)(-v) : (u32)v;

	if(a > 999) a = 999;
	while(name[n]) { OLED_ShowChar(x + n*8, y, (u8)name[n], 16); n++; }
	OLED_ShowChar(x + n*8, y, (v < 0) ? '-' : '+', 16);
	n++;
	for(i = 0; i < 3; i++)
		OLED_ShowChar(x + (n+i)*8, y, (u8)((a / p10(2-i)) % 10) + '0', 16);
}

/* 画 "名字 + 分隔符 + 4 位数值"，超程时分隔符变成 '!' */
static void show_chan(u8 x, u8 y, const char *name, int v, u8 size1)
{
	u8 n = 0, i;
	u8 w = size1 / 2;
	u8 bad;

	if(v < 0)    v = 0;
	if(v > 9999) v = 9999;
	bad = (v > 3800) || (v < 300);

	while(name[n])
	{
		OLED_ShowChar(x + n*w, y, (u8)name[n], size1);
		n++;
	}
	OLED_ShowChar(x + n*w, y, bad ? '!' : ':', size1);
	n++;
	for(i = 0; i < 4; i++)
		OLED_ShowChar(x + (n+i)*w, y, (u8)((v / p10(3-i)) % 10) + '0', size1);
}

void page1(void)
{
	int L  = ADC_ConvertedValue[0];    /* PA0 左水平 */
	int R  = ADC_ConvertedValue[3];    /* PA3 右水平 */
	int A1 = ADC_ConvertedValue[1];    /* PA1 */
	int A2 = ADC_ConvertedValue[2];    /* PA2 */
	int sum, bal;
	u8 i, n;

	/* 先把显存整片清零，否则上一屏的字符会残留（例如 SAT 变 OK 时留个 T） */
	for(n = 0; n < 128; n++)
		for(i = 0; i < 8; i++)
			OLED_GRAM[n][i] = 0;

	show_chan(0,  0, "L",  L,  16);
	show_chan(64, 0, "R",  R,  16);
	show_chan(0,  16, "A1", A1, 16);
	show_chan(64, 16, "A2", A2, 16);

	/* 左右之和：判断整体信号强弱，两个都健康时大概在 2000~5000 */
	sum = L + R;
	show_chan(0, 32, "S", sum, 16);

	/* 不平衡度（%）：正数表示右边强，负数表示左边强，目标 |bal| < 10 */
	if(sum >= 200) bal = (int)(((long)(R - L) * 200) / sum);
	else           bal = 0;
	OLED_ShowChar(64, 32, 'B', 16);
	show_signed(72, 32, bal, 3, 16);   /* 3 位：不平衡度能到 ±200，2 位会丢掉百位 */
	OLED_ShowChar(104, 32, '%', 16);

	/* 零点偏置：上电时采到的固定偏差，调完电位器必须重新上电才会重采 */
	OLED_ShowChar(0, 48, 'Z', 16);
	show_signed(8, 48, err_zero, 4, 16);

	/* 状态字 */
	if(R > 3800 || L > 3800)       OLED_ShowString(64, 48, (u8 *)"SAT", 16);
	else if(R < 300 || L < 300)    OLED_ShowString(64, 48, (u8 *)"LOW", 16);
	else if(bal > 15 || bal < -15) OLED_ShowString(64, 48, (u8 *)"ADJ", 16);
	else                           OLED_ShowString(64, 48, (u8 *)"OK",  16);

	OLED_Refresh();
}

/* ================== 跑车实时状态页 ==================
   编译时加 -DSHOW_ON_OLED=2 才会用这一页。

        E+123   T+086      <- 偏差 / 转向量（差速）
        L1834   R1801      <- 左右电感原始值
        S3635   Z+024      <- 信号强度 / 零点偏置
        RUN          460   <- 状态字 / 基础速度

   怎么用它看车：
      E 和 T 一直同号、来回翻 -> 在正常纠偏
      T 长期顶着 ±Steer_Max（状态显示 FULL）-> 转向打满，还在往外偏 = 转弯能力不够
      S 掉到 300 以下（状态显示 LOST）-> 丢线，此时 err 会被门限清零，车会直冲

   状态字：
      RUN   正常循迹
      FULL  转向已打满，此时若还在偏，就是曲率不够（要提速/加 Steer_Max）
      SAT   某一路原始值 >4000，运放削顶，差比和失真
      LOST  两路信号都很弱，判定为丢线
   ==================================================== */
void page2(void)
{
	int L = ADC_ConvertedValue[0];    /* PA0 左水平 */
	int R = ADC_ConvertedValue[3];    /* PA3 右水平 */
	int s = L + R;
	int e = pwm_error;                /* 扣掉死区和零点后的偏差，控制真正用的就是它 */
	int t = pid_Output;               /* 转向量（差速） */
	u8 i, n;

	for(n = 0; n < 128; n++)
		for(i = 0; i < 8; i++)
			OLED_GRAM[n][i] = 0;

	show_schan(0,  0, "E", e);
	show_schan(64, 0, "T", t);

	show_chan(0,  16, "L", L, 16);
	show_chan(64, 16, "R", R, 16);

	show_chan(0,  32, "S", s, 16);
	show_schan(64, 32, "Z", err_zero);

	if(s < 300)                                   OLED_ShowString(0, 48, (u8 *)"LOST", 16);
	else if(L > 4000 || R > 4000)                 OLED_ShowString(0, 48, (u8 *)"SAT",  16);
	else if(t >= Steer_Max || t <= -Steer_Max)    OLED_ShowString(0, 48, (u8 *)"FULL", 16);
	else                                          OLED_ShowString(0, 48, (u8 *)"RUN",  16);

	OLED_ShowNum(96, 48, speed, 3, 16);           /* 基础速度，3 位足够 */

	OLED_Refresh();
}

/* ================== 故障记录页（-DSHOW_ON_OLED=3） ==================
   跑的时候屏上读不出数（车在动），所以把"跑的过程中发生过什么"攒下来，
   停下来再看。上电清零。

        EMAX      本次上电见过的最大偏差
        LOST      累计判为丢线的毫秒数
        FULL      累计转向打满的毫秒数
        L / R     当前实时原始值（顺便确认信号还在）

   怎么读：

      LOST = 0              -> 全程没有丢过线（信号链健康）
      LOST > 0              -> 半路信号断过，车是"看不见线"才冲出去的
      FULL 很大（几百上千）  -> 转弯能力用尽，是真的转不过来
      EMAX 小、LOST=0 且 FULL 也小 -> 既没丢线也没打满，那是打滑或机械问题

   注意：这三个都是"累计毫秒"，不是次数，1000 = 累计 1 秒。 */
void page3(void)
{
	u8 i, n;

	for(n = 0; n < 128; n++)
		for(i = 0; i < 8; i++)
			OLED_GRAM[n][i] = 0;

	OLED_ShowString(0, 0,  (u8 *)"EMAX", 16);
	OLED_ShowString(48, 0, (u8 *)"=", 16);
	show_signed(96, 0, diag_max_e, 3, 16);

	OLED_ShowString(0, 16, (u8 *)"LOST", 16);
	OLED_ShowString(48, 16, (u8 *)"=", 16);
	OLED_ShowNum(96, 16, (u32)diag_lost, 4, 16);

	OLED_ShowString(0, 32, (u8 *)"FULL", 16);
	OLED_ShowString(48, 32, (u8 *)"=", 16);
	OLED_ShowNum(96, 32, (u32)diag_full, 4, 16);

	/* 第四行：N = 丢线发生了几次；E = 最近一次丢线之前的 |偏差|
	   N=1 且 E 很小  -> 信号突然没了（断线/岔口/离开白面），不是跑偏
	   N=1 且 E 很大  -> 先跑偏、后丢线，是转向能力或速度问题
	   N 很大         -> 反复丢，点位/线宽问题 */
	OLED_ShowString(0, 48, (u8 *)"N", 16);
	OLED_ShowNum(16, 48, (u32)diag_lost_n, 2, 16);
	OLED_ShowString(40, 48, (u8 *)"E", 16);
	OLED_ShowNum(56, 48, (u32)diag_lost_e, 3, 16);

	/* 右下角 S = 已经跑了多少秒，用来判断 STOP_MS 标得对不对 */
	{
		extern u32 run_ms;
		OLED_ShowString(80, 48, (u8 *)"S", 16);
		OLED_ShowNum(96, 48, run_ms / 1000, 2, 16);
	}

	OLED_Refresh();
}

/* ================== 起跑准备页 ==================
   上电采完零点后显示。屏幕上出现 OK 表示"车已就绪、可以松手"，
   然后倒计时 2 -> 1 -> 0，数到 0 才开始输出 PWM。
   比赛规则是"车头过起跑线才开始计时"，所以车就放在线前几厘米等这一刻。 */
void page_ready(u8 sec)
{
	u8 i, n;

	for(n = 0; n < 128; n++)
		for(i = 0; i < 8; i++)
			OLED_GRAM[n][i] = 0;

	OLED_ShowString(44, 4,  (u8 *)"READY", 16);
	OLED_ShowString(48, 28, (u8 *)"OK", 16);
	OLED_ShowNum(68, 28, sec, 1, 16);

	OLED_Refresh();
}

/* ================== 实时状态页（起跑前）==================
   车刚放上赛道时显示，用来确认两件事：
     1) 车压对位置了没有 —— L 和 R 应该接近，S 和平时差不多
     2) 手松开了没有 —— 数字不再乱跳
   稳了以后 main 才去采零点，屏幕随后换成 READY / OK。 */
void page_live(void)
{
	int L = ADC_ConvertedValue[0];
	int R = ADC_ConvertedValue[3];
	u8 i, n;

	for(n = 0; n < 128; n++)
		for(i = 0; i < 8; i++)
			OLED_GRAM[n][i] = 0;

	show_chan(0,  0, "L", L, 16);
	show_chan(64, 0, "R", R, 16);
	show_chan(0,  20, "S", L + R, 16);
	show_schan(64, 20, "E", pwm_error);

	if(zero_ready())
		OLED_ShowString(16, 44, (u8 *)"READY", 16);
	else
		OLED_ShowString(0, 44, (u8 *)"PUT ON LINE", 16);

	OLED_Refresh();
}

/* ================== 停车页（自动停车后显示）==================
   跑完断电后把本次用时打在屏上，省得还要另外掐表。
   这是"从发车到断电"的时间，比裁判从车头过线开始算的多 0.1 秒左右。 */
void page_stop(u32 ms)
{
	u8 i, n;

	for(n = 0; n < 128; n++)
		for(i = 0; i < 8; i++)
			OLED_GRAM[n][i] = 0;

	OLED_ShowString(32, 4, (u8 *)"STOP", 16);
	OLED_ShowString(16, 32, (u8 *)"T", 16);
	OLED_ShowNum(32, 32, ms / 1000, 2, 16);
	OLED_ShowString(48, 32, (u8 *)".", 16);
	OLED_ShowNum(56, 32, (ms % 1000) / 100, 1, 16);
	OLED_ShowString(64, 32, (u8 *)"s", 16);

	OLED_Refresh();
}
