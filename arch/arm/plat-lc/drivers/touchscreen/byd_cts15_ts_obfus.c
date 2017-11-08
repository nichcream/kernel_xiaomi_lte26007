/*
 * File:         byd_cts15_ts_extn.c
 *
 * Created:	     2012-10-07
 * Depend on:    byd_bfxxxx_ts.c, byd_bfxxxx_ts.h
 * Description:  extension of BYD TouchScreen IC driver for Android
 *
 * Copyright (C) 2012 BYD Company Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/gpio.h>       // gpio_get_value()
#include <linux/fs.h>		//filp_open(), filp_close(), llseek(), read()
#include <asm/uaccess.h>      // get_fs(), set_fs()
#include <linux/miscdevice.h> // miscdevice, MISC_DYNAMIC_MINOR

// --------------- compile independently --------------------------------------
#include <linux/module.h>
#include <linux/interrupt.h> // disable_irq(), enable_irq()
#include <linux/i2c.h>       // i2c_client, i2c_master_send(), i2c_master_recv(), i2c_transfer(), I2C_M_RD, i2c_smbus_read_i2c_block_data(), i2c_smbus_write_i2c_block_data()
#include <linux/delay.h>     // mdelay()
#include <linux/slab.h>      // kmalloc()

//#include "byd_bf675x_ts.h"
#include <plat/bf675x_ts.h>
#include "byd_cts15_ts_obfus.h"

#if !defined(CONFIG_NON_BYD_DRIVER)
int reset_chip(struct i2c_client *client);
int startup_chip(struct i2c_client *client);
#define CONFIG_PROJECT_IDENTIFY
#define CONFIG_BEAT_TIMER
#ifdef CONFIG_BEAT_TIMER
int heartbeat_start(void);
int heartbeat_cancel(void);
#endif
int byd_ts_read_IC_para(struct i2c_client *client);

#endif
extern struct i2c_client *This_Client;
extern int Irq_Enabled;

//#ifdef CONFIG_NON_BYD_DRIVER

#ifdef TS_DEBUG_MSG
  #define TS_DBG(format, ...) \
	printk(format, ##__VA_ARGS__)
  #define PR_BUF(pr_msg, buf, len) \
  { \
	int _i_; \
	TS_DBG("%s: %s", __FUNCTION__, pr_msg); \
	for (_i_ = 0; _i_ < len; _i_++) { \
		if(_i_%16 == 0) TS_DBG("\n"); \
		/* if(_i_%16 == 0) TS_DBG("\n%02x ", _i_); */ \
		TS_DBG(" %02x", *(buf + _i_)); \
	} \
	TS_DBG("\n"); \
  }
#else
  #define TS_DBG(format, ...)
  #define PR_BUF(pr_msg, buf, len)
#endif

#ifndef CONFIG_DRIVER_BASIC_FUNCTION
	// Acquiring firmware version
	  #define CONFIG_SUPPORT_VERSION_ACQUISITION
	// Resolution acquiring (from chip)
	  #define CONFIG_SUPPORT_RESOLUTION_ACQUISITION
	  #define CONFIG_SUPPORT_MODULE_ID
	  #define CONFIG_SUPPORT_CHIP_ID
	  #define CONFIG_COOR_ORDER_SMOOTH_ALGORITHM
	#ifdef GPIO_TS_EINT
	// Raw data acquiring
	  #define CONFIG_SUPPORT_RAW_DATA_ACQUISITION
	#endif
#endif

//#endif // CONFIG_NON_BYD_DRIVER
// ------------------------------------------------------------------------- //

	// Acquiring firmware version
	  #define I2C_READ_FIRMWARE_VERSION {0x00}
	// resolution acquiring (from chip)
	  //#define I2C_READ_PARAMETERS {0x3c, 0x00} //{0x3c, 0xc1}
	  #define I2C_READ_RESOLUTION {0x3c, 0x9b} //{0x3c, 0x46}
	// Acquiring moudle ID is the must for COF with CTS12/13 chip
	  #define I2C_READ_MODULE_ID {0x09, 0x91}
	#ifdef GPIO_TS_EINT
	// Raw data acquiring
	  #define I2C_READ_RAW_DATA {0x5d, 0xd1, 0xff}  // {0x5e, 0xe1, 0xff}
	  #define I2C_READ_RAW_DATA_SINGLE_FRAME {0x5d, 0xd2, 0xff}
	  #define I2C_EXIT_DEBUG_MODE {0x3b, 0x10}
	// Channel quantity acquiring (Tx Rx)
	  #define I2C_READ_CHANNELS {0x3c, 0x60} // {0x3c, 0x32}
	  #define TX_MAX 64 //32
	  #define RX_MAX 32 //20
	// Reset chip
	  #define I2C_RESET {0x3a, 0xa3}
	// Charger frequency acquiring
	  #define I2C_READ_CHARGER_FREQ {0x3d, 0xd1}
	#endif

/*******************************************************************************
* Function    : smbus_master_recv
* Description : receive a buffer of data from i2c bus: -- FOR PARA OR RAW DATA
*               encapsulation of i2c_smbus_read_i2c_block_data for size limited
*               receiving
*               the data may be break into none-addressable packets and received
*               one by one
* Parameters  : input: client
*               output: buf, len
* Return      : the length of bytes received or transmit status
*******************************************************************************/
#define START_ADDR	0
static int smbus_master_recv(struct i2c_client *client, char *buf, int len)
{
#if !defined(I2C_PACKET_SIZE)
	return i2c_master_recv(client, buf, len);
#else
	int j, packet_number, leftover, start = 0, i = 0;
	unsigned char packet[I2C_PACKET_SIZE]= {0};

	/////////////////////////////////////////////
	if (len <= I2C_PACKET_SIZE) {
		return i2c_master_recv(client, buf, len);
	}
	/////////////////////////////////////////////
	leftover = len % I2C_PACKET_SIZE;
	packet_number = len / I2C_PACKET_SIZE + (leftover > 0 ? 1 : 0);
	for (j = 0; j < packet_number; j++) {
		int packet_len, ret;
		unsigned char reg = 0xda;

	    start = j * I2C_PACKET_SIZE;
		packet_len = (leftover > 0 && j == packet_number - 1) ? leftover : I2C_PACKET_SIZE;
		TS_DBG("offset=0x%02x, packet_size=%d\n", start + START_ADDR, packet_len);
		// Note: 0xda is supposed to be the offset of parameter data, but currently this constant should be given instead
		//ret = gsl_read_interface(client, 0xda, packet, packet_len);
#if I2C_PACKET_SIZE > 32
		ret = i2c_master_send(client, &reg, 1);
		if (ret < 0) {
			pr_err("%s: error in i2c_master_send(): %d\n", __FUNCTION__, ret);
			return ret;
		}
		ret = i2c_master_recv(client, packet, packet_len);
#else
		ret = i2c_smbus_read_i2c_block_data(client, 0xda, packet_len, packet);
#endif
		if (ret < 0) {
			pr_err("%s: error in i2c recv: %d\n", __FUNCTION__, ret);
			return ret;
		}

		for (i = 0; i < ret; i++) { // i < packet_len
			buf[start + i] = packet[i];
        }
	}
	TS_DBG("%s: I2C_PACKET_SIZE:%d, total_packets:%d, last_packet_size:%d\n", __FUNCTION__, I2C_PACKET_SIZE, packet_number, i);
	PR_BUF("last packet:", packet, i);
	return start + i;
#endif
}

/*******************************************************************************
* Function    : smbus_read_bytes
* Description : read data to buffer form i2c bus:
*               send i2c commands/address and then read data
*               (i2c_master_send i2c_master_recv)
* Parameters  : input: client, send_buf, send_len
                output: recv_buf, recv_len
* Return      : length of bytes read or i2c data transfer status
*******************************************************************************/
static int smbus_read_bytes(struct i2c_client *client,
	char *send_buf, int send_len, char *recv_buf, int recv_len)
{
	int ret;

	PR_BUF("i2c send:", send_buf, send_len);
	ret = i2c_smbus_write_i2c_block_data(client, send_buf[0], send_len - 1, send_buf + 1);
	if (ret < 0) {
	    pr_err("%s: err #%d returned by i2c_smbus_write_i2c_block_data()\n", __FUNCTION__, ret);
	    return ret;
	}

	ret = smbus_master_recv(client, recv_buf, recv_len); // SMBUS, recv_len <= 32
	if (ret < 0) {
	    pr_err("%s: err #%d returned by i2c_master_recv()\n", __FUNCTION__, ret);
	}
	TS_DBG("size required/received: %d/%d\n", recv_len, ret);
	return ret;
}

#ifdef CONFIG_SUPPORT_MODULE_ID
/*******************************************************************************
* Function    :  byd_ts_get_module_number
* Description :  get module id for a perticular TP.
* Parameters  :  client, - i2c client
* Return      :  module number or status
*******************************************************************************/
int byd_ts_get_module_number(struct i2c_client *client)
{
	int i, ret, module_num = 0;
	unsigned char module = 0x00, module1 = 0xff;
	for (i = 0; i < 3; i++) { // try 3 times
		unsigned char module_id[] = I2C_READ_MODULE_ID;
		ret = smbus_read_bytes(client, module_id, sizeof(module_id), &module, 1);
		if (ret <= 0) {
			pr_err("%s: i2c read MODULE ID error, errno: %d\n", __FUNCTION__, ret);
		} else if (module == module1) {
			module_num = (int)((module >> 4) | ((module & 0x0C) << 2));
			break;
		} else {
			module1 = module;
		}
	}
	if (ret <= 0 || i > 3) {
		pr_err("%s: module_id may not correct\n", __FUNCTION__);
		return ret;
	}
	if (module_num < 1 || module_num > TX_MAX) {
		printk("%s: MODULE ID 0x%x is not correct.\n", __FUNCTION__, module);
		//printk("module number default to 1.\n");
		module_num = 0;
	}
	if((module & 0x03) == 0x03) {
		printk("%s: the parameters already in IC's flash, they may be overrided. \n", __FUNCTION__);
	/* Deleted for COF, in order to enable parameter download after mass production
		printk("%s: parameter download aborted! \n", __FUNCTION__);
		return -EFAULT;
	*/
	}
	return module_num;
}
#endif // CONFIG_SUPPORT_MODULE_ID

#ifdef CONFIG_SUPPORT_CHIP_ID
/*******************************************************************************
* Function    :  byd_ts_get_chip_number
* Description :  get chip id for a perticular TP.
* Parameters  :  client, - i2c client
* Return      :  module number or status
* chip_num :
	module[1] :2= 6752; 1=6751;
	module[2] :1= B; 2=C;
*******************************************************************************/
int byd_ts_get_chip_number(struct i2c_client *client)
{
	int i, ret, chip_num = 0;
	unsigned char  module[3] = {0};
	for (i = 0; i < 3; i++) { // try 3 times
		unsigned char module_id[] = I2C_READ_MODULE_ID;
		ret = smbus_read_bytes(client, module_id, sizeof(module_id), &module[0], 3);
		if (ret <= 0) {
			pr_err("%s: i2c read CHIP ID error, errno: %d\n", __FUNCTION__, ret);
		} else{
			chip_num = module[1];
			break;
			}
	}
	pr_err("%s: module= %d, %d, chip_num = %d\n", __FUNCTION__,module[0], module[1], chip_num);
	if (chip_num <= 0 || chip_num > 2) {
		pr_err("%s: module_id may not correct\n", __FUNCTION__);
		return ret;
	}
	return chip_num;
}
#endif
/*******************************************************************************
* Function    : i2c_master_write
* Description : write data to a i2c device:
*               (i2c_master_send with given scl rate)
* Parameters  : input: client, send_buf, send_len
*               output: none
* Return      : i2c data transfer status
*******************************************************************************/
static int i2c_master_write(struct i2c_client *client, uint8_t *buf, int len)
{
	struct i2c_msg msgs[1];

	msgs[0].flags = !I2C_M_RD; // client->flags & I2C_M_TEN;
	msgs[0].addr = client->addr;
	msgs[0].len = len;
	msgs[0].buf = &buf[0];
	//msgs[0].scl_rate = 400 * 1000; //	msgs[0].timing = 400;

	return i2c_transfer(client->adapter, msgs, 1);
}
/*
static int i2c_master_read(struct i2c_client *client, char *buf, int len)
{
	struct i2c_msg msgs[1];

	msgs[0].flags = I2C_M_RD; // |= I2C_M_RD;
	msgs[0].addr = client->addr;
	msgs[0].len = len;
	msgs[0].buf = &buf[0];
	//msgs[0].scl_rate = 400 * 1000; // msgs[0].timing = 400; // mtk

	return i2c_transfer(client->adapter, msgs, 1);
	//msleep(5);
}
*/
/*******************************************************************************
* Function    : smbus_send_packets
* Description : send a buffer of data over i2c bus: -- FOR CTS15 PAGE DOWNLOAD
*               encapsulation of i2c_smbus_write_i2c_block_data for size limited
*               transmiting
*               the data may be break into addressable packets and send over one
*               by one
* Parameters  : input: client, buf, len
*               the first data in buf is start address and counted in len. The
*               maximum len must less then a page (1+128)
*               output: none
* Return      : the length of bytes sent or transmit status
*******************************************************************************/
#ifdef I2C_PACKET_SIZE
  #if (I2C_PACKET_SIZE == 8) // MTK specific
	#define I2C_SEND_SIZE_MAX	((I2C_PACKET_SIZE - 2)  / 4 * 4)  // 2 means device_addr + reg_addr
  #else
	#define I2C_SEND_SIZE_MAX	(I2C_PACKET_SIZE / 4 * 4)
  #endif
#endif
static int smbus_send_packets(struct i2c_client *client, char *buf, int len)
{
	int ret;
#if !defined(I2C_PACKET_SIZE)
	ret = i2c_master_write(client, buf, len); //ret = i2c_master_send(client, buf, len);
	if (ret < 0) {
		pr_err("%s: err #%d returned by i2c_master_send()\n", __FUNCTION__, -ret);
	}
	return ret;
#else
	int j, packet_number, leftover, start = 0, i = 0;
	unsigned char packet[I2C_SEND_SIZE_MAX + 1];

	leftover = (len - 1) % I2C_SEND_SIZE_MAX; // len include start addr in buf[0]
	packet_number = (len - 1) / I2C_SEND_SIZE_MAX + (leftover > 0 ? 1 : 0);
	for (j = 0; j < packet_number; j++) {
		int packet_len;

	    start = j * I2C_SEND_SIZE_MAX;
		packet_len = (leftover > 0 && j == packet_number - 1) ? leftover : I2C_SEND_SIZE_MAX;

		packet[0] = start + buf[0]; // addressable
		for (i = 1; i <= packet_len; i++) { // 1 means buf[0] excluded
			packet[i] = buf[start + i];
        }

#if I2C_PACKET_SIZE > 32
		//ret = i2c_master_write(client, packet, packet_len + 1);
		ret = i2c_master_send(client, packet, packet_len + 1);
#else
		ret = i2c_smbus_write_i2c_block_data(client, packet[0], packet_len, packet + 1);
#endif
		if (ret < 0) {
			pr_err("%s: error in i2c send: %d\n", __FUNCTION__, ret);
			return ret;
		}
	}
	TS_DBG("%s: I2C_SEND_SIZE_MAX:%d, total_packets:%d, last_packet_size:%d\n", __FUNCTION__, I2C_SEND_SIZE_MAX, packet_number, i - 1);
	PR_BUF("last packet:", &packet[1], i - 1);
	return start + i - 1;
#endif
}


#ifndef CONFIG_NON_BYD_DRIVER  // use byd driver

int test_i2c(struct i2c_client *client)
{
	int ret, rc = 1;
	char read_buf = 0;
	char write_buf = 0x12;

	ret = i2c_smbus_read_i2c_block_data(client, 0xf0, 1, &read_buf);
	if  (ret  < 0)
		rc --;
	else
		printk("I read reg 0xf0 is %x\n", read_buf);

	msleep(2);
	ret = i2c_smbus_write_i2c_block_data(client, 0xf0, 1, &write_buf);
	if(ret >= 0)
		printk("I write reg 0xf0 0x12\n");

	msleep(2);
	ret = i2c_smbus_read_i2c_block_data(client, 0xf0, 1, &read_buf);
	if(ret <  0 )
		rc --;
	else
		printk("I read reg 0xf0 is 0x%x\n", read_buf);

	return rc;
}

static void clr_reg(struct i2c_client *client)
{
	char buf = 0;

	buf = 0x88;
	i2c_smbus_write_i2c_block_data(client, 0xe0, 1, &buf);
	msleep(20);

	buf = 0x01;
	i2c_smbus_write_i2c_block_data(client, 0x80, 1, &buf);
	msleep(5);

	buf = 0x04;
	i2c_smbus_write_i2c_block_data(client, 0xe4, 1, &buf);
	msleep(5);

	buf = 0x00;
	i2c_smbus_write_i2c_block_data(client, 0xe0, 1, &buf);
	msleep(20);
}

#endif // not CONFIG_NON_BYD_DRIVER

#define CTS15_PAGE_REG	0xf0
#define PAGE_LENGTH 128

static int byd_ts_cts15_download(struct i2c_client *client, int start_page, char *buf, int len)
{
	int j, page_number, ret, err = len, leftover;
	unsigned char page_buf[PAGE_LENGTH + 5] = {0};

	TS_DBG("%s: size of data for download: %d\n", __FUNCTION__, len);
	page_buf[0] = 0x00; // reg; offset in a page

	leftover = len % PAGE_LENGTH;
	page_number = len / PAGE_LENGTH + ((leftover > 0)? 1 : 0);
	for (j = 0; j < page_number; j++) {
		int i, page_len;
	    page_buf[1] = j + start_page;
	    page_buf[2] = 0x00;
	    page_buf[3] = 0x00;
	    page_buf[4] = 0x00;
		ret = i2c_smbus_write_i2c_block_data(client, CTS15_PAGE_REG, 4, page_buf + 1);
		if (ret < 0) return ret;
		TS_DBG("\n %02x, %02x, %02x, %02x, %02x,\n", page_buf[0], page_buf[1], page_buf[2], page_buf[3], page_buf[4]);

		page_len = (leftover > 0 && j == page_number - 1) ? leftover : PAGE_LENGTH;
		for (i = 0; i < page_len; i += 4) {
			page_buf[4 + i] = buf[j * PAGE_LENGTH + i];
			page_buf[3 + i] = buf[j * PAGE_LENGTH + i + 1];
			page_buf[2 + i] = buf[j * PAGE_LENGTH + i + 2];
			page_buf[1 + i] = buf[j * PAGE_LENGTH + i + 3];
        }

#ifdef TS_DEBUG_MSG
		if (j == 0 || j == page_number - 1) PR_BUF("", page_buf + 1, page_len);
#endif
		//ret = gsl_write_interface(client, 0x00, page_buf, page_len);
		ret = smbus_send_packets(client, page_buf, page_len + 1); // offset page_buf[0]=0 must be send
		if (ret < 0) return ret;
	}

	return err;
}
/*
#ifndef I2C_PACKET_SIZE
	  #define I2C_PACKET_SIZE PAGE_LENGTH
#endif
#if (I2C_PACKET_SIZE == 8) // MTK specific
	#define IIC_QUART_PACKET_LEN	((I2C_PACKET_SIZE - 2) / 4) // 2 means device_addr + reg_addr
#else
	#define IIC_QUART_PACKET_LEN	(I2C_PACKET_SIZE / 4)
#endif
static int byd_ts_cts15_download(struct i2c_client *client, int start_page, char *buf, int len)
{
	int j, page_number, ret, err = len, leftover;
	unsigned char page_buf[IIC_QUART_PACKET_LEN * 4 + 1] = {0};

	TS_DBG("%s: size of data for download: %d\n", __FUNCTION__, len);
	page_buf[0] = 0x00; // reg; offset in a page, not used

	leftover = len % PAGE_LENGTH;
	page_number = len / PAGE_LENGTH + ((leftover > 0)? 1 : 0);
	TS_DBG("%s: total pages: %d / %d = %d (w. leftover %d) \n", __FUNCTION__,
			len, PAGE_LENGTH, page_number, leftover);
	TS_DBG("%s: IIC_QUART_PACKET_LEN = (%d - 2) / 4 = %d \n", __FUNCTION__,
			I2C_PACKET_SIZE, IIC_QUART_PACKET_LEN);
	for (j = 0; j < page_number; j++) {
		int i, page_len, send_flag, reg = 0, cur = 0;
	    page_buf[1] = j + start_page;
	    page_buf[2] = 0x00;
	    page_buf[3] = 0x00;
	    page_buf[4] = 0x00;
		//ret = gsl_write_interface(client, CTS15_PAGE_REG, page_buf, 4);
		ret = i2c_smbus_write_i2c_block_data(client, CTS15_PAGE_REG, 4, page_buf + 1);
		if (ret < 0) return ret;
		TS_DBG("\n %02x, %02x, %02x, %02x, %02x,\n", page_buf[0], page_buf[1], page_buf[2], page_buf[3], page_buf[4]);
		send_flag = 0;

		page_len = (leftover > 0 && j == page_number - 1) ? leftover : PAGE_LENGTH;
		for (i = 0; i < page_len; i += 4) {
			page_buf[4 + cur] = buf[j * PAGE_LENGTH + i];
			page_buf[3 + cur] = buf[j * PAGE_LENGTH + i + 1];
			page_buf[2 + cur] = buf[j * PAGE_LENGTH + i + 2];
			page_buf[1 + cur] = buf[j * PAGE_LENGTH + i + 3];
			cur += 4;
			if (0 == send_flag % IIC_QUART_PACKET_LEN) reg = i;
			if (IIC_QUART_PACKET_LEN - 1 == send_flag % IIC_QUART_PACKET_LEN) {
				TS_DBG("%d ", reg);
				//PR_BUF("", page_buf + 1, IIC_QUART_PACKET_LEN * 4);
#if I2C_PACKET_SIZE > 32
				ret = gsl_write_interface(client, reg, page_buf, IIC_QUART_PACKET_LEN * 4);
#else
				ret = i2c_smbus_write_i2c_block_data(client, reg, IIC_QUART_PACKET_LEN * 4, page_buf + 1);
#endif
				if (ret < 0) return ret;
				cur = 0;
			}
			send_flag++;
        }
		if (cur > 0) {
			TS_DBG("%d end", reg);
			//PR_BUF("", page_buf + 1, cur);
#if I2C_PACKET_SIZE > 32
			ret = gsl_write_interface(client, reg, page_buf, cur);
#else
			ret = i2c_smbus_write_i2c_block_data(client, reg, cur, page_buf + 1);
#endif
			if (ret < 0) return ret;
		}
	}
	return err;
}
*/



#if defined(CONFIG_SUPPORT_FIRMWARE_UPG) \
 || defined(CONFIG_SUPPORT_PARAMETER_FILE)

/*******************************************************************************
* Function    : byd_ts_read_file
* Description : read data to buffer form file
* In          : filepath, buffer, len, offset
* Return      : length of data read or negative value in the case of error
                -EIO:    I/O error
                -ENOENT: No file found
*******************************************************************************/
int byd_ts_read_file(char *p_path, char *buf, ssize_t len, int offset)
{
	struct file *fd;
	int ret_len = -EIO;
	char file_path[64];
	char *p_file;

	p_file = strrchr(p_path, '/');  // file name without directory
	if (!p_file) {
		p_file = p_path;
	} else {
		p_file++;
	}

	strcpy(file_path, "/mnt/sdcard/");
	strcat(file_path, p_file);
	fd = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(fd)) {
		TS_DBG("BYD TS: Warning: %s is not found or SD card not exist. \n", file_path);
		fd = filp_open(p_path, O_RDONLY, 0);
		if (!IS_ERR(fd)) {
			strcpy(file_path, p_path);
			goto alternate;
		}
		TS_DBG("BYD TS: Warning: %s is not found. \n", p_path);

#ifdef PARA_ALTERNATE_DIR
		strcpy(file_path, PARA_ALTERNATE_DIR);
		strcat(file_path, "/");
		strcat(file_path, p_file);
		fd = filp_open(file_path, O_RDONLY, 0);
		if (!IS_ERR(fd)) {
			goto alternate;
		}
		TS_DBG("BYD TS: Warning: %s is not found. \n", file_path);
#endif
		printk("%s: Warning: file %s is not found in all specified locations, FILE READ ABORTED!! \n", __FUNCTION__, p_file);
		return -ENOENT;
	}

alternate:
	do {
		mm_segment_t old_fs = get_fs();
		set_fs(KERNEL_DS);

		if (fd->f_op == NULL || fd->f_op->read == NULL) {
			pr_err("BYD TS: Error in set_fs(KERNEL_DS)!! ");
			break;
		}

		if (fd->f_pos != offset) {
			if (fd->f_op->llseek) {
				if (fd->f_op->llseek(fd, offset, 0) != offset) {
					pr_err("BYD TS: Error in llseek()!! ");
					break;
				}
			} else {
				fd->f_pos = offset;
			}
		}

		ret_len = fd->f_op->read(fd, buf, len, &fd->f_pos);
		printk("%s: %d bytes of data read from file %s. \n", __FUNCTION__, ret_len, file_path);

		set_fs(old_fs);

	} while (false);

	if (ret_len == -EIO) pr_err("FILE READ ABORTED!! \n");

	filp_close(fd, NULL);

	return ret_len;
}

/*******************************************************************************
* Function    : byd_ts_read_file_with_id
* Description : filling data to a buffer form file
* In          : data_file, module_num, buffer, len
* Return      : length of data read or negative value in the case of error
                -EIO:    I/O error
                -ENOENT: No file found
*******************************************************************************/
int byd_ts_read_file_with_id(unsigned char const *data_file_c, int module_num, char *buf, ssize_t len, int offset)
{
	int ret;
	unsigned char *data_file_p, data_file[127];

	strcpy(data_file, data_file_c);
	data_file_p = strstr(data_file, "#."); // data_file_p = strrchr(data_file, '.');
	if (data_file_p != NULL) {
		if (module_num < 10) {
			*data_file_p = (unsigned char)(module_num + 0x30);
		} else {
			*data_file_p = (unsigned char)(module_num / 10 + 0x30);
			*(data_file_p + 6) = *(data_file_p + 5); // \0
			*(data_file_p + 5) = *(data_file_p + 4); // t
			*(data_file_p + 4) = *(data_file_p + 3); // a
			*(data_file_p + 3) = *(data_file_p + 2); // d
			*(data_file_p + 2) = *(data_file_p + 1); // .
			*(data_file_p + 1) = (unsigned char)(module_num % 10 + 0x30);
		}
	}
	data_file_p = data_file;

	ret = byd_ts_read_file(data_file_p, buf, len, offset);

	return ret;
}

#endif // ..._FIRMWARE_UPG || ..._PARAMETER_FILE

///////////////////////////////////////////////////////////////////////////////
/*
* Module      : Touchscreen working parameter updating
* Description : tuning touchscreen by writing parameters to its registers
* Interface   : int byd_ts_download(i2c_client, id)
*               in:  i2c_client
*                    id - module number, -1 unknown, 0 firmware download
*               out: status of parameter updating
* Return      : status (error if less than zero)
*/
#if defined(CONFIG_SUPPORT_PARAMETER_UPG) \
 || defined(CONFIG_SUPPORT_PARAMETER_FILE) // Module range

	  #define I2C_WRITE_PARAMETER 0x08, 0xb1
	  #define LEN_HEAD 0 // if use hardware I2C download instruction
	  #define PARA_BUFFER_SIZE 640 // 256
	  #define MAX_DOWNLOAD_BUFFER_SIZE 20 * 1024
	  #define PARA_PAGE_ADDR 0x02
	  #define MOUDLE_PAGE_ADDR 0x00
	  #define PARA_FILE_OFFSET  0 //PARA_PAGE_ADDR * PAGE_LENGTH for .bin file

/*******************************************************************************
 * Function    : byd_ts_download
 * Description : download parameters or firmware to TP IC via I2C
 * In          : client, i2c client
 *               nodule_number, -1 unknown, 0 firmware download
 * Out         : none
 *              -EIO:    I/O error
 *              -ENOENT: No module number found
 *              -ENODATA: No parameter data found
 * Return      : status
*******************************************************************************/
int byd_ts_download(struct i2c_client *client, int module_num)
{
	int ret = 0, err = 0;
	unsigned char *buf = NULL;
	int len = 0;
	int moudle_number = 0, chip_id = 0;

#ifdef CONFIG_SUPPORT_PARAMETER_UPG

#endif

#ifdef CONFIG_SUPPORT_PARAMETER_FILE
	unsigned char *buff = NULL;
	//unsigned char head[] = {I2C_WRITE_PARAMETER}; // useless if use hardware I2C download
	int len_f;
#endif

	TS_DBG("%s entered \n", __func__);

#ifdef CONFIG_BEAT_TIMER
	heartbeat_cancel();
#endif
	disable_irq(client->irq);
#ifdef CONFIG_SUPPORT_MODULE_ID
	if (module_num < 0) {
		module_num = byd_ts_get_module_number(client);
		if (module_num <= 0) {
			pr_err("%s: module number is not correct, default to 1.\n",
				__FUNCTION__);
			err = -ENOENT; // err = module_num;
			module_num = 1;
		}
	} else if (module_num == 0) {
		// TO DO:
	}
#else
	if (module_num < 0) module_num = 1;
#endif
	TS_DBG("byd_ts_get_module_number: module_num =%d \n", module_num);

#ifdef CONFIG_SUPPORT_PARAMETER_UPG
	if(module_num == 0){
#ifdef CONFIG_SUPPORT_CHIP_ID
		buf = Bufmodules;
		len = Len_Bufmoudles;
		TS_DBG("byd_cts15_ts_modules.txt: for module identify \n");
#else
		TS_DBG("module chip_id identify isn't support \n");
#endif
	}else{
#ifdef CONFIG_SUPPORT_CHIP_ID
		chip_id = (int)byd_ts_get_chip_number(client);
		TS_DBG("%s: byd_ts_get_chip_number: chip_id = %d\n", __FUNCTION__, chip_id);
		if(chip_id == 1) //chip_id ==1, 6751
			{
#endif
#ifdef CONFIG_PROJECT_IDENTIFY
			switch (ctp_cob_bf675x) {
				case PREJECT1_ID:
					switch (module_num) {
						case 16:
							moudle_number =1;
							break;

						default:
							moudle_number =0;
							break;
						}
						break;
/*
				case PREJECT2_ID:
					switch (module_num) {
						case 1:
						case 16:
							moudle_number = 1;
							break;
						default:
							moudle_number =0;
							break;
						}

					break;
				case PREJECT3_ID:
					switch (module_num) {
						case 1:
						case 16:
							moudle_number = 1;
							break;
						default:
							moudle_number =0;
							break;
						}
					break;
*/
				default:
					break;
				};
#else
			moudle_number =module_num;
#endif
#ifdef CONFIG_SUPPORT_CHIP_ID
			} else if(chip_id == 2){  //chip_id ==2, 6752
#ifdef CONFIG_PROJECT_IDENTIFY
		switch (ctp_cob_bf675x) {
				case PREJECT1_ID:
					switch (module_num) {
						case 1:
						case 16:
						default:
							moudle_number =0;
							break;
						}
						break;
				default:
					moudle_number =0;
					break;
				};
#else
		moudle_number = module_num + 16;
#endif
		}
else{
		pr_err("%s: byd_ts_get_chip_number error: chip_id = %d\n", __FUNCTION__, chip_id);
		err = chip_id;
		moudle_number =0;
		//return err;
	}
#endif
#ifdef  CUSTOMER_USED
		moudle_number = 0;
#endif
			buf=bf675x_data[moudle_number].BYD_CTS15;
			len = bf675x_data[moudle_number].len_size;
			TS_DBG("bf675x_data[%d]: ctp_cob_bf675x = %d, len = %d \n", moudle_number, ctp_cob_bf675x, len);
			if(len<=0)
			{
				buf=bf675x_data[0].BYD_CTS15;
				len = bf675x_data[0].len_size;
				pr_err("bf675x_data read error: default data bf675x_data[0] is found in driver \n");
			}
			//name_buf = each(bf675x_data[moudle_number].BYD_CTS15);
			//TS_DBG("%s \n", name_buf);

	};
#endif // ...UPG

#if defined(CONFIG_SUPPORT_PARAMETER_FILE) || defined(FIRM_DOWNLOAD_FILE)
  /*  if (module_num == 0) {
		buff = kmalloc(MAX_DOWNLOAD_BUFFER_SIZE, GFP_KERNEL);
		len_f = MAX_DOWNLOAD_BUFFER_SIZE;
	} else {
		buff = kmalloc(MAX_DOWNLOAD_BUFFER_SIZE + LEN_HEAD, GFP_KERNEL);
		for (tmp = 0; tmp < sizeof(head); tmp++) {
			buff[tmp] = head[tmp];
		}
		len_f = PARA_BUFFER_SIZE + LEN_HEAD;
	}
*/
		buff = kmalloc(MAX_DOWNLOAD_BUFFER_SIZE, GFP_KERNEL);
		len_f = MAX_DOWNLOAD_BUFFER_SIZE;

	if(module_num == 0) {
	//  #ifdef FIRM_DOWNLOAD_FILE
		//ret = byd_ts_read_file(FIRM_DOWNLOAD_FILE, buff + LEN_HEAD, len_f - LEN_HEAD, 0);
	//  #endif
	} else {
		ret = byd_ts_read_file_with_id(PARA_DOWNLOAD_FILE, module_num, buff, len_f, PARA_FILE_OFFSET);
			if(ret<0)
			{
				byd_ts_read_file(FIRM_DOWNLOAD_FILE, buff, len_f, 0);
			}
	  }
	if (ret > 0) {
		buf = buff;
		if (len > 0) {
			printk("%s: the driver's default data will be overrided by the data in external file. \n", __FUNCTION__);
		}
		len = ret + LEN_HEAD; // the last byte for CRC included
	} else if (len - LEN_HEAD > 0) { // parameter from driver's parameter array
		printk("%s: %d bytes of default data is found in driver. module number:%d \n", __FUNCTION__, len - LEN_HEAD, module_num);
	}
#endif // ...PARAMETER_FILE

	if (len <= LEN_HEAD) {
		printk("%s: data not found, download aborted, errno:%d\n", __FUNCTION__, ret);
		if(module_num != 0) {
			printk("%s: the parameter in IC code will be used!\n", __FUNCTION__);
		}
	#if defined(CONFIG_SUPPORT_PARAMETER_FILE) || defined(FIRM_DOWNLOAD_FILE)
		kfree(buff);
	#endif
		enable_irq(client->irq);
#ifdef CONFIG_BEAT_TIMER
		heartbeat_start();
#endif
		return -ENODATA; // return to use chip's defaults
	} else if (len != PARA_BUFFER_SIZE + LEN_HEAD && len != MAX_DOWNLOAD_BUFFER_SIZE) {
		printk("%s: Warning - the number of data may be incorrect.\n", __FUNCTION__);
	}

#ifdef CONFIG_NON_BYD_DRIVER
	gslX680_shutdown_low();
	msleep(20);
	gslX680_shutdown_high();
	msleep(20);
#else
	byd_ts_shutdown_low();
	msleep(20);
	byd_ts_shutdown_high();
	msleep(20);
#endif
	ret = test_i2c(client);
	if(ret < 0) {
		pr_err("%s: test_i2c: i2c communication error!\n", __FUNCTION__);
		enable_irq(client->irq);
#ifdef CONFIG_BEAT_TIMER
		heartbeat_start();
#endif
		return ret;
	}
	clr_reg(client);
	reset_chip(client);

	printk("%s: sending data: %x, %x, %x, %x, ... %x, %x, len = %d \n",
		__FUNCTION__, buf[0], buf[1], buf[2], buf[3], buf[len-2], buf[len-1], len);

	//if(module_num == 0) {
		ret = byd_ts_cts15_download(client, 0x00, buf, len);
	//} else {
		//ret = byd_ts_cts15_download(client, PARA_PAGE_ADDR, buf, len);
	//}
	if (ret == len) {
		printk("%s: download success!\n", __FUNCTION__);
	} else if (ret < 0) {
		pr_err("%s: error during cts15 download: %d\n", __FUNCTION__, ret);
	} else {
		pr_err("%s: error during cts15 download: finished size = %d\n", __FUNCTION__, ret);
	}

	startup_chip(client);
#if defined(CONFIG_SUPPORT_PARAMETER_FILE) || defined(FIRM_DOWNLOAD_FILE)
	kfree(buff);
#endif
	msleep(30);

#if defined(CONFIG_COOR_ORDER_SMOOTH_ALGORITHM)
	byd_ts_read_IC_para(client); // cts15 read para for byd_ts_read_coor()
#endif
	enable_irq(client->irq);
#ifdef CONFIG_BEAT_TIMER
	heartbeat_start();
#endif

	return err;
}

#endif // ..._PARAMETER_UPG || ..._PARAMETER_FILE // Module range

///////////////////////////////////////////////////////////////////////////////

#if defined(CONFIG_SUPPORT_RESOLUTION_ACQUISITION)

/*******************************************************************************
* Function    : byd_ts_get_resolution
* Description : read screen resolution from chip via I2c
* In          : i2c_client
* Out         : xy_res, screen resolution (X first and then Y, <=2048)
* Return      : status (error if less than zero)
*******************************************************************************/
// #define BYD_TS_RESOLUTION_REG	70  //0x46-0x49
#define RESO_BUF_SIZE 4
int byd_ts_get_resolution(struct i2c_client *client, int *xy_res)
{
	int ret;
	unsigned char cmd[] = I2C_READ_RESOLUTION; // I2C_READ_PARAMETERS
	unsigned char buf[RESO_BUF_SIZE]; // buf[PARA_BUFFER_SIZE]

	ret = smbus_read_bytes(client, cmd, sizeof(cmd), buf, sizeof(buf));
	if (ret <= 0) {
		pr_err("%s: i2c read RESOLUTION error, errno: %d\n", __FUNCTION__, ret);
		return ret;
	}

	xy_res[0] = (buf[0] << 8) | buf[1];
	xy_res[1] = (buf[2] << 8) | buf[3];

	return ret;
}

#endif //CONFIG_SUPPORT_RESOLUTION_ACQUISITION


#ifdef CONFIG_SUPPORT_VERSION_ACQUISITION
/*******************************************************************************
* Function    : byd_ts_get_firmware_version
* Description : get firmware's version from chip
* In          : i2c_client
* Return      : status (error if less than zero)
*******************************************************************************/
#define VERSION_BUF_SIZE	7
int byd_ts_get_firmware_version(struct i2c_client *client, unsigned char* version)
{
	unsigned char version_id[] = I2C_READ_FIRMWARE_VERSION; // must single byte
	int ret;
	//err = smbus_read_bytes(client, version_id, sizeof(version_id), version, VERSION_SIZE);
	ret = i2c_smbus_read_i2c_block_data(client, version_id[0], VERSION_BUF_SIZE, version);
	if (ret < 0) {
		pr_err("%s: i2c read VERSION# error, errno: %d \n", __FUNCTION__, ret);
	}
	return ret;
}
#endif

///////////////////////////////////////////////////////////////////////////////

#if defined(CONFIG_SUPPORT_RAW_DATA_ACQUISITION) \
 || defined(CONFIG_SUPPORT_PARAMETER_FILE)

/*******************************************************************************
* Function    : byd_ts_get_channel_number
* Description : read screen channel info from chip via I2c
* In          : i2c_client
* Out         : n_tx_rx, the number of channels (Tx * Rx)
* Return      : status (error if less than zero)
*******************************************************************************/
// #define BYD_TS_CHANNEL_REG	50 // 0x32-0x33
#define CHAN_BUF_SIZE 2
int byd_ts_get_channel_number(struct i2c_client *client, char *n_tx_rx)
{
	int ret;
	unsigned char cmd[] = I2C_READ_CHANNELS; //I2C_READ_PARAMETERS;
	unsigned char buf[CHAN_BUF_SIZE]; //buf[PARA_BUFFER_SIZE];

	ret = smbus_read_bytes(client, cmd, sizeof(cmd), buf, sizeof(buf));
	if (ret <= 0) {
		pr_err("%s: i2c read PARAMETER error, errno: %d\n", __FUNCTION__, ret);
		return ret;
	}

	n_tx_rx[0] = buf[0]; // buf[BYD_TS_CHANNEL_REG];
	n_tx_rx[1] = buf[1]; // buf[BYD_TS_CHANNEL_REG + 1];

	return ret;
}

static int byd_ts_open(struct inode *inode, struct file *file) {
	TS_DBG("%s: major=%d, minor=%d\n", __FUNCTION__, imajor(inode), iminor(inode));
	return 0;
}

static int byd_ts_release(struct inode *inode, struct file *file) {
	return 0;
}

#if (EINT_TRIGGER_TYPE == IRQF_TRIGGER_RISING) \
  || (EINT_TRIGGER_TYPE == IRQF_TRIGGER_HIGH)
#define IRQ_TRIGGER_LEVEL 1
#elif (EINT_TRIGGER_TYPE == IRQF_TRIGGER_FALLING) \
  || (EINT_TRIGGER_TYPE == IRQF_TRIGGER_LOW)
#define IRQ_TRIGGER_LEVEL 0
#endif

static unsigned char n_tx_rx[2] = {255, 1}; //{TX_MAX + 1, RX_MAX + 1}; //{1, 1};

static int byd_ts_read(struct file *file, char __user *user_buf,
	size_t const count, loff_t *offset)
{
	int ret;
	//static int len = 2; // = count

	if (count <= 2) { // Tx/Rx is acquired every time we entered tools
		if (n_tx_rx[0] <= 1 || n_tx_rx[1] <= 1
			|| n_tx_rx[0] > TX_MAX || n_tx_rx[1] > RX_MAX) {
			ret = byd_ts_get_channel_number(This_Client, n_tx_rx);
			if (ret < 0) {
				/* in the case of sleeping, try again */
				msleep(5); //mdelay(5);
				ret = byd_ts_get_channel_number(This_Client, n_tx_rx);
			}
			if (n_tx_rx[0] <= 1 || n_tx_rx[1] <= 1
			        || n_tx_rx[0] > TX_MAX || n_tx_rx[1] > RX_MAX) {
				pr_err("%s: error in getting Tx/Rx %d/%d! status: %d\n", __FUNCTION__,
				       n_tx_rx[0], n_tx_rx[1], ret);
				n_tx_rx[0] = 255; //TX_MAX + 1; // 1;
				n_tx_rx[1] = 1;   //RX_MAX + 1; // 1;
			} else {
				printk("%s: Tx x Rx: %d x %d\n", __FUNCTION__, n_tx_rx[0], n_tx_rx[1]);
			}
			//len = n_tx_rx[0] * n_tx_rx[1] * 2 * 2 + (n_tx_rx[0] + n_tx_rx[1]) * 2 * 2;
			//TS_DBG("%s: size of raw data: %d\n", __FUNCTION__, len);
		}
		ret = copy_to_user(user_buf, n_tx_rx, count);

	} else { // count > 2
		char buf[count]; // [len]
		int i;
		for (i = 0; i < 3000 && gpio_get_value(GPIO_TS_EINT) != IRQ_TRIGGER_LEVEL; i++) {
			// Waiting for interruption...(we should not put printk code here or causes delay)
			//mdelay(5); // interrupt may have very short pause width
			msleep(1); // msleep may exceed 10 times longer than it supposed to be
		}
		if (i == 3000) {
			pr_err("%s: error: no DAV signal detected within %d~%d ms.\n", __FUNCTION__, i, i * 10);
			ret = -ENODATA;
			goto exit;
		} else if (i == 0) {
			printk("%s: warning: DAV is already on supposed level (no change detected). \n", __FUNCTION__);
		}

		ret = smbus_master_recv(This_Client, (char *)&buf, count); // len);
		if (ret < 0) {
			pr_err("%s: i2c_master_recv RAWDATA error: %d! size=%d\n", __FUNCTION__, ret, count);
		}
exit:
		enable_irq(This_Client->irq);
		Irq_Enabled = 1;
		// the irq is enabled but IC still working in debug mode //
#ifdef CONFIG_BEAT_TIMER
		heartbeat_start();
#endif

		if (ret < 0) {
			return ret;
		}
		TS_DBG("%s: data size: required:%d returned:%d\n", __FUNCTION__, count, ret);
		ret = copy_to_user(user_buf, buf, ret);
	}

	if (ret < 0) {
		pr_err("%s: copy_to_user() error, errno: %d\n", __FUNCTION__, ret);
		return ret;
	}

	return 0;
}

static int byd_ts_write(struct file *file, const char __user *user_buf,
	size_t const count, loff_t *offset)
{
	return 0;
}

static long byd_ts_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret, len_cmd;
	unsigned char *command = NULL;
	unsigned char cmd_rawdata[] = I2C_READ_RAW_DATA;
	unsigned char cmd_rawdata_sf[] = I2C_READ_RAW_DATA_SINGLE_FRAME;
	unsigned char cmd_exit_debug[] = I2C_EXIT_DEBUG_MODE;
	unsigned char cmd_reset[] = I2C_RESET;
	unsigned char cmd_charger[] = I2C_READ_CHARGER_FREQ;

	//TS_DBG("%s: line=%d, cmd=%d, arg=%ld\n", __FUNCTION__, __LINE__, cmd, arg);
	switch (cmd) {

		case 1:
		case 3: // *** cmd in ioctl() cannot be 2 in Linux ***
			ret = gpio_get_value(GPIO_TS_EINT);
			if (IRQ_TRIGGER_LEVEL == ret) {
				pr_err("%s: warning: initial DAV level is not correct: %d \n", __FUNCTION__, ret);
				printk("%s: check EINT_TRIGGER_TYPE, GPIO_TS_EINT, gpio_get_value() \n", __FUNCTION__);
				ret = -ENODATA;
				return ret;
			}
#ifdef CONFIG_BEAT_TIMER
			heartbeat_cancel();
#endif
			Irq_Enabled = 0;
			disable_irq(This_Client->irq);
			if (cmd == 1) {
				command = cmd_rawdata;
				len_cmd = sizeof(cmd_rawdata);
			} else if (cmd == 3) {
				command = cmd_rawdata_sf;
				len_cmd = sizeof(cmd_rawdata_sf);
			}
			//Raw_Data_Mode = 1;
			printk("%s: i2c send command: rawdata\n", __FUNCTION__);
		break;

		case 4: // on line parameter download
#if defined(CONFIG_SUPPORT_PARAMETER_UPG) || defined(CONFIG_SUPPORT_PARAMETER_FILE)
			printk("%s: i2c send command: download parameter\n", __FUNCTION__);
			ret = byd_ts_download(This_Client, -1); // -1 unknown module number
			if (ret == -ENOENT) { // no module number found, use default
				ret = 0;
			} else if (ret < 0) {
				pr_err("%s: error in parameter download, errno: %d\n", __FUNCTION__, ret);
			}
			n_tx_rx[0] = 255; //TX_MAX + 1; // 1;  // signal re-acquire Tx Rx
			n_tx_rx[1] = 1;   //RX_MAX + 1; // 1;
#else
			printk("%s: parameter download is not supported!\n", __FUNCTION__);
			ret = 0;
#endif
		return ret;

		case 5: // on line firmware update
			printk("%s: i2c send command: download firmware\n", __FUNCTION__);
			//ret = byd_ts_download(This_Client, 0); // 0 - download firmware
			ret = byd_ts_download(This_Client, -1); // 0 - download firmware
			if (ret == -ENOENT || ret == -ENODATA) {
				ret = 0;
			} else if (ret < 0) {
				pr_err("%s: error in download firmware parameters, errno: %d\n", __FUNCTION__, ret);
			}
			n_tx_rx[0] = 255; //TX_MAX + 1; // 1;  // signal re-acquire Tx Rx
			n_tx_rx[1] = 1;   //RX_MAX + 1; // 1;
		return ret;

		case 6:
			command = cmd_exit_debug;
			len_cmd = sizeof(cmd_exit_debug);
			printk("%s: i2c send command: exit_debug_mode\n", __FUNCTION__);
		break;

		case 7:
#ifdef CONFIG_BEAT_TIMER
			heartbeat_cancel();
#endif
			disable_irq(This_Client->irq);
			command = cmd_reset;
			len_cmd = sizeof(cmd_reset);
			//Raw_Data_Mode = 0;
			printk("%s: i2c send command: reset\n", __FUNCTION__);
		break;

		case 8: // charger frequency calculation
			ret = gpio_get_value(GPIO_TS_EINT);
			if (IRQ_TRIGGER_LEVEL == ret) {
				pr_err("%s: warning: initial DAV level is not correct: %d \n", __FUNCTION__, ret);
				printk("%s: check correctness of GPIO_TS_EINT, gpio_get_value() \n", __FUNCTION__);
				ret = -ENODATA;
				return ret;
			}
#ifdef CONFIG_BEAT_TIMER
			heartbeat_cancel();
#endif
			disable_irq(This_Client->irq);
			command = cmd_charger;
			len_cmd = sizeof(cmd_charger);
			printk("%s: i2c send command: charger frequency\n", __FUNCTION__);
		break;

		default:
			TS_DBG("%s: unknown i2c command\n", __FUNCTION__);
		return 0;
	}

	PR_BUF("sending command:", command, len_cmd);
	ret = i2c_smbus_write_i2c_block_data(This_Client, command[0], len_cmd - 1, command + 1);
	if (ret < 0) {
		/* in the case of sleeping, try again */
		printk("%s: warnning: error %d in i2c send command, will try again after 5 ms!\n", __FUNCTION__, ret);
		msleep(5);
		ret = i2c_smbus_write_i2c_block_data(This_Client, command[0], len_cmd - 1, command + 1);
		if (ret < 0) {
			pr_err("%s: i2c_master_send COMMAND error, errno: %d\n", __FUNCTION__, ret);
			if (cmd == 1 || cmd == 3) { // raw data error case, e.g. after sleep, restore normal status
				enable_irq(This_Client->irq);
#ifdef CONFIG_BEAT_TIMER
				heartbeat_start();
#endif
			}
			return ret;
		}
	}

	if (cmd == 7) {
		n_tx_rx[0] = 255; //TX_MAX + 1; // 1;  // signal re-acquire Tx Rx
		n_tx_rx[1] = 1;   //RX_MAX + 1; // 1;
#ifdef CONFIG_BEAT_TIMER
		heartbeat_start();
#endif
		enable_irq(This_Client->irq);
	}
	return 0;
}

static struct file_operations byd_ts_fops = {
	.owner   = THIS_MODULE,
	.open    = byd_ts_open,
	.release = byd_ts_release,
	.read    = byd_ts_read,
	.write   = byd_ts_write,
	.unlocked_ioctl = byd_ts_ioctl,
};

// ******* defining a device for raw data output ********
/*  Variable   : byd_ts_device - misc device for raw data
 *  Accessed by: byd_ts_probe(), byd_ts_remove()
 */
struct miscdevice byd_ts_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = BYD_TS_NAME,
	.fops  = &byd_ts_fops,
};

#endif // CONFIG_SUPPORT_RAW_DATA_ACQUISITION || CONFIG_SUPPORT_PARAMETER_FILE


#include <linux/i2c.h>
extern unsigned char buf[(0x1897+1791-0x1f64)];unsigned char byd_A74[
(0x158a+3077-0x215d)];int i2c_read_bytes(struct i2c_client*client,uint8_t*
byd_V78,int byd_V79,uint8_t*byd_V76,int byd_V77);
#define  N			        (0x10b9+1895-0x1816)
#define  byd_G69		(0x1cc+1574-0x7ed)
#define  byd_G63			(0x23ab+46-0x1bda)
#define  byd_G64			(0x190d+5486-0x267c)
#define  byd_G75		(0x1feb+2792-0x22d4)
#define  byd_C53       (0x1a60+2709-0x24ed)
#define  byd_C54       (0x1119+1871-0x1866)
#define  byd_C52         (0xb02+3014-0x16c5)
#define  byd_C55        (0x18cf+1046-0x1ce4)
#define  byd_C50    (0x1d5d+1603-0x239f)
#define  byd_C51      (0x12b2+3435-0x201c)
const unsigned char byd_A88[]={(0x676+61-0x693),(0xe87+1735-0x153a),
(0x547+3431-0x129e),(0xe90+5127-0x2283),(0x11c1+425-0x1369),(0x491+6605-0x1e59)}
;unsigned char byd_A53[(0xb+6668-0x1a15)]={(0x4e2+5500-0x1a22),
(0x1c5+2179-0x9ae)};
unsigned char byd_A51[(0x789+3706-0x1601)]={(0xd29+719-0xfbc),
(0xb21+2544-0x1476)};
unsigned char byd_A52[(0x1c0b+1334-0x213f)]={(0x8f5+3557-0x169e),
(0xb49+3995-0x1a47)};
unsigned char byd_A80[(0x1d48+843-0x2091)]={(0x7e8+6758-0x2212),
(0xbaf+4579-0x1ce6)};
unsigned char byd_A78[(0x12ab+4289-0x236a)]={(0x18a8+1654-0x1ee2),
(0x8a0+6196-0x1ff9)};unsigned char byd_A79[(0x233+7633-0x2002)]={
(0x19f+7997-0x20a0),(0x278+6525-0x1b19)};typedef struct byd_G67{unsigned short
byd_A57[N];unsigned short byd_A58[N];unsigned short byd_A65[N];unsigned short
byd_A66[N];unsigned int byd_G61;unsigned char byd_A70[N];unsigned char byd_A67[N
];unsigned char byd_A59[N];unsigned char*byd_G51;unsigned char byd_G59;unsigned
char byd_V50;unsigned char byd_G70;unsigned char byd_G65;unsigned char byd_G58;}
byd_G67;typedef struct byd_G62{unsigned short x;unsigned short y;}byd_G62;
typedef struct byd_G68{unsigned short byd_A71[N];unsigned short byd_A72[N];
unsigned short byd_A54[N];unsigned short byd_A55[N];unsigned short byd_A68[N];
unsigned short byd_A69[N];unsigned short byd_A83[N];unsigned short byd_A84[N];
unsigned char byd_A60[N];unsigned char byd_A62[N];unsigned char byd_A63[N];
unsigned char byd_A82[N];byd_G62 byd_A73[N][byd_G69];}byd_G68;byd_G67 byd_G53;
byd_G68 byd_G54;unsigned char byd_G60=(0x784+4473-0x18fd);unsigned short byd_G56
=(0x6e6+5602-0x1cc8);unsigned short byd_G57=(0x1b63+1223-0x202a);static unsigned
 char byd_V100=(0x247+463-0x416);static unsigned char byd_V97=
(0x132f+539-0x154a);static unsigned char byd_V98=(0xd14+4710-0x1f7a);static
unsigned char byd_V95=(0xcc6+4138-0x1cf0);static unsigned char byd_V96=
(0x170a+630-0x1980);static unsigned char byd_V99=(0xbc0+3876-0x19e5);static
unsigned char byd_V102=(0xb5a+5550-0x2108);static unsigned char byd_V93=
(0xa60+3989-0x19f5);static unsigned short byd_V94=(0xcc6+4093-0x1cc3);static
unsigned char byd_G74=(0xe6+8726-0x22fc);static unsigned short byd_G72=
(0xf2+7507-0x1e45);static unsigned short byd_A85[(0x1e1+7407-0x1ecd)][N];static
unsigned short byd_A86[(0xd86+6394-0x267d)][N];static unsigned char byd_A87[N];
void byd_F56(uint32_t i){uint32_t j;byd_G54.byd_A54[i]=(0xadc+79-0xb2b);byd_G54.
byd_A55[i]=(0x1cbc+2130-0x250e);for(j=(0xf20+4446-0x207e);j<byd_G69;j++){byd_G54
.byd_A73[i][j].x=(0x9c9+4858-0x1cc3);byd_G54.byd_A73[i][j].y=
(0x1b3b+1785-0x2234);}byd_G54.byd_A60[i]=(0x204d+589-0x229a);byd_G54.byd_A62[i]=
(0xdc9+204-0xe95);}void byd_F51(void){uint32_t i;for(i=(0x1313+3028-0x1ee7);i<N;
i++){byd_G53.byd_A57[i]=(0x325+8267-0x2370);byd_G53.byd_A58[i]=
(0x1bc3+165-0x1c68);byd_G53.byd_A65[i]=(0xf0c+1162-0x1396);byd_G53.byd_A66[i]=
(0x5c9+5971-0x1d1c);byd_F56(i);}byd_G53.byd_G59=(0xbca+6565-0x256f);byd_G53.
byd_G58=(0x2cf+4502-0x1465);byd_G53.byd_G65=(0x1a62+25-0x1a7b);}void byd_F55(
unsigned char i,unsigned short md[N][N]){uint32_t j=(0xa2+936-0x44a);uint32_t
byd_V72=(0xbc8+2642-0x161a);uint32_t byd_V71=(0xc13+317-0xd50);uint32_t k=
(0x69b+5568-0x1c5b);uint32_t byd_V62=(0x2140+1165-0x25cd);if(i<byd_G53.byd_G70){
for(j=i;j<byd_G53.byd_V50;j++){byd_V72=byd_G53.byd_G51[i];byd_G53.byd_G51[i]=
byd_G53.byd_G51[j];byd_G53.byd_G51[j]=byd_V72;byd_F55(i+(0x4+6796-0x1a8f),md);
byd_V72=byd_G53.byd_G51[i];byd_G53.byd_G51[i]=byd_G53.byd_G51[j];byd_G53.byd_G51
[j]=byd_V72;}}else{byd_V71=(0x18a+358-0x2f0);for(k=(0x7a1+7482-0x24db);k<byd_G53
.byd_G70;k++){byd_V71+=md[byd_G53.byd_A67[k]][byd_G53.byd_A59[k]];}if(byd_V71<=
byd_G53.byd_G61){if(byd_V71<byd_G53.byd_G61){byd_G53.byd_G61=byd_V71;for(k=
(0x1392+353-0x14f3);k<byd_G53.byd_G70;k++){byd_G53.byd_A70[k]=byd_G53.byd_G51[k]
;}}else{byd_V71=(0x1219+1533-0x1816);byd_V62=(0x1d84+801-0x20a5);if(byd_G53.
byd_A67==byd_G53.byd_A67){for(k=(0xd58+3788-0x1c24);k<byd_G53.byd_G70;k++){if(
byd_G53.byd_A70[k]!=byd_G53.byd_A67[k]){byd_V71+=((byd_G54.byd_A71[byd_G53.
byd_A59[k]]-byd_G53.byd_A65[byd_G53.byd_A67[k]])*(byd_G54.byd_A71[byd_G53.
byd_A59[k]]-byd_G53.byd_A65[byd_G53.byd_A67[k]]))+((byd_G54.byd_A72[byd_G53.
byd_A59[k]]-byd_G53.byd_A66[byd_G53.byd_A67[k]])*(byd_G54.byd_A72[byd_G53.
byd_A59[k]]-byd_G53.byd_A66[byd_G53.byd_A67[k]]));byd_V62+=((byd_G54.byd_A71[
byd_G53.byd_A59[k]]-byd_G53.byd_A65[byd_G53.byd_A70[k]])*(byd_G54.byd_A71[
byd_G53.byd_A59[k]]-byd_G53.byd_A65[byd_G53.byd_A70[k]]))+((byd_G54.byd_A72[
byd_G53.byd_A59[k]]-byd_G53.byd_A66[byd_G53.byd_A70[k]])*(byd_G54.byd_A72[
byd_G53.byd_A59[k]]-byd_G53.byd_A66[byd_G53.byd_A70[k]]));}}}else{for(k=
(0x7e7+944-0xb97);k<byd_G53.byd_G70;k++){if(byd_G53.byd_A70[k]!=byd_G53.byd_A59[
k]){byd_V71+=((byd_G54.byd_A71[byd_G53.byd_A59[k]]-byd_G53.byd_A65[byd_G53.
byd_A67[k]])*(byd_G54.byd_A71[byd_G53.byd_A59[k]]-byd_G53.byd_A65[byd_G53.
byd_A67[k]]))+((byd_G54.byd_A72[byd_G53.byd_A59[k]]-byd_G53.byd_A66[byd_G53.
byd_A67[k]])*(byd_G54.byd_A72[byd_G53.byd_A59[k]]-byd_G53.byd_A66[byd_G53.
byd_A67[k]]));byd_V62+=((byd_G54.byd_A71[byd_G53.byd_A70[k]]-byd_G53.byd_A65[
byd_G53.byd_A67[k]])*(byd_G54.byd_A71[byd_G53.byd_A70[k]]-byd_G53.byd_A65[
byd_G53.byd_A67[k]]))+((byd_G54.byd_A72[byd_G53.byd_A70[k]]-byd_G53.byd_A66[
byd_G53.byd_A67[k]])*(byd_G54.byd_A72[byd_G53.byd_A70[k]]-byd_G53.byd_A66[
byd_G53.byd_A67[k]]));}}}if(byd_V71<byd_V62){for(k=(0x22b6+481-0x2497);k<byd_G53
.byd_G70;k++){byd_G53.byd_A70[k]=byd_G53.byd_G51[k];}}}}}}void byd_F50(unsigned
char byd_V82){unsigned char i,j;unsigned char k=(0x1727+3975-0x26ae),k1=
(0x4b+2198-0x8e1),k2=(0x36f+5790-0x1a0d),k3=(0x1c09+2353-0x253a);unsigned short
byd_A61[N][N];unsigned short byd_V57=(0x888+3743-0x1727);unsigned short byd_V52=
(0xf79+3258-0x1c33);unsigned short byd_V103=(0x928+2629-0x136d),byd_V104=
(0x1ef6+1093-0x233b);unsigned char byd_A64[N]={(0xd82+5813-0x2437)};unsigned
char byd_A56[N]={(0x349+1757-0xa26)};for(i=(0x482+3727-0x1311);i<N;i++){byd_G53.
byd_A57[i]=(0x9dd+5528-0x1f75);byd_G53.byd_A58[i]=(0x66a+5924-0x1d8e);byd_G53.
byd_A70[i]=(0x921+5922-0x2043);for(j=(0x43a+112-0x4aa);j<N;j++){byd_A61[i][j]=
byd_G75;}}if(byd_V82<byd_G53.byd_G58){if(byd_G53.byd_G65++<byd_V96){for(i=
(0x12f3+3111-0x1f1a);i<N;i++){byd_G53.byd_A57[i]=byd_G53.byd_A65[i];byd_G53.
byd_A58[i]=byd_G53.byd_A66[i];}return;}}byd_G53.byd_G65=(0x2598+191-0x2657);
byd_G53.byd_G58=byd_V82;if(byd_G53.byd_G59<byd_V82){byd_G53.byd_G59=byd_V82;}for
(i=(0x3e2+3103-0x1001);i<byd_G53.byd_G59;i++){if((byd_G53.byd_A65[i]!=
(0xa4a+1572-0x106e))||(byd_G53.byd_A66[i]!=(0x13c7+2630-0x1e0d))){for(j=
(0x2bd+5168-0x16ed);j<byd_V82;j++){if((byd_G54.byd_A71[j]>=(0x1d2+5911-0x18e9))
&&(byd_G54.byd_A71[j]<=byd_G63)&&(byd_G54.byd_A72[j]>=(0xfe8+2473-0x1991))&&(
byd_G54.byd_A72[j]<=byd_G64)){byd_V103=abs(byd_G54.byd_A71[j]-byd_G53.byd_A65[i]
);byd_V104=abs(byd_G54.byd_A72[j]-byd_G53.byd_A66[i]);byd_V52=byd_V103+byd_V104;
if(byd_V52<byd_G75){byd_A61[i][j]=byd_V52;}}}}}if(byd_G53.byd_G59<
(0x76c+2296-0x1060)){byd_G53.byd_G70=(0x1c81+596-0x1ed5);byd_G53.byd_V50=
(0x68c+1018-0xa86);for(i=(0x13d+6793-0x1bc6);i<byd_G53.byd_G59;i++){byd_G53.
byd_A67[i]=(0x1de7+838-0x212d);byd_G53.byd_A59[i]=i;if((byd_G53.byd_A65[i]!=
(0x649+6201-0x1e82))||(byd_G53.byd_A66[i]!=(0x2032+1179-0x24cd))){byd_G53.
byd_A67[byd_G53.byd_G70]=i;byd_G53.byd_G70++;}}byd_G53.byd_V50=byd_G53.byd_G70;
if(byd_G53.byd_G70>(0x6f3+357-0x858)){if(byd_G53.byd_V50>=byd_V82){byd_G53.
byd_G51=byd_G53.byd_A67;byd_G53.byd_G70=byd_V82;}else{byd_G53.byd_G51=byd_G53.
byd_A59;byd_G53.byd_V50=byd_V82;}byd_G53.byd_G61=(0x130b+3479-0x20a2);for(i=
(0x1f35+92-0x1f91);i<byd_G53.byd_G70;i++){byd_G53.byd_A70[i]=byd_G53.byd_G51[i];
byd_G53.byd_G61+=byd_A61[byd_G53.byd_A67[i]][i];}byd_F55((0x95a+853-0xcaf),
byd_A61);if(byd_G53.byd_G51==byd_G53.byd_A67)
{for(i=(0x4d9+359-0x640);i<byd_G53.byd_G70;i++){k=byd_G53.byd_A70[i];if(byd_A61[
k][i]<byd_G75){byd_A64[k]=(0x38f+2141-0xbeb);byd_A56[i]=(0x1145+2975-0x1ce3);
byd_G53.byd_A57[k]=byd_G54.byd_A71[i];byd_G53.byd_A58[k]=byd_G54.byd_A72[i];
byd_G53.byd_A65[k]=byd_G54.byd_A71[i];byd_G53.byd_A66[k]=byd_G54.byd_A72[i];
byd_G54.byd_A71[i]=(0x44c+7926-0x2342);byd_G54.byd_A72[i]=(0x1ca+2219-0xa75);}}}
else{for(i=(0xb06+5058-0x1ec8);i<byd_G53.byd_G70;i++){k1=byd_G53.byd_A67[i];k2=
byd_G53.byd_A70[i];if(byd_A61[k1][k2]<byd_G75){byd_A64[k1]=(0x8d9+2869-0x140d);
byd_A56[k2]=(0x9ea+4761-0x1c82);byd_G53.byd_A57[k1]=byd_G54.byd_A71[k2];byd_G53.
byd_A58[k1]=byd_G54.byd_A72[k2];byd_G53.byd_A65[k1]=byd_G54.byd_A71[k2];byd_G53.
byd_A66[k1]=byd_G54.byd_A72[k2];byd_G54.byd_A71[k2]=(0x782+3157-0x13d7);byd_G54.
byd_A72[k2]=(0x1aaa+647-0x1d31);}}}}}else{for(k3=(0xee6+409-0x107f);k3<byd_G53.
byd_G59;k3++){byd_V57=byd_G75;for(i=(0xae1+3080-0x16e9);i<byd_G53.byd_G59;i++){
if(((byd_G53.byd_A65[i]!=(0x7ca+4463-0x1939))||(byd_G53.byd_A66[i]!=
(0xa4d+3385-0x1786)))&&(byd_A64[i]==(0xc66+2383-0x15b5))){for(j=
(0x238+4059-0x1213);j<byd_V82;j++){if((byd_A56[j]==(0x7c0+869-0xb25))&&(byd_A61[
i][j]<byd_V57)){byd_V57=byd_A61[i][j];k1=i;k2=j;}}}}if(byd_V57<byd_G75){byd_A64[
k1]=(0xaa7+101-0xb0b);byd_A56[k2]=(0x153+4631-0x1369);byd_G53.byd_A57[k1]=
byd_G54.byd_A71[k2];byd_G53.byd_A58[k1]=byd_G54.byd_A72[k2];byd_G53.byd_A65[k1]=
byd_G54.byd_A71[k2];byd_G53.byd_A66[k1]=byd_G54.byd_A72[k2];byd_G54.byd_A71[k2]=
(0xada+6867-0x25ad);byd_G54.byd_A72[k2]=(0x1b24+3025-0x26f5);}}}for(i=
(0x125b+449-0x141c);i<byd_G53.byd_G59;i++){if((byd_A64[i]==(0x3b3+1400-0x92b))&&
(byd_G53.byd_A65[i]==(0x546+2836-0x105a))&&(byd_G53.byd_A66[i]==
(0x8a6+6329-0x215f))){for(j=(0x367+4215-0x13de);j<byd_V82;j++){if((byd_A56[j]==
(0x633+1238-0xb09))&&((byd_G54.byd_A71[j]!=(0x491+7128-0x2069))||(byd_G54.
byd_A72[j]!=(0x39b+8007-0x22e2)))){byd_G53.byd_A65[i]=byd_G54.byd_A71[j];byd_G53
.byd_A66[i]=byd_G54.byd_A72[j];byd_G53.byd_A57[i]=byd_G54.byd_A71[j];byd_G53.
byd_A58[i]=byd_G54.byd_A72[j];byd_G54.byd_A71[j]=(0x1377+3561-0x2160);byd_G54.
byd_A72[j]=(0x187d+9-0x1886);byd_A64[i]=(0xa36+3019-0x1600);byd_A56[j]=
(0x886+3515-0x1640);break;}}}}for(i=(0x2e0+7903-0x21bf);i<N;i++){if(byd_G53.
byd_A57[i]==(0x487+355-0x5ea)&&byd_G53.byd_A58[i]==(0x244+7773-0x20a1)){byd_G53.
byd_A65[i]=(0x363+8065-0x22e4);byd_G53.byd_A66[i]=(0x76a+1598-0xda8);}}}void
byd_F54(void){uint32_t i;int k;uint32_t byd_V63,byd_V64;uint32_t byd_V56,byd_V53
,byd_V54,byd_V55;uint8_t byd_V81;byd_V81=byd_V100;byd_V56=byd_V95;byd_V53=
(0x141+9403-0x25dc)-(0x1a03+1890-0x2161)-byd_V95;byd_V54=(0x122c+1026-0x162b);
byd_V55=(0x15e1+3179-0x224b);for(i=(0x1132+5072-0x2502);i<N;i++){if(byd_G53.
byd_A57[i]!=(0x1c3+8751-0x23f2)||byd_G53.byd_A58[i]!=(0x15ad+3533-0x237a)){
if(byd_G54.byd_A60[i]==byd_V99){byd_G54.byd_A60[i]=byd_V81;}
if(byd_G54.byd_A60[i]<byd_V81){byd_G54.byd_A60[i]++;byd_G54.byd_A54[i]=
(0x2393+201-0x245c);byd_G54.byd_A55[i]=(0x9e2+7265-0x2643);}else{if(byd_G54.
byd_A62[i]<(0x1c10+2590-0x262d)){byd_G54.byd_A62[i]++;for(k=(0x14db+745-0x17c4);
k<(0x19ff+2991-0x25aa);k++){byd_G54.byd_A73[i][k].x=byd_G53.byd_A57[i];byd_G54.
byd_A73[i][k].y=byd_G53.byd_A58[i];}byd_G54.byd_A54[i]=byd_G53.byd_A57[i];
byd_G54.byd_A55[i]=byd_G53.byd_A58[i];}else{byd_V63=(byd_G53.byd_A57[i]*byd_V56+
byd_G54.byd_A73[i][(0x974+3813-0x1859)].x*byd_V53+byd_G54.byd_A73[i][
(0x48c+2679-0xf02)].x*byd_V54+byd_G54.byd_A73[i][(0x12c9+3857-0x21d8)].x*byd_V55
+(0x7c2+4488-0x193a))>>(0x71c+2210-0xfb9);byd_V64=(byd_G53.byd_A58[i]*byd_V56+
byd_G54.byd_A73[i][(0x106+3072-0xd06)].y*byd_V53+byd_G54.byd_A73[i][
(0x76b+1870-0xeb8)].y*byd_V54+byd_G54.byd_A73[i][(0x2c6+7875-0x2187)].y*byd_V55+
(0xda1+1373-0x12ee))>>(0x5e9+7581-0x2381);for(k=(0x9b+253-0x196);k>=
(0xb3d+1566-0x115b);k--){byd_G54.byd_A73[i][k+(0x141b+982-0x17f0)].x=byd_G54.
byd_A73[i][k].x;byd_G54.byd_A73[i][k+(0x860+1344-0xd9f)].y=byd_G54.byd_A73[i][k]
.y;}byd_G54.byd_A73[i][(0x237+278-0x34d)].x=byd_V63;byd_G54.byd_A73[i][
(0xa5+2021-0x88a)].y=byd_V64;byd_G54.byd_A54[i]=byd_V63;byd_G54.byd_A55[i]=
byd_V64;}}}else{byd_F56(i);}}}uint32_t byd_F53(void){uint32_t i=
(0x1e7d+1824-0x259d),byd_V75=(0xba1+5268-0x2035);uint32_t byd_V65=
(0xad6+7196-0x26f2);uint32_t byd_V66=(0x12f6+1666-0x1978);uint32_t byd_V68,
byd_V69;uint32_t byd_V83,byd_V84,byd_V85,byd_V86;uint32_t byd_V87,byd_V88,
byd_V89,byd_V90;uint32_t byd_V91,byd_V92;unsigned short byd_A75[N]={
(0x971+6665-0x237a)};unsigned short byd_A76[N]={(0xdad+1382-0x1313)};byd_V91=
byd_V97;byd_V92=byd_V98;byd_V83=byd_V91/(0x968+1946-0x10ff);byd_V84=byd_V91*
(0x2358+252-0x2452);byd_V85=byd_V91*(0x51b+4396-0x1644);byd_V86=byd_V91*
(0x604+1497-0xbd9);byd_V87=byd_V92/(0x7f0+4316-0x18c9);byd_V88=byd_V92*
(0x156c+4112-0x257a);byd_V89=byd_V92*(0x7b6+1236-0xc87);byd_V90=byd_V92*
(0x23a+2465-0xbd7);for(i=(0x189b+870-0x1c01);i<N;i++){if((byd_G54.byd_A54[i]!=
(0x341+5951-0x1a80))||(byd_G54.byd_A55[i]!=(0x21b9+658-0x244b))){byd_V75++;
byd_A75[i]=(unsigned short)(((uint32_t)(byd_G54.byd_A54[i]*(byd_G56-
(0x4a3+2105-0xcdb))))/byd_G63);byd_A76[i]=(unsigned short)(((uint32_t)(byd_G54.
byd_A55[i]*(byd_G57-(0xcea+4891-0x2004))))/byd_G64);
#if byd_C50
byd_V68=(0x8a+2889-0xbd3);byd_V69=(0x2cc+5322-0x1796);if(byd_G54.byd_A63[i]==
(0x15a2+188-0x165e)){byd_G54.byd_A63[i]=(0x112b+4428-0x2276);}else{byd_V65=
byd_A75[i]>byd_G54.byd_A68[i]?(byd_A75[i]-byd_G54.byd_A68[i]):(byd_G54.byd_A68[i
]-byd_A75[i]);byd_V66=byd_A76[i]>byd_G54.byd_A69[i]?(byd_A76[i]-byd_G54.byd_A69[
i]):(byd_G54.byd_A69[i]-byd_A76[i]);if((byd_V65>byd_V91&&byd_V66>byd_V87)||(
byd_V65>byd_V83&&byd_V66>byd_V92)){byd_V68=byd_V65;byd_V69=byd_V66;}else{if(
byd_V65>byd_V91)byd_V68=byd_V65;if(byd_V66>byd_V92)byd_V69=byd_V66;}if(byd_V65<=
byd_V84&&byd_V66<=byd_V88){byd_V68=byd_V68/(0x253+5185-0x1690);byd_V69=byd_V69/
(0x18d3+2511-0x229e);}else if(byd_V65<=byd_V85&&byd_V66<=byd_V89){byd_V68=
byd_V68/(0x160d+80-0x165b);byd_V69=byd_V69/(0x1811+2160-0x207f);}else if(byd_V65
<=byd_V86&&byd_V66<=byd_V90){byd_V68=byd_V68*(0x1fb+1985-0x9b9)/
(0x28f+638-0x509);byd_V69=byd_V69*(0xc3a+6821-0x26dc)/(0xd33+4445-0x1e8c);}
byd_A75[i]=byd_A75[i]>byd_G54.byd_A68[i]?(byd_G54.byd_A68[i]+byd_V68):(byd_G54.
byd_A68[i]-byd_V68);byd_A76[i]=byd_A76[i]>byd_G54.byd_A69[i]?(byd_G54.byd_A69[i]
+byd_V69):(byd_G54.byd_A69[i]-byd_V69);}byd_G54.byd_A68[i]=byd_A75[i];byd_G54.
byd_A69[i]=byd_A76[i];
#endif
#if byd_C51
if(byd_G54.byd_A82[i]==(0xc94+3461-0x1a19)){byd_G54.byd_A82[i]=
(0x125b+3619-0x207d);byd_G54.byd_A71[i]=byd_A75[i];byd_G54.byd_A72[i]=byd_A76[i]
;byd_G54.byd_A83[i]=byd_A75[i];byd_G54.byd_A84[i]=byd_A76[i];}else{byd_A75[i]=(
byd_G54.byd_A83[i]+byd_A75[i])/(0x355+8018-0x22a5);byd_A76[i]=(byd_G54.byd_A84[i
]+byd_A76[i])/(0x14ef+213-0x15c2);byd_V65=byd_A75[i]>byd_G54.byd_A83[i]?(byd_A75
[i]-byd_G54.byd_A83[i]):(byd_G54.byd_A83[i]-byd_A75[i]);byd_V66=byd_A76[i]>
byd_G54.byd_A84[i]?(byd_A76[i]-byd_G54.byd_A84[i]):(byd_G54.byd_A84[i]-byd_A76[i
]);if(byd_G54.byd_A82[i]==(0x12e7+4501-0x247b)){if((byd_V65>byd_C53&&byd_V66>
byd_C54)||(byd_V65>byd_C54&&byd_V66>byd_C53)){byd_G54.byd_A71[i]=byd_A75[i];
byd_G54.byd_A72[i]=byd_A76[i];byd_G54.byd_A83[i]=byd_A75[i];byd_G54.byd_A84[i]=
byd_A76[i];byd_G54.byd_A82[i]=(0xa9c+890-0xe14);}else{if(byd_V65>byd_C53){
byd_G54.byd_A71[i]=byd_A75[i];byd_G54.byd_A83[i]=byd_A75[i];byd_G54.byd_A82[i]=
(0x1750+3076-0x2352);}else{byd_G54.byd_A71[i]=byd_G54.byd_A83[i];}if(byd_V66>
byd_C53){byd_G54.byd_A72[i]=byd_A76[i];byd_G54.byd_A84[i]=byd_A76[i];byd_G54.
byd_A82[i]=(0x707+4715-0x1970);}else{byd_G54.byd_A72[i]=byd_G54.byd_A84[i];}}}
else{if((byd_V65>byd_C52&&byd_V66>byd_C55)||(byd_V65>byd_C55&&byd_V66>byd_C52)){
byd_G54.byd_A71[i]=byd_A75[i];byd_G54.byd_A72[i]=byd_A76[i];byd_G54.byd_A83[i]=
byd_A75[i];byd_G54.byd_A84[i]=byd_A76[i];}else{if(byd_V65>byd_C52){byd_G54.
byd_A71[i]=byd_A75[i];byd_G54.byd_A83[i]=byd_A75[i];}else{byd_G54.byd_A71[i]=
byd_G54.byd_A83[i];}if(byd_V66>byd_C52){byd_G54.byd_A72[i]=byd_A76[i];byd_G54.
byd_A84[i]=byd_A76[i];}else{byd_G54.byd_A72[i]=byd_G54.byd_A84[i];}}}}
#else
byd_G54.byd_A71[i]=byd_A75[i];byd_G54.byd_A72[i]=byd_A76[i];
#endif
}else{byd_G54.byd_A71[i]=(0x431+3652-0x1275);byd_G54.byd_A72[i]=
(0x980+7453-0x269d);byd_G54.byd_A83[i]=(0x143b+3580-0x2237);byd_G54.byd_A84[i]=
(0x1056+4419-0x2199);byd_G54.byd_A68[i]=(0xe4b+2913-0x19ac);byd_G54.byd_A69[i]=
(0x12c2+4850-0x25b4);byd_G54.byd_A63[i]=(0x111+4912-0x1441);byd_G54.byd_A82[i]=
(0x1cc7+1878-0x241d);}}return(byd_V75);}uint32_t byd_F58(void){uint32_t i,j;
uint32_t byd_V101=(0x18b0+687-0x1b5f);for(i=(0x19f0+1300-0x1f04);i<N;i++){if(
byd_G53.byd_A57[i]!=(0x408+7104-0x1fc8)||byd_G53.byd_A58[i]!=
(0x124c+3563-0x2037)){if(byd_A87[i]==(0x111d+4970-0x2487)){byd_A87[i]++;byd_A85[
(0x1b1c+863-0x1e7b)][i]=byd_G53.byd_A57[i];byd_A86[(0x1067+3096-0x1c7f)][i]=
byd_G53.byd_A58[i];byd_G53.byd_A57[i]=(0xee2+5196-0x232e);byd_G53.byd_A58[i]=
(0x335+4853-0x162a);}else if(byd_A87[i]==(0x725+7016-0x228c)){byd_A87[i]++;
byd_A85[(0xe32+4828-0x210d)][i]=byd_G53.byd_A57[i];byd_A86[(0xb93+1756-0x126e)][
i]=byd_G53.byd_A58[i];byd_G53.byd_A57[i]=byd_A85[(0x963+6539-0x22ee)][i];byd_G53
.byd_A58[i]=byd_A86[(0xbc+9735-0x26c3)][i];byd_V101++;}else{byd_A85[
(0x41b+6049-0x1bba)][i]=byd_G53.byd_A57[i];byd_A86[(0xb17+1066-0xf3f)][i]=
byd_G53.byd_A58[i];if(((abs(byd_A85[(0xa84+1004-0xe6f)][i]-byd_A85[
(0x934+1955-0x10d7)][i])+abs(byd_A86[(0x1c0d+680-0x1eb4)][i]-byd_A86[
(0x12e7+4826-0x25c1)][i]))<byd_V94)&&(byd_V102==(0x11c4+822-0x14fa))){byd_G53.
byd_A57[i]=byd_A85[(0x478+8721-0x2688)][i];byd_G53.byd_A58[i]=byd_A86[
(0x1738+2368-0x2077)][i];byd_V101++;byd_V102=(0x12ba+3902-0x21f8);}else if((abs(
byd_G53.byd_A57[i]-byd_A85[(0xbea+1465-0x11a2)][i])+abs(byd_G53.byd_A58[i]-
byd_A86[(0x1041+2127-0x188f)][i]))<byd_V93){if(byd_V102==(0x178+1347-0x6ba)){
byd_V102=(0x55b+1958-0xd01);byd_G53.byd_A57[i]=(0x253+5109-0x1648);byd_G53.
byd_A58[i]=(0x3b7+7595-0x2162);}else{byd_G53.byd_A57[i]=(0x139b+4591-0x258a);
byd_G53.byd_A58[i]=(0xa44+6655-0x2443);byd_V102=(0xe6+2671-0xb54);}}else{byd_G53
.byd_A57[i]=byd_A85[(0x79d+4014-0x174a)][i];byd_G53.byd_A58[i]=byd_A86[
(0x218+9021-0x2554)][i];byd_V101++;byd_V102=(0x5d9+493-0x7c6);}for(j=
(0x124f+1879-0x19a6);j<(0x195d+3244-0x2607);j++){byd_A85[j][i]=byd_A85[j+
(0xe6b+1438-0x1408)][i];byd_A86[j][i]=byd_A86[j+(0x1604+390-0x1789)][i];}

}}else{byd_A87[i]=(0xef2+6095-0x26c1);for(j=(0x14a+8161-0x212b);j<
(0xccb+4117-0x1cdd);j++){byd_A85[j][i]=(0x1093+849-0x13e4);byd_A86[j][i]=
(0x16c7+3064-0x22bf);}}}if(byd_V102==(0x1665+2989-0x2211)){return
(0x9ac+5476-0x1e11);}return byd_V101;}void byd_F57(uint32_t byd_V74){uint32_t i;
for(i=(0x1816+2053-0x201b);i<sizeof(byd_A74);i++)
{byd_A74[i]=byd_V74;}}void byd_F52(uint32_t byd_V73){uint32_t i,j,k=
(0x16fb+2545-0x20ec),n=(0x214d+695-0x2404);unsigned short byd_V51=
(0x6f7+7884-0x25c3);if(byd_V73==(0x172b+1993-0x1df5)){byd_F57(
(0x9cb+2546-0x13bd));byd_A74[(0x984+6359-0x225b)]=(0x1afa+826-0x1db4);byd_G72=
(0x1b71+1039-0x1f80);}else{byd_A74[(0x279+6020-0x19fd)]=(0x2+7910-0x1ee8);for(j=
(0x8a8+2108-0x10e3);j<((N<<(0xf17+310-0x104b))+(0x5d1+7354-0x228a));){if((
byd_A74[j+(0xf+6542-0x199b)]&(0x1702+4099-0x2615))==(0x661+860-0x8fd))
{for(k=j;k<((N<<(0x1204+813-0x152f))+(0x97+618-0x300));k+=(0xbe8+1415-0x116b)){
byd_A74[k]=byd_A74[k+(0x455+4309-0x1526)];byd_A74[k+(0x83b+98-0x89c)]=byd_A74[k+
(0x1aed+1410-0x206a)];byd_A74[k+(0x28+7520-0x1d86)]=byd_A74[k+
(0x674+6264-0x1ee6)];byd_A74[k+(0x1df+5174-0x1612)]=byd_A74[k+
(0x12d7+4387-0x23f3)];byd_A74[k+(0x67b+5874-0x1d69)]=(0xe10+2664-0x1878);byd_A74
[k+(0x429+5667-0x1a47)]=(0x11d6+2015-0x19b5);byd_A74[k+(0x30f+1021-0x706)]=
(0x3cc+8746-0x25f6);byd_A74[k+(0x33a+1105-0x784)]=(0x1c7b+982-0x2051);}}else{j+=
(0x305+8888-0x25b9);}}for(i=(0x74f+7701-0x2564);i<N;i++){byd_V51=
(0x158a+3848-0x2491)<<i;if((byd_G54.byd_A71[i]!=(0x570+2414-0xede))||(byd_G54.
byd_A72[i]!=(0x992+5901-0x209f)))
{for(j=(0x99b+194-0xa5c);j<((N)<<(0x8ac+866-0xc0c))+(0xd0c+1399-0x1282);j+=
(0x112+7157-0x1d03)){if(((byd_A74[j]>>(0x366+1451-0x90d))==(i+
(0x1fe6+343-0x213c)))||((byd_A74[j]>>(0xd12+4327-0x1df5))==(0xf61+1237-0x1436)))
{k=j;
break;}}byd_A74[k]=((byd_G54.byd_A71[i])>>(0xbad+2119-0x13ec))|((i+
(0x1ac2+1267-0x1fb4))<<(0x1c62+1235-0x2131));byd_A74[k+(0x4d0+115-0x542)]=
byd_G54.byd_A71[i];if((byd_G72&byd_V51)==(0x1326+3262-0x1fe4)){byd_G72|=byd_V51;
byd_A74[k+(0x7fd+4693-0x1a50)]=(byd_G54.byd_A72[i]>>(0x12ac+3153-0x1ef5))|
(0x3fd+6487-0x1cb4);}else{byd_A74[k+(0x66+9476-0x2568)]=(byd_G54.byd_A72[i]>>
(0x437+3997-0x13cc))|(0xf51+4442-0x201b);}byd_A74[k+(0x490+6931-0x1fa0)]=byd_G54
.byd_A72[i];n++;}else{if((byd_G72&byd_V51)==byd_V51){byd_G72&=~byd_V51;for(j=
(0x2d9+1177-0x771);j<(N<<(0xdb8+1761-0x1497))+(0x430+149-0x4c4);j+=
(0x2eb+8677-0x24cc)){if((byd_A74[j]>>(0x1428+1962-0x1bce))==(i+
(0x14cc+3318-0x21c1)))
{byd_A74[j]|=((i+(0x1da9+1664-0x2428))<<(0x2c4+663-0x557));byd_A74[j+
(0x1259+5081-0x2630)]&=(0x1cbf+2565-0x26b5);byd_A74[j+(0x8d+5633-0x168c)]|=
(0x2c9+7013-0x1d6e);break;}}n++;}}}if(n>=byd_G60)
{n=byd_G60;}byd_A74[(0x2f3+323-0x436)]=n|(0x6a6+1413-0xbab);}}int
byd_ts_read_coor(struct i2c_client*client){int i,byd_V70=(0x1454+2526-0x1e32),
byd_V82;unsigned int byd_V73=(0x12fd+9-0x1306);uint8_t byd_A77[
(0x25b+6579-0x1c0a)]={(0x9c3+2986-0x156d)};



byd_V70=i2c_smbus_read_i2c_block_data(client,(0x3f5+4757-0x1600),
(0xa55+5772-0x20dd),byd_A77);
if(byd_V70<=(0x527+6267-0x1da2)){return byd_V70;}byd_V96=byd_A77[
(0x1e36+144-0x1ec6)];byd_V100=byd_A77[(0x409+7342-0x20b6)];byd_V97=byd_A77[
(0x1dc+4357-0x12df)];byd_V98=byd_A77[(0x166+1161-0x5ec)];

if(byd_V99==(0x1476+1955-0x1b1a))byd_V99=byd_V100;byd_V82=buf[
(0x1dcf+2195-0x2662)]&(0x1136+2649-0x1b80);for(i=(0x1256+261-0x135b);i<N;i++){
byd_G54.byd_A71[i]=(0x17a7+761-0x1aa0);byd_G54.byd_A72[i]=(0x612+6094-0x1de0);}
for(i=(0x22e9+96-0x2349);i<byd_V82;i++){byd_G54.byd_A71[i]=((unsigned short)buf[
(0x1d07+937-0x20ac)*i+(0x11d2+1771-0x18bc)]<<(0x141+78-0x187))|buf[
(0x412+5397-0x1923)*i+(0x57d+2146-0xddd)];byd_G54.byd_A72[i]=((unsigned short)
buf[(0x1401+3087-0x200c)*i+(0x822+3054-0x140d)]<<(0x102b+1905-0x1794))|buf[
(0x42+9700-0x2622)*i+(0xfff+1782-0x16f1)];}if(byd_V82>(0x1c01+802-0x1f23)){
byd_F50(byd_V82);}else{byd_F51();}byd_V73=byd_F58();if(byd_V73==
(0x1308+2978-0x1dab)){return(0x3c5+5394-0x18d7);}byd_F54();byd_V73=byd_F53();if(
byd_V73){byd_G74=(0xbc6+4451-0x1d27);byd_F52(byd_V73);}else{if(byd_G74){if(
byd_G74==(0x2ea+7517-0x2045)){byd_F52(byd_V73);
}else{byd_F52((0x831+6362-0x200c));
}byd_G74--;}else{byd_V70=(0xc3f+6148-0x2443);}}byd_V99=byd_V100;for(i=
(0x9e7+7269-0x264c);i<(0x21c+1096-0x632);i++){buf[i]=byd_A74[i];}return byd_V70;
}int byd_ts_read_IC_para(struct i2c_client*client){unsigned int i,j;unsigned
char byd_G55[(0x3e5+1963-0xb8e)]={(0x13c7+1176-0x185f)};unsigned char byd_A81[
(0x5db+7532-0x2345)]={(0xbfb+5896-0x2303)};int byd_V67=(0xb62+3963-0x1add);
byd_V67=i2c_read_bytes(client,byd_A53,(0x1ddc+1903-0x2549),&byd_G60,
(0x2c6+6754-0x1d27));
byd_V67=i2c_read_bytes(client,byd_A51,(0x113+4552-0x12d9),byd_G55,
(0x272+4059-0x124b));if(byd_V67>(0x613+2122-0xe5d))byd_G56=(byd_G55[
(0xc9a+4435-0x1ded)]<<(0x1333+4901-0x2650))|byd_G55[(0xb30+2444-0x14bb)];else
byd_G56=(0x1f0b+3149-0x2358);byd_V67=i2c_read_bytes(client,byd_A52,
(0x141+7907-0x2022),byd_G55,(0x21f4+327-0x2339));if(byd_V67>(0xbb3+5539-0x2156))
byd_G57=(byd_G55[(0x36f+2821-0xe74)]<<(0x39c+6130-0x1b86))|byd_G55[
(0x2e5+5576-0x18ac)];else byd_G57=(0xd6c+3539-0x133f);
byd_V67=i2c_read_bytes(client,byd_A80,(0x247+1620-0x899),&byd_V95,
(0xc70+1221-0x1134));byd_V67=i2c_read_bytes(client,byd_A78,(0x173d+3596-0x2547),
&byd_V93,(0x15e4+400-0x1773));byd_V67=i2c_read_bytes(client,byd_A79,
(0x1665+4139-0x268e),byd_A81,(0x799+805-0xabc));if(byd_V67>(0xa94+3800-0x196c))
byd_V94=(byd_A81[(0x18c+5161-0x15b5)]<<(0x1362+4186-0x23b4))|byd_A81[
(0x5f7+2126-0xe44)];else byd_V94=(0x984+2924-0x13f1);
byd_G53.byd_G51=byd_G53.byd_A67;byd_F51();for(i=(0x1ab9+2791-0x25a0);i<N;i++){
byd_G54.byd_A71[i]=(0xa62+6583-0x2419);byd_G54.byd_A72[i]=(0x1482+2683-0x1efd);
byd_G54.byd_A83[i]=(0x79f+2476-0x114b);byd_G54.byd_A84[i]=(0x24+5698-0x1666);
byd_G54.byd_A68[i]=(0xa1+4037-0x1066);byd_G54.byd_A69[i]=(0x18cd+2974-0x246b);
byd_G54.byd_A63[i]=(0x2cc+7613-0x2089);byd_G54.byd_A82[i]=(0x100a+4623-0x2219);
byd_A87[i]=(0x1796+998-0x1b7c);for(j=(0x376+6058-0x1b20);j<(0x1987+1203-0x1e37);
j++){byd_A85[j][i]=(0x1dc3+1836-0x24ef);byd_A86[j][i]=(0xd6+4763-0x1371);}}
return byd_V67;}


