#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

int fd;

#define DEVICE_NAME   "buttons_fasync"  //设备名/dev/buttons_fasync

void my_signal_fun(int signum)
{
	unsigned char key_val;
	int a = 100;
	int b = 100;
	read(fd, &key_val, 1);
	printf("key_val: 0x%x\n", key_val);
	sleep(5);
}

int main(int argc, char **argv)
{
	unsigned char key_val;
	int ret;
	int Oflags;
	time_t timep;
	
	/*打开设备*/
	fd = open("/dev/buttons_fasync", O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
		return 0;
	}
	else
		printf("%s open ok!\n",DEVICE_NAME);

	/*应用程序捕捉SIGIO信号
	*设置进程接收到该信号的处理函数
	*/
	signal(SIGIO, my_signal_fun);

	/*将filp->owner设置为当前的进程
	*filp所指向的文件可读或者可写就会给filp->owner发消息
	*/
	fcntl(fd, F_SETOWN, getpid());
	
	/*获得该设备的打开方式*/
	Oflags = fcntl(fd, F_GETFL); 
	
	/*设置该文件的标志为FASYNC
	*设置文件的标志为FASYNC将导致驱动程序的fasync被调用
	*这样文件就开始处于异步通知状态了
	*/
	fcntl(fd, F_SETFL, Oflags | FASYNC);
	
	while (1)
	{
		sleep(10);
		//time(&timep);
		//printf("%s",ctime(&timep));
		printf("wake up!\n");
	}
	
	return 0;
}

