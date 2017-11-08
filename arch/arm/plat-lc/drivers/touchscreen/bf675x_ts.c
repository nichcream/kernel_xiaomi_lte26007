 /*
 * File:         byd_bf675x_ts.c
 *
 * Created:	     2012-10-07
 * Depend on:    byd_bf675x_ts.h
 * Description:  BYD TouchScreen IC driver for Android
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

/* History
20140821, Zhu Xiang,
		  Modify the register address of ESD_Test from 68 to 114 .
20140801, Zhu Xiang,
		  Add the <linux/hrtimer.h> in driver.
		  Add micro define : CONFIG_BEAT_TIMER
		  Add global variable : Beat_Interval,This_Hrtimer;
			timer, byd_monitor_work, *byd_monitor_workqueue in struct byt_ts_t;

		  Add Function :  byd_ts_timer_func(),read_beat_para(),
			heartbeat_start(),heartbeat_cancel(),byd_monitor_worker(),

20140630, Xing Weican,	IC algorithm partially intruduced in driver
          function added: byd_ts_read_coor(), byd_ts_read_IC_para()
          global variable: buf[50]
*/

#include <linux/module.h>
//#include <linux/platform_device.h>
//#include <linux/irq.h>
#include <linux/input.h>
#include <linux/slab.h>      // kzalloc()
//#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
//#endif
#include <linux/interrupt.h> // disable_irq(), disable_irq_nosync()
#include <linux/i2c.h>       // i2c_client, i2c_master_send(), i2c_transfer(), I2C_M_RD
#include <linux/delay.h>     // mdelay()

#include <plat/bf675x_ts.h>
//#ifdef CONFIG_REPORT_PROTOCOL_B // add TRACKING_ID for protocol A as well
#include <linux/input/mt.h> // Protocol B, for input_mt_slot(), input_mt_init_slot()
//#endif

////////////////// extension for additional functionalities ///////////////////
// --------------- compile independently --------------------------------------
#include <linux/miscdevice.h> // misc_register(), misc_deregister()

// interface for accessing external functions and global variables
int byd_ts_download(struct i2c_client *client, int module_num);
int byd_ts_get_resolution(struct i2c_client *client, int *xy_res);
int byd_ts_get_firmware_version(struct i2c_client *client, unsigned char* version);
int byd_ts_read_coor(struct i2c_client *client);
int byd_ts_read_IC_para(struct i2c_client *client);
extern struct miscdevice byd_ts_device;
// ----------------------------------------------------------------------------

//#include <linux/kernel.h> 4:KERN_WARNING 5:KERN_NOTICE 6:KERN_INFO 7:KERN_DEBUG
#ifdef TS_DEBUG_MSG
  #define TS_DBG(format, ...) \
	printk(format, ##__VA_ARGS__)
	//printk(KERN_INFO "BYD_TS *** " format "\n", ## __VA_ARGS__)
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

	// ORDER SMOOTH ALGORITHM in driver code.
	  #define CONFIG_COOR_ORDER_SMOOTH_ALGORITHM

	// Use "heart_beating" for ESD TEST.
	  #define CONFIG_BEAT_TIMER

	//for ARK A32/A33
	 // #define CONFIG_PROJECT_IDENTIFY

	#ifdef GPIO_TS_EINT
	// Raw data acquiring
	  #define CONFIG_SUPPORT_RAW_DATA_ACQUISITION
	#endif

#endif

/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

   /* The max number of points/fingers for muti-points TP */
	  #define BYD_TS_MT_NUMBER_POINTS	5 // maximum fingers 10

   /* The maxmum finger ids the IC suppored */
	  #define BYD_IC_POINTS_MAX        10 //////2014-0919 LINX

   /* The size (and max size) of finger touching surface of touch panel */
	  #define BYD_TS_TOUCHSIZE	100

/*    对Android 2.3及以下版本，除了可以使用过去旧协议外，还可以选择触摸设备匿名
      协议（协议A，4.0版本的协议） */
#if !defined(CONFIG_ANDROID_4_ICS) && !defined(CONFIG_REPORT_PROTOCOL_B)
	//#define CONFIG_REPORT_PROTOCOL_A
#endif

   /* Touch Panel Default Resolution:
      The RESOLUTION VALUES MUST BE SAME as those in the chip's resolution of
      given TP. They are used when the TP's IC chip does not support the
      functionality of reading resolution from the chip.
      Since Linux/Adroid is able to automatically adapt TP's resolution to
      Screen's (LCD's), a constant resolution as 2048x2048 is STRONGLY
	  RECOMMENDED.*/
	  #define TS_X_RES  2048 // 320
	  #define TS_Y_RES  2048 // 480

   /* to enable the functionality for resolution transformation in the driver
      (for some special case), the screen (LCD) resolution below should be
      presented */
	//#define REPORT_X_RES  320 //800
	//#define REPORT_Y_RES  480

   /* The swap of X Y is ONLY NEEDD when the resolution of X and Y is same
      (e.g. 2048x2048) and only if this swap is absolutely necessary. DO NOT
      try to use this as a way to adapt screen's orientation otherwise. */
	//#define REPORT_XY_SWAPPED

   /* Coordinate transformation, used only if the coordinates in the chip is not
      properly transformed. NOTE: Changing the chip's firmware is prefered way. */
	//#define REPORT_X_REVERSED
	#define REPORT_Y_REVERSED

   /* About Touch Panel (TP) resolution and screen resolution transformation:
        The TP resolution values and its order MUST BE SAME AS the one in
      the chip's resolution registers (0x0c). The order of the resolution
      values determines the orientation of the screen, e.g. 480x320 indicates
      landscape (i.e. horizontal) while 320x480 represents portrait (vertical).
      Be aware that the order of X and Y is determined by the order of the
      presenting of the coordinate values in point registers, the 1st register
      is alwayse X and the 2nd is constantly Y.
        Although this TP driver is capable of transforming TP's resolution to LCD's,
      since Linux/Adroid's up layer is also able to automatically adapt TP's
      resolution to the LCD's (as long as the driver correctly reporting TP's
      resolution), the transformation in the driver is really not necessary,
      and presenting a constant TP resolution as 2048x2048 is STRONGLY
      RECOMMENDED. */

#ifdef CONFIG_BEAT_TIMER

#define BYD_MONITOR
//struct mutex mutex;	// mutual exclution for monitor thread

#include <linux/hrtimer.h>
int heartbeat_start(void);
int heartbeat_cancel(void);

/* Variable   : This_Hrtimer - hrtimer
 * Defined in : byd_ts_probe()
 * Accessed by: heartbeat_start(),heartbeat_cancel().
 */
static struct hrtimer *This_Hrtimer;

/* Variable   : Beat_Interval - time interval for heartbeat
 * Defined in : read_beat_para()
 * Accessed by: heartbeat_start(),heartbeat_cancel(),byd_ts_probe().
 */
static unsigned char Beat_Interval = 0;

#endif // CONFIG_BEAT_TIMER

struct byd_ts_t {
	struct input_dev *input;
	struct i2c_client *client;
	struct work_struct ts_work;
	struct workqueue_struct *ts_work_queue;

 #ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend ts_early_suspend;
 #endif

 #ifdef CONFIG_BEAT_TIMER
	struct hrtimer timer;
   #ifdef BYD_MONITOR
	struct delayed_work byd_monitor_work;
	struct workqueue_struct *byd_monitor_workqueue;
   #endif
 #endif
};

/*** Global variables *********************************************************/

/* Variable   : This_Client - i2c client
 * Defined in : byd_ts_probe()
 * Accessed by: byd_ts_exit(), byd_bfxxxx_ts_extn.c: byd_ts_ioctl(), byd_ts_read()
 */
struct i2c_client *This_Client;

/* Variable   : buf - buffer for touch
 * Defined in : byd_ts_read_coor()
 * Accessed by: byd_ts_work()
 */
unsigned char buf[1 + BYD_IC_POINTS_MAX * 4 + 9];

//for a23,a33 project identify
int ctp_cob_bf675x = 0;
/* Variable   : Irq_Enabled - flag indecating interrupt enabled
 * Accessed by: byd_ts_work(), byd_ts_irq_handler(), some functions for raw data
 */
int Irq_Enabled = 1;

// ----------------------------------------------------------------------------

/* Variable   : XY_res_out - resolution (X*Y) reported to upper layer, literally
 *                  LCD resolution, but Linux/Android accept TP resolution
 *              XY_res_ts  - TP resolution (X*Y), used when REPORT_?_RES are
 *                  presented
 * Defined in : byd_ts_probe()
 * Accessed by: byd_ts_work()
 */
#if defined(REPORT_X_RES) && defined(REPORT_Y_RES)
static int XY_res_ts[2] = {TS_X_RES, TS_Y_RES};          // initialized to TP res
static int XY_res_out[2] = {REPORT_X_RES, REPORT_Y_RES}; // initialized to LCD res
#else
static int XY_res_out[2] = {TS_X_RES, TS_Y_RES};         // initialized to TP res
#endif

/* Variable   : Byd_Ts_Keys - key definition
 * Defined in : byd_ts_probe()
 * Accessed by: byd_ts_register_input_device()
 */
 static unsigned char Byd_Ts_Keys[] = BYD_TS_KEYS;


/*******************************************************************************
* Function    : i2c_read_bytes
* Description : read data to buffer form i2c bus:
*               send i2c commands/address and then read data
*               (i2c_master_send i2c_master_recv)
* Parameters  : input: client, send_buf, send_len
                output: recv_buf, recv_len
* Return      : i2c data transfer status
*******************************************************************************/
int i2c_read_bytes(struct i2c_client *client,
	char *send_buf, int send_len, char *recv_buf, int recv_len)
{
	struct i2c_msg msgs[2];

	msgs[0].flags = !I2C_M_RD; // client->flags & I2C_M_TEN;
	msgs[0].addr = client->addr;
	msgs[0].len = send_len;
	msgs[0].buf = &send_buf[0];
	//msgs[0].scl_rate = 400 * 1000; // msgs[0].timing = 400; // mtk

	msgs[1].flags = I2C_M_RD; // |= I2C_M_RD;
	msgs[1].addr = client->addr;
	msgs[1].len = recv_len;
	msgs[1].buf = &recv_buf[0];
	//msgs[1].scl_rate = 400 * 1000; // msgs[1].timing = 400;; // mtk

	return i2c_transfer(client->adapter, msgs, 2);
	//msleep(5);
}


/*******************************************************************************
* Function    : reset_chip
* Description : reset chip and entering programming mode for CTS15
* Parameters  : input: client
                output: none
* Return      : i2c data transfer status
*******************************************************************************/
int reset_chip(struct i2c_client *client)
{
	int ret, err = 0;
	unsigned char buf[] = {0, 0, 0, 0};

	buf[0] = 0x88;
	ret = i2c_smbus_write_i2c_block_data(client, 0xe0, 1, buf);
	if (ret < 0) err = ret;
	msleep(20);

	buf[0] = 0x04;
	ret = i2c_smbus_write_i2c_block_data(client, 0xe4, 1, buf);
	if (ret < 0) err = ret;
	msleep(10);

	buf[0] = 0x00;
	ret = i2c_smbus_write_i2c_block_data(client, 0xbc, 4, buf);
	if (ret < 0) err = ret;
	msleep(10);

	if (err < 0) {
	    pr_err("%s: errno:%d\n", __FUNCTION__, err);
	}
	return err;
}

/*******************************************************************************
* Function    : startup_chip
* Description : start chip running for CTS15
* Parameters  : input: client
                output: none
* Return      : i2c data transfer status
*******************************************************************************/
int startup_chip(struct i2c_client *client)
{
	int err = 0;
	unsigned char buf = 0x00;
	err = i2c_smbus_write_i2c_block_data(client, 0xe0, 1, &buf);
	if (err < 0)
	    pr_err("%s: errno:%d\n", __FUNCTION__, err);
	msleep(30); // wait 30ms until interrupt pause passed. original: msleep(10)
	return err;
}

/*******************************************************************************
* Function    : check_mem_data
* Description : check CRC correctness for CTS15 and if not, download firmware
* Parameters  : input: client
                output: none
* Return      : none
*******************************************************************************/
static void check_mem_data(struct i2c_client *client)
{
	unsigned char buf[] = {0, 0, 0, 0}; // reg = 0xb0;

	//msleep(30); moved into startup_chip()
	//i2c_read_bytes(client, &reg, 1, buf, sizeof(buf));
	i2c_smbus_read_i2c_block_data(client, 0xb0, sizeof(buf), buf);

	if (buf[3] != 0x5a || buf[2] != 0x5a || buf[1] != 0x5a || buf[0] != 0x5a) {
		printk("%s: check mem 0xb0 = %x %x %x %x \n", __FUNCTION__, buf[3], buf[2], buf[1], buf[0]);
		byd_ts_download(client, 0);
#if defined(CONFIG_SUPPORT_PARAMETER_UPG) || defined(CONFIG_SUPPORT_PARAMETER_FILE)
		byd_ts_download(client, -1);
#endif
	}
}


#ifdef CONFIG_BEAT_TIMER

/*******************************************************************************
* Function    : read_beat_para()
* Description : read parameters from chip for CTS15
* Parameters  : input: client
*               output: none
* Return      : status (error if less than zero) or the time (n*1s)
*******************************************************************************/
unsigned char read_beat_para(struct i2c_client *client)
{
	unsigned char ret;
	unsigned char cmd[] = {0x3c,114};//0x3c :cmd ;[115-1=114, REG_115: ESD_Test register]
	unsigned char buf[] = {0, 0, 0, 0};
	Beat_Interval = 0;
	ret = i2c_read_bytes(client, cmd, sizeof(cmd), buf, 1);
	if (ret < 0) {
		pr_err("%s: i2c read PARAMETER error: %d\n", __FUNCTION__, ret);
		return ret;
	}
	//PR_BUF("Read all para in IC", buf, 256);
	if ((buf[0] & 0x80) == 0x00) {
		ret = 0;
		TS_DBG("%s: beat timer function not enabled in IC \n", __FUNCTION__);
	} else {
		ret = buf[0] & 0x7F;
		Beat_Interval = ret;
		if (ret > 0) {
			TS_DBG("%s: Beat_Interval (MTBR): %d\n", __FUNCTION__, ret);
		} else {
			ret = -1;
			pr_err("%s: error - Beat_Interval is 0\n", __FUNCTION__);
		}
	}
	return ret;
}

int heartbeat_start(void)
{
	if (Beat_Interval) {
		hrtimer_start(This_Hrtimer, ktime_set(Beat_Interval + 5, 0), HRTIMER_MODE_REL);
		TS_DBG("hrTimer:%ds ", Beat_Interval + 5);
	}
	return 0;
}

int heartbeat_cancel(void)
{
	if(Beat_Interval) {
		hrtimer_cancel(This_Hrtimer);
		TS_DBG("%s: Cancel hrTimer\n", __FUNCTION__);
	}
	return 0;
}

#endif // CONFIG_BEAT_TIMER

#ifdef BYD_MONITOR
static void byd_monitor_worker(struct work_struct *byd_work)
{
	//disable_irq_nosync(This_Client->irq);
	printk("%s: *** IC heart beat not detected within %d seconds *** \n", __FUNCTION__, Beat_Interval + 5);
	byd_ts_download(This_Client, 0);
#if defined(CONFIG_SUPPORT_PARAMETER_UPG) || defined(CONFIG_SUPPORT_PARAMETER_FILE)
	byd_ts_download(This_Client, -1);
#endif
#if defined(CONFIG_COOR_ORDER_SMOOTH_ALGORITHM)
	byd_ts_read_IC_para(This_Client);
#endif

#ifdef CONFIG_BEAT_TIMER
	heartbeat_start();
	TS_DBG("\n");
#endif
	//enable_irq(This_Client->irq);
}
#endif // BYD_MONITOR

#ifdef CONFIG_BEAT_TIMER
/*******************************************************************************
* Function    :  byd_ts_timer_func
* Description :  ISR for IC heartbeat detection, response when timeout
* In          :  timer associated with this function
* Return      :  working mode of timer, HRTIMER_NORESTART returned indicates
*                there is no need for timer to restart
*******************************************************************************/
static enum hrtimer_restart byd_ts_timer_func(struct hrtimer *timer)
{
	struct byd_ts_t *ts = container_of(timer, struct byd_ts_t, timer);

	if (!delayed_work_pending(&ts->byd_monitor_work)) {
		queue_delayed_work(ts->byd_monitor_workqueue, &ts->byd_monitor_work, 20);
		TS_DBG("%s: Add byd_monitor_work to byd_monitor_workqueue \n", __FUNCTION__);
	} else { // qi ximing: this may cause dada lost??
		pr_err("*** byd_monitor_workqueue is full! ***\n");
	}

	TS_DBG("%s: go to func [byd_monitor_worker]\n", __FUNCTION__);
	return HRTIMER_NORESTART;
}
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
/*******************************************************************************
* Function    :  byd_ts_early_suspend
* Description :  put ts to sleep or shutdown mode
* In          :  handler
* Return      :  none
*******************************************************************************/
static void byd_ts_early_suspend(struct early_suspend *handler)
{
	struct byd_ts_t *ts;
	TS_DBG("%s\n", __FUNCTION__);
	ts =  container_of(handler, struct byd_ts_t, ts_early_suspend);

#ifdef BYD_MONITOR
//	TS_DBG( "%s: cancel byd_monitor_work\n", __FUNCTION__);
//	cancel_delayed_work_sync(&ts->byd_monitor_work);
#endif
#ifdef CONFIG_BEAT_TIMER
	heartbeat_cancel();
#endif
	disable_irq(ts->client->irq);
	byd_ts_shutdown_low();
}

/*******************************************************************************
* Function    :  byd_ts_late_resume
* Description :  ts re-entry the normal mode and schedule the work, there need
                 to be a little time for ts ready
* In          :  handler
* Return      :  none
*******************************************************************************/
static void byd_ts_late_resume(struct early_suspend *handler)
{
	struct byd_ts_t *ts;
	TS_DBG("%s\n", __FUNCTION__);
	ts =  container_of(handler, struct byd_ts_t, ts_early_suspend);

	byd_ts_shutdown_high(); // hardware interrupt signaled here
	msleep(10); // avoid some unexpected interrupt after resume
	// after initial process, irq port is pull low by IC software
  #if defined(TS_DEBUG_MSG) && 0
	TS_DBG("%s: Warning: check_mem_data disabled in debug mode!\n", __FUNCTION__);
	// for interrupt and I2C test:
	TS_DBG("%s: Debugging: testing interrupt and I2C comm in debug mode:\n", __FUNCTION__);
	TS_DBG("%s: Debugging: hardware interrupt supposed if irq pin has a pull-up resistor.\n", __FUNCTION__);
	enable_irq(ts->client->irq); // start hardware interrupt read data here
	msleep(5); // wait until data being read by hardware interrupt
	TS_DBG("%s: Debugging: software interrupt supposed here.\n", __FUNCTION__);
	reset_chip(ts->client); // cause an interrupt by irq pin pull-up resistor
	startup_chip(ts->client); // after startup_chip, software will pull-down irq pin again
  #else
	reset_chip(ts->client);
	startup_chip(ts->client);
	msleep(20); // after startup_chip, avoid unexpected interrupt during chip init
	check_mem_data(ts->client);
///////////////////////////////////////////////////////////////////////////////
  #if defined(CONFIG_REPORT_PROTOCOL_B) //&& !defined(CONFIG_REMOVE_POINT_STAY) //&& !defined(TS_DEBUG_MSG)
	{	int i;
		for (i = 0; i < BYD_IC_POINTS_MAX; i++) {
			input_mt_slot(ts->input, i + 1);
			input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
			//input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
		}
		input_sync(ts->input);
		TS_DBG("ALL UP\n");
	}
  #endif
/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
	enable_irq(ts->client->irq);
  #endif

  #ifdef BYD_MONITOR
	TS_DBG( "%s: queue byd_monitor_work\n", __FUNCTION__);
  #endif
  #ifdef CONFIG_BEAT_TIMER
	heartbeat_start();
	TS_DBG("\n");
  #endif

  /* Test for IIC comunication  -xingweican   -20140725
  {
	uint8_t err = 0, j = 0;
	char cmd[1] = {0x0b};
	msleep(600);
	for (j = 0; j < 3; j++) {
		mdelay(50);
		err = i2c_master_send(ts->client, cmd, 1);
		if (err < 0) {
			pr_err("%s: err #%d by i2c_master_send(), size: 1, send[0]:0x%02x\n", __FUNCTION__, err, cmd[0]);
		}
	}
  }
  */
}

#endif // CONFIG_HAS_EARLYSUSPEND

/*******************************************************************************
* Function    : Linux callback function
*******************************************************************************/
//#define BYD_TS_DEVICE		BYD_TS_NAME
#define BYD_TS_REGISTER_BASE	0x5c
#define BYD_TS_MT_POINT_REG	0x5d
#define BYD_TS_MT_TOUCH_REG	0x5c
#define BYD_TS_MT_TOUCH_MASK	0xF0 // 0xD0 for ignoring bit5 (charger id)
#define BYD_TS_MT_TOUCH_FLAG	0x80
#define BYD_TS_MT_KEY_FLAG	0x90
#define BYD_TS_MT_NPOINTS_MASK	0x0f

static void byd_ts_work(struct work_struct *work)
{
	int x, y, ret = 0;
	struct byd_ts_t *ts = container_of(work, struct byd_ts_t, ts_work);
	char cmd;
	uint8_t touch_type;   //screen touch or key touch
	uint8_t touch_numb;
	uint8_t id;
	uint8_t i;
	static int Touch_key_down = 0;

	//TS_DBG("*** ISR WORK entered ***\n");
#ifdef CONFIG_BEAT_TIMER
		heartbeat_start();
#endif

#if !defined(I2C_PACKET_SIZE) || (I2C_PACKET_SIZE > 32)
//////2014-0919 LINX read the number of touches before coordinates
	cmd = BYD_TS_REGISTER_BASE;
	ret = i2c_read_bytes(ts->client, &cmd, 1, &buf[0], 1 + 1 * 4); // read typs and first point
	if (ret <= 0) {
		pr_err("%s: i2c read register error, errno: %d\n", __FUNCTION__, ret);
		goto out;
	}
	touch_numb = buf[BYD_TS_MT_TOUCH_REG - BYD_TS_REGISTER_BASE]
			& BYD_TS_MT_NPOINTS_MASK;

	if (touch_numb > BYD_IC_POINTS_MAX) {
		pr_err("%s: error number of points: %d, please check ptotocol. \n", __FUNCTION__, touch_numb);
		goto out;
	}

	if(touch_numb > 1) { // read second point as well as afterwords
		uint32_t read_buffer_size = (touch_numb - 1) * 4;
		cmd = BYD_TS_MT_POINT_REG + 1 * 4;
		ret = i2c_read_bytes(ts->client, &cmd, 1, &buf[1 + 1 * 4], read_buffer_size);
		if (ret <= 0) {
			pr_err("%s: i2c read register error, errno: %d\n", __FUNCTION__, ret);
			goto out;
		}
	}
///////////////////////////////////
#else
	ret = i2c_smbus_read_i2c_block_data(ts->client, BYD_TS_REGISTER_BASE, 1 + 1 * 4, buf);
	if (ret <= 0) {
		pr_err("%s: i2c read register 0x%02x error, errno: %d. please check protocol.\n",
			  __FUNCTION__, BYD_TS_REGISTER_BASE, ret);
		goto out;
	}
	touch_numb = buf[BYD_TS_MT_TOUCH_REG - BYD_TS_REGISTER_BASE]
			& BYD_TS_MT_NPOINTS_MASK; // buf[0x5c - 0x5c] & 0x0f;
	if (touch_numb > BYD_IC_POINTS_MAX) {
		pr_err("%s: error number of points: %d, please check ptotocol. \n", __FUNCTION__, touch_numb);
		goto out;
	}
	for (i = 1; i < touch_numb; i++) {
		id = i * 4 + 1; // id means index here
		ret = i2c_smbus_read_i2c_block_data(ts->client, BYD_TS_REGISTER_BASE + id, 4, &buf[id]);
	}
#endif
//return;
#ifdef CONFIG_BEAT_TIMER
	if (buf[0] == 0x55 && buf[1] == 0xaa && buf[2] == 0x55 && buf[3] == 0xaa) {
		TS_DBG("%s: Read buf when ESD_enable :\n"
		       " buf = {0x%x, 0x%x, 0x%x, 0x%x} \n",
		 __FUNCTION__, buf[0], buf[1], buf[2], buf[3]);
		goto out;
	}
#endif

#if defined(CONFIG_COOR_ORDER_SMOOTH_ALGORITHM)
	// cts15 read para and process raw coordinates
	if((buf[0] & 0xF0) == 0x80){	// if touch type is point touching
		ret = byd_ts_read_coor(ts->client);
		if (ret < 0) {
			pr_err("%s: error in byd_ts_process_coord: %d\n", __FUNCTION__, ret);
			goto out;
		} else if (ret == 0) {
			TS_DBG("%s: zero returned by byd_ts_process_coord! \n", __FUNCTION__);
			goto out;
		}
	}
#endif

	/*       "CS: 0.TOUCH 1.X1H+Y1H 2.X1L 3.Y1L ...\n"
	         "MC: 0.TOUCH 1.ID+X1H  2.X1L 3.ST+Y1H 4.Y1L ...\n" */
	//TS_DBG("buf[0-8]:  %x %x %x %x %x %x %x %x %x \n",
	//	buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);

	touch_type = buf[BYD_TS_MT_TOUCH_REG - BYD_TS_REGISTER_BASE]
			& BYD_TS_MT_TOUCH_MASK; // buf[0x5c - 0x5c] & 0xF0

	if(touch_type != BYD_TS_MT_TOUCH_FLAG && touch_type != BYD_TS_MT_KEY_FLAG) {
///////////////// parameter download protocol /////////////////////////////////
#if defined(CONFIG_SUPPORT_PARAMETER_UPG) || defined(CONFIG_SUPPORT_PARAMETER_FILE)
		//if(touch_type != 0 && touch_type != 0xf0) {
		if(touch_type == 0x50  // 0x55 is protocol for parameter download
			&& (buf[BYD_TS_MT_TOUCH_REG - BYD_TS_REGISTER_BASE + 1]
			& BYD_TS_MT_TOUCH_MASK) == 0x50  // touch_id
			&& (buf[BYD_TS_MT_TOUCH_REG - BYD_TS_REGISTER_BASE + 3]
			& BYD_TS_MT_TOUCH_MASK) == 0x50) {// touch_state
			printk("%s: parameters lost! download again...", __FUNCTION__);
			printk("(register #0-#3: %x, %x, %x, %x)\n", buf[0], buf[1], buf[2], buf[3]);
			//disable_irq(ts->client->irq);
			byd_ts_download(ts->client, -1); // -1 unknown module number
			goto out;
		}
#endif
/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
		pr_err("%s: data error in registers! register #0-#3: %x, %x, %x, %x\n",
			__FUNCTION__, buf[0], buf[1], buf[2], buf[3]);
		goto out;
	}

	// after ORDER_SMOOTH_ALGORITHM, total touchs may be changed, reload touch_numb
	touch_numb = buf[BYD_TS_MT_TOUCH_REG - BYD_TS_REGISTER_BASE]
			& BYD_TS_MT_NPOINTS_MASK;
	if (touch_numb == 0) {
		if (Touch_key_down) {
				TS_DBG(" key %d up\n", Touch_key_down);
				input_report_key(ts->input, Byd_Ts_Keys[Touch_key_down - 1], 0);
		}
		Touch_key_down = 0;

	} // end of if touch_numb==0

	if(touch_type == BYD_TS_MT_TOUCH_FLAG) {
	  TS_DBG("#P=%d", touch_numb);
	  if (touch_numb == 0) {
#if (defined(CONFIG_ANDROID_4_ICS) && !defined(CONFIG_REPORT_PROTOCOL_B)) \
    || defined(CONFIG_REPORT_PROTOCOL_A)
		TS_DBG(" (ALL UP)");
		input_mt_sync(ts->input);
		input_sync(ts->input);
#elif defined(CONFIG_REMOVE_POINT_STAY)	&& defined(CONFIG_REPORT_PROTOCOL_B)
		TS_DBG(" ALL UP (CONFIG_REMOVE_POINT_STAY enabled)\n");
		for (i = 0; i < BYD_IC_POINTS_MAX; i++) {
			input_mt_slot(ts->input, i + 1);
			input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
			//input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
		}
		input_sync(ts->input);
#endif

		TS_DBG("\n*** Warning: The TP driver is working in DEBUG mode. Please switch off TS_DEBUG_MSG in driver's .h file ***\n");
#ifdef CONFIG_DRIVER_BASIC_FUNCTION
		TS_DBG("*** Warning: The TP driver is basic version. Please switch off CONFIG_DRIVER_BASIC_FUNCTION in driver's .h file ***\n");
#endif
		goto out;
	  } // end of if touch_numb==0

		for (i = 0; i < touch_numb && i < BYD_TS_MT_NUMBER_POINTS; i++) { // i <= touch_numb &&
			uint8_t finger_size;

			uint8_t touch_state;
			id = (buf[i * 4 + BYD_TS_MT_POINT_REG - BYD_TS_REGISTER_BASE] & 0xf0) >> 4;
			touch_state = buf[i * 4 + BYD_TS_MT_POINT_REG - BYD_TS_REGISTER_BASE + 2] & 0xf0;
			finger_size = (touch_state == 0xC0 /* || touch_numb == 0 */) ? 0 : BYD_TS_TOUCHSIZE; // 0xC0 represents touch up
			x = ( ((buf[i * 4 + BYD_TS_MT_POINT_REG - BYD_TS_REGISTER_BASE] & 0x0f) << 8)
				   | buf[i * 4 + BYD_TS_MT_POINT_REG - BYD_TS_REGISTER_BASE + 1] );
			y = ( ((buf[i * 4 + BYD_TS_MT_POINT_REG - BYD_TS_REGISTER_BASE + 2] & 0x0f) << 8)
				   | buf[i * 4 + BYD_TS_MT_POINT_REG - BYD_TS_REGISTER_BASE + 3] );
			// dealing with error from IC
			if (id <= 0 || id > BYD_IC_POINTS_MAX || (x == 0 && y == 0)) {
				pr_err(" %d[%u,%u]%d err", i+1, x, y, id);
				continue;
			}

			//TS_DBG(" %d[%u,%u]", i+1, x, y);
#if defined(CONFIG_ANDROID_4_ICS) || defined(CONFIG_REPORT_PROTOCOL_A)
			//if (i >= touch_numb) break;
			if (finger_size <= 0) {
	#ifdef CONFIG_REPORT_PROTOCOL_B
				TS_DBG(" %d(up)%d", i + 1, id);
				input_mt_slot(ts->input, id);
				// 20140915, qiximing, use "tracking id" instead of "slot state" (refer to gsl protocol B):
				input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
				//input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
	#else  // 20140909, qiximing, add "else" (refer to gsl protocol A):
				TS_DBG(" %d(up%d)", i + 1, id);
				input_mt_sync(ts->input);
	#endif
				continue;
			}
#else // for Android 2.3
			//if (i >= touch_numb) finger_size = 0;
#endif

#ifdef REPORT_XY_SWAPPED
			int tmp = y;
			y = x;
			x = tmp;
		#if defined(REPORT_X_RES) && defined(REPORT_Y_RES)
			// the precision of all variables must be greater than 3 bytes here
			x = (x * (XY_res_out[0] - 1)) / (XY_res_ts[1] - 1);
			y = (y * (XY_res_out[1] - 1)) / (XY_res_ts[0] - 1);
		#endif
#else
		#if defined(REPORT_X_RES) && defined(REPORT_Y_RES)
			x = (x * (XY_res_out[0] - 1)) / (XY_res_ts[0] - 1);
			y = (y * (XY_res_out[1] - 1)) / (XY_res_ts[1] - 1);
		#endif
#endif
#ifdef REPORT_X_REVERSED
			x = (XY_res_out[0] - 1) - x;
#endif
#ifdef REPORT_Y_REVERSED
			y = (XY_res_out[1] - 1) - y;
#endif
			// dealing with error from IC
			if (x < 0 || y < 0 || x >= XY_res_out[0] || y >= XY_res_out[1]) {
				TS_DBG(" %d[%u,%u] err", i+1, x, y);
				continue;
			}
			TS_DBG(" %d(%u,%u)", i + 1, x, y);
#ifdef CONFIG_ANDROID_4_ICS
	#ifdef CONFIG_REPORT_PROTOCOL_B
			TS_DBG("%d", id);
			input_mt_slot(ts->input, id);
			// 20140915, qiximing, use "tracking id" instead of "slot state" (refer to gsl protocol B):
			input_report_abs(ts->input, ABS_MT_TRACKING_ID, id);
			//input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
	#else // 20140909, qiximing, add "else" (refer to gsl protocol A):
			TS_DBG("%d", id);
			input_report_abs(ts->input, ABS_MT_TRACKING_ID, id);
	#endif
#elif !defined(CONFIG_ANDROID_4_ICS) && !defined(CONFIG_REPORT_PROTOCOL_A)
			input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, finger_size);
		#if defined(TS_DEBUG_MSG)
			if (finger_size <= 0) TS_DBG("up");
		#endif
#endif
			input_report_abs(ts->input, ABS_MT_POSITION_X, x);
			input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
#ifndef CONFIG_REPORT_PROTOCOL_B
			input_mt_sync(ts->input);
#endif
		} // *********** end of for touch_numb ***********

		TS_DBG(" end\n");
	} else if(touch_type == BYD_TS_MT_KEY_FLAG) { // key tapped

		if (sizeof(Byd_Ts_Keys) > 0) {
			if(buf[BYD_TS_MT_POINT_REG - BYD_TS_REGISTER_BASE] == touch_numb * 2 - 1) {
				Touch_key_down = touch_numb;
			}
			if (Touch_key_down) {
				TS_DBG("Key %d down,", touch_numb);
				input_report_key(ts->input, Byd_Ts_Keys[touch_numb - 1], 1);
			}
#ifndef CONFIG_REPORT_PROTOCOL_B
			input_mt_sync(ts->input);
#endif
		} else {
			pr_err("%s: the key 0x%x is not defined!\n", __FUNCTION__,
			  buf[BYD_TS_MT_TOUCH_REG - BYD_TS_REGISTER_BASE]);
			goto out;
		}
	}

	input_sync(ts->input);

out:
	enable_irq(ts->client->irq);
	Irq_Enabled = 1;
}

/*******************************************************************************
* Function    : Linux callback function
*******************************************************************************/
static irqreturn_t byd_ts_irq_handler(int irq, void *dev_id)
{
	struct byd_ts_t *ts = (struct byd_ts_t *)dev_id;
	//TS_DBG("@@@@ %s entered, irq=%d, @@@@\n", __FUNCTION__, irq);
	if (!Irq_Enabled) {
		TS_DBG("@@@@ %s: Warning: IRQ already disabled! You may need 'return IRQ_HANDLED;' @@@@\n", __FUNCTION__);
		//return IRQ_HANDLED;
	}
	Irq_Enabled = 0;
	disable_irq_nosync(ts->client->irq); // should NOT use disable_irq()
	if (!work_pending(&ts->ts_work)) {
		queue_work(ts->ts_work_queue,&ts->ts_work);
	} else { // qi ximing: this may cause dada lost??
		printk("*** BYD TS QUEUE: touch data pending - queue is full! ***\n");
	}

	return IRQ_HANDLED;
}



/*******************************************************************************
* Function    :  byd_ts_platform_init
* Description :  platform specfic configration.
*                This function is usually used for interrupt pin or I2C pin
                 configration during initialization of TP driver.
* Parameters  :  client, - i2c client or NULL if not appropriate
* Return      :  Optional IRQ number or status if <= 0
*******************************************************************************/
static int byd_ts_platform_init(struct i2c_client *client) {
    int irq = 0;

	printk("%s: entered \n", __FUNCTION__);

	if(gpio_request(WAKE_PORT, "TP_SHUTDOWN") != 0) {
		printk("%s:WAKE_PORT gpio_request() error\n", __FUNCTION__);
		return -EIO;
	}
	gpio_direction_output(WAKE_PORT, 1);
	//gpio_set_value(WAKE_PORT, 1); // GPIO_HIGH
	msleep(20);

    /* config port for IRQ */
	if(gpio_request(GPIO_TS_EINT, NULL) != 0) {
		printk("%s:GPIO_TS_EINT gpio_request() error\n", __FUNCTION__);
		return -EIO;
	}
    gpio_direction_input(GPIO_TS_EINT);     // IO configer as input port
	//s3c_gpio_setpull(GPIO_TS_EINT, S3C_GPIO_PULL_UP); //gpio_pull_updown(GPIO_TS_EINT, 1);
	msleep(20);

#ifdef EINT_NUM
    /* (Re)define IRQ if not given or value of "client->irq" is not correct */
    irq = EINT_NUM;
    /* if the IRQ number is already given in the "i2c_board_info" at
     platform end, this will cause it being overrided. */
    if (client != NULL) {
        client->irq = irq;
    }
#endif

    /** warning: irq_set_irq_type() call may cause an unexpected interrupt **/
    //irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);   // IRQ_TYPE_LEVEL_LOW

    /* Returned IRQ is used only for adding device to the bus by the driver.
       otherwise, the irq would not necessarilly to be returned here (or,
       a status zero can be returned instead) */
    return irq;
}


/*******************************************************************************
* Function    : byd_ts_register_input_device
* Description : register tp chip as a input device of Linux
* In          : client, i2c client
                input_dev, struct of Linux input device
* Return      : status
*******************************************************************************/
static int byd_ts_register_input_device(struct i2c_client *client, struct input_dev *input_dev)
{
	int i, err;

//////////////////// read screen parameters from chip /////////////////////////
#if defined(CONFIG_SUPPORT_RESOLUTION_ACQUISITION)
	int xy_res_out[2];
	err = byd_ts_get_resolution(client, xy_res_out);
	if (xy_res_out[0] < 1 || xy_res_out[1] < 1
	 || xy_res_out[0] > 2048 || xy_res_out[1] > 2048) {
		pr_err("%s: error resolution range: %d x %d !\n", __FUNCTION__,
			xy_res_out[0], xy_res_out[1]);
		err = -1;
	}

	if (err < 0) {
		pr_err("%s: CONFIG_SUPPORT_RESOLUTION_ACQUISITION may not supported, "
				"default to %d x %d.\n", __FUNCTION__, TS_X_RES, TS_Y_RES);
	} else {
		TS_DBG("%s: resolution get from TP: %d x %d (ret=%d)\n", __FUNCTION__,
			xy_res_out[0], xy_res_out[1], err);
	#if defined(REPORT_X_RES) && defined(REPORT_Y_RES)
		XY_res_ts[0] = xy_res_out[0];   // tp res = tp res
		XY_res_ts[1] = xy_res_out[1];
	#else
		XY_res_out[0] =  xy_res_out[0]; // report res = tp res
		XY_res_out[1] =  xy_res_out[1];
	#endif
	}
#endif
/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#ifdef REPORT_XY_SWAPPED
	i = XY_res_out[1];
	XY_res_out[1] = XY_res_out[0];
	XY_res_out[0] = i;
#endif
	printk("%s: resolution reported: %d x %d\n", __FUNCTION__,
			XY_res_out[0], XY_res_out[1]);

	input_dev->name = BYD_TS_NAME;
	//input_dev->phys  = BYD_TS_DEVICE;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = BYD_TS_DRIVER_VERSION_CODE;

	//set_bit(EV_SYN, input_dev->evbit); // can be omitted?
	set_bit(EV_KEY, input_dev->evbit); // for button only?
	set_bit(EV_ABS, input_dev->evbit); // must
#ifdef CONFIG_ANDROID_4_ICS
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit); // tells the device is a touchscreen
	#ifdef CONFIG_REPORT_PROTOCOL_B
		//input_mt_init_slots(input_dev, BYD_TS_MT_NUMBER_POINTS + 1); // 16
		input_mt_init_slots(input_dev, BYD_TS_MT_NUMBER_POINTS + 1, 0); // 16
		// qiximing, 20140905, refer to code for gt868/gt968, following 2 lines replace above line
		//input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, BYD_TS_MT_NUMBER_POINTS, 0, 0); // 16
	    //input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, BYD_TS_TOUCHSIZE, 0, 0);
	#else  // 20140916, qiximing add "else": TRACKING_ID for protocol A
	    //set_bit(BTN_TOUCH, input_dev->keybit);
		set_bit(ABS_MT_TRACKING_ID, input_dev->absbit);
	#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, XY_res_out[0] - 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, XY_res_out[1] - 1, 0, 0);
#else
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, XY_res_out[0], 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, XY_res_out[1], 0, 0);
  #ifndef CONFIG_REPORT_PROTOCOL_A
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, BYD_TS_TOUCHSIZE, 0, 0);
  #endif
#endif

	for (i = 0; i < sizeof(Byd_Ts_Keys); i++) {
		set_bit(Byd_Ts_Keys[i],  input_dev->keybit);
		input_set_capability(input_dev, EV_KEY, Byd_Ts_Keys[i]);
	}

	return input_register_device(input_dev);
}

/*******************************************************************************
* Include     : module file for platform specific settings
* Interface   : int byd_ts_platform_init(void)
*               in:  none
*               out: status or irq number if device is added on the fly
* Description : the function is usually used for interrupt pin or I2C pin
                configration during initialization of TP driver.
*******************************************************************************/
//#include "byd_ts_platform_init.c" // *** it is MOVED INTO byd_bf675x_ts.h ***

/*******************************************************************************
* Function    : Linux callback function
*******************************************************************************/
static int byd_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret, err = 0;
	struct input_dev *input_dev;
	struct byd_ts_t *byd_ts = NULL;

#ifndef BUS_NUM
  #ifndef CONFIG_NON_BYD_DRIVER
	ret = byd_ts_platform_init(client);
	if (ret < 0) err = ret;
  #endif
  #ifdef BYD_TS_SLAVE_ADDRESS
 // (Re)defining I2C address if not given or given in the "client->addr" is not correct
	client->addr = BYD_TS_SLAVE_ADDRESS; // |= I2C_ENEXT_FLAG; // |= I2C_HS_FLAG;
  #endif
#endif

	TS_DBG("%s: start. found device: name:%s, deviceId:%s, IRQ:%d, addr:0x%x\n",
		   __FUNCTION__, client->name, id->name, client->irq, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}

////////////////////// firmware downloading ///////////////////////////////////
//  chip code downloading (available only if rootfs is started at this moment)
	ret = byd_ts_download(client, 0); //
	if (ret < 0 && ret != -ENOENT) // ret != no module number
		goto exit_check_functionality_failed;

///////////////////// parameter downloading ///////////////////////////////////
// parameter downloading should be done prior resolution bing read
#if defined(CONFIG_SUPPORT_PARAMETER_UPG) || defined(CONFIG_SUPPORT_PARAMETER_FILE)
	ret = byd_ts_download(client, -1);
	if (ret < 0 && ret != -ENODATA && ret != -ENOENT) err = ret;
#endif

	check_mem_data(client);
/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
//////////////////// read firmware version from chip //////////////////////////
#ifdef CONFIG_SUPPORT_VERSION_ACQUISITION
	{
	unsigned char version[] = {0, 0, 0, 0, 0, 0, 0};
	ret = byd_ts_get_firmware_version(client, version);
	if (ret < 0) err = ret;
	printk("%s: TP firmware version: %02x%02x%x-%02x%02x%02x-%.2x\n", __FUNCTION__, // ##-PROJ.CO.PA
		version[0], version[1], version[2], version[3], version[4], version[5], version[6]);
	}
#endif
/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
#if defined(CONFIG_COOR_ORDER_SMOOTH_ALGORITHM)
	ret = byd_ts_read_IC_para(client);	// cts15 read para for byd_ts_read_coor()
#endif

	byd_ts = kzalloc(sizeof(struct byd_ts_t), GFP_KERNEL);
	if (!byd_ts) {
		pr_err("%s: request memory failed\n", __FUNCTION__);
		ret= -ENOMEM;
		goto exit_request_memory_failed;
	}

	This_Client = client;
	byd_ts->client = client;
	i2c_set_clientdata(client, byd_ts);

	// input device
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: input allocate device failed\n", __FUNCTION__);
		ret = -ENOMEM;
		goto exit_input_dev_allocate_failed;
	}

	ret = byd_ts_register_input_device(client, input_dev);
	if (ret < 0) {
		pr_err("%s: input device regist failed\n", __FUNCTION__);
		goto exit_input_register_failed;
	}

	byd_ts->input = input_dev;

	//create work queue
	INIT_WORK(&byd_ts->ts_work, byd_ts_work);
	byd_ts->ts_work_queue= create_singlethread_workqueue("byd_ts");

#ifdef CONFIG_HAS_EARLYSUSPEND
	byd_ts->ts_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	byd_ts->ts_early_suspend.suspend = byd_ts_early_suspend;
	byd_ts->ts_early_suspend.resume = byd_ts_late_resume;
	register_early_suspend(&byd_ts->ts_early_suspend);
#endif

//////////////////// raw data can be passed up for deguging ///////////////////
#if defined(CONFIG_SUPPORT_RAW_DATA_ACQUISITION) \
 || defined(CONFIG_SUPPORT_PARAMETER_FILE)
	ret = misc_register(&byd_ts_device);
	if (ret) {
		pr_err("%s: misc device register (for tools) failed! \n", __FUNCTION__);
		err = ret; //		goto irq_request_err;
	}
#endif
/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

	//register irq
	if(client->irq > 0) {
		ret =  request_irq(client->irq, byd_ts_irq_handler, EINT_TRIGGER_TYPE, client->name, byd_ts); // | IRQF_SHARED
		if (ret < 0) {
			pr_err("%s: IRQ setup failed! errno: %d\n", __FUNCTION__, ret);
			goto irq_request_err;
		}
	} else {
		pr_err("%s: IRQ setup failed! client-irq = %d\n", __FUNCTION__, client->irq);
		goto irq_request_err;
	}
	//disable_irq(client->irq);

#ifdef CONFIG_BEAT_TIMER
	Beat_Interval = read_beat_para(byd_ts->client);
	This_Hrtimer = &byd_ts->timer;	//Initial This_Hrtimer

	if (Beat_Interval > 0) {
		hrtimer_init(&byd_ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL); // Initial the struc hrtimer
		byd_ts->timer.function = byd_ts_timer_func;
		hrtimer_start(&byd_ts->timer, ktime_set(Beat_Interval + 5, 0), HRTIMER_MODE_REL);
	#ifdef BYD_MONITOR
		TS_DBG("%s: Init work, byd_monitor_workqueue\n", __FUNCTION__);
		INIT_DELAYED_WORK(&(byd_ts->byd_monitor_work), byd_monitor_worker);
		byd_ts->byd_monitor_workqueue = create_singlethread_workqueue("byd_monitor_workqueue");
	#endif
	}
#endif

	if (err >= 0) {
		printk("%s: *** Probe Success! ***\n%s\n", __FUNCTION__,
			   BYD_TS_DRIVER_DESCRIPTION);
	} else {
		printk("%s: *** Probe finished with some errors! ***\n%s\n", __FUNCTION__,
			   BYD_TS_DRIVER_DESCRIPTION);
	}

	//enable_irq(client->irq);
	return 0;

irq_request_err:
	destroy_workqueue(byd_ts->ts_work_queue);

#ifdef CONFIG_BEAT_TIMER
	TS_DBG("%s: hrtimer_cancel\n", __FUNCTION__);
	heartbeat_cancel();
#endif

exit_input_register_failed:
	input_free_device(input_dev);
exit_input_dev_allocate_failed:
	kfree(byd_ts);
exit_request_memory_failed:
exit_check_functionality_failed:
	printk("%s: Probe Fail!\n",__FUNCTION__);
	return ret;
}


#ifdef BUS_NUM

/*******************************************************************************
* Function    :  byd_ts_i2c_attach_device
* Description :  add a i2c device to i2c bus (instantiate the i2c device)
* In          :  bus_num, the number of the I2C bus segment, on which the device
                 is attached, on the target board
                 irq, interrupt request number
* Return      :  status
*******************************************************************************/
int byd_ts_i2c_attach_device(int bus_num, int irq)
{
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int err;
	struct i2c_board_info board_info = {
		I2C_BOARD_INFO(BYD_TS_NAME, BYD_TS_SLAVE_ADDRESS),   /*** slave addr, 0x0a/0x14 BF6852, 0x2c/0x58 BF6711a, 0x34 BF6712b ***/
		.irq = irq
	};

	TS_DBG("%s: %s, i2c_bus_num:%d, IRQ:%d, addr:0x%x\n",
		     __FUNCTION__, board_info.type, bus_num, irq, board_info.addr);

	//i2c_register_board_info(bus_num, &board_info, 1);

	adapter = i2c_get_adapter(bus_num);
	if (!adapter) {
		printk("%s: can't get i2c adapter, bus number: %d\n", __FUNCTION__, bus_num);
		err = -ENODEV;
		goto err_driver;
	}

	client = i2c_new_device(adapter, &board_info);
	if (!client) {
		printk("%s:  can't add i2c device at 0x%x\n",
			__FUNCTION__, (unsigned int)board_info.addr);
		err = -ENODEV;
		goto err_driver;
	}

	i2c_put_adapter(adapter);

	return 0;

err_driver:
	return err;
}
#endif // ifdef BUS_NUM


/*******************************************************************************
* Function    : Linux callback function
*******************************************************************************/
static int byd_ts_remove(struct i2c_client *client)
{
	struct byd_ts_t *byd_ts = i2c_get_clientdata(client);

	TS_DBG("%s\n", __FUNCTION__);

	//remove queue
	flush_workqueue(byd_ts->ts_work_queue); // cancel_work_sync(&byd_ts->work);
	destroy_workqueue(byd_ts->ts_work_queue);
	i2c_set_clientdata(client, NULL);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&byd_ts->ts_early_suspend);
#endif
	input_unregister_device(byd_ts->input);
	input_free_device(byd_ts->input);
	free_irq(client->irq, byd_ts);

#ifdef CONFIG_BEAT_TIMER
	heartbeat_cancel();
#endif
#ifdef BYD_MONITOR
	if (Beat_Interval > 0) {
		cancel_delayed_work_sync(&byd_ts->byd_monitor_work);
		destroy_workqueue(byd_ts->byd_monitor_workqueue);
	}
#endif
//////////////////// raw data functionality removing //////////////////////////
#if defined(CONFIG_SUPPORT_RAW_DATA_ACQUISITION) \
 || defined(CONFIG_SUPPORT_PARAMETER_FILE)
	misc_deregister(&byd_ts_device);
#endif
/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
	kfree(byd_ts);

	return 0;
}

/*******************************************************************************
* Global Variable    : TS driver object
*******************************************************************************/
static const struct i2c_device_id byd_ts_id[] = { // devices supported by this driver
	{ BYD_TS_NAME, 0 }, // i2c_divice_id->name is same as i2c_client->name, must match i2c_board_info->type
	{ }
};

#define BYD_TS_DRIVER_NAME BYD_TS_NAME
static struct i2c_driver byd_ts_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = BYD_TS_DRIVER_NAME, // driver name can be found under /sys/bus/i2c/drivers/
	},

	.probe      = byd_ts_probe,
	.remove     = byd_ts_remove,
	.id_table = byd_ts_id, // one driver can support many devices (clients)
};

/*******************************************************************************
* Function    : Linux callback function
*******************************************************************************/
static int __init byd_ts_init(void)
{
	int ret = -1;

	printk("********************************\n");
	printk("* init BYD touch screen driver  \n");
	printk("*      %s (CTS15)\n", BYD_TS_NAME);
	printk("********************************\n");

#ifdef BUS_NUM
	ret = byd_ts_platform_init(NULL);
	if (ret <= 0) { // ret is irq here
		pr_err("%s: can't add dirver %s, irq <= 0\n", __FUNCTION__, BYD_TS_NAME);
		return -ENODEV;
	}
	ret = byd_ts_i2c_attach_device(BUS_NUM, ret);
	if (ret != 0) {
		//pr_err("%s: can't attach i2c device to bus %d\n", __FUNCTION__, BUS_NUM);
		return -ENODEV;
	}
#endif

	ret = i2c_add_driver(&byd_ts_driver);
	if (ret != 0) {
		pr_err("%s: can't add i2c driver\n", __FUNCTION__);
		return -ENODEV;
	}

	return 0;
}

/*******************************************************************************
* Function    : Linux callback function
*******************************************************************************/
static void __exit byd_ts_exit(void)
{
	TS_DBG("%s: addr=0x%x; i2c_name=%s",__FUNCTION__, This_Client->addr, This_Client->name);
#ifdef BUS_NUM
	i2c_unregister_device(This_Client);
#endif
	i2c_del_driver(&byd_ts_driver);
}

module_init(byd_ts_init);
module_exit(byd_ts_exit);

MODULE_AUTHOR("Simon Chee <qi.ximing@byd.com>");
MODULE_DESCRIPTION(BYD_TS_DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");
 