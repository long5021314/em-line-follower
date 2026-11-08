#include "adc.h"
#include "string.h"
#define ADC1_DR_Address    ((u32)0x4001244C)

__IO u16 ADC_ConvertedValue[4]={0,0,0,0};  

/*配置采样通道端口 使能GPIO时钟	  设置ADC采样PA0,1,2,3端口信号*/
 void ADC1_GPIO_Config(void)
{ 
	GPIO_InitTypeDef GPIO_InitStructure;    
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;		    //GPIO设置为模拟输入
  GPIO_Init(GPIOA, &GPIO_InitStructure);   
}


/*配置ADC1的工作模式为MDA模式  */
 void ADC1_Mode_Config(void)
{
  DMA_InitTypeDef DMA_InitStructure;
  ADC_InitTypeDef ADC_InitStructure;	
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE); //使能MDA1时钟
	/* DMA channel1 configuration */
	DMA_DeInit(DMA1_Channel1);  //指定DMA通道
  DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1_DR_Address;//设置DMA外设地址
  DMA_InitStructure.DMA_MemoryBaseAddr = (u32)&ADC_ConvertedValue;	//设置DMA内存地址，ADC转换结果直接放入该地址
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC; //外设为设置为数据传输的来源
  DMA_InitStructure.DMA_BufferSize = 4;	//DMA缓冲区设置为4；
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;// 外设地址固定
  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;// 半字
  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;// 半字
  DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;// 循环传输
  DMA_InitStructure.DMA_Priority = DMA_Priority_High;// 指定配置DMA通道的软件优先级
  DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;// 指定DMA通道是否使用内存到内存的传输方式
	
  DMA_Init(DMA1_Channel1, &DMA_InitStructure);
  
  /* Enable DMA channel1 */
  DMA_Cmd(DMA1_Channel1, ENABLE);  //使能DMA通道

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);	//使能ADC1时钟
     
  /* ADC1 configuration */
  ADC_InitStructure.ADC_Mode = ADC_Mode_Independent; //使用独立模式，扫描模式
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;   
  ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;  // 开启连续转换模式，即不停地进行ADC转换
  ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; // 不使用外部触发转换
  ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;           // 采集数据右对齐
  ADC_InitStructure.ADC_NbrOfChannel = 4;  // 只有4个转换通道
  ADC_Init(ADC1, &ADC_InitStructure);
	
	
  /* ADC1 regular channel11 configuration */ 
  ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5); //通道1采样周期55.5个时钟周期
  ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_239Cycles5); //通道2采样周期55.5个时钟周期
	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 3, ADC_SampleTime_239Cycles5); //通道3采样周期55.5个时钟周期
  ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 4, ADC_SampleTime_239Cycles5); //通道4采样周期55.5个时钟周期

  /* Enable ADC1 DMA */
  ADC_DMACmd(ADC1, ENABLE);	 //使能ADC的DMA
  
  /* Enable ADC1 */
  ADC_Cmd(ADC1, ENABLE); //使能ADC1

  /* Enable ADC1 reset calibaration register */   
  ADC_ResetCalibration(ADC1);          // 复位校准寄存器
  /* Check the end of ADC1 reset calibration register */
  while(ADC_GetResetCalibrationStatus(ADC1));   // 等待校准寄存器复位完成

  /* Start ADC1 calibaration */
  ADC_StartCalibration(ADC1);             // ADC校准
  /* Check the end of ADC1 calibration */
  while(ADC_GetCalibrationStatus(ADC1));        // 等待校准完成
     
  /* Start ADC1 Software Conversion */ 
  ADC_SoftwareStartConvCmd(ADC1, ENABLE);  //开始转换
}


//adc1初始化
void ADC1_init(void) 
{
	ADC1_GPIO_Config();
	ADC1_Mode_Config();
}


/*
  获取电感数值
  ADC_right_aclinic      右边水平电感读数
  ADC_right_vertical     右边垂直电感读数
  ADC_left_vertical      左边垂直电感读数
  ADC_left_aclinic       左边水平电感读数
*/
int ADC_right_aclinic =0;
int ADC_right_vertical=0;
int ADC_left_vertical=0;
int ADC_left_aclinic =0;

/* ================== 偏差计算：差比和 + 开机自动零点 ==================
   上一版用的是"先减最小值、再除以量程"：
       norm = (v - MIN) * 1000 / (MAX - MIN)
   思路没错，但它必须先知道每一路的 MAX（即最大读数）。
   MAX 猜大了，偏差被压小、转向几乎没有权限；猜小了又直接饱和。
   车上没做标定时，MAX=3000 只是我猜的，这一步本身就不可靠。

   换成差比和，用比值而不是绝对幅度：

       err = (R - L) / (R + L) * 1000

   好处：满量程天然就是 ±1000，电感灵敏度、运放增益、离地高度怎么变
   都不影响这个比例，不需要标定量程。

   剩下的唯一问题是"左右增益不等"造成的固定偏置（左路弱 -> 车在中线
   时 err 也不是 0）。这个用开机自动零点减掉：上电后前 200 次有效
   采样取平均当零点，之后一直减这个值。
   → 开机时请把车正放在导线中心附近、静止约 1 秒。
   ==================================================================== */
int track_err = 0;      /* 当前偏差：-1000(车偏左) ~ +1000(车偏右) */
int err_zero  = 0;      /* 开机自校准测出的固定偏置 */
int adc_sum   = 0;      /* 左右之和：用来判断有没有信号 */

static long zero_acc  = 0;
static int  zero_cnt  = 0;
static u8   zero_done = 0;
u8 zero_enable = 0;   /* main 确认车放稳后置 1，之前不累加零点 */
static long sum_acc   = 0;
static int  sum_ref   = 0;   /* 开机时（车在导线中线）的左右之和，用来定噪声门限 */

/* ---------- 丢线处理的状态 ----------
   LOST_ERR：判定丢线后强制给出的偏差幅度。
   400 对应 steer ≈ 0.93 x 400 = 372，足以把车拐回线，又不至于直接打死。 */
#define LOST_ERR  400

static u8  lost_cnt = 0;     /* 连续丢线的毫秒数（1kHz 下每周期 +1） */
static int lost_dir = 0;     /* 丢线前最后一次的偏差方向 */
u8 adc_lost = 0;             /* 给 main.c / 状态页用的"当前是否丢线" */

int get_track_error(void)
{
	int l = ADC_left_aclinic;
	int r = ADC_right_aclinic;
	int s = l + r;
	int e;

	adc_sum = s;

	/* 差比和：满量程天然 ±1000，与信号绝对幅度无关 */
	e = (s > 80) ? ((r - l) * 1000 / s) : 0;

	/* ---------- 开机自动零点 ---------- */
	if(!zero_done && zero_enable)
	{
		/* 只有"踩在导线上"的样本才拿来当零点：s 太小说明车不在线上，
		   拿它当零点会把误差固化进去。s>=200 是个保守门槛。
		   ★ 没在导线上就一直等，不再"试够次数就放弃"——
		   以前一旦上电时车被拿在手上，就会放弃零点（err_zero=0、sum_ref=0），
		   等于把"左右增益不等"的固定偏置彻底交还给车，跑起来必然偏一侧。 */
		if(s >= 200)
		{
			zero_acc += e;
			sum_acc  += s;
			zero_cnt++;

			/* 采够 200 次有效样本就收尾。控制环是 1kHz，所以约 200 毫秒。
			   注意：以前这里写"10~20 毫秒"是旧版（主循环跑多快算多快）的注释，早就不对了。 */
			if(zero_cnt >= 200)
			{
				int z = (int)(zero_acc / zero_cnt);
				if(z >  700) z =  700;
				if(z < -700) z = -700;
				err_zero = z;
				sum_ref  = (int)(sum_acc / zero_cnt);
				if(sum_ref < 200) sum_ref = 200;
				zero_done = 1;
			}
		}
	}

	/* 车离导线太远：两路都很小，相除只会放大噪声。
	   门限取开机中线时的 1/6，跟信号强度自动匹配。

	   ★ 11-06 20:55 的教训：这里原来是 track_err = 0。
	   0 的意思是"我很正，直着走"——所以车一旦被推到白面边缘、两路信号同时变弱，
	   程序就认为它居中，于是不打方向，顺着当时的朝向一路直冲出赛道。
	   视频里 1.96~2.32 秒车身角度完全冻结、直直冲下白面，就是这个造成的。

	   现在改成：保持上一次的偏差方向，并把幅度抬到 LOST_ERR，
	   让车拐回去找线，而不是直行。连续丢 5ms 才认，防单点噪声。 */
	if(zero_done && sum_ref > 0 && s < sum_ref / 6)
	{
		if(lost_cnt < 250) lost_cnt++;
		adc_lost = 1;
		if(lost_cnt >= 5)
		{
			int d = (track_err > 0) ? 1 : ((track_err < 0) ? -1 : lost_dir);
			if(d != 0)
			{
				lost_dir  = d;
				track_err = d * LOST_ERR;
			}
		}
		return track_err;
	}
	lost_cnt = 0;
	adc_lost = 0;

	e -= err_zero;
	if(e >  1000) e =  1000;
	if(e < -1000) e = -1000;

	track_err = e;
	return e;
}

/* 零点是否已经采完。给 main() 用：上电后要等车静止采满零点，再让电机转 */
u8 zero_ready(void)
{
	return zero_done;
}

/* 一阶低通 y(n)=0.6x(n)+0.4y(n-1)
   系数由 0.8 改到 0.6：前端检波后已经是 5ms RC 平滑过的直流，软件这层再重滤就是纯延迟。
   时间常数从约 4.3ms 降到约 2.0ms —— 车越快，同样的延迟换成"多跑了多少毫米"就越大，
   入弯就会晚一步，表现为慢慢往外漂。所以提速时先削这里，而不是加阻尼。
   原来 output 是局部变量、每次进函数都归零，等于没滤波；
   但也不能简单加 static —— 那样左右两路会共用同一份状态、互相污染。
   所以左右各自留一份。 */
static float L_filter = 0.0f, R_filter = 0.0f;
static u8    filter_ready = 0;

void get_ADC_values(void)
{
	if(!filter_ready)
	{
		L_filter = (float)ADC_ConvertedValue[0];
		R_filter = (float)ADC_ConvertedValue[3];
		filter_ready = 1;
	}

	L_filter = 0.6f * (float)ADC_ConvertedValue[0] + 0.4f * L_filter;
	R_filter = 0.6f * (float)ADC_ConvertedValue[3] + 0.4f * R_filter;

	ADC_left_aclinic   = (int)L_filter;      /* PA0 左水平 */
	ADC_right_aclinic  = (int)R_filter;      /* PA3 右水平 */

	/* 竖直通道暂时不用（做三岔时再放开，注意右竖直是 [2] 不是 [3]） */
	ADC_left_vertical  = 0;
	ADC_right_vertical = 0;
}

/* 旧接口，保留但已不再用于左右水平通道 */
float first_order_low_pass_filter(char *type, float k, float number)
{
	float output = 0.0f;
	output = k*number + (1-k)*output;
	if(strcmp(type, "float") == 0) return (float)(output);
	else return (int)(output);
}


