#include <stdio.h>	 //printf
#include <arpa/inet.h> //inet_addr htons
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/wait.h>   //socket bind listen accept connect
#include <netinet/in.h> //sockaddr_in
#include <stdlib.h>		//exit
#include <unistd.h>		//close
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
using namespace std;
#define EPOLL_SIZE 50

#define N 128
#define errlog(errmsg)                                              \
	do                                                              \
	{                                                               \
		perror(errmsg);                                             \
		printf("%s --> %s --> %d\n", __FILE__, __func__, __LINE__); \
		exit(1);                                                    \
	} while (0)

void do_list(int acceptfd)
{
	DIR *dirp;
	struct dirent *dir_ent;
	char buf[N] = {};

	//打开目录
	if ((dirp = opendir(".")) == NULL)
	{
		errlog("fail to opendir");
	}

	//读取文件名
	while ((dir_ent = readdir(dirp)) != NULL)
	{
		//隐藏文件不发送
		if (dir_ent->d_name[0] == '.')
		{
			continue;
		}

		//发送文件名
		strcpy(buf, dir_ent->d_name);

		send(acceptfd, buf, N, 0);
	}

	//发送一个结束指令，让对方退出recv函数
	strcpy(buf, "OVER****");
	send(acceptfd, buf, N, 0);

	printf("目录发送完毕\n");

	return;
}

int do_download(int acceptfd, char *filename)
{
	int fd;
	char buf[N] = {};
	ssize_t bytes;

	//打开文件，判断文件是否存在
	if ((fd = open(filename, O_RDONLY)) < 0)
	{
		//如果文件不存在，告知客户端
		if (errno == ENOENT)
		{
			strcpy(buf, "NO");
			send(acceptfd, buf, N, 0);

			return -1;
		}
		else
		{
			errlog("fail to open");
		}
	}

	//如果文件存在，告知客户端
	strcpy(buf, "YES");
	send(acceptfd, buf, N, 0);

	//读取文件内容并发送
	while ((bytes = read(fd, buf, N)) > 0)
	{
		send(acceptfd, buf, bytes, 0);
	}

	// 防止数据粘包
	sleep(1);

	strcpy(buf, "OVER****");
	send(acceptfd, buf, N, 0);

	printf("文件发送完毕\n");

	return 0;
}

int do_upload(int acceptfd, char *filename)
{
	int fd;
	char buf[N] = {};
	ssize_t bytes;

	//创建文件
	if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0664)) < 0)
	{
		errlog("fail to open");
	}

	//接收数据并写入文件
	while ((bytes = recv(acceptfd, buf, N, 0)) > 0)
	{
		if (strncmp(buf, "OVER****", 8) == 0)
		{
			break;
		}

		write(fd, buf, bytes);
	}

	printf("文件接收完毕\n");
	return 0;
}
void *connect_handle(void *argv)
{
	char buf[N] = {};
	int epfd = ((int *)argv)[0];
	epoll_event acceptfd;
	acceptfd.data.fd = ((int *)argv)[1];

	ssize_t bytes;
	while (1)
	{
		bytes = recv(acceptfd.data.fd, buf, N, 0);
		if (bytes == 0)
		{
			epoll_ctl(epfd, EPOLL_CTL_DEL, acceptfd.data.fd, NULL);
			cout << "close client:" << acceptfd.data.fd << endl;
			close(acceptfd.data.fd);
			break;
		}
		else if (bytes < 0)
		{
			if (errno == EAGAIN)
			{
				break;
			}
		}
		else
		{
			printf("buf = %s\n", buf); //L G+filename P+filename
			switch (buf[0])
			{
			case 'L':
				do_list(acceptfd.data.fd);

				break;
			case 'G':

				do_download(acceptfd.data.fd, buf + 2); //G filename
				break;
			case 'P':

				do_upload(acceptfd.data.fd, buf + 2); //p filename
				break;
			}
			break;
		}
	}
	return NULL;
}
int main(int argc, const char *argv[])
{
	pid_t pid;
	int epfd, event_cnt;
	struct epoll_event *ep_events;
	struct epoll_event event;

	int sockfd, acceptfd;
	struct sockaddr_in serveraddr, clientaddr;
	socklen_t addrlen = sizeof(serveraddr);
	char buf[N] = {};
	ssize_t bytes;

	if (chdir("/")==-1)
	{
		perror("chdir() error ");
	}
	if (argc < 3)
	{
		printf("您输入的参数太少了: %s <ip> <port>\n", argv[0]);
		exit(1);
	}

	//第一步：创建套接字
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
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

	//第三步：将套接字域网络信息结构体绑定
	if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	{
		errlog("fail to bind");
	}

	//第四步：将套接字设置为监听状态
	if (listen(sockfd, 5) < 0)
	{
		errlog("fail to listen");
	}

	epfd = epoll_create(EPOLL_SIZE);
	ep_events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * EPOLL_SIZE);
	event.events = EPOLLIN;
	event.data.fd = sockfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event);

	while (1)
	{

		event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
		if (event_cnt == -1)
		{
			perror("epoll_wait() error");
			break;
		}

		for (int i = 0; i < event_cnt; i++)
		{
			if (ep_events[i].data.fd == sockfd)
			{
				addrlen = sizeof(clientaddr);
				acceptfd = accept(sockfd, (struct sockaddr *)&clientaddr, &addrlen);
				//setNonBlackingMode(acceptfd);
				event.events = EPOLLIN; //条件边缘触发
				event.data.fd = acceptfd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, acceptfd, &event);
				cout << "connect client: " << acceptfd << endl;
			}
			else
			{
				pid = fork();
				if (pid == -1)
				{
					perror("fork() error ");
					break;
				}
				if (pid == 0)
				{
					close(sockfd);
					pthread_t id;
					int param[] = {epfd, ep_events[i].data.fd};
					pthread_create(&id, NULL, connect_handle, (void *)param);
					pthread_join(id, NULL);
					//close(ep_events[i].data.fd);
					break;
				}
			}
		}
	}
	close(sockfd);
	close(epfd);
	return 0;
}