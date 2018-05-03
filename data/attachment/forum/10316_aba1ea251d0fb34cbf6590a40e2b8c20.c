#include <rtthread.h>
#include <lwip/api.h>
#include <stdint.h>
#include "telnet.h"
#include "usart.h"

#define PORTSERVER_RX_BUFFER  128
#define PORTSERVER_TX_BUFFER  128

#define MAX_PORT_NUM		6

struct telnet_session *portserver[MAX_PORT_NUM];


/* RT-Thread Device Driver Interface */
static rt_err_t portserver_init(rt_device_t dev)
{
    return RT_EOK;
}

static rt_err_t portserver_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

static rt_err_t portserver_close(rt_device_t dev)
{
    return RT_EOK;
}

static rt_err_t portserver_control(rt_device_t dev, rt_uint8_t cmd, void *args)
{
    return RT_EOK;
}

static rt_size_t portserver_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size)
{
    rt_size_t result;
	int port = (int)dev->user_data;
	
	rt_kprintf("%s %d: portserver_read: port %d\r\n", __FILE__, __LINE__, port);
    /* read from rx ring buffer */
    rt_sem_take(portserver[port]->rx_ringbuffer_lock, RT_WAITING_FOREVER);
    result = rb_get(&(portserver[port]->rx_ringbuffer), buffer, size);
    rt_sem_release(portserver[port]->rx_ringbuffer_lock);

    return result;
}

static rt_size_t portserver_write(rt_device_t dev, rt_off_t pos, const void* buffer, rt_size_t size)
{
    const rt_uint8_t *ptr;
	int port = (int)dev->user_data;

	rt_kprintf("%s %d: portserver_write: port %d\r\n", __FILE__, __LINE__, port);
	
    ptr = (rt_uint8_t*)buffer;

    rt_sem_take(portserver[port]->tx_ringbuffer_lock, RT_WAITING_FOREVER);
    while (size)
    {
        if (*ptr == '\n')
            rb_putchar(&portserver[port]->tx_ringbuffer, '\r');

        if (rb_putchar(&portserver[port]->tx_ringbuffer, *ptr) == 0)  /* overflow */
            break;
        ptr++;
		size--;
    }
    rt_sem_release(portserver[port]->tx_ringbuffer_lock);

    /* send to network side */
    rt_event_send(portserver[port]->nw_event, NW_TX);

    return (rt_uint32_t)ptr - (rt_uint32_t)buffer;
}

/* portserver rx callback function */
void portserver_rx_callback(struct netconn *conn, enum netconn_evt evt, rt_uint16_t len)
{
	err_t neterr;
	ip_addr_t addr;
	u16_t port = 0;

	neterr = netconn_getaddr(conn, &addr, &port, 1);
	if (neterr || port < 2001 || port > (2000 + MAX_PORT_NUM))
	{
		rt_kprintf("%s %d: ip addr or port error: neterr %d, port %d\r\n",
            __FILE__, __LINE__, neterr, port);
		return;
	}
    else
        rt_kprintf("portserver_callback port %d...\n", port);

	port -= 2001;
	if (portserver[port] != NULL)
	{
        if (evt == NETCONN_EVT_RCVPLUS)
        {
            rt_event_send(portserver[port]->nw_event, NW_RX);
            rt_kprintf("portserver_callback: port %d receive event\n", port+1);
        }
        
        if (conn->state == NETCONN_CLOSE)
        {
            rt_event_send(portserver[port]->nw_event, NW_CLOSED);
            rt_kprintf("portserver_callback: port %d close event\n", port+1);
        }
	}
}

/* process rx data */
void portserver_rx(struct telnet_session* telnet, rt_uint8_t *data, rt_size_t length,
          rt_device_t dev, const char *devname)
{
    rt_size_t rx_length, index;
	rt_uint8_t tx_buffer[32];
	rt_kprintf("portserver_rx in %s.........\r\n", devname);

    for (index = 0; index < length; index ++)
    {
        switch(telnet->state)
        {
        case STATE_IAC:
            if (*data == TELNET_IAC)
            {
                /* take semaphore */
                rt_sem_take(telnet->rx_ringbuffer_lock, RT_WAITING_FOREVER);
                /* put buffer to ringbuffer */
                rb_putchar(&(telnet->rx_ringbuffer), *data);
                /* release semaphore */
                rt_sem_release(telnet->rx_ringbuffer_lock);

                telnet->state = STATE_NORMAL;
            }
            else
            {
                /* set telnet state according to received package */
                switch (*data)
                {
                case TELNET_WILL: telnet->state = STATE_WILL; break;
                case TELNET_WONT: telnet->state = STATE_WONT; break;
                case TELNET_DO:   telnet->state = STATE_DO; break;
                case TELNET_DONT: telnet->state = STATE_DONT; break;
                default: telnet->state = STATE_NORMAL; break;
                }
            }
            break;
        
        /* don't option */
        case STATE_WILL:
        case STATE_WONT:
            telnet_send_option(telnet, TELNET_DONT, *data);
            telnet->state = STATE_NORMAL;
            break;

        /* won't option */
        case STATE_DO:
        case STATE_DONT:
            telnet_send_option(telnet, TELNET_WONT, *data);
            telnet->state = STATE_NORMAL;
            break;

        case STATE_NORMAL:
            if (*data == TELNET_IAC)
                telnet->state = STATE_IAC;
            else if (*data != '\r') /* ignore '\r' */
            {
                rt_sem_take(telnet->rx_ringbuffer_lock, RT_WAITING_FOREVER);
                /* put buffer to ringbuffer */
                rb_putchar(&(telnet->rx_ringbuffer), *data);
                rt_sem_release(telnet->rx_ringbuffer_lock);
            }
            break;
        }

        data++;
    }

	memset(tx_buffer, 0, sizeof(tx_buffer));
    rt_sem_take(telnet->rx_ringbuffer_lock, RT_WAITING_FOREVER);
    /* get total size */
    rx_length = rb_available(&telnet->rx_ringbuffer);
	rx_length = rb_get(&(telnet->rx_ringbuffer), tx_buffer, sizeof(tx_buffer));
    rt_sem_release(telnet->rx_ringbuffer_lock);

	//write the received data to uart
	if (rx_length)
	{
		rt_kprintf("portserver_rx write to %s: %s\r\n", devname, tx_buffer);
		rt_device_write(dev, 0, tx_buffer, rx_length);
	}

    /* indicate there are reception data */
    //if ((rx_length > 0) && (telnet->device.rx_indicate != RT_NULL))
    //    telnet->device.rx_indicate(&telnet->device, rx_length);
}

rt_err_t uart_input(rt_device_t dev, rt_size_t size)
{
	int port = get_uart_num(dev);
	rt_uint8_t tx_buffer[32];
	rt_size_t  length, i;
	
	if (port < 0 || port > MAX_PORT_NUM)
	{
		rt_kprintf("%s %d: incorrect port %d\r\n", __FILE__, __LINE__, port);
		return RT_ERROR;
	}

    rt_sem_take(portserver[port]->tx_ringbuffer_lock, RT_WAITING_FOREVER);
	while (size > 0)
	{
		if (size < 32)
		{
			length = rt_device_read(dev, 0, tx_buffer, size);
			size = 0;
		}
		else
		{
			length = rt_device_read(dev, 0, tx_buffer, 32);
			size -= 32;
		}

		for (i = 0; i < length; i++)
		{
    		/* put buffer to ringbuffer */
    		rb_putchar(&(portserver[port]->tx_ringbuffer), tx_buffer[i]);
		}
	}
    
    rt_sem_release(portserver[port]->tx_ringbuffer_lock);
	
	rt_event_send(portserver[port]->nw_event, NW_UART);
    return RT_EOK;
}


void portserver_thread(void* parameter)
{
    struct netbuf *buf;
    struct netconn *conn, *newconn;
	rt_device_t device = RT_NULL;
	rt_uint32_t event;
	char uartname[8], portname[8];
    err_t err;
	rt_err_t result;
	int port = (int)parameter;

	if ((port < 0) || (port > MAX_PORT_NUM))
	{
		rt_kprintf("port number %d is invalid...\r\n", port);
		return;
	}
	
	memset(uartname, 0, sizeof(uartname));
	memset(portname, 0, sizeof(portname));
	rt_sprintf(uartname, "uart%d", port+1);
	rt_sprintf(portname, "port%d", port+1);
	rt_kprintf("port server thread for port%d and %s\r\n", port+1, uartname);
	device = rt_device_find(uartname);
	if (device == RT_NULL)
	{
		rt_kprintf("device %s: not found! thread exit\r\n", uartname);
		return;
	}
	
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, NULL, port + 2001);
	netconn_listen(conn);

	/* register telnet device */
    portserver[port]->device.type     = RT_Device_Class_Char;
    portserver[port]->device.init     = portserver_init;
    portserver[port]->device.open     = portserver_open;
    portserver[port]->device.close    = portserver_close;
    portserver[port]->device.read     = portserver_read;
    portserver[port]->device.write    = portserver_write;
    portserver[port]->device.control  = portserver_control;
    /* user data is port */
	portserver[port]->device.user_data = parameter;

	/* register telnet device */
    rt_device_register(&portserver[port]->device, portname,
                       RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STREAM);

	while(1)
	{
		/* Grab new connection. */
        err = netconn_accept(conn, &newconn);
        if (err != RT_EOK)
        {
        	rt_thread_delay(20);
			continue;
		}

		/* set network rx call back */
        newconn->callback = portserver_rx_callback;
		rt_kprintf("port %d connected, open %s...\r\n", port+1, uartname);

		/* only connected to open uart */
		rt_device_set_rx_indicate(device, uart_input);
		if (rt_device_open(device, RT_DEVICE_OFLAG_RDWR|RT_DEVICE_FLAG_INT_RX
		 	|RT_DEVICE_FLAG_STREAM) != RT_EOK)
		{
			rt_kprintf("device %s: open failed!\r\n", uartname);
			continue;
		}

		portserver[port]->state = STATE_NORMAL;
		while(1)
		{
			/* try to send all data in tx ringbuffer */
            telnet_process_tx(portserver[port], newconn);

			/* receive network event */
            result = rt_event_recv(portserver[port]->nw_event,
                                   NW_MASK, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                                   RT_TICK_PER_SECOND, &event);
            if (result == RT_EOK)
            {
            	/* get event successfully */
                if (event & NW_RX)
                {
                	rt_kprintf("%s %d: portserver_thread nw_rx event...\r\n", __FILE__, __LINE__);
                    /* do a rx procedure */
                    err= netconn_recv(newconn, &buf);
                    if (buf != RT_NULL)
                    {
                        rt_uint8_t *data;
                        rt_uint16_t length;

                        /* get data */
                        netbuf_data(buf, (void**)&data, &length);
                        portserver_rx(portserver[port], data, length, device, uartname);
                        /* release buffer */
                        netbuf_delete(buf);
                    }
                    else if (newconn->last_err == ERR_CLSD)
                    {
                        /* close connection */
    					netconn_close(conn);
						rt_device_close(device);
						rt_kprintf("%s %d: portserver_thread error close\r\n", __FILE__, __LINE__);
                        break;
                    }
                }
                else if ((event & NW_TX)||(event & NW_UART))
                {
                    telnet_process_tx(portserver[port], newconn);
					rt_kprintf("%s %d: portserver_thread nw_tx event %x...\r\n", __FILE__, __LINE__, event);
                }
                else if (event & NW_CLOSED)
                {
                    /* process close */
                    netconn_close(conn);
					rt_device_close(device);
					rt_kprintf("%s %d: portserver_thread event close\r\n", __FILE__, __LINE__);
                    break;
                }
            }
			
			rt_thread_delay(5);
		}
	}
}

void portserver_svr(void)
{
    rt_thread_t tid;
	rt_uint8_t *ptr;
	char semName[10];
	rt_uint8_t i;
	
	for (i = 1; i < MAX_PORT_NUM; i++)
	{
		portserver[i] = rt_malloc(sizeof(struct telnet_session));
		if (portserver[i] == NULL)
		{
			rt_kprintf("portserver thread: no memory\n");
			return;
		}

		/* init ringbuffer */
        ptr = rt_malloc(PORTSERVER_RX_BUFFER);
        rb_init(&(portserver[i]->rx_ringbuffer), ptr, PORTSERVER_RX_BUFFER);
        /* create rx ringbuffer lock */
		memset(semName, 0, sizeof(semName));
		rt_sprintf(semName, "rxuart%d", (i+1));
        portserver[i]->rx_ringbuffer_lock = rt_sem_create(semName, 1, RT_IPC_FLAG_FIFO);
        ptr = rt_malloc (PORTSERVER_TX_BUFFER);
        rb_init(&(portserver[i]->tx_ringbuffer), ptr, PORTSERVER_TX_BUFFER);
        /* create tx ringbuffer lock */
		memset(semName, 0, sizeof(semName));
		rt_sprintf(semName, "txuart%d", (i+1));
        portserver[i]->tx_ringbuffer_lock = rt_sem_create(semName, 1, RT_IPC_FLAG_FIFO);

        /* create network event */
		memset(semName, 0, sizeof(semName));
		rt_sprintf(semName, "evtuart%d", (i+1));
        portserver[i]->nw_event = rt_event_create(semName, RT_IPC_FLAG_FIFO);

		memset(semName, 0, sizeof(semName));
		rt_sprintf(semName, "portsvr%d", (i+1));
		tid = rt_thread_create(semName, portserver_thread, (void *)i,
                           1024, 26+i, 5+i);
    	if (tid != RT_NULL)
        	rt_thread_startup(tid);
	}
}

