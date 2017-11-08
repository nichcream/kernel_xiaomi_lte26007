/*******************************************************************************
 * Source file : realtek_fm_ctl.c
 * Description : REALTEK_FM Receiver driver for linux.
 * Date        : 05/11/2011
 * 
 * Copyright (C) 2011 Spreadtum Inc.
 *
 ********************************************************************************
 * Revison
 2011-05-11  aijun.sun   initial version 
 *******************************************************************************/
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <plat/mfp.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
//#include <linux/regulator/consumer.h>
//#include <mach/regulator.h>


#include "realtek_fm_ctrl.h"
#include "rtl8723b_fm.h"
#include "plat/rtk-fm.h"

#ifdef CONFIG_DEVINFO_COMIP
#include <plat/comip_devinfo.h>
#endif

//#define FM_SHOW_SNR_DEBUG 1

//#define REALTEK_FM_DEBUG 1
//#define DEBUG_DUMP_REGISTER

#ifdef REALTEK_FM_DEBUG
#define dbg_info(args...) printk(args)
#else
#define dbg_info(args...)
#endif

#define GPIO_FM_EN MFP_PIN_GPIO(143)

#define REALTEK_FM_WAKEUP_CHECK_TIME         100   // ms 
#define REALTEK_FM_WAKEUP_TIMEOUT            800   

#define REALTEK_FM_SEEK_CHECK_TIME           10 // ms

#define REALTEK_FM_TUNE_DELAY                50    // ms 

#define I2C_RETRY_DELAY                   5
#define I2C_RETRIES                       3

#define REALTEK_FM_DEV_NAME	"REALTEK_FM"
#define REALTEK_FM_I2C_NAME    REALTEK_FM_DEV_NAME
#define REALTEK_FM_I2C_ADDR    0x12


#define FM_START_FREQUENCY 87000000
#define FM_END_FREQUENCY 108000000

#ifndef GPIO_FM_EN
#define GPIO_FM_EN -1
#endif 

#ifdef FM_VOLUME_ON
/*gordon for full search timeout begin*/
//static void fm_volume_on_worker(struct work_struct *private_);
//static DECLARE_DELAYED_WORK(fm_volume_on, fm_volume_on_worker);
/*gordon for full search timeout end*/
#endif

static void fm_show_snr_worker(struct work_struct *private_);
static DECLARE_DELAYED_WORK(fm_show_snr, fm_show_snr_worker);

extern int sprd_3rdparty_gpio_fm_enable;

struct mutex		mutex;


struct realtek_fm_drv_data {
	struct i2c_client *client;
	struct class      fm_class;
	int               opened_before_suspend;
	int				bg_play_enable; // enable/disable background play.

	atomic_t          fm_opened;
	atomic_t          fm_searching;
	int               current_freq;
	int			last_seek_freq;
	int               current_volume;
 	u8                muteOn;

	FM_MODULE *pFm;
	FM_MODULE FmModuleMemory;

	// Base interface module
	BASE_INTERFACE_MODULE *pBaseInterface;
	BASE_INTERFACE_MODULE BaseInterfaceModuleMemory;	
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend; 
#endif
};


struct realtek_fm_drv_data *realtek_fm_dev_data = NULL;


static int realtek_fm_set_volume(struct realtek_fm_drv_data *cxt, u8 volume);



struct realtek_seek_info{

	int FrequencyHz;
	long SnrDbNum;
	long SnrDbDen;
	ulong Jif;
	struct realtek_seek_info *next;
};

static struct realtek_seek_info *pre_seek_info = NULL;
static struct realtek_seek_info *seek_info1 = NULL;
static struct realtek_seek_info *seek_info2 = NULL;
static struct realtek_seek_info *seek_info = NULL;

int seek_freq[100] = {0};

static int 
custom_i2c_read(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	unsigned char *pReadingBytes,
	unsigned long ByteNum
	)
{
	struct realtek_fm_drv_data *cxt;
	int err;
	int tries;
	int kk;

	struct i2c_msg msgs[] = {
		{
			.addr = DeviceAddr,
			.flags =  I2C_M_RD,
			.len = ByteNum,
			.buf = pReadingBytes,
		},
	};


	err = 0;
	tries = 0;
	kk = 0;

	pBaseInterface->GetUserDefinedDataPointer(pBaseInterface, (void **)&cxt);	

	do {
		err = i2c_transfer(cxt->client->adapter, msgs, 1);
		if (err != 1)
			{
				msleep_interruptible(I2C_RETRY_DELAY);
				dev_err(&cxt->client->dev, "custom_i2c_read : %d Read transfer error\n", tries);
				dbg_info( "i2c_read err msg: DeviceAddr = %x\n",DeviceAddr);
				dbg_info( "i2c_read err msg: ByteNum = %d\n",ByteNum);
				for(; kk < ByteNum; kk++)
					dbg_info( "i2c_read err msg: pReadingBytes = %x\n",*((unsigned char*)pReadingBytes + kk));
			}
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&cxt->client->dev, "custom_i2c_read : Read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}



static int
custom_i2c_write(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	const unsigned char *pWritingBytes,
	unsigned long ByteNum
	)
{
	struct realtek_fm_drv_data *cxt;
	int err;
	int tries;
	int kk;
	
	struct i2c_msg msgs[] = { 
		{ 
			.addr = DeviceAddr,
	 		.flags = 0,
			.len = ByteNum, 
			.buf = (unsigned char*)pWritingBytes, 
		},
	};
		

	err = 0;
	tries = 0;
	kk = 0;

	pBaseInterface->GetUserDefinedDataPointer(pBaseInterface, (void **)&cxt);

	do {
		err = i2c_transfer(cxt->client->adapter, msgs, 1);
		if (err != 1)
		{
			msleep_interruptible(I2C_RETRY_DELAY);
			dev_err(&cxt->client->dev, "custom_i2c_write : %d write transfer error\n", tries);
			dbg_info( "i2c_write err msg: DeviceAddr = %x\n",DeviceAddr);
			dbg_info( "i2c_write err msg: ByteNum = %d\n",ByteNum);
			for(; kk < ByteNum; kk++)
				dbg_info( "i2c_write err msg: pWritingBytes = %x\n",*((unsigned char*)pWritingBytes + kk));
		}
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&cxt->client->dev, "custom_i2c_write : write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}



void
custom_wait_ms(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned long nMinDelayTime
	)
{
	unsigned long usec;
	
	do {
		usec = (nMinDelayTime > 8000) ? 8000 : nMinDelayTime;
		msleep(usec);
		nMinDelayTime -= usec;
		} while (nMinDelayTime > 0);

	return;

}



static void realtek_fm_gpio_en(struct realtek_fm_drv_data *cxt,bool turn_on)
{
	struct i2c_client *client = cxt->client;

	struct rtk_fm_platform_data *pdata = client->dev.platform_data;

	comip_mfp_config(pdata->fm_en, MFP_PIN_MODE_GPIO);

	if(turn_on){
		if(pdata->fm_en>0){
			gpio_direction_output(pdata->fm_en, 1);
			mdelay(100);
		}
	}else{
		if(pdata->fm_en>0){
			gpio_direction_output(pdata->fm_en, 0);
			mdelay(150);
		}
	}
}



//NOTES:
//This function is private call by 'probe function', so not add re-entry
//protect.
static int
realtek_fm_power_on(
	struct realtek_fm_drv_data *cxt
	)
{
	unsigned long data;
	unsigned int times;
	
	dbg_info( "+FM power on\n");

	/* turn on fm enable*/
	realtek_fm_gpio_en(cxt,true);

	//I2C_write_bitsB(addr_fm, hex2dec('010d'), 0, 0, 0); % en_sleep LDO12H, 0: disable, 1: enable
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x010d, 0, 0, 0) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	msleep(1);
	//% I2C_write_bitsB(addr_fm, hex2dec('0101'), 1, 1, 1); %% rfafe_en, 0: turn off LDOD, 1: turn on LDOD
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 1, 1, 1) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	msleep(1);
	//% I2C_write_bitsB(addr_fm, hex2dec('0101'), 2, 2, 1); %% ldoa_en, 0: turn off LDOA, 1: turn on LDOA
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 2, 2, 1) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	msleep(1);
	//% I2C_write_bitsB(addr_fm, hex2dec('0101'), 3, 3, 0); %% RFC_ISO_ON, 0: disable, 1: enable
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 3, 3, 0) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	msleep(1);
	//% I2C_write_bitsB(addr_fm, hex2dec('0101'), 4, 4, 0); %% ISO_ON, 0: disable, 1: enable
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 4, 4, 0) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	msleep(1);
	//% I2C_write_bitsB(addr_fm, hex2dec('0101'), 5, 5, 1); %% RFC software rstn 
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 5, 5, 1) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	msleep(10);	
	
	//rfc_write(hex2dec('0'), hex2dec('a000'));
	if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0, 0xa000) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	msleep(10);	

	//rfc_write(hex2dec('27'), hex2dec('9800'));
	if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x27, 0x9800) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x04, 0x1354) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	msleep(10);	

	times = 0;
	do{
		times++;
		msleep(5);
		if(cxt->pFm->GetRegMaskBits(cxt->pFm, 0x0120, 7, 0, &data) != FUNCTION_SUCCESS)
			goto error_status_set_registers;
	}while( (data!=0xff) && (times<200) );

	if(data!=0xff)
	{
		dbg_info( "FM power on : timeout err BB Reg.0x0120 = 0x%x\n", data);
		goto error_status_set_registers;
	}
	
	//I2C_write_bitsB(addr_fm, hex2dec('0101'), 0, 0, 1); % i2c_clk_sel, 0: ring 12M, 1: gated_40M
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 0, 0, 1) != FUNCTION_SUCCESS)
		goto error_status_set_registers;

	msleep(10);

	//I2C_write_bitsB(addr_fm, hex2dec('010d'), 3, 3, 0); % ck_12m_en, 0: disable, 1: enable
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x010d, 3, 3, 0) != FUNCTION_SUCCESS)
		goto error_status_set_registers;
	
	msleep(10);	

	dbg_info( "-FM power on\n");


	return FM_FUNCTION_SUCCESS;

error_status_set_registers:

	dbg_info( "-FM power on : err\n");


	return FM_FUNCTION_ERROR;

}



static int
realtek_fm_close(
	struct realtek_fm_drv_data *cxt
	)
{
#ifdef DEBUG_DUMP_REGISTER
	unsigned long Value;
	int i;
#endif
	if (!atomic_read(&cxt->fm_opened)) {
		dev_err(&cxt->client->dev,
			"FM close: already closed, ignore this operation\n");
		return FM_FUNCTION_ERROR;
	}
	atomic_cmpxchg(&cxt->fm_opened, 1, 0);

	dbg_info( "+FM close\n");
#if 0
	//%% turn on 12M Ring osc.
	//I2C_write_bitsB(addr_fm, hex2dec('010d'), 3, 3, 1); % ck_12m_en, 0: disable, 1: enable
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x010d, 3, 3, 1) != FUNCTION_SUCCESS)
		goto error_status_set_registers;	

	//%% set CLK to 12MHz.
	//I2C_write_bitsB(addr_fm, hex2dec('0101'), 0, 0, 0); % i2c_clk_sel, 0: ring 12M, 1: gated_40M
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 0, 0, 0) != FUNCTION_SUCCESS)
		goto error_status_set_registers;
	
	//%%%% set FM into sleep mode %%%%
	//I2C_write_bitsB(addr_fm, hex2dec('0101'), 5, 5, 0); %% RFC software rstn 
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 5, 5, 0) != FUNCTION_SUCCESS)
		goto error_status_set_registers;	

	
	msleep(100);

	//I2C_write_bitsB(addr_fm, hex2dec('0101'), 3, 3, 1); %% RFC_ISO_ON, 0: disable, 1: enable
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 3, 3, 1) != FUNCTION_SUCCESS)
		goto error_status_set_registers;	

	msleep(100);

	//I2C_write_bitsB(addr_fm, hex2dec('0101'), 2, 2, 0); %% ldoa_en, 0: turn off LDOA, 1: turn on LDOA
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 2, 2, 0) != FUNCTION_SUCCESS)
		goto error_status_set_registers;	

	msleep(100);
	
	//I2C_write_bitsB(addr_fm, hex2dec('0101'), 1, 1, 0); %% rfafe_en, 0: turn off LDOD, 1: turn on LDOD
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0101, 1, 1, 0) != FUNCTION_SUCCESS)
		goto error_status_set_registers;	

	msleep(100);

	//%% enable LDO12H sleep mode.
	//I2C_write_bitsB(addr_fm, hex2dec('010d'), 0, 0, 1); % en_sleep LDO12H, 0: disable, 1: enable
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x010d, 0, 0, 1) != FUNCTION_SUCCESS)
		goto error_status_set_registers;	

	//I2C_write_bitsB(addr_fm, hex2dec('0001'), 0, 0, 0); %% disable FM
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0001, 0, 0, 0) != FUNCTION_SUCCESS)
		goto error_status_set_registers;	

	//%% hold BB software rst.
	//I2C_write_bitsB(addr_fm, hex2dec('0002'), 0, 0, 1);
	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0002, 0, 0, 1) != FUNCTION_SUCCESS)
		goto error_status_set_registers;	
	
#endif

#ifdef FM_SHOW_SNR_DEBUG
	cancel_delayed_work(&fm_show_snr);
#endif

#ifdef DEBUG_DUMP_REGISTER

	//BaseBand Addr=0x0001~0x0120
	for (i = 1; i <= 0x0120; i++) {
		cxt->pFm->GetRegMaskBits(cxt->pFm, i, 15, 0, &Value);
		dbg_info( "BB 0x%x 0x%x\n", i , Value);
	}

	cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 0);

	//RFC Addr=0x00~0xFF
	for (i = 0; i <= 0xff; i++) {
		cxt->pFm->GetRfcReg2Bytes(cxt->pFm, i, &Value);
		dbg_info( "RFC 0x%x 0x%x\n", i , Value);
	}

	cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 1);
#endif

	/* turn off fm enable */
	realtek_fm_gpio_en(cxt,false);

	dbg_info( "-FM close\n");

	return FM_FUNCTION_SUCCESS;

}



//
// Notes: Before this call, device must be power on.
//
static int
realtek_fm_open(
	struct realtek_fm_drv_data *cxt
	)
{
	int ret;
	unsigned long chipid;
	unsigned long data;
	unsigned int retry_no = 3;

	ret = -EINVAL;

	if (atomic_read(&cxt->fm_opened)) {
		dev_err(&cxt->client->dev,
			"FM open: already opened, ignore this operation\n");
		return FM_FUNCTION_ERROR;
	}
retry:
	ret = realtek_fm_power_on(cxt);
	if (ret < 0)
		return FM_FUNCTION_ERROR;	

	rtl8723b_fm_Get_Chipid(cxt->pFm, &chipid);

	dev_info(&cxt->client->dev, "Realtek FM driver 14-07-11,FM open Chip ID = 0x%lx\n", chipid);

	if(rtl8723b_fm_Initialize(cxt->pFm))	goto error;

	//+ register dump check
	if(cxt->pFm->GetRfcReg2Bytes(cxt->pFm, 0x00, &data)){
		dev_err(&cxt->client->dev, "-GetRfcReg2Bytes 0x00 Failed!\n");
		goto error;
	}
	if(data != 0xa000)
	{
		dev_err(&cxt->client->dev, "-FM open: err Rfc Reg.0x00=0x%lx\n", data);
		goto error;
	}
	
	if(cxt->pFm->GetRfcReg2Bytes(cxt->pFm, 0x21, &data)){
		dev_err(&cxt->client->dev, "-GetRfcReg2Bytes 0x21 Failed!\n");
		goto error;
	}
	if(data != 0xcd53)
	{
		dev_err(&cxt->client->dev, " -FM open :err Rfc Reg.0x21=0x%lx\n", data);
		goto error;
	}


	if(cxt->pFm->GetRegMaskBits(cxt->pFm, 0x0001, 7, 0, &data)) {
		dev_err(&cxt->client->dev, "-GetRfcReg2Bytes 0x0001 Failed!\n");
		goto error;
	}
	if(data != 0x1)
	{
		dev_err(&cxt->client->dev, " -FM open :err BB Reg.0x0001=0x%lx\n", data);
		goto error;
	}

	if(cxt->pFm->GetRegMaskBits(cxt->pFm, 0x0002, 7, 0, &data)) {
		dev_err(&cxt->client->dev, "-GetRfcReg2Bytes 0x0002 Failed!\n");
		goto error;
	}
	if(data != 0x0)
	{
		dev_err(&cxt->client->dev, " -FM open :err BB Reg.0x0002=0x%lx\n", data);
		goto error;
	}
	//- register dump check

	if(rtl8723b_fm_SetBand(cxt->pFm, FM_BAND_87_108MHz) ){
		dev_err(&cxt->client->dev, "-Set Band Failed!\n");
		goto error;
	}
	if(rtl8723b_fm_Set_Channel_Space( cxt->pFm,FM_CHANNEL_SPACE_100kHz )){
		dev_err(&cxt->client->dev, "-Set Channel Space Failed!\n");
		goto error;
	}
	if(rtl8723b_fm_SetTune_Hz(cxt->pFm, cxt->current_freq*100*1000)!= FM_FUNCTION_SUCCESS){
		dev_err(&cxt->client->dev, "-SetTune Failed!\n");
		goto error;
	}
	cxt->pFm->pBaseInterface->WaitMs(cxt->pFm->pBaseInterface,  100);
	
	atomic_cmpxchg(&cxt->fm_opened, 0, 1);

	cxt->current_volume = 14;

	if(realtek_fm_set_volume(cxt, 0)) {
		dev_err(&cxt->client->dev, "-Set Volume Failed!\n");
		goto error;
	}
	dbg_info( "-FM open: FM is opened\n");

        //rfc_write(hex2dec('23'), hex2dec('5bf6')); %% power on DAC
	if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x23, 0x5bf6)) {
		dev_err(&cxt->client->dev, "-SetRfcReg2Bytes 0x23 Failed!\n");
		goto error;
	}

#ifdef FM_SHOW_SNR_DEBUG
	schedule_delayed_work(&fm_show_snr, HZ/4);
#endif
	return FM_FUNCTION_SUCCESS;

error:

	dev_err(&cxt->client->dev, "-FM open : err\n");
	if(retry_no--){
		dev_err(&cxt->client->dev, "retry_no: %d", retry_no);
		realtek_fm_gpio_en(cxt,false);
		msleep(50);
		goto retry;
	}
	
	return FM_FUNCTION_ERROR;
	
}



//
//Notes:Set FM tune, the frequency 100KHz unit.
//
static int
realtek_fm_set_tune(
	struct realtek_fm_drv_data *cxt,
	u16 frequency
	)
{
	if (!atomic_read(&cxt->fm_opened)) {
		
		dev_err(&cxt->client->dev, "FM set_tune: FM not open\n");
		
		return FM_FUNCTION_ERROR;
	}

	dbg_info( "+FM set_tune %d\n", frequency);
	
	if(rtl8723b_fm_SetTune_Hz(cxt->pFm, frequency*100*1000)!= FM_FUNCTION_SUCCESS)	goto error;

	cxt->current_freq = frequency;

	dbg_info( "-FM set_tune\n");

	return FM_FUNCTION_SUCCESS;
	
error:

	dev_err(&cxt->client->dev, "-FM set tune: err...\n");
	
	return FM_FUNCTION_ERROR;
}



//
// NOTES: Get tune frequency, with 100KHz unit.
 //
static int
realtek_fm_get_frequency(
	struct realtek_fm_drv_data *cxt
	)
{
	u16 frequency;
	int FrequencyHz;
	
	frequency = 0;
	
	if (!atomic_read(&cxt->fm_opened)) {

		dev_err(&cxt->client->dev, "FM get_frequency: FM not open\n");

		return FM_FUNCTION_ERROR;
	}

	dbg_info( "+FM get_frequency\n");
	
	if(rtl8723b_fm_GetFrequencyHz(cxt->pFm, &FrequencyHz)!= FM_FUNCTION_SUCCESS)	goto error;

	frequency = FrequencyHz/100000;

	dbg_info( "-FM get_frequency = %d\n", frequency);

	return frequency;

error:

	dev_err(&cxt->client->dev, "-FM get_frequency : err\n");

	return FM_FUNCTION_ERROR;
	
}



//NOTES:
//Stop fm search and clear search status 
 //
static int
realtek_fm_stop_search(
	struct realtek_fm_drv_data *cxt
	)
{
	dbg_info( "+FM stop_search\n");


	if (atomic_read(&cxt->fm_searching)) {

		atomic_cmpxchg(&cxt->fm_searching, 1, 0);

		if(rtl8723b_fm_Seek( cxt->pFm, FM_SEEK_DISABLE))	goto error;

	}

	dbg_info( "-FM stop_search\n");

	return FM_FUNCTION_SUCCESS;

error:

	dev_err(&cxt->client->dev, "-FM stop_search : err\n");

	return FM_FUNCTION_ERROR;
}


//gordon

/*static void fm_volume_on_worker(struct work_struct *private_)
{
#if 1 
	struct realtek_fm_drv_data *cxt = realtek_fm_dev_data;

	if (!atomic_read(&cxt->fm_opened)) {

		dev_err(&cxt->client->dev, "fm_volume_on_worker: FM not open\n");

		return;
	}

	dbg_info( "+FM volume on\n");

	if(cxt->current_volume != 0)
	{
		if(rtl8723b_fm_SetMute(cxt->pFm, 0))            goto error;
	}

	dbg_info( "-FM volume on success\n");

	return;

error:

	dbg_info( "-FM volume on : err\n");
#endif
}*/


static void fm_show_snr_worker(struct work_struct *private_)
{
	int FrequencyHz1;
	long SnrDbNum1;
	long SnrDbDen1;
	unsigned long RFGain, RXBBGain;
#ifdef DEBUG_DUMP_REGISTER
	unsigned long snr_debug;
	unsigned long Value;
	int i;
#endif
	int rssi;
	
	struct realtek_fm_drv_data *cxt = realtek_fm_dev_data;

	if (!atomic_read(&cxt->fm_opened)) {

		dev_err(&cxt->client->dev, "fm_show_snr_worker: FM not open\n");

		return;
	}

	if(rtl8723b_fm_GetFrequencyHz(cxt->pFm, &FrequencyHz1))		goto error;
	if(rtl8723b_fm_GetSNR(cxt->pFm, &SnrDbNum1, &SnrDbDen1))		goto error;
	if(rtl8723b_fm_GetRSSI(cxt->pFm, &rssi))		goto error;

	if(cxt->pFm->GetReadOnlyRegMaskBits(cxt->pFm, 21, FM_ADDR_DEBUG_0, 2, 0, &RFGain))		goto error;
	if(cxt->pFm->GetReadOnlyRegMaskBits(cxt->pFm, 21, FM_ADDR_DEBUG_0, 7, 3, &RXBBGain))		goto error;

#ifdef DEBUG_DUMP_REGISTER
	snr_debug = SnrDbNum1*10/SnrDbDen1;
	dbg_info( " Show Freq:%d, SNR:%ld, RF Gain:%d, RXBB Gain:%d, rssi=%d\n", FrequencyHz1/100000, snr_debug, RFGain, RXBBGain, rssi);

	if ((snr_debug == 10) && (RFGain == 0) &&(RXBBGain == 18)) {
		//BaseBand Addr=0x0001~0x0120
		for (i = 1; i <= 0x0120; i++) {
			cxt->pFm->GetRegMaskBits(cxt->pFm, i, 15, 0, &Value);
			dbg_info( "BB 0x%x 0x%x\n", i , Value);
		}

		cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 0);

		//RFC Addr=0x00~0xFF
		for (i = 0; i <= 0xff; i++) {
			cxt->pFm->GetRfcReg2Bytes(cxt->pFm, i, &Value);
			dbg_info( "RFC 0x%x 0x%x\n", i , Value);
		}

		cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 1);
	}
#else
	dbg_info( " Show Freq:%d, SNR:%ld, RF Gain:%d, RXBB Gain:%d, rssi=%d\n", FrequencyHz1/100000, SnrDbNum1*10/SnrDbDen1, RFGain, RXBBGain, rssi);
#endif
	if(FrequencyHz1 == 60*1000*1000)
	{
		dbg_info( " ERR : Freq = 60MHz, #### FM RE-OPEN ####\n");
		dbg_info( " +===============================\n");
		if(realtek_fm_close(cxt))		goto error;
		if(realtek_fm_open(cxt))		goto error;
		dbg_info( " -===============================\n");
	}

	cancel_delayed_work(&fm_show_snr);
	schedule_delayed_work(&fm_show_snr, HZ/2);

error:
	return;
}


static int rtl8723b_fm_StartSeek(struct realtek_fm_drv_data *cxt,
		struct realtek_seek_info *seek_info){
	ulong   jiffies_comp;
	unsigned long SeekTuneComplete;
	u8      is_timeout;

	dbg_info("gordon test into fm start seek\n");

	if(rtl8723b_fm_Seek( cxt->pFm, FM_SEEK_ENABLE ))
		goto error;

	jiffies_comp = jiffies;

	SeekTuneComplete = 0;

	do
	{

#ifdef FM_VOLUME_ON
		//gordon
		//dbg_info(
		//"gordon
		//before
		//schedule\n");
		cancel_delayed_work(&fm_volume_on);
		schedule_delayed_work(&fm_volume_on,
				HZ/2);
#endif
		if(atomic_read(&cxt->fm_searching) == 0)
				break;

		msleep_interruptible(REALTEK_FM_SEEK_CHECK_TIME);
		if(signal_pending(current))
				break;

		if(cxt->pFm->GetReadOnlyRegMaskBits(cxt->pFm,10,FM_ADDR_DEBUG_1,5,5,&SeekTuneComplete))
			goto error;

		if(rtl8723b_fm_GetFrequencyHz(cxt->pFm, &seek_info->FrequencyHz)!= FM_FUNCTION_SUCCESS)
			goto error;

		if(rtl8723b_fm_GetSNR(cxt->pFm, &seek_info->SnrDbNum, &seek_info->SnrDbDen)!= FM_FUNCTION_SUCCESS)
			goto error;


		dbg_info( " ***FM full_search-ING  ...  : #SeekTuneComplete=%ld, freq out=%dHz, SNR=%ld ***\n", SeekTuneComplete, seek_info->FrequencyHz,seek_info->SnrDbNum*10/seek_info->SnrDbDen);

		is_timeout = time_after(jiffies, jiffies_comp +
				msecs_to_jiffies(7500));

	}while( (SeekTuneComplete==0) && (!is_timeout) );

	if(rtl8723b_fm_GetFrequencyHz(cxt->pFm, &seek_info->FrequencyHz)!= FM_FUNCTION_SUCCESS)
		goto error;

	if(rtl8723b_fm_SetTune_Hz(cxt->pFm, seek_info->FrequencyHz)!= FM_FUNCTION_SUCCESS)	goto error;

	if(rtl8723b_fm_GetSNR(cxt->pFm, &seek_info->SnrDbNum, &seek_info->SnrDbDen)!=
			FM_FUNCTION_SUCCESS)  goto error;

	seek_info->Jif = jiffies;

	dbg_info("is timeout %d\n", is_timeout);
	if(is_timeout)
		return 1;

	return 0;
error:
	return -1;

}

/*
for real full search

*/
static int
realtek_fm_real_full_search(
	struct realtek_fm_drv_data *cxt,
	u16 frequency,
	u8 seek_dir,
	u32 time_out,
	u16 *freq_num
	)
{
	int before_freq;
	int num = 0;
	unsigned long RFGain;
	int Status = 0;

	if (!atomic_read(&cxt->fm_opened)) {
		dev_err(&cxt->client->dev, "FM Full search: FM not open\n");
		return FM_FUNCTION_ERROR;
	}

	if (atomic_read(&cxt->fm_searching)){
		dev_err(&cxt->client->dev, "FM Full search :  busy searching!!!\n");
		return -EBUSY;
	}

	dev_info(&cxt->client->dev, "%s, \n", __func__);


	atomic_cmpxchg(&cxt->fm_searching, 0, 1);
	if(rtl8723b_fm_SetMute(cxt->pFm, 1))        goto error;

	msleep(30);

	if(cxt->pFm->GetReadOnlyRegMaskBits(cxt->pFm, 21, FM_ADDR_DEBUG_0, 2, 0,
					&RFGain))  goto error;

	dbg_info( " ###FM full_search : RFGain = %d###\n", RFGain);

	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 0))     goto error;

	if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x0f, 0x4000))   goto error;

	switch(RFGain){
		case 7:
			if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0xfe00))
				goto error;
			break;

		case 6:
			if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0xe030))
				goto error;
			break;

		case 5:
			if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0x8018))
				goto error;
			break;

		case 4:
			if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0x43f8))
				goto error;
				break;

		case 0:
			if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x00, 0xa000))
				goto error;
			if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x0f, 0x4000))
				goto error;
			if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0xfe00))
				goto error;
			break;
		default:
			if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0x13f8))
				goto error;
			break;
		}

		if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 1))
			goto error;

		if (frequency == 0)
		{
			before_freq = cxt->current_freq*100*1000;
			if(rtl8723b_fm_SetTune_Hz(cxt->pFm,
				cxt->current_freq*100*1000 )!= FM_FUNCTION_SUCCESS)
				goto error;
		} else
		{
			before_freq = frequency*100*1000;
			if(rtl8723b_fm_SetTune_Hz(cxt->pFm,
				frequency*100*1000 )!= FM_FUNCTION_SUCCESS)
				goto error;
		}

		if(seek_dir==RTL_SEEK_DIR_UP){
			if(rtl8723b_fm_SetSeekDirection( cxt->pFm, FM_SEEK_DIR_UP ))    goto error;
		}else{
			if(rtl8723b_fm_SetSeekDirection( cxt->pFm, FM_SEEK_DIR_DOWN ))  goto error;
		}

	seek_info1 = kzalloc(sizeof(struct realtek_seek_info), GFP_KERNEL);
	if(seek_info1 == NULL){
		dbg_info("alloc seek info fail\n");
		Status = FM_FUNCTION_ERROR;
		goto alloc_data_failed;
	}
	seek_info1->next = NULL;
	seek_info2 =  seek_info1;
	seek_info = seek_info1;
	do{
		if(rtl8723b_fm_StartSeek(cxt, seek_info2) != 0) goto error;
		if(!(atomic_read(&cxt->fm_searching))){
			//quit loop when scan been stop for some reason
			printk(" !! fm full search been canced before finish. ");
			Status = -EINTR;
			goto error;
		}
		dbg_info("before freq is %d, seek freq is %d\n", before_freq, seek_info2->FrequencyHz);
		if(seek_dir==RTL_SEEK_DIR_UP){
			if(seek_info2->FrequencyHz <= before_freq){
				kfree(seek_info2);		//last freq
				seek_info2 = NULL;
				pre_seek_info->next=NULL;
				break;
			}
		}else{
			if(seek_info2->FrequencyHz >= before_freq){
				kfree(seek_info2);
				seek_info2 = NULL;
				pre_seek_info->next=NULL;
				break;
			}
		}
		before_freq =  seek_info2->FrequencyHz;
		seek_info2 = kzalloc(sizeof(struct realtek_seek_info), GFP_KERNEL);
		if(seek_info2 == NULL){
			dbg_info("alloc seek info fail\n");
			Status = FM_FUNCTION_ERROR;
			goto alloc_data_failed;
		}
		seek_info2->next = NULL;
		pre_seek_info = seek_info1;
		seek_info1->next = seek_info2;
		seek_info1 = seek_info2;

	}while(1);

//#if 0
	for(seek_info1 = seek_info, num = 1; seek_info1 != NULL && num < 100;){
		dbg_info("seek info freq is %d, snr: snrnum is : %d, snrden is :%d \n ", seek_info1->FrequencyHz, seek_info1->SnrDbNum ,seek_info1->SnrDbDen);
		if(seek_info1->SnrDbNum < 896 ){
			seek_info1 = seek_info1->next;
			continue;
		}
		if(seek_dir==RTL_SEEK_DIR_UP){
			if((seek_info1->next != NULL) && (seek_info1->FrequencyHz + 100000 ==
						seek_info1->next->FrequencyHz) && (seek_info1->SnrDbNum <= seek_info1->next->SnrDbNum+ 128)){
				seek_info1 = seek_info1->next;
				continue;
			}
			seek_freq[num++] = seek_info1->FrequencyHz / 100000;
			if((seek_info1->next != NULL) &&(seek_info1->FrequencyHz + 100000 ==
						seek_info1->next->FrequencyHz)){
				seek_info1 = seek_info1->next->next;
			}
			else
				seek_info1 = seek_info1->next;
		}else{
			if((seek_info1->next != NULL) && (seek_info1->FrequencyHz - 100000 ==
						seek_info1->next->FrequencyHz) && (seek_info1->SnrDbNum <= seek_info1->next->SnrDbNum
							+ 128)){
				seek_info1 = seek_info1->next;
				continue;
			}
			seek_freq[num++] = seek_info1->FrequencyHz / 100000;
			if((seek_info1->next != NULL) &&(seek_info1->FrequencyHz - 100000 ==
						seek_info1->next->FrequencyHz)){
				seek_info1 = seek_info1->next->next;
			}
			else
				seek_info1 = seek_info1->next;
		}
		dbg_info("end for num: %d \n",num);

	}
//#endif

	*freq_num = num - 1;
	Status = FM_FUNCTION_SUCCESS;
	if(*freq_num != 0)
		if(rtl8723b_fm_SetTune_Hz(cxt->pFm,  seek_freq[0]*100*1000 )!= FM_FUNCTION_SUCCESS)	goto error;


alloc_data_failed:
	for(seek_info1 = seek_info; seek_info1 != NULL; seek_info1 = seek_info1->next){
		kfree(seek_info1);
	}
		seek_info1 = seek_info2 = seek_info = NULL;

	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 0))	goto error;

	if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x0f, 0x0000))	goto error;

	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 1))	goto error;

	//cxt->current_freq = *freq_found;

	rtl8723b_fm_SetMute(cxt->pFm, 0);

	atomic_cmpxchg(&cxt->fm_searching, 1, 0);

	return Status;

error:

	cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x0f, 0x0000);

	cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 1);

	rtl8723b_fm_SetMute(cxt->pFm, 0);

	dev_err(&cxt->client->dev, "-FM full_search : err\n");

	atomic_cmpxchg(&cxt->fm_searching, 1, 0);

	Status = (-EINTR)? : FM_FUNCTION_ERROR;

	*freq_num = 0;

	return Status;
}



// NOTES: 
// Search frequency from current frequency, if a channel is found, the 
// frequency will be read out.
// This function is timeout call. If no channel is found when time out, a error 
// code will be given. The caller can retry search(skip "do seek")   to get 
// channel. 
//
static int
realtek_fm_full_search(
	struct realtek_fm_drv_data *cxt, 
	u16 frequency, 
	u8 seek_dir,
	u32 time_out,
	u16 *freq_found
	)
{
	int FrequencyHz1;
	int FrequencyHz2;
	int FrequencyTest;

	unsigned long SeekTuneComplete;

	int Status;
	ulong   jiffies_comp;
	u8      is_timeout;
   	
	long SnrDbNum1, SnrDbNum2;
	long SnrDbDen1, SnrDbDen2;
	unsigned long RFGain, RXBBGain;
	
	if (!atomic_read(&cxt->fm_opened)) {

		dev_err(&cxt->client->dev, "FM Full search: FM not open\n");

		return FM_FUNCTION_ERROR;
	}

	if (atomic_read(&cxt->fm_searching)){

		dev_err(&cxt->client->dev, "FM Full search :  busy searching!!!\n");
		
		return -EBUSY;	
	}
	
	dbg_info( "+FM full_search\n");	

	atomic_cmpxchg(&cxt->fm_searching, 0, 1);

	if(rtl8723b_fm_SetMute(cxt->pFm, 1))		goto error;

	msleep(30);

	if(cxt->pFm->GetReadOnlyRegMaskBits(cxt->pFm, 21, FM_ADDR_DEBUG_0, 2, 0, &RFGain))	goto error;

	dbg_info( " ###FM full_search : RFGain = %d###\n", RFGain);

	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 0))		goto error;

	if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x0f, 0x4000))	goto error;

	switch(RFGain)
	{
		case 7:
		if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0xfe00))	goto error;
		break;

		case 6:
		if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0xe030))	goto error;
		break;

		case 5:
		if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0x8018))	goto error;
		break;

		case 4:
		if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0x43f8))	goto error;
		break;

		case 0:
		if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x00, 0xa000))	goto error;
		if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x0f, 0x4000))	goto error;
		if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0xfe00))	goto error;
		break;
		default:
		if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x06, 0x13f8))	goto error;
		break;
	}

	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 1))		goto error;

  	if (frequency == 0)
	{
		if(rtl8723b_fm_SetTune_Hz(cxt->pFm,  cxt->current_freq*100*1000 )!= FM_FUNCTION_SUCCESS)	goto error;
	}
	else
	{
		if(rtl8723b_fm_SetTune_Hz(cxt->pFm,  frequency*100*1000 )!= FM_FUNCTION_SUCCESS)	goto error;
	}

	if(seek_dir==RTL_SEEK_DIR_UP)
	{	
		if(rtl8723b_fm_SetSeekDirection( cxt->pFm, FM_SEEK_DIR_UP ))	goto error;
	}
	else
	{
		if(rtl8723b_fm_SetSeekDirection( cxt->pFm, FM_SEEK_DIR_DOWN ))	goto error;
	}

	jiffies_comp = jiffies;

reseek:	
	
	
	if(rtl8723b_fm_Seek( cxt->pFm, FM_SEEK_ENABLE ))	goto error;


	SeekTuneComplete = 0;

	do
	{

#ifdef FM_VOLUME_ON
		//gordon
		//dbg_info( "gordon before schedule\n");		
		cancel_delayed_work(&fm_volume_on);
		schedule_delayed_work(&fm_volume_on, HZ/2);
#endif
		if (atomic_read(&cxt->fm_searching) == 0)
			break;
        
		msleep_interruptible(REALTEK_FM_SEEK_CHECK_TIME);
		if (signal_pending(current))
			break;

		if(cxt->pFm->GetReadOnlyRegMaskBits(cxt->pFm, 10, FM_ADDR_DEBUG_1, 5, 5, &SeekTuneComplete)) goto error;

		if(rtl8723b_fm_GetFrequencyHz(cxt->pFm, &FrequencyHz1)!= FM_FUNCTION_SUCCESS)	goto error;

		if(rtl8723b_fm_GetSNR(cxt->pFm, &SnrDbNum1, &SnrDbDen1)!= FM_FUNCTION_SUCCESS)	goto error;

		if(cxt->pFm->GetReadOnlyRegMaskBits(cxt->pFm, 21, FM_ADDR_DEBUG_0, 2, 0, &RFGain))	goto error;
		if(cxt->pFm->GetReadOnlyRegMaskBits(cxt->pFm, 21, FM_ADDR_DEBUG_0, 7, 3, &RXBBGain))	goto error;

		dbg_info( " ***FM full_search-ING  ...  : #SeekTuneComplete=%ld, freq out =%dHz, SNR=%ld, RFGain =%d, RXBBGain=%d***\n", SeekTuneComplete, FrequencyHz1, SnrDbNum1*10/SnrDbDen1, RFGain, RXBBGain);		
		
		is_timeout = time_after(jiffies, jiffies_comp + msecs_to_jiffies(7500));
		
	}while( (SeekTuneComplete==0) && (!is_timeout) );

	if(SeekTuneComplete==0)
	{
		dbg_info( " FM full_search : seek timeout      ERROR!!!\n");
	}

	if( SeekTuneComplete==1 )
	{
		Status = FM_FUNCTION_SUCCESS;
	}
	else
	{
		Status = FM_FUNCTION_FULL_SEARCH_ERROR;
	}

	msleep(2);
		
	if(rtl8723b_fm_GetFrequencyHz(cxt->pFm, &FrequencyHz1)!= FM_FUNCTION_SUCCESS)	goto error;

	if(rtl8723b_fm_SetTune_Hz(cxt->pFm, FrequencyHz1)!= FM_FUNCTION_SUCCESS)	goto error;

	if(rtl8723b_fm_GetFrequencyHz(cxt->pFm, &FrequencyTest)!= FM_FUNCTION_SUCCESS)	goto error;

	dbg_info( " FM full_search : *** bef  rtl8723b_fm_SetTune_Hz freq = %d, after rtl8723b_fm_SetTune_Hz freq = %d***\n", FrequencyHz1, FrequencyTest);
	if(rtl8723b_fm_GetSNR(cxt->pFm, &SnrDbNum1, &SnrDbDen1)!= FM_FUNCTION_SUCCESS)	goto error;

	*freq_found = FrequencyHz1/100000;

#if 1
	if(Status == FM_FUNCTION_SUCCESS)
	{

		if(SnrDbNum1 < 896)
		{
			dbg_info( " FM full_search : SnrDbNum1 < 896 ..............so RE-SEEK\n");
			goto reseek;
		}

		dbg_info( " FM full_search :                         a. Freq:%d, SNR:%ld\n", FrequencyHz1/100000, SnrDbNum1*10/SnrDbDen1);

		if(seek_dir==RTL_SEEK_DIR_UP)
			FrequencyHz2 = FrequencyHz1+100000;
		else
			FrequencyHz2 = FrequencyHz1-100000;

		if(rtl8723b_fm_SetTune_Hz(cxt->pFm, FrequencyHz2)!= FM_FUNCTION_SUCCESS)	goto error;

		if(rtl8723b_fm_GetSNR(cxt->pFm, &SnrDbNum2, &SnrDbDen2)!= FM_FUNCTION_SUCCESS)	goto error;

		dbg_info( " FM full_search :                      b. Freq:%d, SNR:%ld\n", FrequencyHz2/100000, SnrDbNum2*10/SnrDbDen2);

		if(SnrDbNum1 <= SnrDbNum2 + 128)
		{
			dbg_info( " FM full_search :   snr1_%ld <= snr2_%ld+0.5...............so RE-SEEK\n", SnrDbNum1*10/SnrDbDen1, SnrDbNum2*10/SnrDbDen2);

			if(rtl8723b_fm_SetTune_Hz(cxt->pFm, FrequencyHz1)!= FM_FUNCTION_SUCCESS)	goto error;
			
			goto reseek;
		}
		else
		{
			if(rtl8723b_fm_SetTune_Hz(cxt->pFm, FrequencyHz1)!= FM_FUNCTION_SUCCESS)	goto error;
			
		}

		if(  ((cxt->last_seek_freq+1) == *freq_found) || ((cxt->last_seek_freq-1) == *freq_found) )
		{
			dbg_info( " FM full_search : Diff (last_seek_freq = %d, freq_found = %d) =100kHz , so RE-SEEK\n",cxt->last_seek_freq, *freq_found);
			goto reseek;
		}
		else
		{
			cxt->last_seek_freq = *freq_found;
		}


	}
#endif

	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 0))	goto error;

	if(cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x0f, 0x0000))	goto error;

	if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 1))	goto error;

	cxt->current_freq = *freq_found;

	rtl8723b_fm_SetMute(cxt->pFm, 0);

	atomic_cmpxchg(&cxt->fm_searching, 1, 0);

	dbg_info( 
		"-FM full_search 1 : current freq=%d, dir=%d, timeout=%d\n", frequency, seek_dir, time_out);

	dbg_info( 
		"-FM full_search 2 : ***Status = %d, freq find out =%dHz***\n",Status, *freq_found);

	return Status;

error:

	cxt->pFm->SetRfcReg2Bytes(cxt->pFm, 0x0f, 0x0000);

	cxt->pFm->SetRegMaskBits(cxt->pFm, 0x006c, 0, 0, 1);

	rtl8723b_fm_SetMute(cxt->pFm, 0);

	dev_err(&cxt->client->dev, "-FM full_search : err\n");	


	atomic_cmpxchg(&cxt->fm_searching, 1, 0);	

	return FM_FUNCTION_ERROR;
}


#if 1
#define VOLUME_LEVEL_LEN		16
unsigned long VOLUME_LEVEL[VOLUME_LEVEL_LEN] = 
{
	0,
	60,
	70,
	83,
	98,
	115,
	136,
	161,
	189,
	224,
	264,
	311,
	367,
	432,
	510,
	601,
};
#else

#define VOLUME_LEVEL_LEN		8
unsigned long VOLUME_LEVEL[VOLUME_LEVEL_LEN] = 
{
	0,
	//60,
	//70,
	83,
	//98,
	115,
	//136,
	161,
	//189,
	224,
	//264,
	311,
	//367,
	432,
	//510,
	601,
};

#endif

static int realtek_fm_set_volume(struct realtek_fm_drv_data *cxt, u8 volume)
{
	int i;

	if (!atomic_read(&cxt->fm_opened)) {

		dev_err(&cxt->client->dev, "FM set_volume: FM not open\n");

		return FM_FUNCTION_ERROR;
	}

	if(volume > 15 || volume < 0)
		return FM_FUNCTION_ERROR;
		
	dbg_info( "+FM set_volume : volume = %d\n", volume);



	if( cxt->current_volume >= volume)    //volume down
	{
		for( i = cxt->current_volume;  i >= volume ; i--)
		{
			if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0024, 11, 0, VOLUME_LEVEL[i])) goto error;

		}

	}
	else if( cxt->current_volume < volume)  //volume up
	{
		for( i = cxt->current_volume+1;  i <= volume ; i++)
		{
			if(cxt->pFm->SetRegMaskBits(cxt->pFm, 0x0024, 11, 0, VOLUME_LEVEL[i])) goto error;

		}
	}

	cxt->current_volume = volume;

	dbg_info( "-FM set_volume : volume = %d\n", volume);


	return FM_FUNCTION_SUCCESS;
	
error:

	dbg_info( "-FM set_volume : err\n");

	return FM_FUNCTION_ERROR;	

}



static int realtek_fm_get_volume(struct realtek_fm_drv_data *cxt)
{
	unsigned long Value;
	unsigned long volume;
	
	if (!atomic_read(&cxt->fm_opened)) {

		dev_err(&cxt->client->dev, "FM get_volume: FM not open\n");

		return FM_FUNCTION_ERROR;
	}

	dbg_info( "+FM get_volume\n");
	
	if(cxt->pFm->GetRegMaskBits(cxt->pFm, 0x0024, 11, 0, &Value)) goto error;

	for(volume = 0 ; volume < VOLUME_LEVEL_LEN ; volume++)
	{
		if(Value == VOLUME_LEVEL[volume])
			break;
	}

	if(volume == VOLUME_LEVEL_LEN)
	{
		dbg_info( " FM get_volume err : find no correct volume level\n");
	}

	dbg_info( "-FM get_volume = %ld\n",  volume);
	
	return volume;

error:

	dbg_info( "-FM get_volume : err\n");

	return FM_FUNCTION_ERROR;	
	
}



static int realtek_fm_misc_open(struct inode *inode, struct file *filep)
{
	int ret;

	ret = nonseekable_open(inode, filep);

	if (ret < 0) {

		pr_err("realtek_fm open misc device failed.\n");

		return FM_FUNCTION_ERROR;
	}

	filep->private_data = realtek_fm_dev_data;

	return FM_FUNCTION_SUCCESS;
}

static int realtek_fm_misc_close(struct inode *inode, struct file *filep)
{
	int ret;
	struct realtek_fm_drv_data  *dev_data   = filep->private_data;
	ret = realtek_fm_close(dev_data);

	if (ret < 0) {

		pr_err("realtek_fm_misc_close misc device failed.\n");

		return FM_FUNCTION_ERROR;
	}

	return FM_FUNCTION_SUCCESS;
}

static long realtek_fm_misc_ioctl(struct file *filep,
        unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	int iarg = 0;
	int buf[30] = {0};
	struct realtek_fm_drv_data  *dev_data   = filep->private_data;
	int rssi;
	unsigned long snr, snr_dem;

	switch (cmd) {
	case REALTEK_FM_IOCTL_ENABLE:
		if (copy_from_user(&iarg, argp, sizeof(iarg)) || iarg > 1) {
			ret =  -EFAULT;
		}
		if (iarg ==1) {
			ret = realtek_fm_open(dev_data);
		}
		else {
			ret = realtek_fm_close(dev_data);
		}

 		break;

	case REALTEK_FM_IOCTL_GET_ENABLE:
		iarg = atomic_read(&dev_data->fm_opened);
		if (copy_to_user(argp, &iarg, sizeof(iarg))) {
			ret = -EFAULT;
		}
		break;

	case REALTEK_FM_IOCTL_SET_TUNE:
		if (copy_from_user(&iarg, argp, sizeof(iarg))) {
			ret = -EFAULT;
		}
		ret = realtek_fm_set_tune(dev_data, iarg);
		break;

	case REALTEK_FM_IOCTL_GET_FREQ:
		iarg = realtek_fm_get_frequency(dev_data);
		if (copy_to_user(argp, &iarg, sizeof(iarg))) {
			ret = -EFAULT;
		}
		break;

	case REALTEK_FM_IOCTL_SEARCH:
		if (copy_from_user(buf, argp, sizeof(buf))) {
			ret = -EFAULT;
		}
		ret = realtek_fm_full_search(dev_data,
			buf[0], /* start frequency */
			buf[1], /* seek direction*/
			buf[2], /* time out */
			(u16*)&buf[3]);/* frequency found will be stored to */

		break;

	case REALTEK_FM_IOCTL_FULL_SEARCH:
		dbg_info("rtkfm gordon in ioctl full search");
		if (copy_from_user(buf, argp, sizeof(buf))) {
			ret = -EFAULT;
		}
		ret = realtek_fm_real_full_search(dev_data,
			buf[0], /* start frequency */
			buf[2], /* seek direction*/
			buf[3], /* time out */
			(u16*)&buf[4]); /* freq num */
		dev_info(&dev_data->client->dev, "freq num is %d\n",buf[4] );
		seek_freq[0] = buf[4];
		if(copy_to_user(argp, seek_freq,sizeof(int)*(buf[4]+1))){
			ret = -EFAULT;
		}
		dbg_info("Randy copy to user  \n");

		break;

	case REALTEK_FM_IOCTL_STOP_SEARCH:
		ret = realtek_fm_stop_search(dev_data);
		break;

	case REALTEK_FM_IOCTL_MUTE:
		break;

	case REALTEK_FM_IOCTL_SET_VOLUME:
		if (copy_from_user(&iarg, argp, sizeof(iarg))) {
			ret = -EFAULT;
		}
		ret = realtek_fm_set_volume(dev_data, (u8)iarg);
		break;

	case REALTEK_FM_IOCTL_GET_VOLUME:
		iarg = realtek_fm_get_volume(dev_data);
		if (copy_to_user(argp, &iarg, sizeof(iarg))) {
			ret = -EFAULT;
		}
		break;
	case REALTEK_FM_IOCTL_GET_STATUS:
            /* rssi */
		rtl8723b_fm_GetRSSI(dev_data->pFm, &rssi);
		buf[0] = rssi;

            /* snr */
		rtl8723b_fm_GetSNR(dev_data->pFm, &snr, &snr_dem);
		buf[1] = snr/snr_dem;
            if (copy_to_user(argp, buf, sizeof(int[2]))) {
                ret = -EFAULT;
            }
            break;



	default:
		return -EINVAL;
	}

	return ret;
}



static const struct file_operations realtek_fm_misc_fops = {
	.owner = THIS_MODULE,
	.open  = realtek_fm_misc_open,
	.release = realtek_fm_misc_close,
	.unlocked_ioctl = realtek_fm_misc_ioctl,
};



static struct miscdevice realtek_fm_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = REALTEK_FM_DEV_NAME,
	.fops  = &realtek_fm_misc_fops,
};



//------------------------------------------------------------------------------
//realtek fm class attribute method
// ------------------------------------------------------------------------------
static ssize_t realtek_fm_attr_open(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
	u8 open;

	if (size) {
		open = simple_strtol(buf, NULL, 10);
		if (open)
			realtek_fm_open(realtek_fm_dev_data);
		else
			realtek_fm_close(realtek_fm_dev_data);
	}

	return size;
}



static ssize_t realtek_fm_attr_get_open(struct class *class, struct class_attribute *attr, char *buf)
{
	u8 opened;

	opened = atomic_read(&realtek_fm_dev_data->fm_opened);
	if (opened) 
		return sprintf(buf, "Opened\n");
	else
		return sprintf(buf, "Closed\n");
}



static ssize_t realtek_fm_attr_set_tune(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
	u16 frequency;

	if (size) {
		frequency = simple_strtol(buf, NULL, 10);/* decimal string to int */


		realtek_fm_set_tune(realtek_fm_dev_data, frequency);
	}

	return size;
}



static ssize_t realtek_fm_attr_get_frequency(struct class *class, struct class_attribute *attr, char *buf)
{
	u16 frequency;

	frequency = realtek_fm_get_frequency(realtek_fm_dev_data);

	return sprintf(buf, "Frequency %d\n", frequency);
}



static ssize_t realtek_fm_attr_search(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
	u32 timeout;
	u16 frequency;
	u8  seek_dir;
	u16 freq_found;
	char *p = (char*)buf;
	char *pi = NULL;

	if (size) {
		while (*p == ' ') p++;
		frequency = simple_strtol(p, &pi, 10); /* decimal string to int */
		if (pi == p) goto out;

		p = pi;
		while (*p == ' ') p++;
		seek_dir = simple_strtol(p, &pi, 10);
		if (pi == p) goto out;

		p = pi;
		while (*p == ' ') p++;
		timeout = simple_strtol(p, &pi, 10);
		if (pi == p) goto out;

		realtek_fm_full_search(realtek_fm_dev_data, frequency, seek_dir, timeout, &freq_found);
	}

out:
	return size;
}



static ssize_t realtek_fm_attr_set_volume(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
	u8 volume;

	if (size) {
		volume = simple_strtol(buf, NULL, 10);/* decimal string to int */

		realtek_fm_set_volume(realtek_fm_dev_data, volume);
	}

	return size;
}



static ssize_t realtek_fm_attr_get_volume(struct class *class, struct class_attribute *attr, char *buf)
{
	u8 volume;

	volume = realtek_fm_get_volume(realtek_fm_dev_data);

	return sprintf(buf, "Volume %d\n", volume);
}



static struct class_attribute realtek_fm_attrs[] = {
    __ATTR(fm_open,   S_IRUSR|S_IWUSR, realtek_fm_attr_get_open,      realtek_fm_attr_open),
    __ATTR(fm_tune,   S_IRUSR|S_IWUSR, realtek_fm_attr_get_frequency, realtek_fm_attr_set_tune),
    __ATTR(fm_seek,   S_IWUSR,         NULL,                          realtek_fm_attr_search),
    __ATTR(fm_volume, S_IRUSR|S_IWUSR, realtek_fm_attr_get_volume,    realtek_fm_attr_set_volume),
    {},     
};



void realtek_fm_sysfs_init(struct class *class)
{
	class->class_attrs = realtek_fm_attrs;
}



static void fm_gpio_init(int fm_en)
{

	printk("fm_gpio_init::%d\n",fm_en);

	if (fm_en > 0) {
		gpio_request(fm_en, "fm_en");
	} else {
		printk("fm enable pin is not set, please check\n");
	}
}



static void fm_gpio_deinit(int fm_en)
{
	if(fm_en > 0){
		gpio_free(fm_en);
	}else{
		printk("fm enable pin is not set, please check\n");
	}
}




//------------------------------------------------------------------------------ 
//REALTEK_FM i2c device driver.
// ------------------------------------------------------------------------------
static int realtek_fm_remove(struct i2c_client *client)
{
	struct realtek_fm_drv_data  *cxt = i2c_get_clientdata(client);
	struct rtk_fm_platform_data *pdata;
	realtek_fm_close(cxt);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&cxt->early_suspend);
#endif
	pdata = (struct rtk_fm_platform_data *)client->dev.platform_data;

	fm_gpio_deinit(pdata->fm_en);
	misc_deregister(&realtek_fm_misc_device);
	class_unregister(&cxt->fm_class);
	kfree(cxt);

	dbg_info( "%s\n", __func__);

	return 0;    
}


static int realtek_fm_resume(struct i2c_client *client)
{
	struct realtek_fm_drv_data *cxt = i2c_get_clientdata(client);
	dbg_info( "%s, FM opened before suspend: %d\n", 
		__func__, cxt->opened_before_suspend);

#ifdef FM_VOLUME_ON
	schedule_delayed_work(&fm_volume_on, HZ/2);
#endif

	if (cxt->opened_before_suspend) {
		cxt->opened_before_suspend = 0;
        
		realtek_fm_open(cxt);

		realtek_fm_set_volume(cxt, cxt->current_volume);
	}

	return 0;
}


static int realtek_fm_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct realtek_fm_drv_data *cxt = i2c_get_clientdata(client);

	dbg_info( "%s, FM opend: %d\n", 
		__func__, atomic_read(&cxt->fm_opened));

	if (atomic_read(&cxt->fm_opened) && cxt->bg_play_enable == 0) {
		cxt->opened_before_suspend = 1;
		realtek_fm_close(cxt);
	}

    return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND

static void realtek_fm_early_resume (struct early_suspend* es)
{

	dbg_info("%s.\n", __func__);

	if (realtek_fm_dev_data) {
		realtek_fm_resume(realtek_fm_dev_data->client);
	}
}

static void realtek_fm_early_suspend (struct early_suspend* es)
{   
	dbg_info("%s.\n", __func__);

	if (realtek_fm_dev_data) {
		realtek_fm_suspend(realtek_fm_dev_data->client, (pm_message_t){.event=0});
	}
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

#ifdef CONFIG_DEVINFO_COMIP
static ssize_t FM_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "FM:realtek_8723bs\n");
}

static DEVICE_ATTR(FM, S_IRUGO | S_IWUSR, FM_show, NULL);

static struct attribute *FM_attributes[] = {
	&dev_attr_FM.attr,
};
#endif

static int realtek_fm_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id
	)
{
	struct realtek_fm_drv_data *cxt;
	int ret;
	struct rtk_fm_platform_data *pdata;

	ret = -EINVAL;
	cxt = NULL;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "realtek_fm driver: client is not i2c capable.\n");
		ret = -ENODEV;
		goto i2c_functioality_failed;
	}

	cxt = kzalloc(sizeof(struct realtek_fm_drv_data), GFP_KERNEL);
	if (cxt == NULL) {
		dev_err(&client->dev, "Can't alloc memory for module data.\n");
		ret = -ENOMEM;
		goto alloc_data_failed;
	}

	mutex_init(&mutex);

	cxt->client = client;
	i2c_set_clientdata(client, cxt);

	atomic_set(&cxt->fm_searching, 0);
	atomic_set(&cxt->fm_opened, 0);

	BuildBaseInterface(
		&cxt->pBaseInterface,
		&cxt->BaseInterfaceModuleMemory,
		9,
		8,
		custom_i2c_read,
		custom_i2c_write,
		custom_wait_ms
		);

	BuildRtl8723bFmModule(&cxt->pFm,
						&cxt->FmModuleMemory,
						cxt->pBaseInterface,
						REALTEK_FM_I2C_ADDR);

	cxt->pBaseInterface->SetUserDefinedDataPointer(cxt->pBaseInterface, cxt);
	pdata = (struct rtk_fm_platform_data *)client->dev.platform_data;

	fm_gpio_init(pdata->fm_en);
	//ret = realtek_fm_power_on(cxt);
	//if (ret < 0) {
		//goto poweron_failed;
	//}

	//rtl8723b_fm_Get_Chipid(cxt->pFm, &chipid);
	//printk("realtek_fm_probe: chipid = 0X%x\n", chipid);
			
	cxt->fm_class.owner = THIS_MODULE;
	cxt->fm_class.name = "fm_class";
	realtek_fm_sysfs_init(&cxt->fm_class);
	ret = class_register(&cxt->fm_class);
	if (ret < 0) {
		dev_err(&client->dev, "realtek_fm class init failed.\n");
		goto class_init_failed;
	}

	ret = misc_register(&realtek_fm_misc_device);
	if (ret < 0) {
		dev_err(&client->dev, "realtek_fm misc device register failed.\n");
		goto misc_register_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	cxt->early_suspend.suspend = realtek_fm_early_suspend;
	cxt->early_suspend.resume  = realtek_fm_early_resume;
	cxt->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&cxt->early_suspend);
#endif
	cxt->opened_before_suspend = 0;
	cxt->bg_play_enable = 1;
	cxt->current_freq = 870; /* init current frequency, search may use it. */
	cxt->last_seek_freq = 0;
	cxt->current_volume = 0;
    
	realtek_fm_dev_data = cxt;

#ifdef CONFIG_DEVINFO_COMIP
		comip_devinfo_register(FM_attributes, ARRAY_SIZE(FM_attributes));
		printk("%s:comip_devinfo_register FM finished\n",__func__);
#endif


	return ret; 

misc_register_failed:
	misc_deregister(&realtek_fm_misc_device);
class_init_failed:    
	class_unregister(&cxt->fm_class);
	kfree(cxt);
alloc_data_failed:
i2c_functioality_failed:
	dev_err(&client->dev,"realtek_fm driver init failed.\n");
	return ret;
}


static const struct i2c_device_id realtek_fm_i2c_id[] = { 
    { REALTEK_FM_I2C_NAME, 0 }, 
    { }, 
};

MODULE_DEVICE_TABLE(i2c, realtek_fm_i2c_id);

static struct i2c_driver realtek_fm_i2c_driver = {
	.driver = {
		.name = REALTEK_FM_I2C_NAME,
	},
	.probe    =  realtek_fm_probe,
	.remove   =  realtek_fm_remove,
 	.id_table =  realtek_fm_i2c_id,
};


static int __init realtek_fm_driver_init(void)
{
	int  ret = 0;        

	dbg_info("REALTEK_FM driver: init\n");
	ret =  i2c_add_driver(&realtek_fm_i2c_driver);

	return ret;
}

static void __exit realtek_fm_driver_exit(void)
{

	dbg_info("REALTEK_FM driver exit\n");	

	i2c_del_driver(&realtek_fm_i2c_driver);
 	return;
}

module_init(realtek_fm_driver_init);
module_exit(realtek_fm_driver_exit);

MODULE_DESCRIPTION("REALTEK FM radio driver");
MODULE_AUTHOR("Leadcore Inc.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.1");
