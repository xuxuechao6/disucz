#include <rtthread.h>
#include <rtdef.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h> 

#define malloc      rt_malloc
#define realloc     rt_realloc
#define free        rt_free

#define DEBUG_MODE

#ifdef DEBUG_MODE
#define DEBUG_PRINTF    rt_kprintf
#else
#define DEBUG_PRINTF(...)
#endif

char server[] = "www.lewei50.com";
char head[160];
char *user_data = RT_NULL;
int user_str_length = 0;

rt_bool_t begin = RT_FALSE;
rt_bool_t end = RT_FALSE;

#ifdef RT_USING_FINSH
#include <finsh.h>
int lewei_append(const char * name, int value)
{
    int length;
    char * ptr;

    if(begin == RT_FALSE)
    {
        user_data = (char *)malloc(2);
        if(user_data == NULL)
        {
            return -1;
        }

        user_data[0] = '[';
        user_data[1] = 0;
        user_str_length = 1;

        begin = RT_TRUE;
    }

    if(user_data == NULL)
    {
        return -1;
    }

    length  = 23; /* >>{"Name":"","Value":""},<< */
    length += 8;  /* name */
    length += 10; /* value */

    ptr = (char *)realloc(user_data, user_str_length + length + 1);
    if(ptr == NULL)
    {
        return -1;
    }
    user_data = ptr;

    ptr = user_data + user_str_length;

    length = rt_sprintf(ptr,
            "{\"Name\":\"%s\",\"Value\":\"%d\"},",
            name,
            value);

    user_str_length += length;
    DEBUG_PRINTF("\n append:%s\r\n", ptr);

	return RT_EOK;
}

void leweiclient_init(const char * user_key, const char * gateway)
{
    char *ptr = head;
    int head_length = 0;
    int tmp;

    user_data = NULL;
    user_str_length = 0;

    begin = RT_FALSE;
    end = RT_FALSE;

    // build head.
	 tmp = rt_sprintf(ptr,
                "POST /api/V1/gateway/UpdateSensors/%s HTTP/1.1\r\n",
                gateway);
	
    head_length += tmp;
    ptr += tmp;

    // build userkey.
    tmp = rt_sprintf(ptr,
                  "userkey: %s\r\n",
                  user_key);
    head_length += tmp;
    ptr += tmp;

    // build Host.
    tmp = rt_sprintf(ptr, "Host: www.lewei50.com\r\n");
    head_length += tmp;
    ptr += tmp;

    // build User-Agent.
    // tmp = rt_sprintf(ptr, "User-Agent: RT-Thread ART\r\n");
    // head_length += tmp;
    // ptr += tmp;
}

int lewei_connect(void)
{
	int sock;
	char *url;
	
	struct hostent *host;
	struct sockaddr_in server_addr;

	url = server;
	host = gethostbyname(url);
	
	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		rt_kprintf("Socket() failed\n");
		return -RT_ERROR;
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80);
	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
    rt_memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));
	
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) ==  -1)
	{
		rt_kprintf("Connect() failed\n");
		closesocket(sock);
		return -RT_ERROR;
	}

	return sock;
}

int lewei_send(void)
{
    int result = 0;
	int bytes, sock;
	
	char send_data[128];
	char recv_data[1024];

	sock = lewei_connect();

	bytes = send(sock, head, strlen(head), 0);
	rt_kprintf("%d bytes, %s\n", bytes, head);

	rt_memset(send_data, '\0', sizeof(send_data));
	rt_sprintf(send_data, "Content-Length: %d\r\n", strlen(user_data) + 1);
	bytes = send(sock, send_data, strlen(send_data), 0);
	DEBUG_PRINTF("%d bytes, %s\n", bytes, send_data);
	
	rt_memset(send_data, '\0', sizeof(send_data));
	rt_sprintf(send_data, "Connection: close\r\n");
	bytes = send(sock, send_data, strlen(send_data), 0);
	DEBUG_PRINTF("%d bytes, %s\n", bytes, send_data);;

	rt_memset(send_data, '\0', sizeof(send_data));
	rt_sprintf(send_data, "\r\n");
	bytes = send(sock, send_data, strlen(send_data), 0);
	DEBUG_PRINTF("%d bytes, %s\n", bytes, send_data);
	
	bytes = send(sock, user_data, strlen(user_data), 0);
	DEBUG_PRINTF("%d bytes, %s\n", bytes, user_data);;

	rt_memset(send_data, '\0', sizeof(send_data));
	rt_sprintf(send_data, "]");
	bytes = send(sock, send_data, strlen(send_data), 0);
	DEBUG_PRINTF("%d bytes, %s\n", bytes, send_data);

	DEBUG_PRINTF("data:%s]\r\n", user_data);

	rt_memset(recv_data, '\0', sizeof(recv_data));
	while ((bytes = recv(sock, recv_data, sizeof(recv_data) - 1, 0)) > 0);
	
	result = 0;

send_exit:
	begin = RT_FALSE;
	end   = RT_FALSE;
	closesocket(sock);

    if(user_data != NULL)
    {
        free(user_data);
    }

    return result;
}
FINSH_FUNCTION_EXPORT(lewei_send, send data to www.lewei50.com)
FINSH_FUNCTION_EXPORT(lewei_append, append data)
#endif
