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
#include "async_noti.h"
#include <pthread.h>

int fd;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void analyse_psu_info(unsigned int msg_data)
{
    info.data = msg_data;

    if (PSU != data_id) return;
    if (PLUG_IN == data_info) {
        //do something
        printf("PSU change: psu%d plugged in\n", data_no);
    } else if (PLUG_OUT == data_info) {
        //do something
        printf("PSU change: psu%d plugged out\n", data_no);
    } else if (WORK_FAULT == data_info) {
        printf("PSU change: psu%d work fault\n", data_no);
    } else if (WORK_GOOD == data_info) {
        printf("PSU change: psu%d work good\n", data_no);
    }
}


void psu_signal_fun(int signum)
{
	unsigned int  *info_arr;
	int i, ret;
    unsigned long msg_num = 0;

    if (info_arr)

    printf("signal handle fun, get a signal\n");
    pthread_mutex_lock(&mutex);
	ret = ioctl(fd, READ_PSU_INFO, &msg_num);
    if (ret < 0){
        printf("ioctrl failed, ret = %d\n", ret);
        return;
    }

		printf("ret = %d, MSG num : %d\n", ret, msg_num);
	if (ret == 0 && msg_num > 0) {
		info_arr = malloc(msg_num * sizeof(unsigned int));
		if (!info_arr) {
            printf("No memory\n");
            return;
        }

        ret = read(fd, info_arr, sizeof(unsigned int) * msg_num);
        if (ret < 0) {
            printf("msg_num is not correct\n");
            return;
        } else if (ret != msg_num)
		    printf("info msg num left %d\n", ret);

        for (i = 0; i < msg_num; ++i) {
			//printf("psu key_val: 0x%x\n", info_arr[i]);
            analyse_psu_info(info_arr[i]);
		}
	}

    free(info_arr);
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

	fcntl(fd, F_SETOWN, getpid());

	/* get the device open method */
	Oflags = fcntl(fd, F_GETFL);

	/* set fasync */
	fcntl(fd, F_SETFL, Oflags | FASYNC);

    pthread_mutex_init(&mutex, NULL); /* init mutex */

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
	signal(SIGIO, psu_signal_fun);

    while (1)
	{
		sleep(100);
		//time(&timep);
		//printf("%s",ctime(&timep));
		printf("wake up!\n");
	}

	return 0;
}

