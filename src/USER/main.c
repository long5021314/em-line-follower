#include "sys.h"
#include "init.h"
#include "display.h"
#include "adc.h"
#include "motor.h"
#include "delay.h"

/* ======================= 可调参数（只动这几个） =======================
   speed      基础速度：单位就是 PWM 计数值，硬件上限 Speed_Max=999
   Steer_Max  最大差速：必须 < speed，保证内侧轮永远正转、不反转
   p          比例：偏差 × p = 差速。偏差满偏 ±1000，p=0.40 时 750 打满舵
   d          微分：先给 0；直道跑顺了再补 0.05 ~ 0.15
   Dead_Zone  中线死区：偏差小于它就不修方向，专治"正中线附近来回抖"
   Sign_Flip  如果车往反方向修（越修越偏），把它改成 1
   Steer_Trim 直线中值：车摆正上电、偏差也接近 0，但它还是固定偏一侧时，
              用它抵掉（两轮速度不等或前万向轮不正）。往左偏就填正值试，从 ±20 开始
   ==================================================================== */
 int   speed     = 950;
 int   Steer_Max = 638;
 float   p       = 1.02f;
 float   d       = 0.075f;
                           //还抖就往下减（0.065/0.05）；直道慢摆则往上加
 int   speed_min = 450;
 int   Dead_Zone = 25;
 int   Sign_Flip = 0;      //方向反了就改 1
 int   Steer_Trim = 0;     //直线中值：两轮速度不等/万向轮不正时，用它抵掉

 int fork_flag  = 700;    //三岔标志
 int stop_flag  = 500;    //停车标志

int Num;
u32 run_ms = 0;      /* 控制环已运行毫秒数：停车判据 + 屏幕显示 */

/* ---------- 故障记录：跑完停下来看这三个数 ----------
   跑的过程中屏上读不出数（车在动、4Hz 刷新），所以把"发生过什么"攒起来。
   diag_max_e  本次上电见过最大的 |偏差|
   diag_lost   累计判为丢线的毫秒数（>0 说明中途信号断过）
   diag_full   累计转向打满的毫秒数（>0 说明转弯能力用尽过）
   上电清零，只在 control() 里累加。 */
int diag_max_e  = 0;   /* 本次上电见过最大的 |偏差| */
int diag_lost   = 0;   /* 累计丢线毫秒数 */
int diag_full   = 0;   /* 累计"转向用到顶"的毫秒数 */
int diag_lost_n = 0;   /* 丢线发生了多少次（次数，不是毫秒） */
int diag_lost_e = 0;   /* 最近一次进入丢线前的 |偏差| */

/* OLED 页面选择（编译时用 -DSHOW_ON_OLED=n 指定，不定义就是不刷屏）
     0 = 不刷屏：跑车最省事，控制环完全不受影响
     1 = 调参页：L/R/不平衡度/零点，专门用来调电位器
     2 = 状态页：偏差/转向量/信号强度/状态字，跑车时看车在干什么
   SPI 屏刷一整屏约 2ms（老 I2C 屏是 20~30ms），所以开着屏对控制环的影响已经很小；
   但跑正式成绩时仍建议回到 0。 */
/* ================= 自动停车（现场细则第 3 条，必须有）=================
   规则原文：跑完一圈后赛车需要自动停止在起始线之后 2 米之内的赛道内，
             如果没有停止在规定的区域内，比赛计时成绩增加 10 秒。
   也就是说"跑完不停"不是小事，是直接多 10 秒 —— 19.78 会变成 29.78。

   做法：从控制环开始计时，跑够 STOP_MS 毫秒就电机断电并死等，
   必须重新上电才能再跑（避免停车后又自己窜出去）。

   怎么定 STOP_MS：
       STOP_MS = 实测一圈用时 * 1000 + 700 ~ 1200
   多出来的 0.7~1.2 秒让车滑过起跑线 0.5~0.9 米，落在"线后 2 米内"。
       例：实测 18.4s -> 18400 + 1000 = 19400
   默认 20000 是保守值：750 版会停在线后约 1.1 米，665 版约 0.15 米。
   ！！！ 若实际用时超过 19.5 秒，20000 会让车"还没过线就停"，
   那一趟直接算未完成，必须把 STOP_MS 调大。
   ==================================================================== */
/* ===== 弯道切内量（11-07 新增）=====
   左弯时把转向量减掉 CUT_IN，车就会稳定地"吃"在弯道内侧；
   右弯反之。越大越贴内，也越靠近边线。
   120 约等于往内侧偏 2.5cm；觉得不够可以加到 180，觉得贴边了降到 80。 */
#ifndef CUT_IN
#define CUT_IN 260
#endif

/* ===== 自动停车开关（11-07）=====
   USE_AUTO_STOP = 0：不停车，一直跑到断电或被人抓住（手动计时用）
   USE_AUTO_STOP = 1：跑够 STOP_MS 毫秒就断电、停车并在屏上显示用时
   停车那段代码一直留在 control() 里，只是用这个开关决定编不编进去。 */
#ifndef USE_AUTO_STOP
#define USE_AUTO_STOP 1
#endif

#ifndef STOP_MS
#define STOP_MS 43700
#endif
#ifndef SHOW_ON_OLED
#define SHOW_ON_OLED 0
#endif

int main(void)
{
	all_init();//所有函数初始化

	/* ============ 起跑流程（11-07 重做）============
	   比赛是"车头过起跑线才开始计时"，所以车要先放在起点线之前，
	   而且必须压在导线正中 —— 零点就是拿这一刻的读数当"正中"。

	   第 1 段 实时状态
	     上电先显示 L / R / S / E 的实时值，用来确认：车放对位置没有
	     （L 和 R 应该接近、S 和平时差不多）、手松开了没有（数字不乱跳）。
	   第 2 段 稳了才采零点
	     左右之和连续 300ms 波动小于 2%，才算放稳，这时才开始采零点
	     （200ms 平均）。以前是上电就采，车还在手里晃，零点必然偏。
	   第 3 段 OK + 倒计时
	     零点到手 -> READY / OK 2 -> 1 -> 0 -> 自动发车。
	     看到 OK 就可以松手，不用抢。 */
	extern u8 zero_enable;      /* adc.c 里定义，置 1 才开始累加零点 */

	motor_control(0, 0);
	{
		u16 t = 0, show = 0, wait_guard = 0;   /* wait_guard: 等不稳时的兜底 */
		int s_lo = 30000, s_hi = 0;

		while(1)
		{
			int s;
			get_ADC_values();
			get_track_error();

			if(++show >= 100)          /* 10Hz 刷实时页，别把 SPI 占满 */
			{
				show = 0;
				page_live();
			}

			s = ADC_ConvertedValue[0] + ADC_ConvertedValue[3];
			if(s < s_lo) s_lo = s;
			if(s > s_hi) s_hi = s;

			if(++wait_guard > 10000) break;   /* 10 秒还没稳（没放线上/一直抖），别卡死，直接往下走 */

			if(++t >= 300)             /* 每 300ms 判一次稳不稳 */
			{
				if(s_lo > 200 && (s_hi - s_lo) * 50 < (s_hi + s_lo))
					break;             /* 有信号，且波动小于 2% */
				t = 0; s_lo = 30000; s_hi = 0;
			}
			delay_ms(1);
		}
	}

	/* 车放稳了，这时候采的零点才可信 */
	zero_enable = 1;
	{
		u16 guard = 0;
		while(!zero_ready() && guard < 1500)
		{
			get_ADC_values();
			get_track_error();
			if((guard & 63) == 0) page_live();
			delay_ms(1);
			guard++;
		}
	}

	/* 零点到手 -> OK -> 2 秒倒计时 -> 发车 */
	motor_control(0, 0);
	page_ready(2);  delay_ms(700);
	page_ready(1);  delay_ms(700);
	page_ready(0);  delay_ms(600);
	motor_control(0, 0);

	while(1)
		{
			control();

			Num++;
			/* 标定版的刷屏已挪进 control() 里按控制周期计数，见那里 */

		}
}


//控制函数，放在中断内的
int pwm_error=0,pid_Output;
int timer=0;
u8 time_flag=0;
void fork_judge(void);

void control(void)
{
	static u16 cnt_prev = 0;
	static u8  sub      = 0;
	static u16 idle     = 0;
	static u8  first    = 1;
	static int last_e   = 0;
	static float d_filt = 0.0f;
	static u16 show_cnt = 0;
	static u8  der_hold = 0;     /* 丢线/刚回线时冻结微分，避免"一脚舵" */
	int steer, der;
	int spd_now = speed;         /* 本周期实际用的基础速度（转弯降速后的） */

	/* ---------------- 定周期：1 kHz ----------------
	   TIM4 就是电机 PWM 的定时器：72MHz/36/1000 = 2kHz，即每 0.5ms 回绕一次。
	   数两次回绕 = 1ms，控制环就严格跑在 1kHz。
	   为什么必须定周期：原来的微分量是"每次循环算一次"，而主循环比 ADC 快上百倍，
	   相邻两次读到的偏差往往一模一样 -> 微分恒为 0，等于**根本没有阻尼**。
	   纯比例控制在"差分驱动"这种对象上就是无阻尼振荡，直道上必然左右摆。 */
	{
		u16 c = TIM4->CNT;
		if(c < cnt_prev)              /* 计数器回绕 = 过去了一个 PWM 周期 */
		{
			sub++;
			idle = 0;
		}
		else if(++idle > 4000)        /* 兜底：万一时基异常，也不让车彻底不动 */
		{
			sub  = 2;
			idle = 0;
		}
		cnt_prev = c;
		if(sub < 2) return;           /* 不到 1ms：直接返回，PWM 保持上一次的占空比 */
		sub = 0;
	}

	/* ---- 跑够时间就断电停车（开关见文件头 USE_AUTO_STOP）---- */
	run_ms++;                        /* 不停车时它只用来在屏上显示当前秒数 */
#if USE_AUTO_STOP
	if(run_ms >= STOP_MS)
	{
		motor_control(0, 0);
		page_stop(run_ms);          /* 把本次用时打在屏上，不用另外掐表 */
		for(;;)
		{
			motor_control(0, 0);   /* 保持断电；也让编译器不敢优化掉这个死循环 */
		}
	}
#endif

	get_ADC_values();                 //获取电感读数（1kHz 采样，自带一阶低通）

	pwm_error = get_track_error();    //差比和偏差：-1000(车偏左) ~ +1000(车偏右)
	                                  //函数内部已减掉开机自校准的零点
	if(Sign_Flip) pwm_error = -pwm_error;

	if(pwm_error > -Dead_Zone && pwm_error < Dead_Zone) pwm_error = 0;

	/* 真正的微分：先算"每秒变化率"，再低通一次抑制 ADC 噪声，最后乘 d。
	   第一个周期不算微分，避免起步瞬间打一脚舵。 */
	if(first)
	{
		last_e = pwm_error;
		first  = 0;
	}
	der    = (pwm_error - last_e) * 1000;        /* 1kHz -> 乘 1000 得到每秒变化率 */
	last_e = pwm_error;
	if(der >  30000) der =  30000;
	if(der < -30000) der = -30000;

	/* 丢线时偏差会被强制拉到 LOST_ERR(400)，这一下是"假跳变"，
	   微分会算出天文数字再给一脚反舵，把车甩出去。
	   所以：丢线期间、以及刚回线的第一个周期，微分整体冻结。 */
	if(adc_lost || der_hold)
	{
		der_hold = adc_lost ? 1 : 0;
		der      = 0;
		d_filt   = 0.0f;
		last_e   = pwm_error;
	}

	d_filt += 0.18f * ((float)der - d_filt);     /* 微分低通，时间常数约 5.5ms（原 0.12/8ms，高速下拖后腿） */

	pid_Output = (int)(pwm_error * p) + (int)(d_filt * d);

	/* ===== 弯道切内（11-07，从视频里的走线学来的）=====
	   一直对着导线中心走，走的是弯道最外侧那条弧。视频里跑得顺的那台车
	   是贴着弯内侧过的 —— 半径小一点，弧长就短一点，而且不用提高速度。
	   做法：左弯时把转向量减掉固定的一份，车就会稳定停在内侧；右弯反之。
	   CUT_IN 越大越贴内，离边线也越近。 */
	/* 注意下限：切内量不能把转向切成反向。
	   比如 pid 只有 300、CUT 是 600，直接减就变成 -300，车会朝反方向打 —— 直接冲出去。
	   所以切到 0 就停，宁可"少切"也不能"反打"。11-07 加大 CUT 时补的。 */
	if(pid_Output >  100)
	{
		pid_Output -= CUT_IN;
		if(pid_Output < 0) pid_Output = 0;
	}
	if(pid_Output < -100)
	{
		pid_Output += CUT_IN;
		if(pid_Output > 0) pid_Output = 0;
	}



	steer = pid_Output + Steer_Trim;             //Steer_Trim 抵掉机械/电机造成的固定偏
	if(steer >  Steer_Max) steer =  Steer_Max;   //再限一次，防止内侧轮反转
	if(steer < -Steer_Max) steer = -Steer_Max;

	fork_judge();                     //三岔判断（竖直通道没开，目前是空操作）

	/* ============ 转弯自动降速（11-06 20:55 新增） ============
	   为什么必须加：一路 PWM 只有 0~999 的预算，外侧轮 = 速度 + 差速。
	   速度越高，留给差速的空间越小：

	       速度 605、差速 580  ->  外侧轮要 1185，实际被削到 999
	       -> 真正的差速只有 999-605+580 = 974，最大曲率 = 974/605 = 1.61/b
	       而 400 那版是 800/400 = 2.00/b，也就是说提速后转弯能力掉了 20%

	   转弯能力不够时，车不是"转得慢"，而是在最需要转向的一刻增益突然减半
	   -> 打滑、甩出去、直冲出白面。

	   做法：差速越大，基础速度自动降得越多（纯占空比层面，不改任何物理量）：

	       steer=0    -> 速度 605（直道全速）
	       steer=满分 -> 速度降到 speed_min（默认 460）

	   steer 同时被限制在"不超过当前速度"，保证内侧轮 = 速度-差速 >= 0，
	   永远不倒转（倒转在 L298N 上会有明显的停-反转死区，反而更甩）。
	   满舵时：外侧 460+460=920、内侧 0、差速 920 -> 曲率 920/460 = 2.00/b，
	   和最初最稳的 400 版完全一致，但直道仍是 605。            */
	{
		int asteer = (steer > 0) ? steer : -steer;
		int spd    = speed;

		if(Steer_Max > 0 && speed > speed_min)
			spd = speed - (int)((long)asteer * (speed - speed_min) / Steer_Max);

		if(spd < speed_min) spd = speed_min;
		if(spd > speed)     spd = speed;

		/* ===== 直道吃满 PWM：外轮不许撞 999（11-07 新增）=====
		   以前 speed 卡在 665，是为了给转向预留 334 的余量，代价是直道只用了
		   66% 的电压 —— 这才是"提不上速"的真正原因，电机本身没那么弱。
		   现在改成让 spd 自己让路：steer 小的时候 spd 能顶到 speed，
		   steer 一大 spd 自动降到 999-steer，外轮永远不削顶、差速不打折。
		   依据：同济 RaceKing 技术报告，直道用满占空比、弯前再压。 */
		if(spd > 999 - asteer) spd = 999 - asteer;


		if(steer >  spd) steer =  spd;            //内侧轮不反转
		if(steer < -spd) steer = -spd;

		spd_now = spd;                            /* 给下面的故障记录用 */
		motor_control(spd + steer, spd - steer);  //电机控制
	}

	/* ---------- 故障记录 ---------- */
	{
		static u8  was_lost = 0;
		int ae = (pwm_error > 0) ? pwm_error : -pwm_error;

		if(ae > diag_max_e) diag_max_e = ae;

		/* 丢线：既记总时长，也记"发生过几次"和"丢之前偏了多少"。
		   次数多 = 反复丢；只一次且丢之前偏差很小 = 信号突然没了（断线/岔口）。 */
		if(adc_lost)
		{
			diag_lost++;
			if(!was_lost)
			{
				was_lost    = 1;
				diag_lost_n++;
				diag_lost_e = ae;      /* 注意：进入丢线那一拍，pwm_error 还是丢前的值 */
			}
		}
		else
		{
			was_lost = 0;
		}

		/* FULL 原来的写法是跟 Steer_Max 比，但下面"转弯降速"会把 steer 再压到 spd(<=speed)，
		   所以那个条件永远不成立 —— FULL 恒为 0，等于没有这个指示。
		   改成：只要差速被"降速上限"削掉（steer == ±spd），就算这一拍转向用到了顶。
		   这个定义才是现在真正有意义的"能力用尽"。 */
		if(steer >= spd_now || steer <= -spd_now) diag_full++;
	}

#if SHOW_ON_OLED == 1
	/* 调参版：5Hz 刷电位器页（200 个控制周期 = 200ms） */
	if(++show_cnt >= 200)
	{
		show_cnt = 0;
		page1();
	}
#elif SHOW_ON_OLED == 2
	/* 状态版：4Hz 刷实时状态页（250ms）。比调参页稍慢一点，少占控制环 */
	if(++show_cnt >= 250)
	{
		show_cnt = 0;
		page2();
	}
#elif SHOW_ON_OLED == 3
	/* 记录版：跑的时候不用看，停下来看"EMAX / LOST / FULL"三个累计量 */
	if(++show_cnt >= 250)
	{
		show_cnt = 0;
		page3();
	}
#endif
}


//三岔判断
void fork_judge(void)
{

	if((abs)(ADC_right_aclinic-ADC_left_aclinic)<fork_flag)    //此处三岔判断值（fork_flag）需要调整以适应不同赛道以及不同信号发生器
	{
		if(ADC_right_vertical>=fork_flag || ADC_left_vertical >=fork_flag)
		{
			if((abs)(ADC_right_vertical-ADC_left_vertical)<=fork_flag)
			{
				pwm_error = (ADC_right_aclinic+ADC_right_vertical);
			}
		}
	}
	else pwm_error = pwm_error;
}
