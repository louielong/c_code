#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include "fasync.h"

int fd;

void psu_signal_fun(int signum)
{
	unsigned int  *info_arr;
	int i, ret;
	ret = ioctl(fd, READ_PSU_INFO);
	if (ret = 255)
		return ;
	else if (ret > 0) {
		info_arr = malloc(ret * sizeof(unsigned int));
		read(fd, info_arr, ret);
		for (i = 0; i < ret; ++i) {
			printf("key_val: 0x%x\n", info_arr[i]);
		}
	}
}

void fan_signal_fun(int signum)
{
	unsigned int  *info_arr;
	int i, ret;
	ret = ioctl(fd, READ_FAN_INFO);
	if (ret = 255)
		return ;
	else if (ret > 0) {
		info_arr = malloc(ret * sizeof(unsigned int));
		read(fd, info_arr, ret);
		for (i = 0; i < ret; ++i) {
			printf("key_val: 0x%x\n", info_arr[i]);
		}
	}
}

void port_signal_fun(int signum)
{
	unsigned int  *info_arr;
	int i, ret;
	ret = ioctl(fd, READ_PORT_INFO);
	if (ret = 255)
		return ;
	else if (ret > 0) {
		info_arr = malloc(ret * sizeof(unsigned int));
		read(fd, info_arr, ret);
		for (i = 0; i < ret; ++i) {
			printf("key_val: 0x%x\n", info_arr[i]);
		}
	}
}

int main(int argc, char **argv)
{
	unsigned char key_val;
	int ret;
	int Oflags;
	time_t timep;

	/* open device */
	fd = open("/dev/swctrl", O_RDWR);
	if (fd < 0) {
		printf("can't open %s\n", DEVICE_NAME);
		return -1;
	} else
		printf("%s open success\n", DEVICE_NAME);

	/* set signal handle function */
	signal(PICA8_PSU_SIG, psu_signal_fun);
	//signal(PICA8_FAN_SIG, fan_signal_fun);
	//signal(PICA8_PORT_SIG, port_signal_fun);

	/*将filp->owner设置为当前的进程
	*filp所指向的文件可读或者可写就会给filp->owner发消息
	*/
	fcntl(fd, F_SETOWN, getpid());

	/* get the device open method */
	Oflags = fcntl(fd, F_GETFL);

	/* set fasync */
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

