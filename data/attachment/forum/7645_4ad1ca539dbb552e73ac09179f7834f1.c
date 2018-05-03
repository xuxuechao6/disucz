#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#define SERV_PORT	2345	//端口2345,不能小于1024,1-1024为系统保留端口
#define BACKLOG		5		//最多允许的客户端连接数
#define MAXDATASIZE	100	//接收缓冲区长度

void process_cli(int connfd, struct sockaddr_in client);
void *function(void* arg);
struct ARG 
{
	int connfd;
	struct sockaddr_in client;
};


void main(void* parameter)
{
	int listenfd,connfd;
	pthread_t  tid;
	struct ARG *arg;
	struct sockaddr_in server;
	struct sockaddr_in client;
	socklen_t  len;
	
	int opt =SO_REUSEADDR;

	if ((listenfd =socket(AF_INET, SOCK_STREAM, 0)) == -1) 
	{
		printf("Creatingsocket failed.");
		exit(1);
	}

	setsockopt(listenfd,SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

//	bzero(&server,sizeof(server));
	memset(&(server.sin_zero), 0, sizeof(server.sin_zero));
	server.sin_family=AF_INET;
	server.sin_port=htons(SERV_PORT);
	server.sin_addr.s_addr= htonl (INADDR_ANY);
	if (bind(listenfd,(struct sockaddr *)&server, sizeof(server)) == -1) 
	{
		printf("Bind()error.");
		exit(1);
	}

	if(listen(listenfd,BACKLOG)== -1)
	{
		printf("listen()error\n");
		exit(1);
	}

	len=sizeof(client);
	while(1)
	{
		if ((connfd =accept(listenfd,(struct sockaddr *)&client,&len))==-1) 
		{
			printf("accept() error\n");
			exit(1);
		}
		arg = (struct ARG *)malloc(sizeof(struct ARG));
		arg->connfd =connfd;
		 memcpy((void*)&arg->client, &client, sizeof(client));

		if(pthread_create(&tid, NULL, function, (void*)arg)) 
		{
			printf("Pthread_create() error");
			exit(1);
		}
	}
	close(listenfd);
}


void process_cli(int connfd, struct sockaddr_in client)
{
	int num;
	int i;
	char recvbuf[MAXDATASIZE], sendbuf[MAXDATASIZE], cli_name[MAXDATASIZE];

	printf("You got a connection from %s. \n ",inet_ntoa(client.sin_addr) );
	num = recv(connfd,cli_name, MAXDATASIZE,0);
	if (num == 0) 
	{
		close(connfd);
		printf("Clientdisconnected.\n");
		return;
	}
	cli_name[num] ='\0';
	printf("Client'sname is %s.\n",cli_name);

	while (num =recv(connfd, recvbuf, MAXDATASIZE,0)) 
	{
		recvbuf[num] ='\0';
		printf("Receivedclient( %s ) message: %s\n",cli_name, recvbuf);
		
		for (i = 0; i <num - 1; i++) 
		{
			sendbuf[i] =recvbuf[i];
		}
		sendbuf[num] = '\0';
		send(connfd,sendbuf,strlen(sendbuf),0);
	}
	close(connfd);
}

void* function(void* arg)
{
	struct ARG *info;
	info = (struct ARG*)arg;
	process_cli(info->connfd,info->client);
	free (arg);
	pthread_exit(NULL);
}