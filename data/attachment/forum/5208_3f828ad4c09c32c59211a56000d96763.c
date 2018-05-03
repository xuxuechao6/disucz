/********************************************************************
 * File      : drv_stm32f2_eth.c
 * This file is part of RTU
 * Copyright (c) 2012-2013 HuNan YLTK Automation Technology Co.,Ltd

 * PROPRIETARY RIGHTS of XXXXXX Company are involved in the subject matter of this material.
 * All manufacturing, reproduction, use,and?sales rights pertaining to this subject matter
 * are governed by the license agreement. The recipient of this software implicitly accepts
 * the terms of the license.

 * Change Logs:
 * Date           Author       Notes
 * 2014-08-28     pw           first version
*********************************************************************/

#include <rtthread.h>
#include <netif/ethernetif.h>
#include "lwipopts.h"
#include "stm32f2x7_eth.h"
#include "stm32f2x7_eth_conf.h"

/*#define CHECKSUM_BY_HARDWARE        1*/

/*网口不通：1 MAC芯片工作模式；2 MCO2时钟是否开启 3 PHY芯片地址是否匹配*/

//#define RMII_MODE  // In this case the System clock frequency is configured
// to 100 MHz, for more details refer to system_stm32f2xx.c
#define MII_MODE

#ifdef     MII_MODE
#define PHY_CLOCK_MOC2
#endif

#define DP83848_PHY_ADDRESS         0x01 

#define netifGUARD_BLOCK_TIME       0

/* Ethernet Rx & Tx DMA Descriptors */
extern ETH_DMADESCTypeDef  DMARxDscrTab[ETH_RXBUFNB], DMATxDscrTab[ETH_TXBUFNB];

/* Ethernet Receive buffers  */
extern uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE];

/* Ethernet Transmit buffers */
extern uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE];

/* Global pointers to track current transmit and receive descriptors */
extern ETH_DMADESCTypeDef  *DMATxDescToSet;
extern ETH_DMADESCTypeDef  *DMARxDescToGet;

/* Global pointer for last received frame infos */
extern ETH_DMA_Rx_Frame_infos *DMA_RX_FRAME_infos;

#define MAX_ADDR_LEN 6

struct rt_stm32_eth
{
    /* inherit from ethernet device */
    struct eth_device parent;

    /* interface address info. */
    rt_uint8_t  dev_addr[MAX_ADDR_LEN];            /* hw address    */
};
static struct rt_stm32_eth stm32_eth_device;

static struct rt_semaphore tx_wait;
static rt_bool_t tx_is_waiting = RT_FALSE;

static void ETH_MACDMA_Config(void);
static uint32_t ETH_LED_Config(uint16_t PHYAddress);
static uint32_t ETH_LINK_ITConfig(uint16_t PHYAddress);

/* interrupt service routine */
void ETH_IRQHandler(void)
{
    rt_uint32_t status, ier;

    /* enter interrupt */
    rt_interrupt_enter();

    status = ETH->DMASR;
    ier = ETH->DMAIER;

    if(status & ETH_DMA_IT_MMC)
    {
        rt_kprintf("ETH_DMA_IT_MMC\r\n");
        ETH_DMAClearITPendingBit(ETH_DMA_IT_MMC);
    }

    if(status & ETH_DMA_IT_NIS)
    {
        rt_uint32_t nis_clear = ETH_DMA_IT_NIS;

        /* [0]:Transmit Interrupt. */
        if((status & ier) & ETH_DMA_IT_T) /* packet transmission */
        {
            rt_kprintf("ETH_DMA_IT_T\r\n");

            if (tx_is_waiting == RT_TRUE)
            {
                tx_is_waiting = RT_FALSE;
                rt_sem_release(&tx_wait);
            }

            nis_clear |= ETH_DMA_IT_T;
        }

        /* [2]:Transmit Buffer Unavailable. */

        /* [6]:Receive Interrupt. */
        if((status & ier) & ETH_DMA_IT_R) /* packet reception */
        {
            /*rt_kprintf("ETH_DMA_IT_R\r\n");*/
            /* a frame has been received */
            eth_device_ready(&(stm32_eth_device.parent));

            nis_clear |= ETH_DMA_IT_R;
        }

        /* [14]:Early Receive Interrupt. */

        ETH_DMAClearITPendingBit(nis_clear);
    }

    if(status & ETH_DMA_IT_AIS)
    {
        rt_uint32_t ais_clear = ETH_DMA_IT_AIS;
        rt_kprintf("ETH_DMA_IT_AIS\r\n");

        /* [1]:Transmit Process Stopped. */
        if(status & ETH_DMA_IT_TPS)
        {
            rt_kprintf("AIS ETH_DMA_IT_TPS\r\n");
            ais_clear |= ETH_DMA_IT_TPS;
        }

        /* [3]:Transmit Jabber Timeout. */
        if(status & ETH_DMA_IT_TJT)
        {
            rt_kprintf("AIS ETH_DMA_IT_TJT\r\n");
            ais_clear |= ETH_DMA_IT_TJT;
        }

        /* [4]: Receive FIFO Overflow. */
        if(status & ETH_DMA_IT_RO)
        {
            rt_kprintf("AIS ETH_DMA_IT_RO\r\n");
            ais_clear |= ETH_DMA_IT_RO;
        }

        /* [5]: Transmit Underflow. */
        if(status & ETH_DMA_IT_TU)
        {
            rt_kprintf("AIS ETH_DMA_IT_TU\r\n");
            ais_clear |= ETH_DMA_IT_TU;
        }

        /* [7]: Receive Buffer Unavailable. */
        if(status & ETH_DMA_IT_RBU)
        {
            rt_kprintf("AIS ETH_DMA_IT_RBU\r\n");
            ais_clear |= ETH_DMA_IT_RBU;
        }

        /* [8]: Receive Process Stopped. */
        if(status & ETH_DMA_IT_RPS)
        {
            rt_kprintf("AIS ETH_DMA_IT_RPS\r\n");
            ais_clear |= ETH_DMA_IT_RPS;
        }

        /* [9]: Receive Watchdog Timeout. */
        if(status & ETH_DMA_IT_RWT)
        {
            rt_kprintf("AIS ETH_DMA_IT_RWT\r\n");
            ais_clear |= ETH_DMA_IT_RWT;
        }

        /* [10]: Early Transmit Interrupt. */

        /* [13]: Fatal Bus Error. */
        if(status & ETH_DMA_IT_FBE)
        {
            rt_kprintf("AIS ETH_DMA_IT_FBE\r\n");
            ais_clear |= ETH_DMA_IT_FBE;
        }

        ETH_DMAClearITPendingBit(ais_clear);
    }

    /* leave interrupt */
    rt_interrupt_leave();
}

/* RT-Thread Device Interface */
/* initialize the interface */
static rt_err_t rt_stm32_eth_init(rt_device_t dev)
{
    int i;

    /* MAC address configuration */
    ETH_MACAddressConfig(ETH_MAC_Address0, (u8*)&stm32_eth_device.dev_addr[0]);

    /* Initialize Tx Descriptors list: Chain Mode */
    ETH_DMATxDescChainInit(DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
    /* Initialize Rx Descriptors list: Chain Mode  */
    ETH_DMARxDescChainInit(DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);

    /* Enable Ethernet Rx interrrupt */
    {
        for(i = 0; i < ETH_RXBUFNB; i++)
        {
            ETH_DMARxDescReceiveITConfig(&DMARxDscrTab[i], ENABLE);
        }
    }

#ifdef CHECKSUM_BY_HARDWARE
    /* Enable the checksum insertion for the Tx frames */
    {
        for(i = 0; i < ETH_TXBUFNB; i++)
        {
            ETH_DMATxDescChecksumInsertionConfig(&DMATxDscrTab[i],
                                                 ETH_DMATxDesc_ChecksumTCPUDPICMPFull);
        }
    }
#endif

    /* Enable MAC and DMA transmission and reception */
    ETH_Start();

    return RT_EOK;
}

static rt_err_t rt_stm32_eth_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

static rt_err_t rt_stm32_eth_close(rt_device_t dev)
{
    return RT_EOK;
}

static rt_size_t rt_stm32_eth_read(rt_device_t dev, rt_off_t pos, void* buffer,
                                   rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return 0;
}

static rt_size_t rt_stm32_eth_write (rt_device_t dev, rt_off_t pos,
                                     const void* buffer, rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return 0;
}

static rt_err_t rt_stm32_eth_control(rt_device_t dev, rt_uint8_t cmd,
                                     void *args)
{
    switch(cmd)
    {
        case NIOCTL_GADDR:
            /* get mac address */
            if(args) memcpy(args, stm32_eth_device.dev_addr, 6);
            else return -RT_ERROR;
            break;

        default :
            break;
    }

    return RT_EOK;
}

void show_frame(struct pbuf *q)
{
    int i = 0;

    char *ptr = q->payload;

    for( i = 0; i < q->len; i++ )
        rt_kprintf("0x%02X ", *(ptr++));
    rt_kprintf("\n");
}

/* ethernet device interface */
/* transmit packet. */
rt_err_t rt_stm32_eth_tx( rt_device_t dev, struct pbuf* p)
{
    rt_err_t ret;
    struct pbuf *q;
    uint32_t l = 0;
    u8 *buffer ;

    if (( ret = rt_sem_take(&tx_wait, netifGUARD_BLOCK_TIME) ) == RT_EOK)
    {
        buffer =  (u8 *)(DMATxDescToSet->Buffer1Addr);
        for(q = p; q != NULL; q = q->next)
        {
            /*show_frame(q);*/
            rt_memcpy((u8_t*)&buffer[l], q->payload, q->len);
            l = l + q->len;
        }
        if( ETH_Prepare_Transmit_Descriptors(l) == ETH_ERROR )
            rt_kprintf("Tx Error\n");
        rt_sem_release(&tx_wait);
    }
    else
    {
        rt_kprintf("Tx Timeout\n");
        return ret;
    }

    /* Return SUCCESS */
    return RT_EOK;
}

/* reception packet. */
struct pbuf *rt_stm32_eth_rx(rt_device_t dev)
{
    struct pbuf *p, *q;
    u16_t len;
    uint32_t l = 0, i = 0;
    FrameTypeDef frame;
    u8 *buffer;
    __IO ETH_DMADESCTypeDef *DMARxNextDesc;

    p = RT_NULL;

    /* Get received frame */
    frame = ETH_Get_Received_Frame_interrupt();

    if( frame.length > 0 )
    {
        /* check that frame has no error */
        if ((frame.descriptor->Status & ETH_DMARxDesc_ES) == (uint32_t)RESET)
        {
            //rt_kprintf("Get a frame %d buf = 0x%X, len= %d\n", framecnt++, frame.buffer, frame.length);
            /* Obtain the size of the packet and put it into the "len" variable. */
            len = frame.length;
            buffer = (u8 *)frame.buffer;

            /* We allocate a pbuf chain of pbufs from the pool. */
            p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
            //p = pbuf_alloc(PBUF_LINK, len, PBUF_RAM);

            /* Copy received frame from ethernet driver buffer to stack buffer */
            if (p != NULL)
            {
                for (q = p; q != NULL; q = q->next)
                {
                    rt_memcpy((u8_t*)q->payload, (u8_t*)&buffer[l], q->len);
                    l = l + q->len;
                }
            }
        }

        /* Release descriptors to DMA */
        /* Check if received frame with multiple DMA buffer segments */
        if (DMA_RX_FRAME_infos->Seg_Count > 1)
        {
            DMARxNextDesc = DMA_RX_FRAME_infos->FS_Rx_Desc;
        }
        else
        {
            DMARxNextDesc = frame.descriptor;
        }

        /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
        for (i = 0; i < DMA_RX_FRAME_infos->Seg_Count; i++)
        {
            DMARxNextDesc->Status = ETH_DMARxDesc_OWN;
            DMARxNextDesc = (ETH_DMADESCTypeDef *)(DMARxNextDesc->Buffer2NextDescAddr);
        }

        /* Clear Segment_Count */
        DMA_RX_FRAME_infos->Seg_Count = 0;


        /* When Rx Buffer unavailable flag is set: clear it and resume reception */
        if ((ETH->DMASR & ETH_DMASR_RBUS) != (u32)RESET)
        {
            /* Clear RBUS ETHERNET DMA flag */
            ETH->DMASR = ETH_DMASR_RBUS;

            /* Resume DMA reception */
            ETH->DMARPDR = 0;
        }
    }
    return p;
}

static void NVIC_Configuration(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;

    /* 2 bit for pre-emption priority, 2 bits for subpriority */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* Enable the Ethernet global Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = ETH_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* Connect EXTI Line to INT Pin */
    //SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource14);


#if 1
    /* Configure EXTI line */
    /* Configure EXTI line */
    EXTI_InitStructure.EXTI_Line = EXTI_Line12;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Enable and set the EXTI interrupt to the highest priority */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
#endif
}

/*
 * GPIO Configuration for ETH
 */
static void GPIO_Configuration(void)
{

    GPIO_InitTypeDef GPIO_InitStructure;

    /* Enable GPIOs clocks */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB |
                           RCC_AHB1Periph_GPIOC |
                           RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOG, ENABLE);
    /* Enable SYSCFG clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    /* MII/RMII Media interface selection --------------------------------------*/
#ifdef MII_MODE
#ifdef PHY_CLOCK_MCO1
    /* Configure MCO1 (PA8) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    /*GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;*/
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_MCO);

    /* Output HSE clock (25MHz) on MCO1 pin (PA8) to clock the PHY */
    RCC_MCO1Config(RCC_MCO1Source_HSE, RCC_MCO1Div_1);
#endif

#ifdef PHY_CLOCK_MOC2
    //Configure MCO2 (PC9)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_MCO);
    //Output HSE clock(25MHz) on MCO2 pin(PC9) to clock the PHY
    RCC_MCO2Config(RCC_MCO2Source_HSE, RCC_MCO2Div_1);
#endif /* PHY_CLOCK_MCO */

    SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_MII);
#elif defined RMII_MODE  /* Mode RMII with STM322xG-EVAL */

    SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII);
#endif

    /* Ethernet pins configuration ************************************************/
    /*

                                             LQFP176                LQFP144            UFBGA176
         ETH_MII_CRS ----------------------> PA0                    PH2
         ETH_MII_RX_CLK/ETH_RMII_REF_CLK---> PA1
         ETH_MDIO -------------------------> PA2
         ETH_MII_COL ----------------------> PA3                    PH3
         ETH_MII_RX_DV/ETH_RMII_CRS_DV ----> PA7
         ETH_MII_RXD2 ---------------------> PB0                    PH6
         ETH_MII_RXD3 ---------------------> PB1                    PH7
         ETH_MII_TXD3 ---------------------> PB8
         ETH_MII_RX_ER --------------------> PB10                PI10
         ETH_MII_TXD0/ETH_RMII_TXD0 -------> PB12
         ETH_MII_TXD1/ETH_RMII_TXD1 -------> PB13
         ETH_MDC --------------------------> PC1
         ETH_MII_TXD2 ---------------------> PC2
         ETH_MII_TX_CLK -------------------> PC3
         ETH_MII_RXD0/ETH_RMII_RXD0 -------> PC4
         ETH_MII_RXD1/ETH_RMII_RXD1 -------> PC5
         ETH_MII_TX_EN/ETH_RMII_TX_EN -----> PG11

         新增引脚：
         PHY_RESET ------------------------> PB11
         PHY_PWR/INT ----------------------> PF9   --?>PA12
                                                   */
    /* Configure PA0, PA1, PA2 and PA7 */
    GPIO_InitStructure.GPIO_Pin     = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
                                      GPIO_Pin_3 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType   = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd    = GPIO_PuPd_NOPULL ;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_ETH);//PA0=ETH_MII_CRS
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_ETH);//PA3=ETH_MII_COL
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_ETH);

    /* ConfigurePB0, PB1, PB5, PB10, PB12 and PB13 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_10
                                  | GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource1, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource12, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_ETH);

    /* Configure PC1, PC2, PC3, PC4 and PC5 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4
                                  | GPIO_Pin_5;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource2, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource3, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource4, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource5, GPIO_AF_ETH);

    /* Configure PE2 */
    /*GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOE, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource2, GPIO_AF_ETH);//ETH_MII_TXD3
    */
    /* Configure PG11 */
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_11;
    GPIO_Init(GPIOG, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource11, GPIO_AF_ETH);

    /*将PB7 PB11配置为IO输出*/
    /*GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN ;//PHY_TX_ER
    GPIO_Init(GPIOB,&GPIO_InitStructure);
    */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;//PHY_RESET
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* Enable the INT (PF9) Clock */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    /* Configure INT pin as input */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Connect EXTI Line to INT Pin */
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource12);

    GPIO_ResetBits(GPIOB, GPIO_Pin_11);//复位PHY
    rt_thread_delay(2);
    GPIO_SetBits(GPIOB, GPIO_Pin_11);//复位PHY
}


/*配置PHY LED功能*/
static uint32_t ETH_LED_Config(uint16_t PHYAddress)
{
    uint16_t tmpreg = 0;

    /*配置LED模式未mode 3*/
    tmpreg = ETH_ReadPHYRegister(PHYAddress, PHY_CR);
    tmpreg |= (uint16_t)(PHY_CR_LED_CNFG1);
    tmpreg &= (uint16_t)~(PHY_CR_LED_CNFG0);

    if(!(ETH_WritePHYRegister(PHYAddress, PHY_CR, tmpreg)))
    {
        /* Return ERROR in case of write timeout */
        return ETH_ERROR;
    }

    return ETH_SUCCESS;
}

static uint32_t ETH_LINK_ITConfig(uint16_t PHYAddress)
{
    uint32_t tmpreg = 0;

    /* Read MICR register */
    tmpreg = ETH_ReadPHYRegister(PHYAddress, PHY_MICR);

    /* Enable output interrupt events to signal via the INT pin */
#if PHY_DP83848_LXT971_SEL
    tmpreg |= (uint32_t)PHY_MICR_INT_EN |
              PHY_MICR_LINK_INT_EN;/*使能中断并使能连接状态事件中断*/
    if(!(ETH_WritePHYRegister(PHYAddress, PHY_MICR, tmpreg)))
    {
        /* Return ERROR in case of write timeout */
        return ETH_ERROR;
    }
#else
    tmpreg |= (uint32_t)PHY_MICR_INT_EN | PHY_MICR_INT_OE;
    if(!(ETH_WritePHYRegister(PHYAddress, PHY_MICR, tmpreg)))
    {
        /* Return ERROR in case of write timeout */
        return ETH_ERROR;
    }
#endif

#if PHY_DP83848_LXT971_SEL
    /*LXT971 Link interrupt enable is in preview step*/
#else
    /* Read MISR register */
    tmpreg = ETH_ReadPHYRegister(PHYAddress, PHY_MISR);

    /* Enable Interrupt on change of link status */
    tmpreg |= (uint32_t)PHY_MISR_LINK_INT_EN;
    if(!(ETH_WritePHYRegister(PHYAddress, PHY_MISR, tmpreg)))
    {
        /* Return ERROR in case of write timeout */
        return ETH_ERROR;
    }
#endif
    /* Return SUCCESS */
    return ETH_SUCCESS;

}

/**
  * @brief  Configures the Ethernet Interface
  * @param  None
  * @retval None
  */
static void ETH_MACDMA_Config(void)
{
    ETH_InitTypeDef ETH_InitStructure;

    /* Enable ETHERNET clock  */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_ETH_MAC | RCC_AHB1Periph_ETH_MAC_Tx |
                           RCC_AHB1Periph_ETH_MAC_Rx, ENABLE);

    /* Reset ETHERNET on AHB Bus */
    ETH_DeInit();

    /* Software reset */
    ETH_SoftwareReset();

    /* Wait for software reset */
    while (ETH_GetSoftwareResetStatus() == SET);

    /* ETHERNET Configuration --------------------------------------------------*/
    /* Call ETH_StructInit if you don't like to configure all ETH_InitStructure parameter */
    ETH_StructInit(&ETH_InitStructure);

    /* Fill ETH_InitStructure parametrs */
    /*------------------------   MAC   -----------------------------------*/
    ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
    //ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Disable;
    //  ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
    //  ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;

    ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
    ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
    ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
    ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
    ETH_InitStructure.ETH_BroadcastFramesReception =
        ETH_BroadcastFramesReception_Enable;
    ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
    ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
    ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
#ifdef CHECKSUM_BY_HARDWARE
    ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;
#endif

    /*------------------------   DMA   -----------------------------------*/

    /* When we use the Checksum offload feature, we need to enable the Store and Forward mode:
    the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum,
    if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
    ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame =
        ETH_DropTCPIPChecksumErrorFrame_Enable;
    ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
    ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;

    ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;
    ETH_InitStructure.ETH_ForwardUndersizedGoodFrames =
        ETH_ForwardUndersizedGoodFrames_Disable;
    ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;
    ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
    ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;
    ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;
    ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
    ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

    /* Configure Ethernet */
    if( ETH_Init(&ETH_InitStructure, DP83848_PHY_ADDRESS) ==  ETH_ERROR )
        rt_kprintf("ETH init error, may be no link\n");

    /* Enable the Ethernet Rx Interrupt */
    ETH_DMAITConfig(ETH_DMA_IT_NIS | ETH_DMA_IT_R | ETH_DMA_IT_T, ENABLE);
}

void EXTI15_10_IRQHandler(void)
{
    if(EXTI_GetITStatus(EXTI_Line12) != RESET)
    {
        /* Clear interrupt pending bit */
        EXTI_ClearITPendingBit(EXTI_Line12);

        if(ETH_ReadPHYRegister(DP83848_PHY_ADDRESS, PHY_MISR) & PHY_LINK_STATUS)
        {
            if(ETH_ReadPHYRegister(DP83848_PHY_ADDRESS, PHY_SR) & 0x0001)
            {
                rt_kprintf("eth link up\n");
                eth_device_linkchange(&stm32_eth_device.parent , RT_TRUE);
            }
            else
            {
                rt_kprintf("eth link down\n");
                eth_device_linkchange(&stm32_eth_device.parent , RT_FALSE);
            }
        }
    }
}


#define   DevID_SNo0       (*((rt_uint32_t *)0x1FFF7A10));
#define   DevID_SNo1       (*((rt_uint32_t *)0x1FFF7A10+32));
#define   DevID_SNo2       (*((rt_uint32_t *)0x1FFF7A10+64));

rt_err_t rt_hw_stm32_eth_init(uint8_t *mac)
{
    GPIO_Configuration();
    NVIC_Configuration();
    ETH_MACDMA_Config();
    ETH_LED_Config(DP83848_PHY_ADDRESS);

    /*detect the network*/
    ETH_LINK_ITConfig(DP83848_PHY_ADDRESS);

    if (mac[0] == 0x00 &&
        mac[1] == 0x00 &&
        mac[2] == 0x00 &&
        mac[3] == 0x00 &&
        mac[4] == 0x00 &&
        mac[5] == 0x00)
    {
        stm32_eth_device.dev_addr[0] = 0x00;
        stm32_eth_device.dev_addr[1] = 0x60;
        stm32_eth_device.dev_addr[2] = 0x6e;
        {
            uint32_t cpu_id[3] = {0};
            cpu_id[2] = DevID_SNo2;
            cpu_id[1] = DevID_SNo1;
            cpu_id[0] = DevID_SNo0;

            // generate MAC addr from 96bit unique ID (only for test)
            stm32_eth_device.dev_addr[3] = (uint8_t)((cpu_id[0] >> 16) & 0xFF);
            stm32_eth_device.dev_addr[4] = (uint8_t)((cpu_id[0] >> 8) & 0xFF);
            stm32_eth_device.dev_addr[5] = (uint8_t)(cpu_id[0] & 0xFF);
        }
    }
    else
    {
        stm32_eth_device.dev_addr[0] = mac[0];
        stm32_eth_device.dev_addr[1] = mac[1];
        stm32_eth_device.dev_addr[2] = mac[2];
        stm32_eth_device.dev_addr[3] = mac[3];
        stm32_eth_device.dev_addr[4] = mac[4];
        stm32_eth_device.dev_addr[5] = mac[5];
    }

    stm32_eth_device.parent.parent.init       = rt_stm32_eth_init;
    stm32_eth_device.parent.parent.open       = rt_stm32_eth_open;
    stm32_eth_device.parent.parent.close      = rt_stm32_eth_close;
    stm32_eth_device.parent.parent.read       = rt_stm32_eth_read;
    stm32_eth_device.parent.parent.write      = rt_stm32_eth_write;
    stm32_eth_device.parent.parent.control    = rt_stm32_eth_control;
    stm32_eth_device.parent.parent.user_data  = RT_NULL;

    stm32_eth_device.parent.eth_rx     = rt_stm32_eth_rx;
    stm32_eth_device.parent.eth_tx     = rt_stm32_eth_tx;

    /* init tx semaphore */
    rt_sem_init(&tx_wait, "tx_wait", 1, RT_IPC_FLAG_FIFO);

    /* register eth device */
    eth_device_init(&(stm32_eth_device.parent), "e0");
    
    return RT_EOK;
}

static void phy_search(int argc, char** argv)
{
    int i;
    int value;

    for(i = 0; i < 32; i++)
    {
        value = ETH_ReadPHYRegister(i, 2);
        rt_kprintf("addr %02d: %04X\n", i, value);
    }
}

#ifdef FINSH_USING_MSH
#include <finsh.h>
MSH_CMD_EXPORT(phy_search, search phy use MDIO);
#endif

