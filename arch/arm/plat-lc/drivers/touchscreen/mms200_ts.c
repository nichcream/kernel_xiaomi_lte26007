/*
 * MELFAS MMS200 Touchscreen Driver
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 */

#include "mms200_ts.h"

static int esd_cnt;
static void mms_ts_early_suspend(struct early_suspend *h);
static void mms_ts_late_resume(struct early_suspend *h);
static int mms_ts_config(struct mms_ts_info *info);


/* mms_ts_enable - wake-up func (VDD on)  */
static void mms_ts_enable(struct mms_ts_info *info)
{
	if (info->enabled)
		return;

	mutex_lock(&info->lock);
	gpio_request(info->pdata->gpio_vdd_en,"mms200 Reset");
	gpio_direction_output(info->pdata->gpio_vdd_en, 1);
//	info->pdata->power_ic(&info->client->dev, 1);
	msleep(50);

	info->enabled = true;
	enable_irq(info->irq);

	mutex_unlock(&info->lock);

}

/* mms_ts_disable - sleep func (VDD off) */
static void mms_ts_disable(struct mms_ts_info *info)
{
	if (!info->enabled)
		return;

	mutex_lock(&info->lock);

	disable_irq(info->irq);

	msleep(50);
	gpio_request(info->pdata->gpio_vdd_en,"mms200 Reset");
	gpio_direction_output(info->pdata->gpio_vdd_en, 0);
//	info->pdata->power_ic(&info->client->dev, 0);
	msleep(100);

	info->enabled = false;

	mutex_unlock(&info->lock);
}

/* mms_reboot - IC reset */
//static void mms_reboot(struct mms_ts_info *info)
void mms_reboot(struct mms_ts_info *info)
{
#if 0
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);

	i2c_lock_adapter(adapter);

	gpio_request(info->pdata->gpio_vdd_en, "mms200 Reset");
	msleep(50);
	gpio_direction_output(info->pdata->gpio_vdd_en, 0);
	msleep(100);

	gpio_direction_output(info->pdata->gpio_vdd_en, 1);
	msleep(50);

	i2c_unlock_adapter(adapter);
#else
	info->pdata->power_ic(&info->client->dev, 0);
	msleep(20);
	info->pdata->power_ic(&info->client->dev, 1);
#endif
}

/*
 * mms_clear_input_data - all finger point release
 */
void mms_clear_input_data(struct mms_ts_info *info)
{
	int i;

	for (i = 0; i < MAX_FINGER_NUM; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, false);
	}
	input_sync(info->input_dev);

	return;
}

/* mms_report_input_data - The position of a touch send to platfrom  */
void mms_report_input_data(struct mms_ts_info *info, u8 sz, u8 *buf)
{
	int i;
	struct i2c_client *client = info->client;
	int id;
	int x;
	int y;
	int touch_major;
	int pressure;
	int key_code;
	int key_state;
	u8 *tmp;

	printk("mms_report_input_data Start\n");


	if (buf[0] == MMS_NOTIFY_EVENT) {
		dev_info(&client->dev, "TSP mode changed (%d)\n", buf[1]);
		goto out;
	} else if (buf[0] == MMS_ERROR_EVENT) {
		dev_info(&client->dev, "Error detected, restarting TSP\n");
		mms_clear_input_data(info);
		mms_reboot(info);
		esd_cnt++;
		if (esd_cnt>= ESD_DETECT_COUNT)
		{
			i2c_smbus_write_byte_data(info->client, MMS_MODE_CONTROL, 0x04);
			esd_cnt =0;
		}
		goto out;
	}

	for (i = 0; i < sz; i += FINGER_EVENT_SZ) {
		tmp = buf + i;
//		printk("mms_ts sz = %d tmp0=%x,tmp1=%x,tmp2=%x,tmp3=%x,tmp4=%x,tmp5=%x\n",sz,tmp[0],tmp[1],tmp[2],tmp[3],tmp[4],tmp[5]);
		esd_cnt =0;
		if (tmp[0] & MMS_TOUCH_KEY_EVENT) {
			switch (tmp[0] & 0xf) {
			case 1:
				key_code = KEY_MENU;
				break;
			case 2:
				key_code = KEY_BACK;
				break;
			default:
				dev_err(&client->dev, "unknown key type\n");
				goto out;
				break;
			}

			key_state = (tmp[0] & 0x80) ? 1 : 0;
			input_report_key(info->input_dev, key_code, key_state);

		} else {
			id = (tmp[0] & 0xf) -1;

			x = tmp[2] | ((tmp[1] & 0xf) << 8);
			y = tmp[3] | (((tmp[1] >> 4 ) & 0xf) << 8);
			touch_major = tmp[4];
			pressure = (tmp[5]>>1)+10;


			printk("MMS_TOUCH_EVENT X=%d,Y=%d,id=%d \n",x,y,id);

//			input_mt_slot(info->input_dev, id);

			if (!(tmp[0] & 0x80)) {
//				input_report_abs(info->input_dev, ABS_MT_PRESSURE,0);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, false);
				pressure =0;
//				continue;
			} else {

				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, true);
			}

			input_report_abs(info->input_dev, ABS_MT_POSITION_X, 720-x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, 1280-y);
			input_report_abs(info->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, id);
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
//			input_mt_sync(info->input_dev);
			input_report_key(info->input_dev, BTN_TOUCH,1);
		}
		input_mt_sync(info->input_dev);
	}

	input_sync(info->input_dev);

out:
	return;

}

/* mms_ts_interrupt - interrupt thread */
static irqreturn_t mms_ts_interrupt(int irq, void *dev_id)
{
	struct mms_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 buf[MAX_FINGER_NUM * FINGER_EVENT_SZ] = { 0, };
	int ret = 0;
	int error = 0;
	int sz;
	u8 reg = MMS_INPUT_EVENT;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = &reg,
			.len = 1,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = buf,
		},
	};

	printk("mms_ts_interrupt Start\n");
	//Read event packet size

	sz = i2c_smbus_read_byte_data(client, MMS_EVENT_PKT_SZ);

	//Check read error
	if(sz < 0){
		dev_err(&client->dev, "ERROR : failed to read event packet size - error code [%d]\n", sz);
		error = sz;
		sz = FINGER_EVENT_SZ;
	}
	//Check packet size
	if (sz > sizeof(buf)) {
		dev_err(&client->dev, "ERROR : event buffer overflow - buffer [%d], packet [%d]\n", sizeof(buf), sz);
		error = -1;
		sz = FINGER_EVENT_SZ;
	}

	//Read event packet
	msg[1].len = sz;
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));

	//Check event packet size
	if(error < 0){
		//skip
	}
	else if (ret != ARRAY_SIZE(msg)) {
		dev_err(&client->dev, "ERROR : failed to read event packet - size [%d], error code [%d])\n", sz, ret);
	}
	else {
		mms_report_input_data(info, sz, buf);
	}

	return IRQ_HANDLED;
}

/*
 * mms_ts_input_open - Register input device after call this function
 * this function is wait firmware flash wait
 */
static int mms_ts_input_open(struct input_dev *dev)
{
	struct mms_ts_info *info = input_get_drvdata(dev);
	int ret = 1;

#if MMS_USE_INIT_DONE
	ret = wait_for_completion_interruptible_timeout(&info->init_done,
			msecs_to_jiffies(90 * MSEC_PER_SEC));
#endif

	if (ret > 0) {
		if (info->irq != -1) {
			mms_ts_enable(info);
			ret = 0;
		} else {
			ret = -ENXIO;
		}
	} else {
		dev_err(&dev->dev, "error while waiting for device to init\n");
		ret = -ENXIO;
	}

	return ret;
}

/*
 * mms_ts_input_close -If device power off state call this function
 */
static void mms_ts_input_close(struct input_dev *dev)
{
	struct mms_ts_info *info = input_get_drvdata(dev);

	mms_ts_disable(info);
}


/**
* Update firmware from kernel built-in binary
*/
int mms_fw_update_from_kernel(struct mms_ts_info *info)
{
	const char *fw_name = FW_NAME;
	const struct firmware *fw;
	int retires = 3;
	int ret;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	printk("fw_name = %s",fw_name);

	request_firmware(&fw, fw_name, &info->client->dev);

	if (!fw) {
		dev_err(&info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		goto ERROR;
	}

	do {
//		ret = mms_flash_fw(info, fw->data, fw->size, false, true);
		ret = mms_flash_fw(info,fw->data,fw->size,false);
		if(ret >= 0){
			break;
		}
	} while (--retires);

	if (!retires) {
		dev_err(&info->client->dev, "%s [ERROR] mms_flash_fw failed\n", __func__);
		goto ERROR;
	}

	release_firmware(fw);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	release_firmware(fw);

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}
#if 0
void mms_fw_update_controller(const struct firmware *fw, void * context)
//void mms_fw_update_controller(const struct firmware *fw, struct i2c_client *client)
{
	struct mms_ts_info *info = context;
	int retires = 3;
	int ret;

	printk("%s \n",__func__);

	if (!fw) {
		dev_err(&info->client->dev, "failed to read firmware\n");
		//printk(KERN_ERR"failed to read firmware!!\n");
#if MMS_USE_INIT_DONE
		complete_all(&info->init_done);
#endif
		return;
	}



	do {
		ret = mms_flash_fw(info,fw->data,fw->size,false);
//		ret = mms_flash_fw(info,fw->data,fw->size,false);
	} while (ret && --retires);

	if (!retires) {
		dev_err(&info->client->dev, "failed to flash firmware after retires\n");
	}
	release_firmware(fw);
}
#else
void mms_fw_update_controller(struct mms_ts_info *info, const struct firmware *fw, struct i2c_client *client)
{
	int retry = 3;
	int ret;

    printk("mms_fw_update_controller \n");

	do {
		ret = mms_flash_fw(info, fw->data, fw->size, false);
		if(ret >= 0){
			break;
		}
	} while (--retry);

	if (!retry) {
		printk("failed to flash firmware after retires\n");
	}
}
#endif
/*
 * mms_ts_config - f/w check download & irq thread register
 */
static int mms_ts_config(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	printk("mms_ts_config Start\n");

	//Set IRQ
	client->irq = gpio_to_irq(info->pdata->gpio_resetb);
	dev_info(&info->client->dev, "%s - gpio_to_irq [%d]\n", __func__, client->irq);

	/* Request GPIO and register IRQ. */

	if(gpio_request(info->pdata->gpio_resetb, "mms_ts")) {
		printk(KERN_ERR"mms_ts request GPIO error !!\n");
		goto out;
	}

	gpio_direction_input(info->pdata->gpio_resetb);

	ret = request_irq(client->irq,
	                  mms_ts_interrupt,
	                  IRQF_TRIGGER_LOW,
	                  "mms_ts",
	                  info);

	if (ret) {
		dev_err(&client->dev, "failed to register irq\n");
		goto out;
	}

	disable_irq(client->irq);
	info->irq = client->irq;
	barrier();

	info->tx_num = i2c_smbus_read_byte_data(client, MMS_TX_NUM);
	info->rx_num = i2c_smbus_read_byte_data(client, MMS_RX_NUM);
	info->key_num = i2c_smbus_read_byte_data(client, MMS_KEY_NUM);


	dev_info(&client->dev, "Melfas touch controller initialized\n");
	mms_reboot(info);
#if MMS_USE_INIT_DONE
	complete_all(&info->init_done);
#else
	enable_irq(client->irq);
#endif

out:
	return ret;
}

//melfas test mode


static ssize_t mms_force_update(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
        struct i2c_client *client = to_i2c_client(dev);
	struct file *fp;
	mm_segment_t old_fs;
	size_t fw_size, nread;
	int error = 0;
	int result = 0;
	disable_irq(client->irq);
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(EXTRA_FW_PATH, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(&info->client->dev,
		"%s: failed to open %s.\n", __func__, EXTRA_FW_PATH);
		error = -ENOENT;
		goto open_err;
	}
	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (0 < fw_size) {
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data,fw_size, &fp->f_pos);
		dev_info(&info->client->dev,
		"%s: start, file path %s, size %u Bytes\n", __func__,EXTRA_FW_PATH, fw_size);
		if (nread != fw_size) {
			    dev_err(&info->client->dev,
			    "%s: failed to read firmware file, nread %u Bytes\n", __func__, nread);
		    error = -EIO;
		} else{
			result=mms_flash_fw(info,fw_data,fw_size, true); // last argument is full update
		}
		kfree(fw_data);
	}
	filp_close(fp, current->files);
open_err:
	enable_irq(client->irq);
	set_fs(old_fs);
	return result;
}
static DEVICE_ATTR(fw_update, 0666, mms_force_update, NULL);

static struct attribute *mms_attrs[] = {
	&dev_attr_fw_update.attr,
	NULL,
};

static const struct attribute_group mms_attr_group = {
	.attrs = mms_attrs,
};

#if MMS_USE_DEVICETREE
static int mms_parse_dt(struct device *dev, struct mms_ts_platform_data *pdata)
{
	int rc;
	u32 temp_val;
	struct device_node *np = dev->of_node;

	//Read property
	rc = of_property_read_u32(np, "melfas,max_x", &temp_val);
	if (rc) {
		dev_err(dev, "Failed to read max_x\n");
		pdata->max_x = 1080;
	} else {
		pdata->max_x = temp_val;
	}

	rc = of_property_read_u32(np, "melfas,max_y", &temp_val);
	if (rc) {
		dev_err(dev, "Failed to read max_y\n");
		pdata->max_y = 1920;
	} else {
		pdata->max_y = temp_val;
	}

	rc = of_property_read_u32(np, "melfas,max_id", &temp_val);
	if (rc) {
		dev_err(dev, "Failed to read max_id\n");
		pdata->max_id = MAX_FINGER_NUM;
	} else {
		pdata->max_id = temp_val;
	}

	//Get GPIO
	pdata->gpio_resetb = of_get_named_gpio(np, "melfas,irq-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_resetb)) {
		dev_err(dev, "%s [ERROR] irq-gpio\n", __func__);
		return -1;
	}

	pdata->gpio_vdd_en = of_get_named_gpio(np, "melfas,vdd_en-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_vdd_en)) {
		dev_err(dev, "%s [ERROR] vdd_en-gpio\n", __func__);
		return -1;
	}

	//Config GPIO
	rc = gpio_request(pdata->gpio_resetb, "melfas,irq-gpio");
	if (rc < 0){
		dev_err(dev, "%s [ERROR] gpio_request\n", __func__);
		return rc;
	}

	rc = gpio_tlmm_config(GPIO_CFG(pdata->gpio_resetb, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if (rc < 0){
		dev_err(dev, "%s [ERROR] gpio_tlmm_config\n", __func__);
		return rc;
	}

	return 0;
}
#endif
/*
const u8 mms_fw_binary[]= {
	#include "mms_ts.h"
};
struct firmware mms_fw_name =
{
	.size=sizeof(mms_fw_binary),
	.data=&mms_fw_binary[0],
};

static mms_firmware_update(struct i2c_client *client)
{


//	void mms_fw_update_controller(const struct firmware *fw, void * context);
	mms_fw_update_controller(&mms_fw_name,client);
}
*/


static s8 melfas_i2c_test(struct i2c_client *client)
{
    u8 retry = 0;
    s8 ret = -1;


    while(retry++ < 100)
    {
	  ret = i2c_smbus_read_byte_data(client, MMS_TX_NUM);
        if (ret > 0)
        {

            return ret;
        }

        msleep(10);
    }
	printk("%s %d\n",__func__,ret);
    return ret;
}

const u8 MELFAS_lead_binary[] = {
#include "melfas_mms200.mfsb.h"
};

struct firmware melfas_fw =
{
	.size = sizeof(MELFAS_lead_binary),
	.data = &MELFAS_lead_binary[0],
};

static int melfas_fw_update(struct mms_ts_info *info, struct i2c_client *client)
{

	mms_fw_update_controller(info,&melfas_fw,client);

	return 1;
}

static int  mms_ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mms_ts_info *info;
	struct mms_ts_platform_data 	*pdata;
	struct input_dev *input_dev;
	int ret = 0;

	printk("begin mms_ts_probe\n");

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "info is NULL\n");
		return -EINVAL;
	}

	info->client = client;

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		dev_err(&client->dev, "pdata is NULL\n");
		return -EINVAL;
	}

	if(pdata->power_ic_flag)
		pdata->power_ic(&info->client->dev, 1);

	if(pdata->power_i2c_flag)
		pdata->power_i2c(&info->client->dev, 1);


	msleep(20);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocated memory\n");
		return -ENOMEM;
	}

	info->pdata = pdata;
	info->input_dev = input_dev;
#if MMS_USE_INIT_DONE
	init_completion(&info->init_done);
#endif
	info->irq = -1;

	mutex_init(&info->lock);

	snprintf(info->phys, sizeof(info->phys),
		"%s/input0", dev_name(&client->dev));

	input_dev->name = "MELFAS MMS200 Touchscreen";
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
#if MMS_USE_INIT_DONE
	input_dev->open = mms_ts_input_open;
	input_dev->close = mms_ts_input_close;
#endif

	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_PRESSURE, input_dev->absbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev,
	                     ABS_MT_POSITION_X, 0, info->pdata->max_scrx/*SCREEN_MAX_X*/, 0, 0);
	input_set_abs_params(input_dev,
	                     ABS_MT_POSITION_Y, 0,info->pdata->max_scry/* SCREEN_MAX_Y*/, 0, 0);
	input_set_abs_params(input_dev,
	                     ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(input_dev,
	                     ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_abs_params(input_dev,
	                     ABS_MT_TRACKING_ID, 0, 5, 0, 0);

	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);

#if MMS_HAS_TOUCH_KEY
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(KEY_MENU, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);
#endif

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "failed to register input dev\n");
		return -EIO;
	}
	input_set_drvdata(input_dev, info);
	i2c_set_clientdata(client, info);

	mms_reboot(info);

	msleep(100);

/*fw download*/
#ifdef CONFIG_FW_AUTO_UPDATE
	ret = melfas_fw_update(info,client);
	if(ret){
		dev_err(&client->dev, "%s [ERROR] mms_fw_update_from_kernel\n", __func__);
		printk("%s [ERROR] mms_fw_update_from_kernel\n", __func__);
	}
#endif
	mms_ts_config(info);

	esd_cnt = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = mms_ts_early_suspend;
	info->early_suspend.resume = mms_ts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

#ifdef __MMS_LOG_MODE__
	if(mms_ts_log(info)){
		dev_err(&client->dev, "failed to create Log mode.\n");
		return -EAGAIN;
	}

	info->class = class_create(THIS_MODULE, "mms_ts");
	device_create(info->class, NULL, info->mms_dev, NULL, "mms_ts");
#endif

#ifdef __MMS_TEST_MODE__
	if (mms_sysfs_test_mode(info)){
		dev_err(&client->dev, "failed to create sysfs test mode\n");
		return -EAGAIN;
	}

#endif
	if (sysfs_create_group(&client->dev.kobj, &mms_attr_group)) {
		dev_err(&client->dev, "failed to create sysfs group\n");
		return -EAGAIN;
	}

	if (sysfs_create_link(NULL, &client->dev.kobj, "mms_ts")) {
		dev_err(&client->dev, "failed to create sysfs symlink\n");
		return -EAGAIN;
	}


	dev_notice(&client->dev, "mms dev initialized\n");
	printk("end mms_ts_probe\n");

	return 0;
}

static int __exit mms_ts_remove(struct i2c_client *client)
{
	struct mms_ts_info *info = i2c_get_clientdata(client);

	if (info->irq >= 0)
		free_irq(info->irq, info);
#ifdef __MMS_TEST_MODE__
	sysfs_remove_group(&info->client->dev.kobj, &mms_attr_group);
	mms_sysfs_remove(info);
	sysfs_remove_link(NULL, "mms_ts");
	kfree(info->get_data);
#endif

#ifdef __MMS_LOG_MODE__
	device_destroy(info->class, info->mms_dev);
	class_destroy(info->class);
#endif

	input_unregister_device(info->input_dev);
	unregister_early_suspend(&info->early_suspend);
	kfree(info->fw_name);
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
static int mms_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users) {
		mms_ts_disable(info);
		mms_clear_input_data(info);
	}

	mutex_unlock(&info->input_dev->mutex);
	return 0;

}

static int mms_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users)
		mms_ts_enable(info);

	mutex_unlock(&info->input_dev->mutex);

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_suspend(&info->client->dev);
}

static void mms_ts_late_resume(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_resume(&info->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops mms_ts_pm_ops = {
	.suspend	= mms_ts_suspend,
	.resume		= mms_ts_resume,
}
#endif

#if MMS_USE_DEVICETREE
static struct of_device_id mms_match_table[] = {
	{ .compatible = "melfas,mms_ts",},
	{ },
};
#endif

static const struct i2c_device_id mms_ts_id[] = {
	{"mms200", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, mms_ts_id);

static struct i2c_driver mms_ts_driver = {
	.probe		= mms_ts_probe,
	.remove		= __exit_p(mms_ts_remove),
	.driver		= {
				.name	= "mms200",

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
				.pm	= &mms_ts_pm_ops,
#endif
	},
	.id_table	= mms_ts_id,
};

static int __init mms_ts_init(void)
{
	return i2c_add_driver(&mms_ts_driver);
}

static void __exit mms_ts_exit(void)
{
	return i2c_del_driver(&mms_ts_driver);
}

module_init(mms_ts_init);
module_exit(mms_ts_exit);

MODULE_VERSION("2014.11.14");
MODULE_DESCRIPTION("MELFAS MMS200 Touchscreen Driver");
MODULE_LICENSE("GPL");

