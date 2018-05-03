#include <rtthread.h>
#include <finsh.h>
//#include <dfs_posix.h>
#include "board.h"
#include "rtthread.h"
#include "wm8950.h"
#include "lwip/sockets.h"

#define RT_SEND_UDP     0
#define RT_SEND_SOCKET  1

#define  mempll_block_size      (1024 + 100)    //16384
static rt_uint8_t mempool[mempll_block_size * 4];
static rt_uint32_t rx_notify = 0;
static rt_size_t   rx_size = 0;
static rt_size_t   thread_exit = 0;

static rt_err_t pcm_rx_notify(rt_device_t dev,  rt_size_t size)
{
    /* release memory block */
    //rt_mp_free(buffer);
    rx_notify = 1;
    rx_size = size;
    return RT_EOK;
}

void pcm_read(void* parameter)
{
	rt_device_t device;
    rt_device_t device_led;
	rt_uint8_t *codec_buffer = RT_NULL;
	rt_uint32_t cnt = 0, ok_cnt = 0;
    rt_size_t  ret = 0;
#if RT_SEND_UDP  
    struct udp_pcb *UdpPcb; 
    struct ip_addr ipaddr; 
    struct pbuf *p; 
#endif   

#if RT_SEND_SOCKET
    int sock;
    //sendto中使用的对方地址
    struct sockaddr_in toAddr;   
    
    //在recvfrom中使用的对方主机地址
    //struct sockaddr_in fromAddr;    
#endif    
    
    rt_kprintf("pcm_read() thread enter!\n");
	/* open audio device and set tx done call back */
    device = rt_device_find("snd");
    if(device == RT_NULL)
    {
        rt_kprintf("audio device not found!\r\n");
        return;
    }
	
    //rt_device_set_tx_complete(device, wav_tx_done);
	rt_device_set_rx_indicate(device, pcm_rx_notify);
    rt_device_open(device, RT_DEVICE_OFLAG_WRONLY);		// init() function has been called when open

#if RT_SEND_UDP  
    // lwip 初始化
    p = pbuf_alloc(PBUF_RAW,sizeof(mempool),PBUF_RAM); 
    //p->payload=(void *)mempool; 
    
    IP4_ADDR(&ipaddr,172,16,12,55); 
    UdpPcb = udp_new(); 
 
    udp_bind(UdpPcb,IP_ADDR_ANY,1025);
    udp_connect(UdpPcb,&ipaddr,1024); 
#endif
    
#if RT_SEND_SOCKET
    sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(sock < 0)
    {
        rt_kprintf("create socket failed.\r\n");
        return;
    }   
    memset(&toAddr,0,sizeof(toAddr));
    toAddr.sin_family=AF_INET;
    toAddr.sin_addr.s_addr=inet_addr("172.16.12.55");
    toAddr.sin_port = htons(4000);
        
#endif    


	/* find and open led device*/
    device_led = rt_device_find("led");
    if(device_led == RT_NULL)
    {
        rt_kprintf("led device not found!\r\n");
        return;
    }
    rt_device_open(device_led, RT_DEVICE_OFLAG_WRONLY);		// init() function has been called when open
    
    while(!thread_exit)
    {
        if(rx_notify == 1)
        {
            //codec_buffer = rt_malloc(rx_size+4);
            codec_buffer = mempool;
            if(codec_buffer == RT_NULL)
            {
                rt_kprintf("rt_malloc() failed! no enough memory.\n");
                break;
            }
            else
            {
                if(rx_size >= (mempll_block_size * 4))
                {
                    rx_size = (mempll_block_size * 4);
                }
                ret = rt_device_read(device, 0, codec_buffer, rx_size);
                //ret = 1;
                if(ret <= 0)
                {
                    rt_kprintf("rt_device_read() error: num = %d!\n", ret);
                }
                else
                {
                    //rt_kprintf("r(%d)\n", ret);
#if RT_SEND_UDP  
                    p->len = p->tot_len = rx_size;
                    memcpy(p->payload, codec_buffer, rx_size);
                    udp_send(UdpPcb,p); 
#endif   
                    ok_cnt++;
                    rt_device_write(device_led, 0, &ok_cnt, sizeof(int));
                   
#if RT_SEND_SOCKET
//#if 0                   
                    if(sendto(sock,codec_buffer,rx_size,0,(struct sockaddr*)&toAddr,sizeof(toAddr)) != rx_size)
                    {
                         rt_kprintf("sendto() failed.\r\n");
                         //closesocket(sock);
                         //return;
                    }
#endif                    
                    //rt_kprintf("%d\n", ok_cnt);
                }
                //rt_free(codec_buffer);
            }
            rx_notify = 0;
        }
        else
        {
            cnt++;
            //rt_thread_sleep(1);   //延迟1个OS Tick
            if(cnt > 0x0004ffff)
            {
                //break;
                //rt_device_control(device, CODEC_CMD_RESET, RT_NULL);
                cnt = 0;
            }
        }
    }
    rt_device_close(device_led);
    closesocket(sock);
    //rt_device_control(device, CODEC_CMD_RESET, RT_NULL);
    /* close device and file */
    rt_device_close(device);
    rt_kprintf("pcm_read() thread stop successfully!\n");
}

//FINSH_FUNCTION_EXPORT(pcm_read, pcm read test. e.g: pcm_read())


/* 打开采集线程*/
rt_thread_t pcm_on(void)
{
	rt_thread_t tid;

	tid = rt_thread_create("pcmcap",
							pcm_read, RT_NULL,
							2048, (RT_THREAD_PRIORITY_MAX/3 - 1), 20);

	if (tid != RT_NULL)
    {
        thread_exit = 0;
		rt_thread_startup(tid);
        //rt_kprintf("pcm_read() thread start successfully!\n");
    }
	return tid;
}
FINSH_FUNCTION_EXPORT(pcm_on, pcm read on.)


/* 关闭采集线程*/
int pcm_off(void)
{
    thread_exit = 1;
    return 0;
}
FINSH_FUNCTION_EXPORT(pcm_off, pcm read off.)


