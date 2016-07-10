#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/workqueue.h>
//#include "../cpld/cpld.h"
#include "async_noti.h"

#define FAN_NUM         4	//BOX hardware FAN number
#define SFP_NUM         0	//BOX hardware sfp port number
#define QSFP_NUM        32	//BOX hardware qsfp port number
#define PSU_NUM         2       //BOX hardware psu num

#define PORT_SFP_DEV_START  (1)
#define PORT_SFP_DEV_TOTAL  (PORT_SFP_DEV_START + 0)
#define PORT_QSFP_DEV_START (PORT_SFP_DEV_TOTAL)
#define PORT_QSFP_DEV_TOTAL (PORT_QSFP_DEV_START + 32 - 1)


dev_t dev_id;			/* device num */
struct cdev async_cdev;		/* char device */
struct class *async_class;
struct fasync_struct *async_noti;

struct semaphore event_sem;

struct workqueue_struct *check_wq;	/* work queue struct */
struct delayed_work check_dwq;

typedef struct {
	int mode;
	char psu_first_read;
	char fan_first_read;
	char port_first_read;
	char who_read;
}read_flag;
read_flag *swctrl_flag;

/* msg record struct */
typedef struct {
    unsigned int status_change;       /* status change count number */
    unsigned char status_record[16];      /* record device status */
    unsigned int status_change_arr[64];  /* record device status change arr */
}status_record;

status_record *psu_record;
status_record *fan_record;
status_record *port_record;
#if 0
/* msg record array */
static void add_hardware_info(status_info hw_info, status_record *record)
{
	if (down_interruptible(&event_sem) < 0)
		return;

	record->status_change_arr[record->status_change] = hw_info.data;
	++record->status_change;

	up(&event_sem);

	return;
}

/* PSU Present CPLD 0x60
 * 0x02
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |       | PSU2  | PSU2  | PSU2  |       | PSU1  | PSU1  | PSU1  |
 * |       | ALERT |  OK   | PRSNT |       | ALERT |  OK   | PRSNT |
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * Get psu present  stauts
 * return 1 when plug in, 0 when plug out, -1 when failed
 */
static char get_one_psu_present(unsigned short cpld_addr,
	unsigned char psu_addr, int psu_num)
{
    int value;
    unsigned char offest;

    value = cpld_read(cpld_addr, psu_addr);
    if (value < 0) {
	printk(KERN_ALERT "get psu%d present status failed\n", psu_num);
	return 0;
    }

    offest = psu_num == 1? PSU1_PRESENT_OFFEST : PSU2_PRESENT_OFFEST;
    value = (vaule >> offest) & 0x01;
    if (value == 0)
	return 1;
    else
	return 0;
}


/* PSU Present CPLD 0x60
 * 0x02
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |       | PSU2  | PSU2  | PSU2  |       | PSU1  | PSU1  | PSU1  |
 * |       | ALERT |  OK   | PRSNT |       | ALERT |  OK   | PRSNT |
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * Get psu power stauts
 * return 1 when work good, 0 when plug out, -1 when failed
 */
static char get_one_psu_power(unsigned short cpld_addr,
	unsigned char psu_addr, int psu_num)
{

    int value;
    unsigned char offest;

    value = cpld_read(cpld_addr, psu_addr);
    if (value < 0) {
	printk(KERN_ALERT "get psu%d power status failed\n", num);
	return 0;
    }

    offest = psu_num == 1? PSU1_OK_OFFEST : PSU2_OK_OFFEST;
    value = (value >> offest) & 0x01;
    if (value == 1)
	return 1;
    else
	return 0;
}

/* pack psu status into 8-bits format
 * return 8-bits status info
 */
unsigned char pack_psu_status(int num)
{
    int i;
    unsigned char status, tmp[8], shift;

    /* psu standard format
     * +-------------------------------------------------------+
     * | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
     * +------+------+------+------+------+------+------+------+
     * |      |      | PSU2 | PSU2 |      |      | PSU1 | PSU1 |
     * |      |      |  OK  | PRSNT|      |      |  OK  | PRSNT|
     * +------+------+------+------+------+------+------+------+
     *
     */

    /* psu1 present and power status*/
    tmp[0] = get_one_psu_present(SYS_CPLD1_ADDR, PSU_STATUS_REG, 1);
    tmp[1] = get_one_psu_power(SYS_CPLD1_ADDR, PSU_STATUS_REG, 1);

    /* psu2 present and power status*/
    tmp[4] = get_one_psu_present(SYS_CPLD1_ADDR, PSU_STATUS_REG, 2);
    tmp[5] = get_one_psu_power(SYS_CPLD1_ADDR, PSU_STATUS_REG, 2);

    status = 0;
    for (i = 0; i < 4 * num; ++i) {
	shift = 0x01;
	shift = shift << i;
	/* status & (~shift) -> clear bit i in status */
	/* (tmp << i) & shift -> shift tmp bit 0 to bit i and get bit i */
	status = (status & (~shift)) | ((tmp[i] << i) & shift);
    }

    return status;
}


/* read PSU infomation
 * mode=0 init read psu info
 * mode=1 status change check mode
 */
static void get_psu_info(unsigned char psu_status,
	unsigned char status_rec_offest, unsigned char mode)
{
	unsigned char temp;

	info.data = 0;
	data_id = PSU;

	if (0 == mode) {	/* psu init info read mode */
		/* psu status record */
		psu_record->status_record[status_rec_offest] = psu_status;
		return;
	}

	temp = psu_record->status_record[status_rec_offest];
	if (1 == mode) {	/* psu status check mode */
		if ((psu_status & 0x03) != (temp & 0x03)) {
			data_no = 1;	/* psu1 status change */
			if ((psu_status & 0x01) < (temp & 0x01)) {
				/* psu1 present status present -> not present */
				data_info = PLUG_OUT;
				add_hardware_info(info, psu_record);
			}
		} else if ((psu_status & 0x01) > (temp & 0x01)) {
			/* psu1 present status not present -> present */
			if ((psu_status & 0x02) > (temp & 0x02)) {
				/* psu1 work status fail -> good */
				data_info = WORK_GOOD;
				add_hardware_info(info, psu_record);
			} else if ((psu_status & 0x02) < (temp & 0x02)) {
				/* psu1 work status good -> fail */
				data_info = WORK_FAULT;
				add_hardware_info(info, psu_record);
			}
		}

		if ((psu_status & 0x30) != (temp & 0x30)) {
			data_no = 2;	/* psu2 status change */
			if ((psu_status & 0x10) < (temp & 0x10)) {
				/* psu2 present status present -> not present */
				data_info = PLUG_OUT;
				add_hardware_info(info, psu_record);
			}
		} else if ((psu_status & 0x10) > (temp & 0x10)) {
			/* psu2 present status not present -> present */
			if ((psu_status & 0x20) > (temp & 0x20)) {
				/* psu2 work status fail -> good */
				data_info = WORK_GOOD;
				add_hardware_info(info, psu_record);
			} else if ((psu_status & 0x20) < (temp & 0x20)) {
				/* psu2 work status good -> fail */
				data_info = WORK_FAULT;
				add_hardware_info(info, psu_record);
			}
		}
		/* psu status record */
		psu_record->status_record[status_rec_offest] = psu_status;
	}
}

#ifdef FAN_PRESENT
/* FAN Present CPLD1 0X60
 * FAN  0x0C
 * FANR 0x17
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
 * +-------+-------+-------+-------+-------+-------+-------+-------|
 * |       |       | FAN6  | FAN5  | FAN4  | FAN3  | FAN2  | FAN1  |
 * |       |       | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT |
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * Get fan present status
 * return 1 when fan is present, 0 when fan is not present, -1 when failed
 */
static char get_one_fan_present(unsigned short cpld_addr, int fan_num,
	int fan_loc)
{
    int value;
    unsigned char offest;

    offest = fan_num - 1;
    value = cpld_read(cpld_addr, FAN_PRESENT_REG);

    if (value < 0) {
	printk(KERN_ALERT "get fan%d present failed\n", fan_num);
	return 1;
    }

    value = (value >> offest) & 0x01;
    if (value == 0)
	return 0;
    else
	return 1;
}
#endif

#ifdef FAN_OK
/* FAN Power
 *
 * Get fan power status
 * return 0 when fan work normally, 1 when fan work abnormally, -1 when failed
 */
static char get_one_fan_power(unsigned short cpld_addr, int num, int fan_loc)
{
    int value;
    unsigned char offest, fan_base_reg;

    fan_base_reg = fan_loc == 1? FAN_FAULT_REG : FANR_FAULT_REG;

    value = cpld_read(cpld_addr, fan_base_reg);

    if (value < 0) {
	printk(KERN_ALERT "get fan%d power status failed\n", num);
	return 0;
    }

    offest = num - 1;
    value = (value >> offest) & 0x01;
    if (value == 0)
	return 0;
    else
	return 1;
}
#endif

/* pack fan status into 8-bits format
 * return 8-bits fan status info
 */
unsigned char pack_fan_status(int offest, int num, int fan_loc)
{
    int i;
    unsigned char status, tmp[8] = 0, shift;

    /*
     * fan standard format
     * +------+------+------+------+------+------+------+------+
     * | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
     * +------+------+------+------+------+------+------+------+
     * | FAN4 | FAN4 | FAN3 | FAN3 | FAN2 | FAN2 | FAN1 | FAN1 |
     * |  OK  | PRSNT|  OK  | PRSNT|  OK  | PRSNT|  OK  | PRSNT|
     * +------+------+------+------+------+------+------+------+
     *
     */

#ifdef FAN_PRESENT
    /* fan present*/
    for (i = 0; i < num; ++i) {
	status = get_one_fan_present(SYS_CPLD1_ADDR, offest * 4 + i + 1, fan_loc);
	tmp[i * 2] = status;
    }
#endif

#ifdef FAN_OK
    /* fan power*/
    for (i = 0; i < num; ++i) {
	status = get_one_fan_power(SYS_CPLD1_ADDR, offest * 4 + i + 1, fan_loc);
	tmp[i * 2 + 1] = status;
    }
#endif
    status = 0;
    for (i = 0; i < num * 2; ++i) {
	shift = 0x01;
	shift = shift << i;
	/* status & (~shift) -> clear bit i in status */
	/* (tmp << i) & shift -> shift tmp bit 0 to bit i and get bit i */
	status = (status & (~shift)) | ((tmp[i] << i) & shift);
    }

    return status;
}

/* read FAN infomation;
 * mode=0 init fan info read
 * mode=1 status change check mode
 *
 * fan standard format
 * +------+------+------+------+------+------+------+------+
 * | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
 * +------+------+------+------+------+------+------+------+
 * | FAN4 | FAN4 | FAN3 | FAN3 | FAN2 | FAN2 | FAN1 | FAN1 |
 * |  OK  | PRSNT|  OK  | PRSNT|  OK  | PRSNT|  OK  | PRSNT|
 * +------+------+------+------+------+------+------+------+
 *
 */
static void get_fan_info(unsigned char fan_status, int fan_num_offest,
	int status_rec_offest, unsigned char mode, int num, int fan_loc)
{
    int i;
    unsigned char tmp;
    unsigned char shift;

    info.data = 0;
    data_id = fan_loc == 1? FAN : FANR;

    if (0 == mode) {	/* init fan info read mode */
	/* fan status record */
	fan_record->status_record[status_rec_offest] = fan_status;
	return;
    }

    tmp = fan_record->status_record[status_rec_offest];
    if (1 == mode) {	/* fan status change check mode */
	if (tmp == fan_status)
	    return;	/* fan status not change */

	for (i = 0; i < num; ++i) {
#ifdef FAN_PRESENT
		shift = 0x01 << (2 * i);
		if ((fan_status & shift) > (tmp & shift)) {
		    data_info = PLUG_IN; /* fan status not present -> present */
		    data_no = fan_num_offest * 4 + i + 1;
		    add_hardware_info(info);
		} else if ((fan_status & shift) < (tmp & shift)) {
		    data_info = PLUG_OUT;/* fan status present -> not present */
		    data_no = fan_num_offest * 4 + i + 1;
		    add_hardware_info(info);
		}
#endif

#ifdef FAN_OK
	    shift = 0x01 << (2 * i + 1);
	    if ((fan_status & shift) > (tmp & shift)) {
		data_info = WORK_FAULT;	/* fan status norm -> abnorm */
		data_no = fan_num_offest * 4 + i + 1;
		add_hardware_info(info);
	    } else if ((fan_status & shift) < (tmp & shift)) {
		data_info = WORK_GOOD;	/* fan i+1 status abnorm -> norm */
		data_no = fan_num_offest * 4 + i + 1;
		add_hardware_info(info);
	    }
	}
#endif

	/* fan status record */
	fan_record->status_record[status_rec_offest] = fan_status;
    }
}

/* QSFP CPLD2-3 0x60  0x64
 * 0x0A 0x0B
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * | QSFP8 | QSFP7 | QSFP6 | QSFP5 | QSFP4 | QSFP3 | QSFP2 | QSFP1 |
 * | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT |
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 *
 *
 * Get qsfp plug in stauts
 * return 1 when plug in, 0 when plug out, -1 when failed
 */
static int get_one_qsfp_present(int port)
{
    int value = 0;
    int port_num = port - 1;
    u8 cpld_addr, cpld_reg, offset;

    cpld_addr = (port_num < 16) ? SYS_CPLD2_ADDR: SYS_CPLD3_ADDR;
    cpld_reg = (port_num / 8) % 2 ? CPLD_QSFP_PRESENT_REG_2: CPLD_QSFP_PRESENT_REG_1;
    offset = port_num % 8;

    value = cpld_read(cpld_addr, cpld_reg);

    if (value < 0) {
	printk(KERN_ALERT "get qsfp port %d failed\n", port);
	return 1;
    }
    if (((value >> offest) & 0x01) == 0)
	return 1;
    else
	return 0;
}

/* pack qsfp status into standard 8-bit format
 *
 */
static unsigned char pack_qsfp_status(int offest, int num)
{
    int i;
    unsigned char status, tmp[8], shift;

    /*qsfp standard format
     * +-------+-------+-------+-------+-------+-------+-------+-------+
     * | bit7  | bit6  | bit5  | bit4  | bit3  | bit2  | bit1  | bit0  |
     * +-------+-------+-------+-------+-------+-------+-------+-------+
     * | QSFP8 | QSFP7 | QSFP6 | QSFP5 | QSFP4 | QSFP3 | QSFP2 | QSFP1 |
     * | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT | PRSNT |
     * +-------+-------+-------+-------+-------+-------+-------+-------+
     *
     */

    for (i = 0; i < num; ++i) {
	tmp[i] = get_one_qsfp_present(i + 8 * offest + 1);
    }

    status = 0;
    for (i = 0; i < num; ++i) {
	shift = 0x01;
	shift = shift << i;
	/* status & (~shift) -> clear bit i in status */
	/* (tmp << i) & shift -> shift tmp bit 0 to bit i and get bit i */
	status = (status & (~shift)) | ((tmp[i] << i) & shift);
    }

    return status;
}

/* read port infomation
 * mode=0 init port info read
 * mode=1 status change check mode
 */
static void get_port_info(unsigned char port_status,
	unsigned char status_rec_offest, unsigned char mode)
{
	unsigned int i;
	unsigned char temp;
	unsigned char shift = 0;
#ifdef AS5712
	/*
	 * modify QSFP hw cpld memory mismatching
	 * deatil information see as5712 datasheet
	 */
	unsigned short qsfp_port_num[6] = { 49, 52, 50, 53, 51, 54 };
#endif

	info.data = 0;
	data_id = PORT;

	if (0 == mode) {	/* init port info read mode */
		/* port status record */
		ports_record->status_record[status_rec_offest] = port_status;
		return;
	}

	temp = ports_record->status_record[status_rec_offest];
	if (1 == mode) {
		/* port status change check mode */
		if (temp == port_status)
			return;	/* port status not change */
		for (i = 0; i < 8; ++i) {
			if (i + 1 + 8 * offest >= PORT_QSFP_DEV_TOTAL)
				break;	/* MAX port num */

			shift = 0x01 << i;
			if ((port_status & shift) < (temp & shift)) {
				data_info = PLUG_OUT;	/* status plug in -> out */
				data_no = i + 1 + 8 * offest;
#ifdef AS5712
				if (data_no >= 49) {
					data_no = qsfp_port_num[i];
#endif
				}
				add_hardware_info(info, ports_record);
			} else if ((port_status & shift) > (temp & shift)) {
				data_info = PLUG_IN;	/* status plug out -> in */
				data_no = i + 1 + 8 * offest;
#ifdef AS5712
				if (data_no >= 49) {
					data_no = qsfp_port_num[i];
#endif
				}
				add_hardware_info(info, ports_record);
			}
		}

		/* port status record */
		ports_record->status_record[status_rec_offest] = port_status;
	}
}
#endif
/* hardware check workqueue function */
static void check_info(struct work_struct *work)
{
	//int i, num, rec_num, port_start;
	//unsigned char status;

	if (swctrl_flag->mode != 0)
		swctrl_flag->mode = 1;
#if 0
	status_arr = kmalloc(sizeof(unsigned int) * 64, GFP_KERNEL);
	if (!status_arr) {
		printk(KERN_ALERT "no memory for status_arr\n");
		return;
	}
#endif
#if 0
	/* PSU */
	rec_num = 0;
	status = pack_psu_status(PSU_NUM);
	get_psu_info(status, rec_num, mode);

	/* FAN 1-6 */
	rec_num = 0;
	for (i = 0; i < (FAN_NUM + 3) / 4; ++i) {
	    num = FAN_NUM - i * 4;
	    num = num >= 4? 4 : num;
	    status = pack_fan_status(i, num, FAN);
	    get_fan_info(status, i, count, mode, num, FAN);
	    ++rec_num;
	}

	/* FANR 1-6 */
	for (i = 0; i < (FAN_NUM + 3) / 4; ++i) {
	    num = FAN_NUM - i * 4;
	    num = num >= 4? 4 : num;
	    status = pack_fan_status(i, num, FANR);
	    get_fan_info(status, i, count, mode, num, FANR);
	    ++rec_num;
	}

	/* SFP 1-32 */
	rec_num = 0;
	port_start = PORT_QSFP_DEV_START;
	for (i = 0; i < (SFP_NUM + 7) / 8; ++i) {
		num = QSFP_NUM -i * 8;
		num = num >= 8? 8 : num;
		status = pack_sfp_status(i, num);
		get_port_info(status, rec_num, mode, num, port_start);
		port_start += num;
		++rec_num; /* the count of port in record array */
	}
#endif
	/* if have status change or first check, release signal */
#if 0
	if (psu_record->status_change > 0 || swctrl_flag->mode == 0)
		kill_fasync(&(async_noti), PICA8_PSU_SIG, POLL_IN);
	if (fan_record->status_change > 0 || swctrl_flag->mode == 0)
		kill_fasync(&(async_noti), PICA8_FAN_SIG, POLL_IN);
	if (port_record->status_change > 0 || swctrl_flag->mode == 0)
		kill_fasync(&(async_noti), PICA8_PORT_SIG, POLL_IN);
#endif
	printk(KERN_ALERT "driver alive\n");
	queue_delayed_work(check_wq, &check_dwq, 3 * HZ);
	++swctrl_flag->mode;
}

/* device driver open function */
int async_open(struct inode *inode, struct file *file)
{
	printk(KERN_ALERT "async open\n");
	return 0;
}

/* device driver read function */
ssize_t async_read(struct file * file, char __user * buf, size_t size,
		   loff_t * ppos)
{
	int tmp = 0;

	printk(KERN_ALERT "async read\n");

	if (down_interruptible(&event_sem) < 0) {
	    return -ERESTARTSYS;
	}

	if (swctrl_flag->psu_first_read == 0) {
	    swctrl_flag->psu_first_read = 1;
	    up(&event_sem);
	    return 255;
	}
	if (swctrl_flag->fan_first_read == 0) {
	    swctrl_flag->fan_first_read = 1;
	    up(&event_sem);
	    return 255;
	}
	if (swctrl_flag->port_first_read == 0) {
	    swctrl_flag->port_first_read = 1;
	    up(&event_sem);
	    return 255;
	}

	if (swctrl_flag->who_read == PSU && psu_record->status_change > 0) {
	    if (copy_to_user(buf, psu_record->status_change_arr,\
			sizeof(unsigned int) * psu_record->status_change)) {
		    up(&(event_sem));
		    return -EFAULT;
	    }
	    tmp = psu_record->status_change;
	    psu_record->status_change = 0;
	}

	if (swctrl_flag->who_read == FAN && fan_record->status_change > 0) {
	    if (copy_to_user(buf, fan_record->status_change_arr,\
			sizeof(unsigned int) * fan_record->status_change)) {
		    up(&(event_sem));
		    return -EFAULT;
	    }
	    tmp = psu_record->status_change;
	    fan_record->status_change = 0;
	}

	if (swctrl_flag->who_read == PORT && port_record->status_change > 0) {
	    if (copy_to_user(buf, port_record->status_change_arr,\
			sizeof(unsigned int) * port_record->status_change)) {
		    up(&(event_sem));
		    return -EFAULT;
	    }
	    tmp = psu_record->status_change;
	    port_record->status_change = 0;
	}

	up(&event_sem);

	return tmp;
}

/* device driver ioctl function */
long async_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	printk(KERN_ALERT "async ioctl\n");

	switch (cmd) {

	case READ_PSU_INFO:	/* get psu info number */
		ret = put_user(psu_record->status_change, \
			       (unsigned long __user *)arg);
		swctrl_flag->who_read = PSU;
		break;
	case READ_FAN_INFO:	/* get fan info number */
		ret = put_user(fan_record->status_change, \
			       (unsigned long __user *)arg);
		swctrl_flag->who_read = FAN;
		break;
	case READ_PORT_INFO:	/* get port info number */
		ret = put_user(port_record->status_change, \
			       (unsigned long __user *)arg);
		swctrl_flag->who_read = PORT;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

/* device driver close function */
int async_release(struct inode *inode, struct file *file)
{
	printk(KERN_ALERT "async close\n");

	return 0;
}

/* info fasync function */
int fasync(int fd, struct file *filp, int on)
{
	printk(KERN_ALERT "fasync_helper is calling\n");

	return fasync_helper(fd, filp, on, &async_noti);
}

struct file_operations fasync_fops = {
	.owner = THIS_MODULE,
	.open = async_open,
	.read = async_read,
	.release = async_release,
	.fasync = fasync,
	.unlocked_ioctl = async_ioctl,
};

static int __init dev_init(void)
{
	int ret;

	swctrl_flag = kzalloc(sizeof(read_flag), GFP_KERNEL);
	if (!swctrl_flag) {
		printk(KERN_ALERT "not have enough memory\n");
		return -1;
	}

	ret = alloc_chrdev_region(&dev_id, 0, 1, DEVICE_NAME);
	if (ret) {
		printk(KERN_ALERT "can't get major number\n");
		unregister_chrdev_region(dev_id, 1);
		return ret;
	} else {
		printk(KERN_ALERT "get device major number success\n");
	}

	cdev_init(&async_cdev, &fasync_fops);	/* init cdev */

	ret = cdev_add(&async_cdev, dev_id, 1);
	if (ret) {
		printk(DEVICE_NAME " error %d adding device\n", ret);
		unregister_chrdev_region(dev_id, 1);
		return ret;
	} else
		printk(DEVICE_NAME " chardev register ok\n");

	async_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(async_class)) {
		printk("Err: failed in creating class\n");
		unregister_chrdev_region(dev_id, 1);
		return -1;
	}
	device_create(async_class, NULL, dev_id, NULL, DEVICE_NAME);
	printk(KERN_ALERT " Registered character driver\n");

	sema_init(&event_sem, 1);	/* init mutex */

	check_wq = create_workqueue("check_wq");
	if (!check_wq) {
		printk(KERN_ALERT "no memory for workqueue\n");
		return -ENOMEM;
	}
	printk(KERN_ALERT "create workqueue successful\n");

	INIT_DELAYED_WORK(&check_dwq, check_info);
	queue_delayed_work(check_wq, &check_dwq, 3 * HZ);

	return ret;
}

static void __exit dev_exit(void)
{
	device_destroy(async_class, dev_id);
	class_destroy(async_class);
	unregister_chrdev_region(dev_id, 1);
	cdev_del(&async_cdev);

	if (check_wq) {
		cancel_delayed_work(&check_dwq);
		flush_workqueue(check_wq);
		destroy_workqueue(check_wq);
	}
	if (swctrl_flag)
		kfree(swctrl_flag);

	printk(DEVICE_NAME
	       " Async Notification char driver clean up\n");
}

module_init(dev_init);
module_exit(dev_exit);

MODULE_LICENSE("GPL");
MODULE_VERSION("V0.09");
MODULE_DESCRIPTION("asynchronous notification driver");
