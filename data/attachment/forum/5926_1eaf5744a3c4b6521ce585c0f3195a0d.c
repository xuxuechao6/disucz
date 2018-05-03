#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>

#include "board.h"
#include "wm8950.h"

#define WM_Address 0x1A //0x34(01 1010)
//#define RT_USING_STATIC_BUFF


/* CODEC config */
#define CODEC_MASTER_MODE       0 /* 0: mcu-master, 1: codec-master. */

/* CODEC PLL config */
/* MCLK : 由MCU输出，不同采样率是频率不同 */

#define PLL_N_20480            (8) 
#define PLL_K_20480            (416349)

#define PLL_N_40960            (8)
#define PLL_K_40960            (416349)

#define PLL_N_56448            (8)
#define PLL_K_56448            (416349)

#define PLL_N_81920            (8)
#define PLL_K_81920            (416349)

//#define PLL_N_112896          (7 | PLLPRESCALE)
#define PLL_N_112896            (7)
#define PLL_K_112896            (6254546)
//#define PLL_N_122880          (8 | PLLPRESCALE)
#define PLL_N_122880            (8)
#define PLL_K_122880            (416349)

/* CODEC port define */
#define CODEC_I2S_PORT          SPI2
#define CODEC_I2S_IRQ           SPI2_IRQn

/* I2S ADC DMA Stream definitions */
//#define AUDIO_ADC_I2S_DMA_CLOCK     RCC_AHB1Periph_DMA1
#define AUDIO_ADC_I2S_DMA_STREAM    DMA1_Stream3				// dgu change: SPI2 RX using stream 3, channel 0
#define AUDIO_ADC_I2S_DMA_CHANNEL   DMA_Channel_0
#define AUDIO_ADC_I2S_DMA_IRQ       DMA1_Stream3_IRQn
#define AUDIO_ADC_I2S_DMA_IT_TC     DMA_IT_TCIF3				//dgu change: Stream 3, Transfer Complete

/* I2S DAC DMA Stream definitions */
//#define AUDIO_DAC_I2S_DMA_CLOCK     RCC_AHB1Periph_DMA1
#define AUDIO_DAC_I2S_DMA_STREAM    DMA1_Stream4				// dgu change: SPI2 TX using stream 4, channel 0
#define AUDIO_DAC_I2S_DMA_CHANNEL   DMA_Channel_0
#define AUDIO_DAC_I2S_DMA_IRQ       DMA1_Stream4_IRQn
#define AUDIO_DAC_I2S_DMA_IT_TC     DMA_IT_TCIF4				//dgu change: Stream 4, Transfer Complete

void vol(uint16_t v);
static void codec_send(rt_uint16_t s_data);

#define DATA_NODE_MAX 5
/* data node for Tx Mode */
struct codec_data_node
{
    rt_uint16_t *data_ptr;
    rt_size_t  data_size;
};

struct codec_device
{
    /* inherit from rt_device */
    struct rt_device parent;

	/* adc pcm data list */
    struct codec_data_node adc_data_list[DATA_NODE_MAX];
    rt_uint16_t adc_read_index, adc_put_index;

    /* dac pcm data list */
    struct codec_data_node data_list[DATA_NODE_MAX];
    rt_uint16_t read_index, put_index;

    /* i2c mode */
    struct rt_i2c_bus_device * i2c_device;
};
struct codec_device codec;

#if CODEC_MASTER_MODE
/* WM8950芯片为Master模式，即帧时钟和样本时钟为输出*/
static uint16_t r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | MS ;
#else
/* WM8950芯片为Slave模式，即帧时钟和样本时钟为输入*/
static uint16_t r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8;
#endif


/* CODEC ADC FRAME Buffer*/
#define CODEC_ADC_FRAME_SZ		1024
#define CODEC_ADC_MEMOEY_SZ 	(DATA_NODE_MAX * (CODEC_ADC_FRAME_SZ + 4))

#ifdef RT_USING_STATIC_BUFF
static rt_uint8_t codec_adc_input_memory[DATA_NODE_MAX][CODEC_ADC_FRAME_SZ + 4];
#else
static rt_uint8_t *codec_adc_input_memory = RT_NULL;
#endif

#if !CODEC_MASTER_MODE
static int codec_sr_new = 0;
#endif

rt_err_t sample_rate(int sr);


/* 中断资源初始化*/
static void NVIC_Configuration(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* ADC DMA IRQ Channel configuration */
    NVIC_InitStructure.NVIC_IRQChannel = AUDIO_ADC_I2S_DMA_IRQ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

   	/* DAC DMA IRQ Channel configuration */
    NVIC_InitStructure.NVIC_IRQChannel = AUDIO_DAC_I2S_DMA_IRQ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*
I2S2S_WS    PI0
I2S2_CK     PI1
I2S2ext_SD  PI2
I2S2_SD     PI3
I2S2_MCK    PC6
*/
static void GPIO_Configuration(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;

    /* Connect pins to I2S peripheral  */
    GPIO_PinAFConfig(GPIOI, GPIO_PinSource0, GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOI, GPIO_PinSource1, GPIO_AF_SPI2);
    //GPIO_PinAFConfig(GPIOI, GPIO_PinSource2, GPIO_AF_I2S3ext);
    GPIO_PinAFConfig(GPIOI, GPIO_PinSource3, GPIO_AF_SPI2);

    /* I2S2S_WS */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOI, &GPIO_InitStructure);
    
    /* I2S2_CK, I2S2_SD, I2S2ext_SD */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOI, &GPIO_InitStructure);
	
    /* I2S2_MCK: PC6*/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_SPI2);
    GPIO_Init(GPIOC, &GPIO_InitStructure);

}
/* I2S RX DMA: Stream3, Channel 0*/
static void ADC_DMA_Configuration(rt_uint32_t addr, rt_size_t size)
{
    DMA_InitTypeDef DMA_InitStructure;

    /* Configure the DMA Stream */
    DMA_Cmd(AUDIO_ADC_I2S_DMA_STREAM, DISABLE);
    DMA_DeInit(AUDIO_ADC_I2S_DMA_STREAM);

    /* Set the parameters to be configured */
    DMA_InitStructure.DMA_Channel = AUDIO_ADC_I2S_DMA_CHANNEL;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&CODEC_I2S_PORT->DR);
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)addr;      /* This field will be configured in record function */
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize = (uint32_t)size;      /* This field will be configured in record function */
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(AUDIO_ADC_I2S_DMA_STREAM, &DMA_InitStructure);

    /* Enable SPI DMA Tx request */
    //SPI_I2S_DMACmd(CODEC_I2S_PORT, SPI_I2S_DMAReq_Tx, ENABLE);
    /* Enable SPI DMA Rx request */
    SPI_I2S_DMACmd(CODEC_I2S_PORT, SPI_I2S_DMAReq_Rx, ENABLE);

    DMA_ITConfig(AUDIO_ADC_I2S_DMA_STREAM, DMA_IT_TC, ENABLE);
    DMA_Cmd(AUDIO_ADC_I2S_DMA_STREAM, ENABLE);
}

static void DAC_DMA_Configuration(rt_uint32_t addr, rt_size_t size)
{
    DMA_InitTypeDef DMA_InitStructure;

    /* Configure the DMA Stream */
    DMA_Cmd(AUDIO_DAC_I2S_DMA_STREAM, DISABLE);
    DMA_DeInit(AUDIO_DAC_I2S_DMA_STREAM);

    /* Set the parameters to be configured */
    DMA_InitStructure.DMA_Channel = AUDIO_DAC_I2S_DMA_CHANNEL;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&CODEC_I2S_PORT->DR);
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)addr;      /* This field will be configured in play function */
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_BufferSize = (uint32_t)size;      /* This field will be configured in play function */
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(AUDIO_DAC_I2S_DMA_STREAM, &DMA_InitStructure);

    /* Enable SPI DMA Tx request */
    SPI_I2S_DMACmd(CODEC_I2S_PORT, SPI_I2S_DMAReq_Tx, ENABLE);

    DMA_ITConfig(AUDIO_DAC_I2S_DMA_STREAM, DMA_IT_TC, ENABLE);
    DMA_Cmd(AUDIO_DAC_I2S_DMA_STREAM, ENABLE);
}

static void I2S_Configuration(uint32_t I2S_AudioFreq)
{
    I2S_InitTypeDef I2S_InitStructure;

    /* I2S peripheral configuration */
    I2S_InitStructure.I2S_Standard = I2S_Standard_Phillips;
    I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_16b;
    //I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Enable;
    I2S_InitStructure.I2S_AudioFreq = I2S_AudioFreq;
    //I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;
    I2S_InitStructure.I2S_CPOL = I2S_CPOL_High;
    /* I2S2 configuration */
#if CODEC_MASTER_MODE
    //I2S_InitStructure.I2S_Mode = I2S_Mode_SlaveTx;
    I2S_InitStructure.I2S_Mode = I2S_Mode_SlaveRx;
#else
    //I2S_InitStructure.I2S_Mode = I2S_Mode_MasterTx;
    I2S_InitStructure.I2S_Mode = I2S_Mode_MasterRx;
#endif
    I2S_Init(CODEC_I2S_PORT, &I2S_InitStructure);
}

/* read register value from wm8950, */ 
/*
static void codec_recv()
{
	
}
*/
/* write value into wm8950 register */
static void codec_send(rt_uint16_t s_data)
{
    struct rt_i2c_msg msg;
    rt_uint8_t send_buffer[2];

    RT_ASSERT(codec.i2c_device != RT_NULL);

    send_buffer[0] = (rt_uint8_t)(s_data>>8);
    send_buffer[1] = (rt_uint8_t)(s_data);

    msg.addr  = WM_Address;	
    msg.flags = RT_I2C_WR;
    msg.len   = 2;
    msg.buf   = send_buffer;

    rt_i2c_transfer(codec.i2c_device, &msg, 1);
}

/* init register value in wm8950, called when first open*/
static rt_err_t codec_init(rt_device_t dev)
{
    // R0
    codec_send(REG_SOFTWARE_RESET);

    // R1
    // Set BUFIOEN=1 in register R1
    // Set VMIDSEL[1:0] to required value in register R1.
    // Set MICEN = 1 in register R1
    // Set BIASEN = 1 in register R1
    codec_send(REG_POWER_MANAGEMENT1 | PLLEN | MICBEN | BIASEN | BUFIOEN | VMIDSEL_5K);
		
    // R2
    // Enable other mixers as required 
    // Enable other outputs as required.
    codec_send(REG_POWER_MANAGEMENT2 | BOOSTEN | INPPGAEN | ADCEN);

    // R4
    // Digital inferface setup.
    codec_send(REG_AUDIO_INTERFACE | BCP_NORMAL | FRAMEP_NORMAL | WL_16BITS | FMT_I2S);

    // PLL setup. (MCLK: 12.2896 for 44.1K)
    codec_send(REG_PLL_N  | PLL_N_112896);
    codec_send(REG_PLL_K1 | ((PLL_K_112896>>18) & 0x1F));
    codec_send(REG_PLL_K2 | ((PLL_K_112896>>9) & 0x1FF));
    codec_send(REG_PLL_K3 | ((PLL_K_112896>>0) & 0x1FF));
    
    // R45
    // INPPGAVOL = 35.5dB
    codec_send(REG_LEFT_PGA_GAIN | 0x3F);
    codec_send(r06);
    
    /* 设置采样率 */
    sample_rate(codec_sr_new);

	return RT_EOK;
}

void vol(uint16_t v) // 0~99
{
    v = 63*v/100;
    //v = (v & VOL_MASK) << VOL_POS;
    //codec_send(REG_LOUT1_VOL | v);
    //codec_send(REG_ROUT1_VOL | HPVU | v);
    //codec_send(REG_LOUT2_VOL | v);
    //codec_send(REG_ROUT2_VOL | SPKVU | v);
}

void eq(codec_eq_args_t args)
{
    switch (args->channel)
    {
    case 1:
        codec_send(REG_EQ1 | ((args->frequency & EQC_MASK) << EQC_POS) | ((args->gain & EQG_MASK) << EQG_POS));
        break;

    case 2:
        codec_send(REG_EQ2 | ((args->frequency & EQC_MASK) << EQC_POS) | ((args->gain & EQG_MASK) << EQG_POS) | (args->mode_bandwidth ? EQ2BW_WIDE : EQ2BW_NARROW));
        break;

    case 3:
        codec_send(REG_EQ3 | ((args->frequency & EQC_MASK) << EQC_POS) | ((args->gain & EQG_MASK) << EQG_POS) | (args->mode_bandwidth ? EQ3BW_WIDE : EQ3BW_NARROW));
        break;

    case 4:
        codec_send(REG_EQ4 | ((args->frequency & EQC_MASK) << EQC_POS) | ((args->gain & EQG_MASK) << EQG_POS) | (args->mode_bandwidth ? EQ4BW_WIDE : EQ4BW_NARROW));
        break;

    case 5:
        codec_send(REG_EQ5 | ((args->frequency & EQC_MASK) << EQC_POS) | ((args->gain & EQG_MASK) << EQG_POS));
        break;
    }
}

// TODO eq1() ~ eq5() are just for testing. To be removed.
void eq1(uint8_t freq, uint8_t gain, uint8_t mode)
{
    codec_send(REG_EQ1 | ((freq & EQC_MASK) << EQC_POS) | ((gain & EQG_MASK) << EQG_POS));
}

void eq2(uint8_t freq, uint8_t gain, uint8_t bw)
{
    codec_send(REG_EQ2 | ((freq & EQC_MASK) << EQC_POS) | ((gain & EQG_MASK) << EQG_POS) | (bw ? EQ2BW_WIDE : EQ2BW_NARROW));
}

void eq3(uint8_t freq, uint8_t gain, uint8_t bw)
{
    codec_send(REG_EQ3 | ((freq & EQC_MASK) << EQC_POS) | ((gain & EQG_MASK) << EQG_POS) | (bw ? EQ3BW_WIDE : EQ3BW_NARROW));
}

void eq4(uint8_t freq, uint8_t gain, uint8_t bw)
{
    codec_send(REG_EQ4 | ((freq & EQC_MASK) << EQC_POS) | ((gain & EQG_MASK) << EQG_POS) | (bw ? EQ4BW_WIDE : EQ4BW_NARROW));
}

void eq5(uint8_t freq, uint8_t gain)
{
    codec_send(REG_EQ2 | ((freq & EQC_MASK) << EQC_POS) | ((gain & EQG_MASK) << EQG_POS));
}

void eq3d(uint8_t depth)
{
    //codec_send(REG_3D | ((depth & DEPTH3D_MASK) << DEPTH3D_POS));
}

rt_err_t sample_rate(int sr)
{
    uint16_t r07 = REG_ADDITIONAL;
    uint32_t PLL_N, PLL_K;

    switch (sr)
    {
    case 8000: // MCLK : 2.048 
        PLL_N = PLL_N_20480;
        PLL_K = PLL_K_20480;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_8KHZ;
        break;
/*
    case 11025: // MCLK : 11.2896 
        PLL_N = PLL_N_112896;
        PLL_K = PLL_K_112896;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_12KHZ;
        break;
*/
#if CODEC_MASTER_MODE
    case 12000: // MCLK : 12.288
        PLL_N = PLL_N_122880;
        PLL_K = PLL_K_122880;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_12KHZ;
        break;
#endif

    case 16000: // MCLK : 4.096
        PLL_N = PLL_N_40960;
        PLL_K = PLL_K_40960;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_16KHZ;
        break;

    case 22050: // MCLK : 5.6448
        PLL_N = PLL_N_56448;
        PLL_K = PLL_K_56448;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_24KHZ;
        break;
/*
    case 24000: // MCLK : 12.288 
        PLL_N = PLL_N_122880;
        PLL_K = PLL_K_122880;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_24KHZ;
        break;
*/
    case 32000: // MCLK : 8.192
        PLL_N = PLL_N_81920;
        PLL_K = PLL_K_81920;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_32KHZ;
        break;

    case 44100: // MCLK : 11.2896
        PLL_N = PLL_N_112896;
        PLL_K = PLL_K_112896;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_48KHZ;
        break;

    case 48000: // MCLK : 12.288
        PLL_N = PLL_N_122880;
        PLL_K = PLL_K_122880;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_48KHZ;
        break;
/*
    // Warning: in WM8978 datasheet, sample rate 96K doesn't support.
    case 96000: // MCLK : 12.288
        PLL_N = PLL_N_122880;
        PLL_K = PLL_K_122880;
        r06 = REG_CLOCK_GEN | CLKSEL_PLL | MCLK_DIV1 | BCLK_DIV8 | (r06 & MS);
        r07 |= SR_48KHZ;
        break;
*/
    default:
        return RT_ERROR;
    }
    // PLL setup. (MCLK: 12.2896 for 44.1K)
    codec_send(REG_PLL_N  | PLL_N);
    codec_send(REG_PLL_K1 | ((PLL_K>>18) & 0x1F));
    codec_send(REG_PLL_K2 | ((PLL_K>>9) & 0x1FF));
    codec_send(REG_PLL_K3 | ((PLL_K>>0) & 0x1FF));

    codec_send(r06);
    codec_send(r07);

#if !CODEC_MASTER_MODE
    codec_sr_new = sr;
#endif

    return RT_EOK;
}

/* Exported functions */
#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(vol, Set volume);
FINSH_FUNCTION_EXPORT(eq1, Set EQ1(Cut-off, Gain, Mode));
FINSH_FUNCTION_EXPORT(eq2, Set EQ2(Center, Gain, Bandwidth));
FINSH_FUNCTION_EXPORT(eq3, Set EQ3(Center, Gain, Bandwidth));
FINSH_FUNCTION_EXPORT(eq4, Set EQ4(Center, Gain, Bandwidth));
FINSH_FUNCTION_EXPORT(eq5, Set EQ5(Cut-off, Gain));
//FINSH_FUNCTION_EXPORT(eq3d, Set 3D(Depth));
FINSH_FUNCTION_EXPORT(sample_rate, Set sample rate);
#endif

/* be called when open 'snd' device */
static rt_err_t codec_open(rt_device_t dev, rt_uint16_t oflag)
{
    struct codec_device* device;
    struct codec_data_node* node;
    int index = 0;
	

#ifndef RT_USING_STATIC_BUFF    
    /* malloc ADC memory for codec */
    codec_adc_input_memory = (rt_uint8_t*) rt_malloc (CODEC_ADC_MEMOEY_SZ);
    if(codec_adc_input_memory == RT_NULL)
    {
        rt_kprintf("rt_malloc() failed, no enough memory!\n");
        return RT_ENOMEM;
    }
#endif
	
    device = (struct codec_device*) dev;
    RT_ASSERT(device != RT_NULL);
	
    for(index = 0; index < DATA_NODE_MAX; index++)
    {
        node = &device->adc_data_list[index];
        /* set node attribute */
        node->data_ptr = (rt_uint16_t*) codec_adc_input_memory + index * (CODEC_ADC_FRAME_SZ + 4);
        node->data_size = CODEC_ADC_FRAME_SZ; /* size is byte unit, convert to half word unit */
    }
    /* Start ADC DMA for low-level PCM input*/
    ADC_DMA_Configuration((rt_uint32_t)codec_adc_input_memory, CODEC_ADC_FRAME_SZ/2);

    
#if !CODEC_MASTER_MODE
    /* enable I2S */
    I2S_Cmd(CODEC_I2S_PORT, ENABLE);
#endif
		
    return RT_EOK;
}

/* be called when close 'snd' device */
static rt_err_t codec_close(rt_device_t dev)
{
    {
        /* Stop ADC DMA */
        NVIC_DisableIRQ(AUDIO_ADC_I2S_DMA_IRQ);
        DMA_Cmd(AUDIO_ADC_I2S_DMA_STREAM, DISABLE);
        /* Clear DMA Stream Transfer Complete interrupt pending bit */
        DMA_ClearITPendingBit(AUDIO_ADC_I2S_DMA_STREAM, AUDIO_ADC_I2S_DMA_IT_TC);
#ifndef RT_USING_STATIC_BUFF    
        /* release ADC memory for codec*/
        if(codec_adc_input_memory != RT_NULL)
        {
            rt_free(codec_adc_input_memory);
            codec_adc_input_memory = RT_NULL;
        }
#endif        
    }
		
#if CODEC_MASTER_MODE
    if (r06 & MS)
    {
				/* Stop DAC DMA */
        NVIC_DisableIRQ(AUDIO_DAC_I2S_DMA_IRQ);
        DMA_Cmd(AUDIO_DAC_I2S_DMA_STREAM, DISABLE);
        /* Clear DMA Stream Transfer Complete interrupt pending bit */
        DMA_ClearITPendingBit(AUDIO_DAC_I2S_DMA_STREAM, AUDIO_DAC_I2S_DMA_IT_TC);

        while ((CODEC_I2S_PORT->SR & SPI_I2S_FLAG_TXE) == 0);
        while ((CODEC_I2S_PORT->SR & SPI_I2S_FLAG_BSY) != 0);
        I2S_Cmd(CODEC_I2S_PORT, DISABLE);

        r06 &= ~MS;
        codec_send(r06);

        /* remove all data node */
        if (codec.parent.tx_complete != RT_NULL)
        {
            rt_base_t level = rt_hw_interrupt_disable();

            while (codec.read_index != codec.put_index)
            {
                codec.parent.tx_complete(&codec.parent, codec.data_list[codec.read_index].data_ptr);
                codec.read_index++;
                if (codec.read_index >= DATA_NODE_MAX)
                {
                    codec.read_index = 0;
                }
            }

            rt_hw_interrupt_enable(level);
        }
    }
#else
    /* disable I2S */
    I2S_Cmd(CODEC_I2S_PORT, DISABLE);
#endif

    return RT_EOK;
}

/* be called when ioctrol */
static rt_err_t codec_control(rt_device_t dev, rt_uint8_t cmd, void *args)
{
    rt_err_t result = RT_EOK;

    switch (cmd)
    {
    case CODEC_CMD_RESET:
        result = codec_init(dev);
        break;

    case CODEC_CMD_VOLUME:
        vol(*((uint16_t*) args));
        break;

    case CODEC_CMD_SAMPLERATE:
        result = sample_rate(*((int*) args));
        break;

    case CODEC_CMD_EQ:
        eq((codec_eq_args_t) args);
        break;

    case CODEC_CMD_3D:
        eq3d(*((uint8_t*) args));
        break;

    default:
        result = RT_ERROR;
    }
    return result;
}

/* read pcm from codec in notify call-back */
static rt_size_t codec_read(rt_device_t dev, rt_off_t pos,
                            void* buffer, rt_size_t size)
{
    struct codec_device* device;
    struct codec_data_node* node;
    rt_uint32_t level;
    rt_uint16_t next_index;

    device = (struct codec_device*) dev;
    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(buffer != RT_NULL);
    
    level = rt_hw_interrupt_disable();

    next_index = device->adc_read_index + 1;
    if (next_index >= DATA_NODE_MAX)
        next_index = 0;
    
    /* check data_list empty */
    if (next_index == device->adc_put_index)
    {
        rt_set_errno(-RT_EEMPTY);
        rt_hw_interrupt_enable(level);
        return 0;
    }
    
    node = &device->adc_data_list[device->adc_read_index];
    
    device->adc_read_index = next_index;
    
    if(size < node->data_size)
    {
        memcpy(buffer, node->data_ptr, size);
        rt_hw_interrupt_enable(level);
        return (size >> 1);  //样本个数
    }
    else{
        memcpy(buffer, node->data_ptr, node->data_size);
        rt_hw_interrupt_enable(level);
        return ((node->data_size) >> 1);  //样本个数
    }
    
}

/* write pcm into codec directly using DMA */
static rt_size_t codec_write(rt_device_t dev, rt_off_t pos,
                             const void* buffer, rt_size_t size)
{
    struct codec_device* device;
    struct codec_data_node* node;
    rt_uint32_t level;
    rt_uint16_t next_index;

    device = (struct codec_device*) dev;
    RT_ASSERT(device != RT_NULL);

    next_index = device->put_index + 1;
    if (next_index >= DATA_NODE_MAX)
        next_index = 0;

    /* check data_list full */
    if (next_index == device->read_index)
    {
        rt_set_errno(-RT_EFULL);
        return 0;
    }

    level = rt_hw_interrupt_disable();
    node = &device->data_list[device->put_index];
    device->put_index = next_index;

    /* set node attribute */
    node->data_ptr = (rt_uint16_t*) buffer;
    node->data_size = size >> 1; /* size is byte unit, convert to half word unit */

    next_index = device->read_index + 1;
    if (next_index >= DATA_NODE_MAX)
        next_index = 0;

    /* check data list whether is empty */
    if (next_index == device->put_index)
    {
#if CODEC_MASTER_MODE
        codec_send(r06 & ~MS);
        I2S_Cmd(CODEC_I2S_PORT, DISABLE);
#endif

        NVIC_EnableIRQ(AUDIO_DAC_I2S_DMA_IRQ);
        DAC_DMA_Configuration((rt_uint32_t) node->data_ptr, node->data_size);

#if CODEC_MASTER_MODE
        if ((r06 & MS) == 0)
        {
            I2S_Cmd(CODEC_I2S_PORT, ENABLE);
            r06 |= MS;
            codec_send(r06);
        }
#endif
    }
    rt_hw_interrupt_enable(level);

    return size;
}

void RCC_CLK_Configuration(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOI |
                           RCC_AHB1Periph_GPIOC, ENABLE);
    /* Enable the CODEC_I2S peripheral clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);	//??
}

void RCC_PLL_Configuration(int samplerate)
{
    uint32_t I2S_AudioFreq;
    
    /* 设置PLLI2S锁相环的参数，倍数为N=192， 除数R=3，这样I2S模块的时钟为64MHz*/
    //RCC_PLLI2SConfig(192, 3);
    switch(samplerate)
    {
    case 8000:
        I2S_AudioFreq = I2S_AudioFreq_8k;   // MCLK : 2.048MHz
        RCC_PLLI2SConfig(256, 5);       // 采样率不同，这个PLLI2S的系数不一样，需要重新配置；
        break;
    case 16000:
        I2S_AudioFreq = I2S_AudioFreq_16k;  // MCLK : 4.096MHz
        RCC_PLLI2SConfig(213, 2);       // 采样率不同，这个PLLI2S的系数不一样，需要重新配置；
        break;
    case 22050:
        I2S_AudioFreq = I2S_AudioFreq_22k;  // MCLK : 5.644MHz
        RCC_PLLI2SConfig(429, 4);       // 采样率不同，这个PLLI2S的系数不一样，需要重新配置；
        break;
    case 32000:                             // MCLK : 8.192MHz
        I2S_AudioFreq = I2S_AudioFreq_32k;
        RCC_PLLI2SConfig(213, 2);       // 采样率不同，这个PLLI2S的系数不一样，需要重新配置；
        break;
    case 44100:                             //  MCLK : 11.289MHz
        I2S_AudioFreq = I2S_AudioFreq_44k;
        RCC_PLLI2SConfig(271, 2);       // 采样率不同，这个PLLI2S的系数不一样，需要重新配置；
        break;
    case 48000:                             //  MCLK : 12.288MHz
    default:
        I2S_AudioFreq = I2S_AudioFreq_48k;
        RCC_PLLI2SConfig(258, 3);       // 采样率不同，这个PLLI2S的系数不一样，需要重新配置；
        break;
    }
    /* 配置选择使用PLLI2S作为时钟输入源*/
    RCC_I2SCLKConfig(RCC_I2S2CLKSource_PLLI2S);
    /* 使能PLLI2S锁相环，产生时钟*/
    RCC_PLLI2SCmd(ENABLE);
    
    
    /* 设置采样率*/
    I2S_Configuration(I2S_AudioFreq);
    
}

rt_err_t codec_hw_init(const char * i2c_bus_device_name)
{
    struct rt_i2c_bus_device * i2c_device;

    i2c_device = rt_i2c_bus_device_find(i2c_bus_device_name);
    if(i2c_device == RT_NULL)
    {
        rt_kprintf("i2c bus device %s not found!\r\n", i2c_bus_device_name);
        return -RT_ENOSYS;
    }
    codec.i2c_device = i2c_device;
    
    /* 所用外设资源时钟使能*/
    RCC_CLK_Configuration();
    
    /* 配置I2S外设的采样*/
    RCC_PLL_Configuration(8000);
    codec_sr_new = 8000;
    
    /* 配置中断和端口映射*/
    NVIC_Configuration();
    GPIO_Configuration();

    codec.parent.type        = RT_Device_Class_Sound;
    codec.parent.rx_indicate = RT_NULL;
    codec.parent.tx_complete = RT_NULL;
    codec.parent.user_data   = RT_NULL;

    codec.parent.control = codec_control;
    codec.parent.init    = codec_init;
    codec.parent.open    = codec_open;
    codec.parent.close   = codec_close;
    codec.parent.read    = codec_read;
    codec.parent.write   = codec_write;

    /* set read_index and put index to 0 */
    codec.read_index = 0;
    codec.put_index = 0;

    /* register the device */
    return rt_device_register(&codec.parent, "snd", RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_RX);
}

static void codec_dac_dma_isr(void)
{
    /* switch to next buffer */
    rt_uint16_t next_index;
    void* data_ptr;

    /* enter interrupt */
    rt_interrupt_enter();

    next_index = codec.read_index + 1;
    if (next_index >= DATA_NODE_MAX)
        next_index = 0;

    /* save current data pointer */
    data_ptr = codec.data_list[codec.read_index].data_ptr;

#if !CODEC_MASTER_MODE
    if (codec_sr_new)
    {
        I2S_Configuration(codec_sr_new);
        I2S_Cmd(CODEC_I2S_PORT, ENABLE);
        codec_sr_new = 0;
    }
#endif

    codec.read_index = next_index;
    if (next_index != codec.put_index)
    {
        /* enable next dma request */
        DAC_DMA_Configuration((rt_uint32_t)codec.data_list[codec.read_index].data_ptr,
                          codec.data_list[codec.read_index].data_size);

#if CODEC_MASTER_MODE
        if ((r06 & MS) == 0)
        {
//            CODEC_I2S_PORT->I2SCFGR |= SPI_I2SCFGR_I2SE;
            r06 |= MS;
//            codec_send(r06);
        }
#endif
    }
    else /* codec tx done. */
    {
#if CODEC_MASTER_MODE
        if (r06 & MS)
        {
            DMA_Cmd(AUDIO_DAC_I2S_DMA_STREAM, DISABLE);

            while ((CODEC_I2S_PORT->SR & SPI_I2S_FLAG_TXE) == 0);
            while ((CODEC_I2S_PORT->SR & SPI_I2S_FLAG_BSY) != 0);
            I2S_Cmd(CODEC_I2S_PORT, DISABLE);

            r06 &= ~MS;
//            codec_send(r06);
        }
#endif

        rt_kprintf("*\n");
    } /* codec tx done. */

    /* notify transmitted complete. */
    if (codec.parent.tx_complete != RT_NULL)
    {
        codec.parent.tx_complete(&codec.parent, data_ptr);
    }

    /* leave interrupt */
    rt_interrupt_leave();
}

/* DAC DMA Stream 4 Interrupt Server */ 
void DMA1_Stream4_IRQHandler(void)
{
    /* Test on DMA Stream Transfer Complete interrupt */
    if(DMA_GetITStatus(AUDIO_DAC_I2S_DMA_STREAM, AUDIO_DAC_I2S_DMA_IT_TC))
    {
        /* Clear DMA Stream Transfer Complete interrupt pending bit */
        DMA_ClearITPendingBit(AUDIO_DAC_I2S_DMA_STREAM, AUDIO_DAC_I2S_DMA_IT_TC);

        /* do something */
        codec_dac_dma_isr();
    }
}

//static int led_cnt = 0;

static void codec_adc_dma_isr(void)
{
    /* switch to next buffer */
    rt_uint16_t next_index;
    //void* data_ptr;
/*    
    led_cnt++;
    if(led_cnt & Bit_SET)
    {
        GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_SET);
    }
    else
    {
        GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_RESET);
    }
*/    
    /* enter interrupt */
    rt_interrupt_enter();
	
    next_index = codec.adc_put_index + 1;
    if (next_index >= DATA_NODE_MAX)
        next_index = 0;

    /* save current data pointer */
    //data_ptr = codec.adc_data_list[codec.adc_put_index].data_ptr;

#if !CODEC_MASTER_MODE
    if (codec_sr_new)
    {
        I2S_Configuration(codec_sr_new);
        I2S_Cmd(CODEC_I2S_PORT, ENABLE);
        codec_sr_new = 0;
    }
#endif

    codec.adc_put_index = next_index;
    /* enable next dma request */
    ADC_DMA_Configuration((rt_uint32_t)codec.adc_data_list[codec.adc_put_index].data_ptr,
                           codec.adc_data_list[codec.adc_put_index].data_size/2);
    if (next_index != codec.adc_read_index)
    {
#if CODEC_MASTER_MODE
        if ((r06 & MS) == 0)
        {
//          CODEC_I2S_PORT->I2SCFGR |= SPI_I2SCFGR_I2SE;
            r06 |= MS;
//          codec_send(r06);
        }
#endif
    }
    else /* codec rx put_index has arrived to read_index. */
    {
#if CODEC_MASTER_MODE
        if (r06 & MS)
        {
/*			
            DMA_Cmd(AUDIO_DAC_I2S_DMA_STREAM, DISABLE);
            while ((CODEC_I2S_PORT->SR & SPI_I2S_FLAG_TXE) == 0);
            while ((CODEC_I2S_PORT->SR & SPI_I2S_FLAG_BSY) != 0);
            I2S_Cmd(CODEC_I2S_PORT, DISABLE);

            r06 &= ~MS;
//          codec_send(r06);
*/
            // just increase adc_read_index for over-write the buffer
            next_index = codec.adc_read_index + 1;
            if (next_index >= DATA_NODE_MAX)
                next_index = 0;
            codec.adc_read_index = next_index;
        }
#endif
        {
            next_index = codec.adc_read_index + 1;
            if (next_index >= DATA_NODE_MAX)
                next_index = 0;
            codec.adc_read_index = next_index;
        }
        //rt_kprintf("*\n");
    } /* codec rx done. */

    /* notify transmitted complete. */
    if (codec.parent.rx_indicate != RT_NULL)
    {
        codec.parent.rx_indicate(&codec.parent, codec.adc_data_list[codec.adc_put_index].data_size);
    }

    /* leave interrupt */
    rt_interrupt_leave();
}

/* ADC DMA Stream3 Interrupt Server */ 
void DMA1_Stream3_IRQHandler(void)
{
    /* Test on DMA Stream Transfer Complete interrupt */
    if(DMA_GetITStatus(AUDIO_ADC_I2S_DMA_STREAM, AUDIO_ADC_I2S_DMA_IT_TC))
    {
        /* Clear DMA Stream Transfer Complete interrupt pending bit */
        DMA_ClearITPendingBit(AUDIO_ADC_I2S_DMA_STREAM, AUDIO_ADC_I2S_DMA_IT_TC);

        /* do something */
        codec_adc_dma_isr();
    }
}

