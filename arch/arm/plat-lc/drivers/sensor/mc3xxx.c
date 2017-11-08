/*****************************************************************************
 *
 * Copyright (c) 2014 mCube, Inc.  All rights reserved.
 *
 * This source is subject to the mCube Software License.
 * This software is protected by Copyright and the information and source code
 * contained herein is confidential. The software including the source code
 * may not be copied and the information contained herein may not be used or
 * disclosed except with the written permission of mCube Inc.
 *
 * All other rights reserved.
 *
 * This code and information are provided "as is" without warranty of any
 * kind, either expressed or implied, including but not limited to the
 * implied warranties of merchantability and/or fitness for a
 * particular purpose.
 *
 * The following software/firmware and/or related documentation ("mCube Software")
 * have been modified by mCube Inc. All revisions are subject to any receiver's
 * applicable license agreements with mCube Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/input-polldev.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>


#define MC3XXX_I2C_ADDR		0x4C

#define POLL_INTERVAL_MIN	1
#define POLL_INTERVAL_MAX	500
#define POLL_INTERVAL		100 /* msecs */

// if sensor is standby ,set POLL_STOP_TIME to slow down the poll
#define POLL_STOP_TIME		10000//10s

#define MC3XXX_BUF_SIZE	7

/*****************************
  *  MC3433 REGISTER MAP
  *****************************/
#define MC3433_STATUS_REGISTER			0X03 //read only
#define MC3433_OPERATIONAL_STATE_STATUS_REGISTER	0X04 //read only
#define MC3433_INTERRUPT_ENABLE_REGISTER		0X06 //write only
#define MC3434_MODE_REGISTER			0X07 //write only
#define MC3433_SAMPLE_RATE_REGISHER			0X08 //write only



/***********************************************
 *** REGISTER MAP				//this register map not fully meet 3433 spec
 ***********************************************/
#define MC3XXX_REG_XOUT                    0x00
#define MC3XXX_REG_YOUT                    0x01
#define MC3XXX_REG_ZOUT                    0x02
#define MC3XXX_REG_TILT_STATUS             0x03
#define MC3XXX_REG_SAMPLE_RATE_STATUS      0x04
#define MC3XXX_REG_SLEEP_COUNT             0x05
#define MC3XXX_REG_INTERRUPT_ENABLE        0x06
#define MC3XXX_REG_MODE_FEATURE            0x07
#define MC3XXX_REG_SAMPLE_RATE             0x08
#define MC3XXX_REG_TAP_DETECTION_ENABLE    0x09
#define MC3XXX_REG_TAP_DWELL_REJECT        0x0A
#define MC3XXX_REG_DROP_CONTROL            0x0B
#define MC3XXX_REG_SHAKE_DEBOUNCE          0x0C
#define MC3XXX_REG_XOUT_EX_L               0x0D
#define MC3XXX_REG_XOUT_EX_H               0x0E
#define MC3XXX_REG_YOUT_EX_L               0x0F
#define MC3XXX_REG_YOUT_EX_H               0x10
#define MC3XXX_REG_ZOUT_EX_L               0x11
#define MC3XXX_REG_ZOUT_EX_H               0x12
#define MC3XXX_REG_RANGE_CONTROL           0x20
#define MC3XXX_REG_SHAKE_THRESHOLD         0x2B
#define MC3XXX_REG_UD_Z_TH                 0x2C
#define MC3XXX_REG_UD_X_TH                 0x2D
#define MC3XXX_REG_RL_Z_TH                 0x2E
#define MC3XXX_REG_RL_Y_TH                 0x2F
#define MC3XXX_REG_FB_Z_TH                 0x30
#define MC3XXX_REG_DROP_THRESHOLD          0x31
#define MC3XXX_REG_TAP_THRESHOLD           0x32
#define MC3XXX_REG_PRODUCT_CODE            0x3B


#define MC3XXX_PCODE_3210     0x90
#define MC3XXX_PCODE_3230     0x19
#define MC3XXX_PCODE_3250     0x88
#define MC3XXX_PCODE_3410     0xA8
#define MC3XXX_PCODE_3410N    0xB8
#define MC3XXX_PCODE_3430     0x29
#define MC3XXX_PCODE_3430N    0x39
#define MC3XXX_PCODE_3510B    0x40
#define MC3XXX_PCODE_3530B    0x30
#define MC3XXX_PCODE_3510C    0x10
#define MC3XXX_PCODE_3530C    0x60

#define IS_MC35XX()    ((MC3XXX_PCODE_3510B == s_bPCODE) || (MC3XXX_PCODE_3510C == (s_bPCODE&0xF1)) || (MC3XXX_PCODE_3530B == s_bPCODE) || (MC3XXX_PCODE_3530C == (s_bPCODE&0xF1)))

#define MC3XXX_RESOLUTION_LOW     1
#define MC3XXX_RESOLUTION_HIGH    2

#define MC3XXX_AXIS_X      0
#define MC3XXX_AXIS_Y      1
#define MC3XXX_AXIS_Z      2

#define MC3XXX_LOW_REOLUTION_DATA_SIZE     3
#define MC3XXX_HIGH_REOLUTION_DATA_SIZE    6

static unsigned char    s_bResolution = 0x00;
static unsigned char    s_bPCODE      = 0x00;

typedef struct {
	unsigned short	x;		/**< X axis */
	unsigned short	y;		/**< Y axis */
	unsigned short	z;		/**< Z axis */
} GSENSOR_VECTOR3D;

static GSENSOR_VECTOR3D    gsensor_gain;

/***********************************************
 *** RETURN CODE
 ***********************************************/
#define MC3XXX_RETCODE_SUCCESS                 (0)
#define MC3XXX_RETCODE_ERROR_I2C               (-1)
#define MC3XXX_RETCODE_ERROR_NULL_POINTER      (-2)
#define MC3XXX_RETCODE_ERROR_STATUS            (-3)
#define MC3XXX_RETCODE_ERROR_SETUP             (-4)
#define MC3XXX_RETCODE_ERROR_GET_DATA          (-5)
#define MC3XXX_RETCODE_ERROR_IDENTIFICATION    (-6)

enum {
	MMA_STANDBY = 0,
	MMA_ACTIVED,
};

struct mc3xxx_platform_data{
	int position;
	int mode;
};

struct mc3xxx_data_axis{
	short x;
	short y;
	short z;
};
struct mc3xxx_data{
	struct i2c_client * client;
	struct input_polled_dev *poll_dev;
	struct early_suspend early_suspend;
	struct mutex data_lock;
	int active;
	int position;
	u8 chip_id;
	int mode;
};
static char * mc3xxx_names[] ={
	"mc3433",
	"mc3430",
	"mc3230",
	"mc3233",
};
static int mc3xxx_position_setting[8][3][3] =
{
   {{ 0, -1,  0}, { 1,  0,	0}, {0, 0,	1}},
   {{-1,  0,  0}, { 0, -1,	0}, {0, 0,	1}},
   {{ 0,  1,  0}, {-1,  0,	0}, {0, 0,	1}},
   {{ 1,  0,  0}, { 0,  1,	0}, {0, 0,	1}},
   
   {{ 0, -1,  0}, {-1,  0,	0}, {0, 0,  -1}},
   {{-1,  0,  0}, { 0,  1,	0}, {0, 0,  -1}},
   {{ 0,  1,  0}, { 1,  0,	0}, {0, 0,  -1}},
   {{ 1,  0,  0}, { 0, -1,	0}, {0, 0,  -1}},
};

static int mc3xxx_data_convert(struct mc3xxx_data* pdata,struct mc3xxx_data_axis *axis_data)
{
	short rawdata[3],data[3];
	int i,j;
	int position = pdata->position ;
	if(position < 0 || position > 7 )
   		position = 0;
	rawdata [0] = axis_data->x ; rawdata [1] = axis_data->y ; rawdata [2] = axis_data->z ;  
	for(i = 0; i < 3 ; i++){
		data[i] = 0;
		for(j = 0; j < 3; j++)
			data[i] += rawdata[j] * mc3xxx_position_setting[position][i][j];
	}
	axis_data->x = data[0];
	axis_data->y = data[1];
	axis_data->z = data[2];
	return 0;
}
static char * mc3xxx_id2name(u8 id){
	int index = 0;
	if((id&0xF1) == MC3XXX_PCODE_3530C)
		index = 0;
	else if(id == MC3XXX_PCODE_3430)
		index = 1;
	else if(id == MC3XXX_PCODE_3230)
		index = 2;
	return mc3xxx_names[index];
}


static int mc3xxx_device_stop(struct i2c_client *client)
{
	u8 val;
//	val = i2c_smbus_read_byte_data(client, MC3XXX_REG_MODE_FEATURE);
	i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE,0x40);//was 0x43, but according to spec, 0x40
	val = i2c_smbus_read_byte_data(client, MC3433_OPERATIONAL_STATE_STATUS_REGISTER);
	printk("====%s====, now Operational State Register value is 0x%x \n",__func__, val);
	
	return 0;
}

#ifdef _MC3XXX_SUPPORT_LRF_
/*****************************************
 *** _MC3XXX_LowResFilter
 *****************************************/
static void _MC3XXX_LowResFilter(s16 nAxis, s16 naData)
{
    #define _LRF_DIFF_COUNT_POS                  2
    #define _LRF_DIFF_COUNT_NEG                  (-_LRF_DIFF_COUNT_POS)
    #define _LRF_DIFF_BOUNDARY_POS               (_LRF_DIFF_COUNT_POS + 1)
    #define _LRF_DIFF_BOUNDARY_NEG               (_LRF_DIFF_COUNT_NEG - 1)
    #define _LRF_DIFF_DATA_UNCHANGE_MAX_COUNT    11
    
    signed int    _nCurrDiff = 0;
    signed int    _nSumDiff  = 0;
    s16           _nCurrData = naData;
    
    _nCurrDiff = (_nCurrData - s_taLRF_CB[nAxis].nRepValue);
    
    if ((_LRF_DIFF_COUNT_NEG < _nCurrDiff) && (_nCurrDiff < _LRF_DIFF_COUNT_POS))
    {
        if (s_taLRF_CB[nAxis].nIsNewRound)
        {
            s_taLRF_CB[nAxis].nMaxValue = _nCurrData;
            s_taLRF_CB[nAxis].nMinValue = _nCurrData;
            
            s_taLRF_CB[nAxis].nIsNewRound = 0;
            s_taLRF_CB[nAxis].nNewDataMonitorCount = 0;
        }
        else
        {
            if (_nCurrData > s_taLRF_CB[nAxis].nMaxValue)
                s_taLRF_CB[nAxis].nMaxValue = _nCurrData;
            else if (_nCurrData < s_taLRF_CB[nAxis].nMinValue)
                s_taLRF_CB[nAxis].nMinValue = _nCurrData;
            
            if (s_taLRF_CB[nAxis].nMinValue != s_taLRF_CB[nAxis].nMaxValue)
            {
                if (_nCurrData == s_taLRF_CB[nAxis].nPreValue)
                    s_taLRF_CB[nAxis].nNewDataMonitorCount++;
                else
                    s_taLRF_CB[nAxis].nNewDataMonitorCount = 0;
            }
        }
        
        if (1 != (s_taLRF_CB[nAxis].nMaxValue - s_taLRF_CB[nAxis].nMinValue))
            s_taLRF_CB[nAxis].nRepValue = ((s_taLRF_CB[nAxis].nMaxValue + s_taLRF_CB[nAxis].nMinValue) / 2);
        
        _nSumDiff = (_nCurrDiff + s_taLRF_CB[nAxis].nPreDiff);
        
        if (_nCurrDiff)
            s_taLRF_CB[nAxis].nPreDiff = _nCurrDiff;
        
        if ((_LRF_DIFF_BOUNDARY_NEG < _nSumDiff) && (_nSumDiff < _LRF_DIFF_BOUNDARY_POS))
        {
            if (_LRF_DIFF_DATA_UNCHANGE_MAX_COUNT > s_taLRF_CB[nAxis].nNewDataMonitorCount)
            {
                naData = s_taLRF_CB[nAxis].nRepValue;
                goto _LRF_RETURN;
            }
        }
    }
    
    s_taLRF_CB[nAxis].nRepValue   = _nCurrData;
    s_taLRF_CB[nAxis].nPreDiff    = 0;
    s_taLRF_CB[nAxis].nIsNewRound = 1;
    
_LRF_RETURN:
    
    printk(">>>>> [_MC3XXX_LowResFilter][%d] _nCurrDiff: %4d       _nSumDiff: %4d         _nCurrData:    %4d         Rep:    %4d\n", nAxis, _nCurrDiff, _nSumDiff, _nCurrData, s_taLRF_CB[nAxis].nRepValue);
    
    s_taLRF_CB[nAxis].nPreValue = _nCurrData;
    
    #undef _LRF_DIFF_COUNT_POS
    #undef _LRF_DIFF_COUNT_NEG
    #undef _LRF_DIFF_BOUNDARY_POS
    #undef _LRF_DIFF_BOUNDARY_NEG
    #undef _LRF_DIFF_DATA_UNCHANGE_MAX_COUNT
}
#endif    // END OF #ifdef _MC3XXX_SUPPORT_LRF_

static int mc3xxx_read_data(struct i2c_client *client,struct mc3xxx_data_axis *data)
{
	int ret;
	u8    _baData[MC3XXX_BUF_SIZE] = { 0 };
//	printk("entry mc3xxx_read_data\n");
	
	 if (MC3XXX_RESOLUTION_LOW == s_bResolution){
		ret = i2c_smbus_read_i2c_block_data(client, MC3XXX_REG_XOUT, MC3XXX_LOW_REOLUTION_DATA_SIZE,_baData);
		if (ret < MC3XXX_LOW_REOLUTION_DATA_SIZE){
			printk("ERR: fail to read data via I2C!\n");
			return (MC3XXX_RETCODE_ERROR_I2C);
		}
    
		data->x = _baData[0];
		data->y = _baData[1];
		data->z = _baData[2];

		if(data->x > 127)
			data->x = data->x-256;
		if(data->y > 127)
			data->y = data->y-256;
		if(data->z > 127)
			data->z = data->z-256;
    
//		printk("[%s][low] X: %d, Y: %d, Z: %d\n", __FUNCTION__, data->x, data->y, data->z);

		#ifdef _MC3XXX_SUPPORT_LRF_
		_MC3XXX_LowResFilter(MC3XXX_AXIS_X, data->x);
		_MC3XXX_LowResFilter(MC3XXX_AXIS_Y, data->y);
		_MC3XXX_LowResFilter(MC3XXX_AXIS_Z, data->z);
		#endif
	}
	else if (MC3XXX_RESOLUTION_HIGH == s_bResolution){
		ret = i2c_smbus_read_i2c_block_data(client, MC3XXX_REG_XOUT_EX_L, MC3XXX_HIGH_REOLUTION_DATA_SIZE,_baData);
	if (ret < MC3XXX_HIGH_REOLUTION_DATA_SIZE){
		printk("ERR: fail to read data via I2C!\n");
		return (MC3XXX_RETCODE_ERROR_I2C);
	}
        
	data->x = ((signed short) ((_baData[0]) | (_baData[1]<<8)));
	data->y = ((signed short) ((_baData[2]) | (_baData[3]<<8)));
	data->z = ((signed short) ((_baData[4]) | (_baData[5]<<8)));

//	printk("[%s][high] X: %d, Y: %d, Z: %d\n", __FUNCTION__, data->x, data->y, data->z);
	}
	 
	return 0;
}

static void mc3xxx_report_data(struct mc3xxx_data* pdata)
{
	struct input_polled_dev * poll_dev = pdata->poll_dev;
	struct mc3xxx_data_axis data;
//	printk("entry mc3xxx_report_data\n");
	mutex_lock(&pdata->data_lock);
	if(pdata->active == MMA_STANDBY){
		printk("entry mc3xxx_report_data pdata->active = %d\n",pdata->active);
		poll_dev->poll_interval = POLL_STOP_TIME;  // if standby ,set as 10s to slow the poll,
		goto out;
	}else{
		if(poll_dev->poll_interval == POLL_STOP_TIME)
			poll_dev->poll_interval = POLL_INTERVAL;
	}
	if (mc3xxx_read_data(pdata->client,&data) != 0)
		goto out;
	mc3xxx_data_convert(pdata,&data);
//	printk("mc3xxx_report_data data.x = %d, data.y = %d, data.z = %d\n",data.x, data.y, data.z);
	input_report_abs(poll_dev->input, ABS_X, data.x);
	input_report_abs(poll_dev->input, ABS_Y, data.y);
	input_report_abs(poll_dev->input, ABS_Z, data.z);
	input_sync(poll_dev->input);
out:
	mutex_unlock(&pdata->data_lock);
}

static void mc3xxx_dev_poll(struct input_polled_dev *dev)
{
	struct mc3xxx_data* pdata = (struct mc3xxx_data*)dev->private;
	mc3xxx_report_data(pdata);
}

static ssize_t mc3xxx_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct input_polled_dev *poll_dev = dev_get_drvdata(dev);
	struct mc3xxx_data *pdata = (struct mc3xxx_data *)(poll_dev->private);
	struct i2c_client *client = pdata->client;
	u8 val;
    	int enable;
	printk("entry mc3xxx_enable_show\n");
	mutex_lock(&pdata->data_lock);

	val = i2c_smbus_read_byte_data(client, 0x04);
	printk("====%s====, 0x04 register read value = 0x%x, \n", __func__, val);
	
	if((val == 0x41) && pdata->active == MMA_ACTIVED)
		enable = 1;
	else
		enable = 0;
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "%d\n", enable);
}

static ssize_t mc3xxx_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct input_polled_dev *poll_dev = dev_get_drvdata(dev);
	struct mc3xxx_data *pdata = (struct mc3xxx_data *)(poll_dev->private);
	struct i2c_client *client = pdata->client;
	int ret;
	unsigned long enable;
	printk("entry mc3xxx_enable_store\n");
	enable = simple_strtoul(buf, NULL, 10);    
	mutex_lock(&pdata->data_lock);
	enable = (enable > 0) ? 1 : 0;
	if(enable && pdata->active == MMA_STANDBY){  
		ret = i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, 0x41);
		if(!ret){
			pdata->active = MMA_ACTIVED;
			printk("mma enable setting active \n");
		}
	} else if(enable == 0  && pdata->active == MMA_ACTIVED) {
		ret = i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE,0x40);//standby should be 0x40 for mc3433
		if(!ret){
		 pdata->active= MMA_STANDBY;
		 printk("mma enable setting inactive \n");
		}
	}
	mutex_unlock(&pdata->data_lock);
	return count;
}
static ssize_t mc3xxx_position_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct input_polled_dev *poll_dev = dev_get_drvdata(dev);
	struct mc3xxx_data *pdata = (struct mc3xxx_data *)(poll_dev->private);
	int position = 0;
	mutex_lock(&pdata->data_lock);
	position = pdata->position ;
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "%d\n", position);
}

static ssize_t mc3xxx_position_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct input_polled_dev *poll_dev = dev_get_drvdata(dev);
	struct mc3xxx_data *pdata = (struct mc3xxx_data *)(poll_dev->private);
	int  position;
	position = simple_strtoul(buf, NULL, 10);    
	mutex_lock(&pdata->data_lock);
	pdata->position = position;
	mutex_unlock(&pdata->data_lock);
	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
		   mc3xxx_enable_show, mc3xxx_enable_store);
static DEVICE_ATTR(position, S_IWUSR | S_IRUGO,
		   mc3xxx_position_show, mc3xxx_position_store);

static struct attribute *mc3xxx_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_position.attr,
	NULL
};

static const struct attribute_group mc3xxx_attr_group = {
	.attrs = mc3xxx_attributes,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mc3xxx_suspend(struct early_suspend *h)
{
	int val = 0;
	struct mc3xxx_data *pdata = container_of(h, struct mc3xxx_data, early_suspend);
	struct i2c_client *client = pdata->client;
	if(pdata->active == MMA_ACTIVED)
		mc3xxx_device_stop(client);
	val = i2c_smbus_read_byte_data(client,MC3433_OPERATIONAL_STATE_STATUS_REGISTER);
	printk("%s :MC3433_OPERATIONAL_STATE_STATUS_REGISTER == %x\n", __func__, val);

}

static void mc3xxx_resume(struct early_suspend *h)
{
	int val = 0;
	struct mc3xxx_data *pdata = container_of(h, struct mc3xxx_data, early_suspend);
	struct i2c_client *client = pdata->client;
	if(pdata->active == MMA_ACTIVED){
		val = i2c_smbus_read_byte_data(client,MC3XXX_REG_MODE_FEATURE);
		i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, 0x41);
		val = i2c_smbus_read_byte_data(client,MC3433_OPERATIONAL_STATE_STATUS_REGISTER);
	printk("%s :MC3433_OPERATIONAL_STATE_STATUS_REGISTER == %x\n", __func__, val);
	}
}
#endif

/*****************************************
 *** MC3XXX_SetResolution
 *****************************************/
static void MC3XXX_SetResolution(void)
{
	printk("[%s]\n", __FUNCTION__);

	switch (s_bPCODE){
	case MC3XXX_PCODE_3230:
	case MC3XXX_PCODE_3430:
	case MC3XXX_PCODE_3430N:
	case MC3XXX_PCODE_3530B:
	case MC3XXX_PCODE_3530C:
	s_bResolution = MC3XXX_RESOLUTION_LOW;
	break;

	case MC3XXX_PCODE_3210:
	case MC3XXX_PCODE_3250:
	case MC3XXX_PCODE_3410:
	case MC3XXX_PCODE_3410N:
	case MC3XXX_PCODE_3510B:
	case MC3XXX_PCODE_3510C:
	s_bResolution = MC3XXX_RESOLUTION_HIGH;
	break;

default:
	printk("ERR: no resolution assigned!\n");
	break;
	}
    
	if(MC3XXX_PCODE_3530C==(s_bPCODE&0xF1))
		s_bResolution = MC3XXX_RESOLUTION_LOW;
	else if(MC3XXX_PCODE_3510C==(s_bPCODE&0xF1))
		s_bResolution = MC3XXX_RESOLUTION_HIGH;

	printk("[%s] s_bResolution: %d(1= resolution low, 2=high)\n", __FUNCTION__, s_bResolution);
}

/*****************************************
 *** MC3XXX_ConfigRegRange
 *****************************************/
static void MC3XXX_ConfigRegRange(struct i2c_client *pt_i2c_client)
{
	unsigned char    _baDataBuf[1] = { 0 };

	_baDataBuf[0] = 0x3F;

	if (MC3XXX_RESOLUTION_LOW == s_bResolution)
	_baDataBuf[0] = 0x32;

	if ((MC3XXX_PCODE_3510B == s_bPCODE) || (MC3XXX_PCODE_3510C == (s_bPCODE&0xF1)))
		_baDataBuf[0] = 0x25;
	else if ((MC3XXX_PCODE_3530B == s_bPCODE) || (MC3XXX_PCODE_3530C == (s_bPCODE&0xF1)))
		_baDataBuf[0] = 0x02;

	i2c_smbus_write_byte_data(pt_i2c_client, MC3XXX_REG_RANGE_CONTROL, _baDataBuf[0]);

	printk("[%s] set resolution 0x%X\n", __FUNCTION__, _baDataBuf[0]);
}

/*****************************************
 *** MC3XXX_SetGain
 *****************************************/
static void MC3XXX_SetGain(void)
{
	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 1024;

	if (MC3XXX_RESOLUTION_LOW == s_bResolution){
		gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 86;

		if ((MC3XXX_PCODE_3530B == s_bPCODE) || (MC3XXX_PCODE_3530C == (s_bPCODE&0xF1))){
            			gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 64;
        		}
	}
    
	printk("[%s] gain: %d / %d / %d\n", __FUNCTION__, gsensor_gain.x, gsensor_gain.y, gsensor_gain.z);
}

/*****************************************
 *** MC3XXX_reset
 *****************************************/
 #if 0
static void MC3XXX_reset(struct i2c_client *client)
{
	unsigned char    _baBuf[2] = { 0 };
	int ret = 0;

	printk("===========entry MC3XXX_reset\n");
	_baBuf[0] = i2c_smbus_read_byte_data(client, 0x03);
	printk("  0x03 Status Register value = 0x%x\n",_baBuf[0]);
	_baBuf[0] = i2c_smbus_read_byte_data(client, 0x04);
	printk("  0x04 Operational State Status value = 0x%x\n",_baBuf[0]);


	ret = i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, 0x43);

	printk("===========entry MC3XXX_reset======= write 07 reg ret = %d\n", ret);
	
	_baBuf[0] = i2c_smbus_read_byte_data(client, 0x04);
	printk("entry MC3XXX_reset _baBuf[0] = 0x%x\n",_baBuf[0]);

	if (0x00 == (_baBuf[0] & 0x40))
	{
//	    _baBuf[0] = 0x6D; 
//	    i2c_smbus_write_byte_data(client, 0x1B, _baBuf[0]);
	    
//	    _baBuf[0] = 0x43; 
//	    i2c_smbus_write_byte_data(client, 0x1B, _baBuf[0]);
	}

	_baBuf[0] = 0x43;
	i2c_smbus_write_byte_data(client, 0x07, _baBuf[0]);//0x07 Mode Register

//	_baBuf[0] = 0x80;
//	i2c_smbus_write_byte_data(client, 0x1C, _baBuf[0]);	//0x09-0x1f is not use, said spec

//	_baBuf[0] = 0x80;
//	i2c_smbus_write_byte_data(client, 0x17, _baBuf[0]);	

//	msleep(5);

//	_baBuf[0] = 0x00;
//	i2c_smbus_write_byte_data(client, 0x1C, _baBuf[0]);	

//	_baBuf[0] = 0x00;
//	i2c_smbus_write_byte_data(client, 0x17, _baBuf[0]);	

//	msleep(5);

	//i2c_smbus_read_i2c_block_data(client, 0x21, 6, offset_buf);

	_baBuf[0] = i2c_smbus_read_byte_data(client, 0x04);//0x04 Status Regiser
	printk("==========entry MC3XXX_reset _baBuf[0] = 0x%x, the second time\n",_baBuf[0]);

	if (_baBuf[0] & 0x40)
	{
//	    _baBuf[0] = 0x6D; 
//	    i2c_smbus_write_byte_data(client, 0x1B, _baBuf[0]);
	    
//	    _baBuf[0] = 0x43; 
//	    i2c_smbus_write_byte_data(client, 0x1B, _baBuf[0]);
	}

	i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, 0x41);//0x07 Mode Register, 0x41 = wake

	printk("[%s]\n", __FUNCTION__);

}
#endif

/*****************************************
 *** MC3XXX_Init
 *****************************************/
static int MC3XXX_Init(struct i2c_client *client)
{
	struct mc3xxx_data *pdata = i2c_get_clientdata(client);
	unsigned char    _baDataBuf[2] = { 0 };
	int val;

	i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, 0x40);//was 0x43, according to spec change to 0x40
	//0x43=0100 0011, last 11 was reserved by spec, standby mode should be 00, 0x40

	MC3XXX_SetResolution();

	_baDataBuf[0] = 0x00;

	if (IS_MC35XX())
	    _baDataBuf[0] = 0x0A;

	i2c_smbus_write_byte_data(client, MC3XXX_REG_SAMPLE_RATE, _baDataBuf[0]);

//	_baDataBuf[0] = 0x00;
//	i2c_smbus_write_byte_data(client, MC3XXX_REG_TAP_DETECTION_ENABLE, _baDataBuf[0]);//0x09 register reserved

	_baDataBuf[0] = 0x00;
	i2c_smbus_write_byte_data(client, MC3XXX_REG_INTERRUPT_ENABLE, _baDataBuf[0]);

	MC3XXX_ConfigRegRange(client);

	MC3XXX_SetGain();

	i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, 0x41);

	val = i2c_smbus_read_byte_data(client, 0x04);
	printk("=====%s====,wanna wake the chip ,result is 0x%x  (81?) \n", __func__, val);

	msleep(220);

	pdata->active = MMA_ACTIVED;	//MMA_STANDBY

	printk("[%s]\n", __FUNCTION__);

	return (MC3XXX_RETCODE_SUCCESS);
}

static int mc3xxx_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int result, chip_id;
	struct input_dev *idev;
	struct mc3xxx_data *pdata;
	struct mc3xxx_platform_data *plat_data;
	struct i2c_adapter *adapter;
	struct input_polled_dev *poll_dev;
	printk("mc3xxx_probe\n");
	adapter = to_i2c_adapter(client->dev.parent);
	result = i2c_check_functionality(adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if (!result)
		goto err_out;
	printk("mc3xxx_probe 10111111\n");
	chip_id = i2c_smbus_read_byte_data(client, MC3XXX_REG_PRODUCT_CODE);

	if ((chip_id&0xF1) != MC3XXX_PCODE_3530C ) {
		dev_err(&client->dev,
			"read chip_id = 0x%x , \n", chip_id);
		goto err_out;
	}
	s_bPCODE = chip_id;
	printk("mc3xxx s_bPCODE = 0x%x \n",chip_id);

	
	pdata = kzalloc(sizeof(struct mc3xxx_data), GFP_KERNEL);
	if(!pdata){
		result = -ENOMEM;
		dev_err(&client->dev, "alloc data memory error!\n");
		goto err_out;
	}
	plat_data = client->dev.platform_data;
	/* Initialize the mc3xxx chip */
	pdata->client = client;
	pdata->chip_id = chip_id;
	pdata->mode = plat_data->mode;
	pdata->position = plat_data->position;//CONFIG_SENSORS_MMA_POSITION;
	mutex_init(&pdata->data_lock);
	i2c_set_clientdata(client,pdata);
	
//	MC3XXX_reset(client);
	
	if((MC3XXX_Init(client)!=MC3XXX_RETCODE_SUCCESS))
	{
		dev_err(&client->dev, "MC3XXX_Init error!\n");
		goto err_out;
	}
	
	poll_dev = input_allocate_polled_device();
	if (!poll_dev) {
		result = -ENOMEM;
		dev_err(&client->dev, "alloc poll device failed!\n");
		goto err_alloc_poll_device;
	}
	poll_dev->poll = mc3xxx_dev_poll;
	poll_dev->poll_interval = POLL_STOP_TIME;
	poll_dev->poll_interval_min = POLL_INTERVAL_MIN;
	poll_dev->poll_interval_max = POLL_INTERVAL_MAX;
	poll_dev->private = pdata;
	idev = poll_dev->input;
	idev->name = "acc_mcube";
	idev->uniq = mc3xxx_id2name(pdata->chip_id);
	idev->id.bustype = BUS_I2C;
	idev->evbit[0] = BIT_MASK(EV_ABS);
	input_set_abs_params(idev, ABS_X, -0x7fff, 0x7fff, 0, 0);
	input_set_abs_params(idev, ABS_Y, -0x7fff, 0x7fff, 0, 0);
	input_set_abs_params(idev, ABS_Z, -0x7fff, 0x7fff, 0, 0);
	pdata->poll_dev = poll_dev;
	result = input_register_polled_device(pdata->poll_dev);
	if (result) {
		dev_err(&client->dev, "register poll device failed!\n");
		goto err_register_polled_device;
	}
	result = sysfs_create_group(&idev->dev.kobj, &mc3xxx_attr_group);
	if (result) {
		dev_err(&client->dev, "create device file failed!\n");
		result = -EINVAL;
		goto err_create_sysfs;
	}
	printk("mc3xxx device driver probe successfully\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
		pdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		pdata->early_suspend.suspend = mc3xxx_suspend;
		pdata->early_suspend.resume = mc3xxx_resume;
		register_early_suspend(&pdata->early_suspend);
#endif
	return 0;
err_create_sysfs:
	input_unregister_polled_device(pdata->poll_dev);
err_register_polled_device:
	input_free_polled_device(poll_dev);
err_alloc_poll_device:
	kfree(pdata);
err_out:
	return result;
}

static int __exit mc3xxx_remove(struct i2c_client *client)
{
	struct mc3xxx_data *pdata = i2c_get_clientdata(client);
	struct input_polled_dev *poll_dev = pdata->poll_dev;
	mc3xxx_device_stop(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&pdata->early_suspend);
#endif
	if(pdata){
		input_unregister_polled_device(poll_dev);
		input_free_polled_device(poll_dev);
		kfree(pdata);
	}
	return 0;
}


static const struct i2c_device_id mc3xxx_id[] = {
	{"acc_mc3xxx", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, mc3xxx_id);

//static SIMPLE_DEV_PM_OPS(mc3xxx_pm_ops, mc3xxx_suspend, mc3xxx_resume);
static struct i2c_driver mc3xxx_driver = {
	.driver = {
		   .name = "mc3xxx",
		   .owner = THIS_MODULE,
//		   .pm = &mc3xxx_pm_ops,
		   },
	.probe = mc3xxx_probe,
	.remove = mc3xxx_remove,
	.id_table = mc3xxx_id,
};

static int __init mc3xxx_init(void)
{
	printk(" %s\n",__func__);
	/* register driver */
	int res;
	res = i2c_add_driver(&mc3xxx_driver);
	if (res < 0) {
		printk(KERN_INFO "add mc3xxx i2c driver failed\n");
		return -ENODEV;
	}
	return res;
}

static void __exit mc3xxx_exit(void)
{
	i2c_del_driver(&mc3xxx_driver);
}

MODULE_AUTHOR("MCube, Inc.");
MODULE_DESCRIPTION("mc3xxx 3-Axis Orientation/Motion Detection Sensor driver");
MODULE_LICENSE("GPL");

module_init(mc3xxx_init);
module_exit(mc3xxx_exit);
