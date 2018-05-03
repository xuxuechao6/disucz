/*********************************************************************
 * Filename:    spiflash.c
 *
 * Description:    Tihs flie realize spi flash device drive
 *						   hardware chip is w25q16,furnish filesystem
 *						   using.
 * Author:        wangzw <wangzw@yuettak.com>
 * Created at:    2013-04-26 12:00:00
 *                
 * Modify:
 *
 * 
 *
 * Copyright (C) 2013 Yuettak Co.,Ltd
 ********************************************************************/

#include "spiflash.h"

//#define USE_FLASH_WRITE_CMD

/* hardware define ---------------------------------------------------------*/

#define SPI1_SCK_PIN									GPIO_Pin_5;
#define	 SPI1_MISO_PIN								GPIO_Pin_6
#define SPI1_MOSI_PIN									GPIO_Pin_7
#define SPI1_GPIO_PORT								GPIOA
#define SPI1_APB2_CLOCK								RCC_APB2Periph_SPI1 | RCC_APB2Periph_GPIOA

#define SPI1_CS_PIN										GPIO_Pin_4
#define SPI1_CS_PORT									GPIOA
#define SPI1_CS_CLOCK									RCC_APB2Periph_GPIOA




/* Private define ------------------------------------------------------------*/
#define SPI_FLASH_PageSize      					  256
#define SPI_FLASH_PerWritePageSize      	  256


#define W25X_WriteEnable		     		 				0x06 
#define W25X_WriteDisable		      					0x04 
#define W25X_ReadStatusReg1		    				  0x05 
#define W25X_ReadStatusReg2		    				  0x35 
#define W25X_WriteStatusReg		    				  0x01 
#define W25X_ReadData			        					0x03 
#define W25X_FastReadData		      					0x0B 
#define W25X_FastReadDual		     					  0x3B 
#define W25X_PageProgram		      					0x02 
#define W25X_BlockErase			      					0xD8 // 64k
#define W25X_SectorErase		      					0x20 // 4k
#define W25X_ChipErase			      					0xC7 
#define W25X_PowerDown			      					0xB9 
#define W25X_ReleasePowerDown	   				    0xAB 
#define W25X_DeviceID			        					0xAB 
#define W25X_HighPerformanceMode            0XA3
#define W25X_ManufactDeviceID   					  0x90 
#define W25X_JedecDeviceID		    				  0x9F 

#define WIP_Flag                  					0x01  //Write In Progress (WIP) flag 
#define Dummy_Byte                					0xFF  //moke needful read clock


/********************************************* spi devie data struct **********************************/

/* flash device */
struct flash_device 
{
	struct rt_device               parent;      /**< RT-Thread device struct */
	struct rt_device_blk_geometry  geometry;    /**< sector size, sector count */
	struct rt_spi_device *         spi_device;  /**< SPI interface */
	u32                            max_clock;   /**< MAX SPI clock */
	struct rt_mutex						     lock;		 /* Flash mutex */
};

/*spi device*/
struct flash_device	w25q16_device;

/*******************************************************************************
* Function Name  : rt_hw_spi_init
* Description    : register spi bus and register spi cs device
*                  
* Input				: None
* Output			: None
* Return         	: None
*******************************************************************************/
void rt_hw_spi_init(void)
{
	/*		initialization SPI Bus device		 */
	{		
		GPIO_InitTypeDef 							gpio_initstructure;

		RCC_APB2PeriphClockCmd(SPI1_APB2_CLOCK | SPI1_CS_CLOCK,ENABLE);

		gpio_initstructure.GPIO_Mode = GPIO_Mode_AF_PP;
		gpio_initstructure.GPIO_Speed = GPIO_Speed_50MHz;
		gpio_initstructure.GPIO_Pin = SPI1_MISO_PIN | SPI1_MOSI_PIN | SPI1_SCK_PIN;
		GPIO_Init(SPI1_GPIO_PORT,&gpio_initstructure);
		/*		register spi bus device */
		stm32_spi_register(SPI1,&stm32_spi_bus_1,SPI1_BUS_NAME);
	}
	/*		initialization SPI CS device 		 */
	{
		static struct rt_spi_device 		spi_device;        
		static struct stm32_spi_cs  		spi_cs;	
		GPIO_InitTypeDef						gpio_initstructure;
   
		/*		configure CS clock port pin		*/
		spi_cs.GPIOx = SPI1_CS_PORT;        
		spi_cs.GPIO_Pin = SPI1_CS_PIN;     
		RCC_APB2PeriphClockCmd(SPI1_CS_CLOCK, ENABLE);        
		
		gpio_initstructure.GPIO_Mode = GPIO_Mode_Out_PP;    
		gpio_initstructure.GPIO_Pin = spi_cs.GPIO_Pin;        
		gpio_initstructure.GPIO_Speed = GPIO_Speed_50MHz; 
		GPIO_SetBits(spi_cs.GPIOx, spi_cs.GPIO_Pin);     
		
		GPIO_Init(spi_cs.GPIOx, &gpio_initstructure);     
		/* 	add cs devie go to spi bus devie	*/
		rt_spi_bus_attach_device(&spi_device,SPI1_CS1_NAME, SPI1_BUS_NAME, (void*)&spi_cs);
		
	}
}


/******************************************** flash device drive function ******************************/
static void spi_flash_wait_write_end(struct rt_spi_device *device)
{
	u8 CmdBuf = W25X_ReadStatusReg1;
	u8 RecvData;
	
  do
  {
    RecvData = rt_spi_sendrecv8(device,CmdBuf);
  }
  while ((RecvData & WIP_Flag) == SET); 
}

static void spi_flash_write_enable(struct rt_spi_device *device )
{
	u8 CmdBuf = W25X_WriteEnable;
  
	rt_spi_send(device,&CmdBuf,1);
	
	spi_flash_wait_write_end(device);
}

void spi_flash_sector_erase(struct rt_spi_device *device,u32 SectorAddr)
{
	u8 CmdBuf[4];
	
  spi_flash_write_enable(device);

	CmdBuf[0] = W25X_SectorErase;
	CmdBuf[1] = (SectorAddr & 0x00FF0000) >> 16;
	CmdBuf[2] = (SectorAddr & 0x0000FF00) >> 8;
	CmdBuf[3] = (SectorAddr & 0x000000FF);
	rt_spi_send(device,CmdBuf,4);
  
  spi_flash_wait_write_end(device);
}

static void spi_flash_chip_erase(struct rt_spi_device *device)
{
	u8 Cmd;

	Cmd = W25X_ChipErase;
	
	spi_flash_write_enable(device);

	rt_spi_send(device,&Cmd,1);

	spi_flash_wait_write_end(device);
}

static void spi_flash_page_write(struct rt_spi_device *device,const u8* pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
	u8 CmdBuf[4];
	
  spi_flash_write_enable(device);
  
	CmdBuf[0] = W25X_PageProgram;
	CmdBuf[1] = (WriteAddr & 0x00FF0000) >> 16;
	CmdBuf[2] = (WriteAddr & 0x0000FF00) >> 8;
	CmdBuf[3] = (WriteAddr & 0x000000FF);
	rt_spi_send_then_send(device,CmdBuf,4,pBuffer,NumByteToWrite);

  spi_flash_wait_write_end(device);
}


static void spi_flash_buffer_write(struct rt_spi_device *device,const u8* pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
	u16 pageremain = NumByteToWrite/SPI_FLASH_PerWritePageSize;	   

	spi_flash_sector_erase(device,WriteAddr); 

	while(pageremain--)
	{
    spi_flash_page_write(device,pBuffer,WriteAddr,SPI_FLASH_PerWritePageSize);
    pBuffer += SPI_FLASH_PerWritePageSize;
    WriteAddr += SPI_FLASH_PerWritePageSize;
	}
}

static void spi_flash_buffer_read(struct rt_spi_device *device,u8* pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
	u8 CmdBuf[4];

	CmdBuf[0] = W25X_ReadData;
	CmdBuf[1] = (ReadAddr & 0x00FF0000) >> 16;
	CmdBuf[2] = (ReadAddr & 0x0000FF00) >> 8;
	CmdBuf[3] = (ReadAddr & 0x000000FF);
 
	rt_spi_send_then_recv(device,CmdBuf,4,pBuffer,NumByteToRead);
	
}






/**************************************** spi flash resiger function **********************************/
void rt_flash_lock(struct flash_device* flash_dev)
{
	rt_mutex_take(&(flash_dev->lock),RT_WAITING_FOREVER);
}


void rt_flash_unlock(struct flash_device* flash_dev)
{
	rt_mutex_release(&(flash_dev->lock));
}


static rt_err_t rt_flash_init(rt_device_t dev)
{
	struct flash_device* flash = (struct flash_device *)dev;	

	if(flash->spi_device->bus->owner != flash->spi_device)
	{
		/* 	current configuration spi bus */		
		flash->spi_device->bus->ops->configure(flash->spi_device,&flash->spi_device->config);
	}
	rt_spi_sendrecv8(flash->spi_device,Dummy_Byte);
	
	return RT_EOK;
}



static rt_err_t rt_flash_open(rt_device_t dev, rt_uint16_t oflag)
{
	return RT_EOK;
}


static rt_err_t rt_flash_close(rt_device_t dev)
{
	return RT_EOK;
}


static rt_size_t rt_flash_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size)
{
	struct flash_device* flash = (struct flash_device *)dev;	

	RT_ASSERT(flash != RT_NULL);
	
	rt_flash_lock(flash);
	spi_flash_buffer_read(flash->spi_device,buffer,pos*4096,size*4096);

	rt_flash_unlock(flash);
		
	return size;
}


static rt_size_t rt_flash_write(rt_device_t dev, rt_off_t pos,const void* buffer, rt_size_t size)
{
	struct flash_device* flash = (struct flash_device *)dev;	
	
	RT_ASSERT(flash != RT_NULL);

	rt_flash_lock(flash);
	
	spi_flash_buffer_write(flash->spi_device,buffer,pos*4096,size*4096);

	rt_flash_unlock(flash);
	
	return size;
}


static rt_err_t rt_flash_control(rt_device_t dev, rt_uint8_t cmd, void *args)
{
    struct flash_device* msd = (struct flash_device*)dev;

    RT_ASSERT(dev != RT_NULL);

    if (cmd == RT_DEVICE_CTRL_BLK_GETGEOME)
    {
        struct rt_device_blk_geometry *geometry;

        geometry = (struct rt_device_blk_geometry *)args;
        if (geometry == RT_NULL) return -RT_ERROR;

        geometry->bytes_per_sector = msd->geometry.bytes_per_sector;
        geometry->block_size = msd->geometry.block_size;
        geometry->sector_count = msd->geometry.sector_count;
    }

    return RT_EOK;
}


rt_err_t rt_flash_register(const char * flash_device_name, const char * spi_device_name)
{
  rt_err_t result = RT_EOK;
  struct rt_spi_device * spi_device;
  struct rt_spi_configuration spi2_configuer= 
  {
    RT_SPI_MODE_MASK,                   //spi clock and data mode set
    8,                                      //data width
    0,                                      //reserved
    18000000                           //MAX frequency 18MHz
  };
  rt_memset(&w25q16_device, 0, sizeof(w25q16_device));

  result = rt_mutex_init(&w25q16_device.lock,"flash",RT_IPC_FLAG_FIFO);
  if(result != RT_EOK)
  {
  
#ifdef RT_USING_FINSH
    rt_kprintf("flash mutex init fail\n", spi_device_name);
#endif

    return -RT_ENOSYS;
  }
  spi_device = (struct rt_spi_device *)rt_device_find(spi_device_name);
  if(spi_device == RT_NULL)
  {
  
#ifdef RT_USING_FINSH
  rt_kprintf("spi device %s not found!\n", spi_device_name);
#endif

  return -RT_ENOSYS;
  }
  w25q16_device.spi_device = spi_device;
  w25q16_device.max_clock = 18000000;
  w25q16_device.spi_device->config = spi2_configuer;
  /* register flash device */
  w25q16_device.parent.type    = RT_Device_Class_Block;

  w25q16_device.geometry.bytes_per_sector = 4096;
  w25q16_device.geometry.sector_count = (1024-150);
  w25q16_device.geometry.block_size = 4096;

 

  w25q16_device.parent.init        = rt_flash_init;
  w25q16_device.parent.open      = rt_flash_open;
  w25q16_device.parent.close     = rt_flash_close;
  w25q16_device.parent.read      = rt_flash_read;
  w25q16_device.parent.write     = rt_flash_write;
  w25q16_device.parent.control   = rt_flash_control;

  /* no private, no callback */
  w25q16_device.parent.user_data = RT_NULL;
  w25q16_device.parent.rx_indicate = RT_NULL;
  w25q16_device.parent.tx_complete = RT_NULL;

  result = rt_device_register(&w25q16_device.parent, flash_device_name,
                              RT_DEVICE_FLAG_RDWR  | RT_DEVICE_FLAG_STANDALONE | RT_DEVICE_FLAG_DMA_RX |RT_DEVICE_FLAG_DMA_TX);

  return result;  

}



int rt_spi_flash_init(void)
{
	rt_hw_spi_init();
	rt_flash_register(FLASH1_DEVICE_NAME,SPI1_CS1_NAME);
  
	return 0;
}
INIT_BOARD_EXPORT(rt_spi_flash_init);


















#ifdef RT_USING_FINSH
#include <finsh.h>


void flashread(u32 addr,u32 size)
{
	u32  i = addr;
	u8 dat;

	for(i = addr;i < size;i++)
	{
		spi_flash_buffer_read(w25q16_device.spi_device,&dat,i,1);
		rt_kprintf("%d  = %x 	 ",i,dat);
		if(i % 10 == 0)
		{
			rt_kprintf("\n");
		}
	}
}
FINSH_FUNCTION_EXPORT(flashread,flashread(start_addr, end_addr) --Read_Hex);

void flashreadc(u32 addr,u32 size)
{
	u32  i = addr;
	u8 dat;

	for(i = addr;i < size;i++)
	{
		spi_flash_buffer_read(w25q16_device.spi_device,&dat,i,1);
		rt_kprintf("%c ",dat);
	}
}
FINSH_FUNCTION_EXPORT(flashreadc,flashreadc(start_addr, end_addr)-- Read_String);




void flashsectore(u32 size)
{
	spi_flash_sector_erase(w25q16_device.spi_device,size);
}
FINSH_FUNCTION_EXPORT(flashsectore,flashsectore(Sectore_Addr)--Sectore_Erase);


void flashchipe(u32 size)
{
	spi_flash_chip_erase(w25q16_device.spi_device);
}
FINSH_FUNCTION_EXPORT(flashchipe,flashchipe()--chip_Erase);


void spi_flash_write_nocheck(const u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)   
{ 			 		 
	u16 pageremain;	   
	pageremain=256-WriteAddr%256; 
	
	if(NumByteToWrite<=pageremain)
	{
		pageremain=NumByteToWrite;
	}
	while(1)
	{	   
		spi_flash_page_write(w25q16_device.spi_device,pBuffer,WriteAddr,pageremain);
		if(NumByteToWrite==pageremain)
		{
			break;
		}
			else
		{
			pBuffer+=pageremain;
			WriteAddr+=pageremain;	

			NumByteToWrite-=pageremain;			 
			if(NumByteToWrite>256)pageremain=256;
			else pageremain=NumByteToWrite; 	 
		}
	};	    
} 
#ifdef USE_FLASH_WRITE_CMD
u8 SPI_FLASH_BUF[4097];
void flashwrite(const u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)   
{ 
	u32 secpos;
	u16 secoff;
	u16 secremain;	   
 	u16 i;    

	secpos=WriteAddr / 4096;
	secoff=WriteAddr % 4096;
	secremain=4096-secoff;

	if(NumByteToWrite<=secremain)
	{
		secremain=NumByteToWrite;
	}
	while(1) 
	{	
		spi_flash_write_nocheck(SPI_FLASH_BUF,secpos*4096,4096);
		for(i=0;i<secremain;i++)
		{
			if(SPI_FLASH_BUF[secoff+i]!=0XFF)
			{
				break;
			}
		}
		if(i<secremain)
		{
			spi_flash_sector_erase(w25q16_device.spi_device,secpos);
			for(i=0;i<secremain;i++)	   
			{
				SPI_FLASH_BUF[i+secoff]=pBuffer[i];	  
			}
			spi_flash_write_nocheck(SPI_FLASH_BUF,secpos*4096,4096);
		}
		else 
		{
			spi_flash_write_nocheck(pBuffer,WriteAddr,secremain); 		 
		}
		if(NumByteToWrite==secremain)
		{
			break;
		}
		else
		{
			secpos++;
			secoff=0;

		   	pBuffer+=secremain; 
			WriteAddr+=secremain;
		   	NumByteToWrite-=secremain;				
			if(NumByteToWrite>4096)
			{
				secremain=4096;
			}
			else
			{
				secremain=NumByteToWrite; 
			}		
		}	 
	};	 	 
}

FINSH_FUNCTION_EXPORT(flashwrite,flashwrite(pBuffer,writeaddr,size)--wirte data go to flash);
#endif

#endif

