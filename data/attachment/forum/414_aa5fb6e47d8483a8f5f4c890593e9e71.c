#include <string.h>
#include <rtthread.h>
#include "board.h"
#include "rtdevice.h"

volatile int ITM_RxBuffer = ITM_RXBUFFER_EMPTY; /* used for Debug Input */

static struct rt_device swo_dev;

/* RT-Thread device interface */
static rt_err_t rt_swo_init (rt_device_t dev)
{
    return RT_EOK;
}

static rt_err_t rt_swo_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

static rt_err_t rt_swo_close(rt_device_t dev)
{
    return RT_EOK;
}

static rt_size_t rt_swo_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_size_t count = 0;
    char * ptr = (char *)buffer;

    while(size--)
    {
        if(ITM_CheckChar())
        {
            *ptr++ = ITM_ReceiveChar();
            count++;
        }
        else
        {
            break;
        }
    }

    return count;
}

static rt_size_t rt_swo_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    char *ptr;

    ptr = (char *)buffer;

    if (dev->flag & RT_DEVICE_FLAG_STREAM)
    {
        /* stream mode */
        while (size)
        {
            if (*ptr == '\n')
            {
                ITM_SendChar('\r');
            }

            ITM_SendChar(*ptr);

            ptr ++;
            size --;
        }
    }
    else
    {
        while (size != 0)
        {
            ITM_SendChar(*ptr);

            ptr++;
            size--;
        }
    }

    return (rt_size_t) ptr - (rt_size_t) buffer;
}

static struct rt_thread swo_thread;
static rt_uint8_t swo_thread_stack[768];
static void swo_monitor_thread_entry(void* parameter)
{
    while(1)
    {
        if(ITM_CheckChar())
        {
            if(swo_dev.rx_indicate != RT_NULL)
            {
                swo_dev.rx_indicate(&swo_dev, 1);
            }

            while(ITM_CheckChar());
        }
    }
}

void rt_hw_swo_init(void)
{
    memset(&swo_dev, 0, sizeof(swo_dev));

    /* device initialization */
    swo_dev.type = RT_Device_Class_Char;

    /* device interface */
    swo_dev.init 	    = rt_swo_init;
    swo_dev.open 	    = rt_swo_open;
    swo_dev.close       = rt_swo_close;
    swo_dev.read 	    = rt_swo_read;
    swo_dev.write       = rt_swo_write;

    rt_device_register(&swo_dev,
                       "swo",
                       RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STREAM);
}

void rt_hw_swo_start(void)
{
    rt_err_t result;

    result = rt_thread_init(&swo_thread,
                            "swo",
                            swo_monitor_thread_entry,
                            RT_NULL,
                            &swo_thread_stack[0],
                            sizeof(swo_thread_stack),
                            RT_THREAD_PRIORITY_MAX - 1,
                            1);

    if (result == RT_EOK)
    {
        rt_thread_startup(&swo_thread);
    }
}
