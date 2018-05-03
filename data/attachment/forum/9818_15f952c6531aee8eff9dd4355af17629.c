#include <board.h>
#include <rtthread.h>
#include "gpio.h"
#include "echo.h"


struct rx_msg
{
    rt_device_t dev;
    rt_size_t size;
};
static struct rt_messagequeue rx_mq;
static char uart_rx_buffer[64];
static char msg_pool[2048];


//串口侦听回调函数
rt_err_t uart_input(rt_device_t dev,rt_size_t size)
{
    struct rx_msg msg;
    msg.dev=dev;
    msg.size=size;
   rt_mq_send(&rx_mq,&msg,sizeof(struct rx_msg));//将接收消息放入消息队列
    return RT_EOK;
}

void usr_echo_thread_entry(void* parameter)
{
  //  int count=0;
    struct rx_msg msg; 

    rt_device_t device;//write_device;
    rt_err_t result=RT_EOK;
   //GPIO_Configuration();
    
    device=rt_device_find("uart1");
    if(device!=RT_NULL)
    {
//设置回调函数，打开设备
        rt_device_set_rx_indicate(device,uart_input);
        rt_device_open(device,RT_DEVICE_OFLAG_RDWR);
    }
 //   write_device=device;
    while(1)
    {
			//从消息队列中读取消息
        result=rt_mq_recv(&rx_mq,&msg,sizeof(struct rx_msg),50);
        if(result==-RT_ETIMEOUT)
        {
           //rt_kprintf("timeout count:%d\n,++count");
        }
//成功收到消息
        if(result==RT_EOK)
        {
            rt_uint32_t rx_length;

            rx_length=(sizeof(uart_rx_buffer)-1)>msg.size?
                msg.size:sizeof(uart_rx_buffer)-1;
//读取消息
            rx_length=rt_device_read(msg.dev,0,&uart_rx_buffer[0],rx_length);
            uart_rx_buffer[rx_length]='\0';
//写到写设备中
   //         if(write_device!=RT_NULL)
						rt_device_write(device,0,&uart_rx_buffer[0],rx_length);
               // rt_device_write(write_device,0,&uart_rx_buffer[0],rx_length);
        } 
    }
}

void usr_echo_init(void)
{
		 rt_thread_t thread;
		 rt_err_t result;
	//创建消息队列，分配队列存储空间
		result= rt_mq_init(&rx_mq,
                    "mqt",
                   &msg_pool[0],
                   128 - sizeof (void*),
                   sizeof(msg_pool),
                    RT_IPC_FLAG_FIFO);
		if(result!=RT_EOK)
    {
        rt_kprintf("init message falied\n");
        return;
    }
				
    thread=rt_thread_create("devt",
                   usr_echo_thread_entry,RT_NULL,
                   1024,25,7);
		 if (thread != RT_NULL) 
		 rt_thread_startup(thread);
	 }
