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

#define DEVICE_NAME   "buttons_fasync"  //�豸��/dev/buttons_fasync

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
	
	/*���豸*/
	fd = open("/dev/buttons_fasync", O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
		return 0;
	}
	else
		printf("%s open ok!\n",DEVICE_NAME);

	/*Ӧ�ó���׽SIGIO�ź�
	*���ý��̽��յ����źŵĴ�����
	*/
	signal(SIGIO, my_signal_fun);

	/*��filp->owner����Ϊ��ǰ�Ľ���
	*filp��ָ����ļ��ɶ����߿�д�ͻ��filp->owner����Ϣ
	*/
	fcntl(fd, F_SETOWN, getpid());
	
	/*��ø��豸�Ĵ򿪷�ʽ*/
	Oflags = fcntl(fd, F_GETFL); 
	
	/*���ø��ļ��ı�־ΪFASYNC
	*�����ļ��ı�־ΪFASYNC���������������fasync������
	*�����ļ��Ϳ�ʼ�����첽֪ͨ״̬��
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

