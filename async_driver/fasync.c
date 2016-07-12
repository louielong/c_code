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
#include <pthread.h>

int fd, first_read = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void psu_signal_fun(int signum)
{
	unsigned int  *info_arr;
	int i, ret;
    unsigned long msg_num;
#if 0
    if (first_read == 0) {
	    ret = ioctl(fd, READ_ACK, &msg_num);
        first_read = 1;
        printf("ret = %d, msg_num = %d\nfirst read, do some init\n", ret, msg_num);
		return ;
    }

	ret = ioctl(fd, READ_PSU_INFO, &msg_num);
    if (ret < 0){
        printf("ioctrl failed\n");
    }

	if (ret == 0 && msg_num > 0) {
		info_arr = malloc(ret * sizeof(unsigned int));
		read(fd, info_arr, msg_num);
		printf("info msg num: %d\n", msg_num);
		for (i = 0; i < msg_num; ++i) {
			printf("key_val: 0x%x\n", info_arr[i]);
		}
	} else if (msg_num == 0) {
        printf("get signal, do something\n");
    }

    if (info_arr)
        free(info_arr);
#endif
    pthread_mutex_lock(&mutex);
	ret = read(fd, &i, sizeof(int));
	printf("ret = %d\n", ret);
	if (ret) {
        printf("read failed\n");
        return ;
    }
    printf("key_val: %d\n", i);
    pthread_mutex_unlock(&mutex);
}

int main(int argc, char **argv)
{
	unsigned char key_val;
	int ret, Oflags, num;
	time_t timep;
    struct sigaction act;
    sigset_t mask;
    unsigned long msg_num;

	/* open device */
	fd = open("/dev/swctrl_psu", O_RDWR);
	if (fd < 0) {
		printf("can't open %s\n", PSU_DEVICE_NAME);
		return -1;
	} else
		printf("%s open success\n", PSU_DEVICE_NAME);

	//signal(PICA8_FAN_SIG, fan_signal_fun);
	//signal(PICA8_PORT_SIG, port_signal_fun);

#if 0
    /* real time signal  */
    sigemptyset(&mask);
    sigaddset(&mask, PICA8_PSU_SIG);
    act.sa_sigaction = psu_signal_fun;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(PICA8_PSU_SIG, &act, NULL) < 0) {
            printf("install RTsigal error\n");
    }
#endif

	/*将filp->owner设置为当前的进程
	*filp所指向的文件可读或者可写就会给filp->owner发消息
	*/
	fcntl(fd, F_SETOWN, getpid());

	/* get the device open method */
	Oflags = fcntl(fd, F_GETFL);

	/* set fasync */
	fcntl(fd, F_SETFL, Oflags | FASYNC);

    pthread_mutex_init(&mutex, NULL);

    ret = ioctl(fd, READ_INIT_SYNC, &msg_num);
    if (0 == ret) {
        if (READ_SYNC_ACK == msg_num)
            printf("init sync success\n");
        else {
            printf(" init sync failed\n");
            return -1;
        }
    } else
        printf("ioctrl failed\n");

	/* set signal handle function */
	signal(PICA8_PSU_SIG, psu_signal_fun);

    while (1)
	{
		sleep(100);
		//time(&timep);
		//printf("%s",ctime(&timep));
		printf("wake up!\n");
	}

	return 0;
}

