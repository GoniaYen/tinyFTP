#include <stdio.h>  //printf
#include <arpa/inet.h>  //inet_addr htons
#include <sys/types.h>
#include <sys/socket.h>  //socket bind listen accept connect
#include <netinet/in.h>  //sockaddr_in
#include <stdlib.h>  //exit
#include <unistd.h>  //close
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define N 128
#define errlog(errmsg) do{\
							perror(errmsg);\
							printf("%s --> %s --> %d\n", __FILE__, __func__, __LINE__);\
							exit(1);\
						 }while(0)

void do_help()
{
	printf("*****************************************************\n");
	printf("*****         输入  /   功能          ***************\n");
	printf("*****         list  /   查看服务器所在目录的文件*****\n");
	printf("***** get filename  /   下载服务器所在目录的文件*****\n");
	printf("***** put filename  /   上传文件到服务器*************\n");
	printf("*****         quit  /   退出          ***************\n");
	printf("*****************************************************\n");

	return ;
}

void do_list(int sockfd)
{
	char buf[N] = {};

	//告知服务器执行查看目录文件名的功能
	strcpy(buf, "L");
	send(sockfd, buf, N, 0);

	//接收数据并打印
	while(1)
	{
		recv(sockfd, buf, N, 0);

		//结束标志
		if(strncmp(buf, "OVER****", 8) == 0)
		{
			break;
		}

		printf("*** %s\n", buf);
	}

	printf("文件接收完毕\n");
	
	return ;
}

int do_download(int sockfd, char *filename)
{
	char buf[N] = {};
	int fd;
	ssize_t bytes;

	//发送指令以及文件名，告知服务器实现下载的功能
	sprintf(buf, "G %s", filename);
	send(sockfd, buf, N, 0);

	//接收数据判断文件是否存在
	recv(sockfd, buf, N, 0);

	//如果文件不存在，打印指令
	if(strncmp(buf, "NO", 2) == 0)
	{
		printf("文件%s不存在，请重新输入\n", filename);
		return -1;
	}
	
	//如果文件存在，创建文件
	if((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0664)) < 0)
	{
		errlog("fail to open");
	}
	
	//接收数据并写入文件
	while((bytes = recv(sockfd, buf, N, 0)) > 0)
	{
		if(strncmp(buf, "OVER****", 8) == 0)
		{
			break;
		}

		write(fd, buf, bytes);
	}

	printf("文件下载完毕\n");

	return -1;
}

int do_upload(int sockfd, char *filename)
{
	char buf[N] = {};
	int fd;
	ssize_t bytes;

	//打开文件，判断文件是否存在
	if((fd = open(filename, O_RDONLY)) < 0)
	{
		//如果文件不存在，则退出函数，重新输入
		if(errno == ENOENT)
		{
			printf("文件%s不存在，请重新输入\n", filename);
			return -1;
		}
		else
		{
			errlog("fail to open");
		}
	}

	//如果文件存在，告知服务器执行上传的功能
	sprintf(buf, "P %s", filename);
	send(sockfd, buf, N, 0);

	//读取文件内容并发送
	while((bytes = read(fd, buf, N)) > 0)
	{
		send(sockfd, buf, bytes, 0);
	}

	sleep(1);

	strcpy(buf, "OVER****");
	send(sockfd, buf, N, 0);

	printf("文件上传完毕\n");
	return 0;
}

int main(int argc, const char *argv[])
{
	int sockfd;
	struct sockaddr_in serveraddr;
	socklen_t addrlen = sizeof(serveraddr);
	char buf[N] = {};

	if(argc < 3)
	{
		printf("您输入的参数太少了: %s <ip> <port>\n", argv[0]);
		exit(1);
	}

	//第一步：创建套接字
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		errlog("fail to socket");
	}

	//第二步：填充服务器网络信息结构体
	//inet_addr：将点分十进制ip地址转化为网络字节序的整型数据
	//htons：将主机字节序转化为网络字节序
	//atoi：将数字型字符串转化为整型数据
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	serveraddr.sin_port = htons(atoi(argv[2]));

	//第三步：发送客户端的连接请求
	if(connect(sockfd, (struct sockaddr *)&serveraddr, addrlen) < 0)
	{
		errlog("fail to connect");
	}

	printf("************************\n");
	printf("***请输入help查看选项***\n");
	printf("************************\n");

	while(1)
	{
		printf("input >>> ");
		//输入指令作出相应的判断
		fgets(buf, N, stdin);   //help  list  get+filename put+filename quit
		buf[strlen(buf) - 1] = '\0';

		if(strncmp(buf, "help", 4) == 0)
		{
			do_help();
		}
		else if(strncmp(buf, "list", 4) == 0)
		{
			do_list(sockfd);
		}
		else if(strncmp(buf, "get", 3) == 0)
		{
			do_download(sockfd, buf + 4);   //get filename
		}
		else if(strncmp(buf, "put", 3) == 0)
		{
			do_upload(sockfd, buf + 4);  //put filename
		}
		else if(strncmp(buf, "quit", 4) == 0)
		{
			close(sockfd);
			break;
		}
		else
		{	
			printf("您输入的有误，请输入正确的选项\n");
		}
	}

	return 0;
}
