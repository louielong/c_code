#ifndef __PICA8_FASYNC_H__
#define __PICA8_FASYNC_H__

/* device node name   /dev/swctrl */
#define DEVICE_NAME   "swctrl"

#define READ_PSU_INFO    0x01
#define READ_FAN_INFO    0x02
#define READ_PORT_INFO  0x03

#define PLUG_OUT        0
#define PLUG_IN         1
#define WORK_FAULT      2
#define WORK_GOOD       3

#define PSU             0
#define FAN             1
#define FANR            2
#define PORT            3

#define PICA8_PSU_SIG		SIGIO
#define PICA8_FAN_SIG		SIGUSR1
#define PICA8_PORT_SIG		SIGUSR2

#pragma pack(1)
typedef union {
	unsigned int data;
	struct {
		unsigned char id;
		unsigned short num;
		unsigned char info;
	} byte;
} status_info;

static status_info info;
#define data_id info.byte.id
#define data_info info.byte.info
#define data_no info.byte.num
#pragma pack()

#endif /* __PICA8_FASYNC_H__ */
