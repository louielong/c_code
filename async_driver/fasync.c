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

int fd_psu, fd_fan, fd_port;
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

void analyse_fan_info(unsigned int msg_data)
{
    info.data = msg_data;

    if (FAN != data_id) return;
    if (PLUG_IN == data_info) {
        //do something
        printf("FAN change: psu%d plugged in\n", data_no);
    } else if (PLUG_OUT == data_info) {
        //do something
        printf("FAN change: psu%d plugged out\n", data_no);
    } else if (WORK_FAULT == data_info) {
        printf("FAN change: psu%d work fault\n", data_no);
    } else if (WORK_GOOD == data_info) {
        printf("FAN change: psu%d work good\n", data_no);
    }
}

void analyse_port_info(unsigned int msg_data)
{
    info.data = msg_data;

    if (PORT != data_id) return;
    if (PLUG_IN == data_info) {
        //do something
        printf("PORT change: port%d plugged in\n", data_no);
    } else if (PLUG_OUT == data_info) {
        //do something
        printf("PORT change: port%d plugged out\n", data_no);
    }
}


void psu_signal_fun(int signum)
{
	unsigned int  *info_arr;
	int i, ret;
    unsigned long msg_num = 0;

    if (info_arr)

    printf("psu signal handle fun, get a signal\n");
    pthread_mutex_lock(&mutex);
	ret = ioctl(fd_psu, READ_PSU_INFO, &msg_num);
    if (ret < 0){
        printf("psu ioctrl failed, ret = %d\n", ret);
        return;
    }

	if (ret == 0 && msg_num > 0) {
		if (msg_num > MAX_MSG) {
			printf("psu msg num error, msg_num : %ld\n", msg_num);
			return;
		}
		info_arr = malloc(msg_num * sizeof(unsigned int));
		if (!info_arr) {
            printf("psu malloc No memory\n");
            return;
        }

        ret = read(fd_psu, info_arr, sizeof(unsigned int) * msg_num);
        if (ret < 0) {
            printf("psu msg_num is not correct\n");
            return;
        } else if (ret != msg_num)
		    printf("psu info msg num left %d\n", ret);

        for (i = 0; i < msg_num; ++i) {
			//printf("psu key_val: 0x%x\n", info_arr[i]);
            analyse_psu_info(info_arr[i]);
		}
	}

    free(info_arr);
    pthread_mutex_unlock(&mutex);
}

void fan_signal_fun(int signum)
{
	unsigned int  *info_arr;
	int i, ret;
    unsigned long msg_num = 0;

    if (info_arr)

    printf("fan signal handle fun, get a signal\n");
    pthread_mutex_lock(&mutex);
	ret = ioctl(fd_fan, READ_FAN_INFO, &msg_num);
    if (ret < 0){
        printf("fan ioctrl failed, ret = %d\n", ret);
        return;
    }

	if (ret == 0 && msg_num > 0) {
		if (msg_num > MAX_MSG) {
		printf("fan msg num error, msg_num : %ld\n", msg_num);
			return;
		}
		info_arr = malloc(msg_num * sizeof(unsigned int));
		if (!info_arr) {
            printf("fan malloc No memory\n");
            return;
        }

        ret = read(fd_fan, info_arr, sizeof(unsigned int) * msg_num);
        if (ret < 0) {
            printf("fan msg_num is not correct\n");
            return;
        } else if (ret != msg_num)
		    printf("fan info msg num left %d\n", ret);

        for (i = 0; i < msg_num; ++i) {
			//printf("FAN key_val: 0x%x\n", info_arr[i]);
            analyse_fan_info(info_arr[i]);
		}
	}

    free(info_arr);
    pthread_mutex_unlock(&mutex);
}

void port_signal_fun(int signum)
{
	unsigned int  *info_arr;
	int i, ret;
    unsigned long msg_num;

    if (info_arr)

    printf("port signal handle fun\n");
    pthread_mutex_lock(&mutex);
	ret = ioctl(fd_port, READ_PORT_INFO, &msg_num);
    if (ret < 0){
        printf("port ioctrl failed\n");
        return;
    }

	if (ret == 0 && msg_num > 0) {
		if (msg_num > 128) {
			printf("port msg num error, msg_num = %ld\n", msg_num);
			return;
		}
		info_arr = malloc(msg_num * sizeof(unsigned int));
		if (!info_arr) {
            printf("port malloc No memory\n");
            return;
        }

        ret = read(fd_port, info_arr, sizeof(unsigned int) * msg_num);
        if (ret < 0) {
            printf("port msg_num is not correct\n");
            return;
        } else if (ret != msg_num)
		    printf("port info msg num left %d\n", ret);

        for (i = 0; i < msg_num; ++i) {
			//printf("PORT key_val: 0x%x\n", info_arr[i]);
            analyse_port_info(info_arr[i]);
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

	/**********************************/
	/*         open psu device        */
	/**********************************/
	fd_psu = open("/dev/swctrl_psu", O_RDWR);
	if (fd_psu < 0) {
		printf("can't open %s\n", PSU_DEVICE_NAME);
		return -1;
	} else
		printf("%s open success\n", PSU_DEVICE_NAME);

	fcntl(fd_psu, F_SETOWN, getpid());

	/* get the device open method */
	Oflags = fcntl(fd_psu, F_GETFL);

	/* set fasync */
	fcntl(fd_psu, F_SETFL, Oflags | FASYNC);

	/**********************************/
	/*         open fan device        */
	/**********************************/
	fd_fan = open("/dev/swctrl_fan", O_RDWR);
	if (fd_psu < 0) {
		printf("can't open %s\n", FAN_DEVICE_NAME);
		return -1;
	} else
		printf("%s open success\n", FAN_DEVICE_NAME);

	fcntl(fd_fan, F_SETOWN, getpid());
	Oflags = fcntl(fd_fan, F_GETFL);
	fcntl(fd_fan, F_SETFL, Oflags | FASYNC);

	/**********************************/
	/*         open port device       */
	/**********************************/
	fd_port = open("/dev/swctrl_port", O_RDWR);
	if (fd_port < 0) {
		printf("can't open %s\n", PORT_DEVICE_NAME);
		return -1;
	} else
		printf("%s open success\n", PORT_DEVICE_NAME);

	fcntl(fd_port, F_SETOWN, getpid());
	Oflags = fcntl(fd_port, F_GETFL);
	fcntl(fd_port, F_SETFL, Oflags | FASYNC);

	pthread_mutex_init(&mutex, NULL); /* init mutex */

    ret = ioctl(fd_psu, READ_INIT_SYNC, &msg_num);
    if (0 == ret) {
        if (READ_SYNC_ACK == msg_num)
            printf("psu init sync success\n");
        else {
            printf("psu init sync failed\n");
            return -1;
        }
    } else
        printf("psu ioctrl failed\n");

    ret = ioctl(fd_fan, READ_INIT_SYNC, &msg_num);
    if (0 == ret) {
        if (READ_SYNC_ACK == msg_num)
            printf("fan init sync success\n");
        else {
            printf("fan init sync failed\n");
            return -1;
        }
    } else
        printf("fan ioctrl failed\n");


    ret = ioctl(fd_port, READ_INIT_SYNC, &msg_num);
    if (0 == ret) {
        if (READ_SYNC_ACK == msg_num)
            printf("port init sync success\n");
        else {
            printf("port init sync failed\n");
            return -1;
        }
    } else
        printf("port ioctrl failed\n");

    /* set signal handle function */
	signal(SIGIO, psu_signal_fun);
	signal(SIGIO, fan_signal_fun);
	signal(SIGIO, port_signal_fun);

    while (1)
	{
		sleep(100);
		//time(&timep);
		//printf("%s",ctime(&timep));
		printf("wake up!\n");
	}

	return 0;
}

