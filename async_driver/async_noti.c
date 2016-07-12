#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
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

typedef struct {
    dev_t dev_id;			/* device num */
    struct cdev async_cdev;		/* char device */
    struct class *async_class;
    struct fasync_struct *async_noti;
}dev_node;
dev_node *psu_dev;
dev_node *fan_dev;
dev_node *port_dev;

typedef struct {
    struct workqueue_struct *check_wq;	/* work queue struct */
    struct delayed_work check_dwq;
}wk_que;
wk_que *wk_que_ck;

typedef struct {
    int mode;
    char read;
}read_flag;
read_flag *swctrl_flag;

/* msg record struct */
typedef struct {
    int mode;
    unsigned char status_record[16];      /* record device status */
    unsigned int status_change;       /* status change count number */
    unsigned int status_change_arr[MAX_MSG];  /* record device status change arr */
}sta_record;

sta_record *psu_record;
sta_record *fan_record;
sta_record *port_record;

#if 0
/* msg record array */
static void add_hardware_info(status_info hw_info, status_record *record)
{
	record->status_change_arr[record->status_change] = hw_info.data;
	++record->status_change;

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
 * mode=1 init read psu info
 * mode=2 status change check mode
 */
static void get_psu_info(unsigned char psu_status,
	unsigned char status_rec_offest, unsigned char mode)
{
	unsigned char temp;

	if (0 == mode) return;

	info.data = 0;
	data_id = PSU;

	if (1 == mode) {	/* psu init info read mode */
		/* psu status record */
		psu_record.status_record[status_rec_offest] = psu_status;
		return;
	}

	temp = psu_record.status_record[status_rec_offest];
	if (2 == mode) {	/* psu status check mode */
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
		psu_record.status_record[status_rec_offest] = psu_status;
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

	//if (swctrl_flag->mode != 0)
	//	swctrl_flag->mode = 1;

#if 0
	/* PSU */
	rec_num = 0;
	status = pack_psu_status(PSU_NUM);
	get_psu_info(status, rec_num, psu_record->mode);

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
	//if (swctrl_flag->mode <= 5)
        //kill_fasync(&(psu_dev.async_noti), PICA8_PSU_SIG, POLL_IN);
#if 0
	if (swctrl_flag->mode == 0 || (swctrl_flag->read == 1 && psu_record->status_change > 0))
		kill_fasync(&(async_noti), PICA8_PSU_SIG, POLL_IN);
	if (fan_record->status_change > 0 || swctrl_flag->mode == 0)
		kill_fasync(&(async_noti), PICA8_FAN_SIG, POLL_IN);
	if (port_record->status_change > 0 || swctrl_flag->mode == 0)
		kill_fasync(&(async_noti), PICA8_PORT_SIG, POLL_IN);
#endif
	//printk(KERN_ALERT "psu_record->mode = %d\n", psu_record->mode);
	printk(KERN_ALERT "psu_record->mode = %d\n", 2);
	//if (psu_record->mode == 1)
    //    psu_record->mode = 2;
    //swctrl_flag->mode = 2;
   // psu_record->status_change = 1;
	queue_delayed_work(wk_que_ck->check_wq, &wk_que_ck->check_dwq, 3 * HZ);
}


/************************************************************************/
/*                    PSU device driver                                 */
/************************************************************************/
/* PSU device driver open function */
int psu_dev_open(struct inode *inode, struct file *file)
{
	printk(KERN_ALERT "PSU async open\n");
	return 0;
}

/* PSU device driver read function */
ssize_t psu_dev_read(struct file * file, char __user * buf, size_t size,
		   loff_t * ppos)
{
	int tmp = 0;

	printk(KERN_ALERT "PSU dev read\n");
	if (psu_record->mode == 1) {
        tmp = copy_to_user(buf, &swctrl_flag->mode, sizeof(int));
	    if (tmp) {
		    return -EFAULT;
	    }
	}

	if (psu_record->status_change > 0) {
	    if (copy_to_user(buf, psu_record->status_change_arr,\
			sizeof(unsigned int) * psu_record->status_change)) {
		    return -EFAULT;
	    }
	    tmp = psu_record->status_change;
	    psu_record->status_change = 0;
        memset(psu_record->status_change_arr, 0, MAX_MSG);
	}

	return tmp;
}

/* PSU device driver ioctl function */
long psu_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	printk(KERN_ALERT "PSU dev ioctl\n");

	switch (cmd) {
	case READ_INIT_SYNC:	/* init sync */
		/* init psu_record->data */
        psu_record->mode = 1;
        psu_record->status_change = 0;
        memset(psu_record->status_record, 0, 16);
        memset(psu_record->status_change_arr, 0, MAX_MSG);
		ret = put_user(READ_SYNC_ACK, (unsigned long __user *)arg);
		break;
	case READ_PSU_INFO:	/* get psu info number */
		ret = put_user(psu_record->status_change, \
			       (unsigned long __user *)arg);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

/* PSU device driver close function */
int psu_dev_release(struct inode *inode, struct file *file)
{
	/* clean psu_record->data */
    psu_record->mode = 0;
    psu_record->status_change = 0;
    memset(psu_record->status_record, 0, 16);
    memset(psu_record->status_change_arr, 0, MAX_MSG);
    printk(KERN_ALERT "PSU dev close\n");

	return 0;
}

/* PSU fasync function */
int psu_dev_fasync(int fd, struct file *filp, int on)
{
	printk(KERN_ALERT "PSU fasync_helper is calling\n");

	return fasync_helper(fd, filp, on, &psu_dev->async_noti);
}

/* PSU file struct */
struct file_operations psu_dev_fops = {
	.owner = THIS_MODULE,
	.open = psu_dev_open,
	.read = psu_dev_read,
	.release = psu_dev_release,
	.fasync = psu_dev_fasync,
	.unlocked_ioctl = psu_dev_ioctl,
};

/************************************************************************/
/*                    FAN device driver                                 */
/************************************************************************/
/* FAN device driver open function */
int fan_dev_open(struct inode *inode, struct file *file)
{
	printk(KERN_ALERT "FAN async open\n");
	return 0;
}

/* FAN device driver read function */
ssize_t fan_dev_read(struct file * file, char __user * buf, size_t size,
		   loff_t * ppos)
{
	int tmp = 0;

	printk(KERN_ALERT "FAN async read\n");
	if (fan_record->mode == 1) {
        tmp = copy_to_user(buf, &swctrl_flag->mode, sizeof(int));
	    if (tmp) {
		    return -EFAULT;
	    }
	}

	if (fan_record->status_change > 0) {
	    if (copy_to_user(buf, fan_record->status_change_arr,\
			sizeof(unsigned int) * fan_record->status_change)) {
		    return -EFAULT;
	    }
	    tmp = fan_record->status_change;
	    fan_record->status_change = 0;
        memset(fan_record->status_change_arr, 0, MAX_MSG);
	}

	return tmp;
}

/* FAN device driver ioctl function */
long fan_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	printk(KERN_ALERT "FAN async ioctl\n");

	switch (cmd) {
	case READ_INIT_SYNC:	/* init sync */
		/* init fan_record data */
        fan_record->mode = 1;
        fan_record->status_change = 0;
        memset(fan_record->status_record, 0, 16);
        memset(fan_record->status_change_arr, 0, MAX_MSG);
		ret = put_user(READ_SYNC_ACK, (unsigned long __user *)arg);
		break;
	case READ_FAN_INFO:	/* get fan info number */
		ret = put_user(fan_record->status_change, \
			       (unsigned long __user *)arg);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

/* FAN device driver close function */
int fan_dev_release(struct inode *inode, struct file *file)
{
	/* clean fan_record data */
    fan_record->mode = 0;
    fan_record->status_change = 0;
    memset(fan_record->status_record, 0, 16);
    memset(fan_record->status_change_arr, 0, MAX_MSG);

    printk(KERN_ALERT "FAN async close\n");

	return 0;
}

/* FAN fasync function */
int fan_dev_fasync(int fd, struct file *filp, int on)
{
	printk(KERN_ALERT "FAN fasync_helper is calling\n");

	return fasync_helper(fd, filp, on, &fan_dev->async_noti);
}

/* FAN file struct */
struct file_operations fan_dev_fops = {
	.owner = THIS_MODULE,
	.open = fan_dev_open,
	.read = fan_dev_read,
	.release = fan_dev_release,
	.fasync = fan_dev_fasync,
	.unlocked_ioctl = fan_dev_ioctl,
};


/************************************************************************/
/*                    PORT device driver                                 */
/************************************************************************/
/* PORT device driver open function */
int port_dev_open(struct inode *inode, struct file *file)
{
	printk(KERN_ALERT "PORT async open\n");
	return 0;
}

/* PORT device driver read function */
ssize_t port_dev_read(struct file * file, char __user * buf, size_t size,
		   loff_t * ppos)
{
	int tmp = 0;

	printk(KERN_ALERT "PORT async read\n");
	if (port_record->mode == 1) {
        tmp = copy_to_user(buf, &swctrl_flag->mode, sizeof(int));
	    if (tmp) {
		    return -EFAULT;
	    }
	}

	if (port_record->status_change > 0) {
	    if (copy_to_user(buf, port_record->status_change_arr,\
			sizeof(unsigned int) * port_record->status_change)) {
		    return -EFAULT;
	    }
	    tmp = port_record->status_change;
	    port_record->status_change = 0;
        memset(port_record->status_change_arr, 0, MAX_MSG);
	}

	return tmp;
}

/* PORT device driver ioctl function */
long port_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	printk(KERN_ALERT "PORT async ioctl\n");

	switch (cmd) {
	case READ_INIT_SYNC:	/* init sync */
		/* init port_record data */
        port_record->mode = 1;
        port_record->status_change = 0;
        memset(port_record->status_record, 0, 16);
        memset(port_record->status_change_arr, 0, MAX_MSG);
		ret = put_user(READ_SYNC_ACK, (unsigned long __user *)arg);
		break;
	case READ_PORT_INFO:	/* get port info number */
		ret = put_user(port_record->status_change, \
			       (unsigned long __user *)arg);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

/* PORT device driver close function */
int port_dev_release(struct inode *inode, struct file *file)
{
	/* clean port_record data */
    port_record->mode = 0;
    port_record->status_change = 0;
    memset(port_record->status_record, 0, 16);
    memset(port_record->status_change_arr, 0, MAX_MSG);

    printk(KERN_ALERT "PORT async close\n");

	return 0;
}

/* PORT fasync function */
int port_dev_fasync(int fd, struct file *filp, int on)
{
	printk(KERN_ALERT "PORT fasync_helper is calling\n");

	return fasync_helper(fd, filp, on, &port_dev->async_noti);
}

/* PORT file struct */
struct file_operations port_dev_fops = {
	.owner = THIS_MODULE,
	.open = port_dev_open,
	.read = port_dev_read,
	.release = port_dev_release,
	.fasync = port_dev_fasync,
	.unlocked_ioctl = port_dev_ioctl,
};


static int PICA8_device_init(sta_record **dev_record, dev_node **dev_node,
        char* dev_name, struct file_operations dev_fops)
{
    int ret;

    *dev_record = kzalloc(sizeof(sta_record), GFP_KERNEL);
	if (!*dev_record)
		return -ENOMEM;
    *dev_node = kzalloc(sizeof(dev_node), GFP_KERNEL);
	if (!dev_node)
		return -ENOMEM;
	ret = alloc_chrdev_region(&(*dev_node)->dev_id, 0, 1, dev_name);
	if (ret) {
		printk(KERN_ALERT "can't get major number\n");
		unregister_chrdev_region((*dev_node)->dev_id, 1);
		return ret;
	}

    cdev_init(&(*dev_node)->async_cdev, &dev_fops);	/* init cdev */

	ret = cdev_add(&(*dev_node)->async_cdev, (*dev_node)->dev_id, 1);
	if (ret) {
		printk(KERN_ALERT " error %d adding device\n", ret);
		unregister_chrdev_region((*dev_node)->dev_id, 1);
		return ret;
	}

	(*dev_node)->async_class = class_create(THIS_MODULE, dev_name);
	if (IS_ERR((*dev_node)->async_class)) {
		printk("Err: failed in creating class\n");
		unregister_chrdev_region((*dev_node)->dev_id, 1);
		return -1;
	}

	device_create((*dev_node)->async_class, NULL, (*dev_node)->dev_id, NULL, dev_name);
	printk(KERN_ALERT " Registered %s character driver\n", dev_name);

    return 0;
}



static int __init dev_init(void)
{
	int ret=0;

	swctrl_flag = kzalloc(sizeof(read_flag), GFP_KERNEL);
	if (!swctrl_flag)
		return -ENOMEM;

#if 0
    psu_record = kzalloc(sizeof(sta_record), GFP_KERNEL);
	if (!psu_record)
		return -ENOMEM;

    psu_dev = kzalloc(sizeof(dev_node), GFP_KERNEL);
	if (!psu_dev)
		return -ENOMEM;
	ret = alloc_chrdev_region(&psu_dev->dev_id, 0, 1, PSU_DEVICE_NAME);
	if (ret) {
		printk(KERN_ALERT "can't get major number\n");
		unregister_chrdev_region(psu_dev->dev_id, 1);
		return ret;
	} else {
		printk(KERN_ALERT "get device major number success\n");
	}
	cdev_init(&psu_dev->async_cdev, &psu_dev_fops);	/* init cdev */
	ret = cdev_add(&psu_dev->async_cdev, psu_dev->dev_id, 1);
	if (ret) {
		printk(PSU_DEVICE_NAME " error %d adding device\n", ret);
		unregister_chrdev_region(psu_dev->dev_id, 1);
		return ret;
	} else
		printk(PSU_DEVICE_NAME " chardev register ok\n");

	psu_dev->async_class = class_create(THIS_MODULE, PSU_DEVICE_NAME);
	if (IS_ERR(psu_dev->async_class)) {
		printk("Err: failed in creating class\n");
		unregister_chrdev_region(psu_dev->dev_id, 1);
		return -1;
	}
	device_create(psu_dev->async_class, NULL, psu_dev->dev_id, NULL, PSU_DEVICE_NAME);
	printk(KERN_ALERT " Registered character driver\n");
#endif

    PICA8_device_init(&psu_record, &psu_dev, PSU_DEVICE_NAME, psu_dev_fops);
    PICA8_device_init(&fan_record, &fan_dev, FAN_DEVICE_NAME, fan_dev_fops);
    PICA8_device_init(&port_record, &port_dev, PORT_DEVICE_NAME, port_dev_fops);

	wk_que_ck = kzalloc(sizeof(wk_que), GFP_KERNEL);
	if (!wk_que_ck)
		return -ENOMEM;

	wk_que_ck->check_wq = create_workqueue("check_info");
	if (!wk_que_ck->check_wq) {
		printk(KERN_ALERT "no memory for workqueue\n");
		return -ENOMEM;
	} else
	    printk(KERN_ALERT "create workqueue successful\n");

	INIT_DELAYED_WORK(&wk_que_ck->check_dwq, check_info);
	queue_delayed_work(wk_que_ck->check_wq, &wk_que_ck->check_dwq, 3 * HZ);

    return ret;
}

static void PICA8_device_exit(sta_record **dev_record, dev_node **dev_node)
{
    if (*dev_record)
        kfree(*dev_record);

    if (*dev_node) {
	    device_destroy((*dev_node)->async_class, (*dev_node)->dev_id);
	    class_destroy((*dev_node)->async_class);
	    unregister_chrdev_region((*dev_node)->dev_id, 1);
	    cdev_del(&(*dev_node)->async_cdev);
    }
}

static void __exit dev_exit(void)
{
#if 1
	device_destroy(psu_dev->async_class, psu_dev->dev_id);
	class_destroy(psu_dev->async_class);
	unregister_chrdev_region(psu_dev->dev_id, 1);
	cdev_del(&psu_dev->async_cdev);
#endif
    PICA8_device_exit(&psu_record, &psu_dev);
    PICA8_device_exit(&fan_record, &fan_dev);
    PICA8_device_exit(&port_record, &port_dev);

	if (wk_que_ck->check_wq) {
		cancel_delayed_work(&wk_que_ck->check_dwq);
		flush_workqueue(wk_que_ck->check_wq);
		destroy_workqueue(wk_que_ck->check_wq);
	}
	if (swctrl_flag)
		kfree(swctrl_flag);

	printk(PSU_DEVICE_NAME
	       " Async Notification char driver clean up\n");
}

module_init(dev_init);
module_exit(dev_exit);

MODULE_LICENSE("GPL");
MODULE_VERSION("V0.09");
MODULE_DESCRIPTION("asynchronous notification driver");
