/*
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2009  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the codec driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	 1.0.0		2012-02-04	tangyong	created
 *
 */

#include <plat/comip_codecs_interface.h>


struct snd_soc_codec_driver *psoc_codec_dev;
struct snd_soc_dai_driver *psoc_dai_dev;
struct comip_codec_ops *pcomip_codec_ops_dev;
struct comip_codec_interface_platform_data *pintdata;

/******************************************************************************
*  Private Function Declare
******************************************************************************/
static const struct codec_core_func_t codec_core_func_TBL[]= {
	 {MIXER_OUTPUT_PATH,codec_set_output_path}
	,{MIXER_OUTPUT_VOLUME,codec_set_output_volume}
	,{MIXER_INPUT_VOLUME,codec_set_input_gain}
	,{MIXER_OUTPUT_SAMPLERATE,codec_set_output_samplerate}
	,{MIXER_INPUT_SAMPLERATE,codec_set_input_samplerate}
	,{MIXER_INPUT_MUTE,codec_set_input_mute}
	,{MIXER_OUTPUT_MUTE,codec_set_output_mute}
	,{MIXER_INPUT_PATH,codec_set_input_path}
	,{MIXER_ENABLE_VOICE,codec_enable_voice}
	,{MIXER_ENABLE_FM,codec_enable_fm}
	,{MIXER_MP_POWER_SAVE,codec_mp_powersave}
	,{MIXER_DOUBLE_MICS,codec_set_double_mics_mode}
	,{MIXER_ENABLE_ALC, codec_enable_alc}
};

int codec_set_output_path(int nType, int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->set_output_path))
		return 0;
	if((VOICE_TYPE == nType)&& (DD_BLUETOOTH == nValue)) {
		codec_enable_bluetooth(VOICE_TYPE,1);
	}else {
		if(VOICE_TYPE == nType)
			codec_enable_bluetooth(VOICE_TYPE,0);
	}

	return pcomip_codec_ops_dev->set_output_path(nType, nValue);
}
int codec_set_input_path(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->set_input_path))
		return 0;
	return pcomip_codec_ops_dev->set_input_path(nType,nValue);
}

int codec_set_output_volume(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->set_output_volume))
		return 0;
	return pcomip_codec_ops_dev->set_output_volume(nType,nValue);
}

int codec_set_input_gain(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->set_input_gain))
		return 0;

	return pcomip_codec_ops_dev->set_input_gain(nType,nValue);
}

int codec_set_output_mute(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->set_output_mute))
		return 0;
	return pcomip_codec_ops_dev->set_output_mute(nType,nValue);
}

int codec_set_input_mute(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->set_input_mute))
		return 0;
	return pcomip_codec_ops_dev->set_input_mute(nType,nValue);
}

int codec_set_output_samplerate(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->set_output_samplerate))
		return 0;
	return pcomip_codec_ops_dev->set_output_samplerate(nValue);
}

int codec_set_input_samplerate(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->set_input_samplerate))
		return 0;
	return pcomip_codec_ops_dev->set_input_samplerate(nValue);
}

int codec_enable_voice(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->enable_voice))
		return 0;
	if(nValue == 1) {
		pintdata->pcm_switch(1);
		raw_notifier_call_chain(&call_notifier_list, VOICE_CALL_START,NULL);
	} else {
		pintdata->pcm_switch(0);
		raw_notifier_call_chain(&call_notifier_list, VOICE_CALL_STOP,NULL);
	}
	return pcomip_codec_ops_dev->enable_voice(nValue);
}

int codec_enable_fm(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->enable_fm))
		return 0;
	if(nValue){
		raw_notifier_call_chain(&fm_notifier_list, FM_START,NULL);
	}else{
		raw_notifier_call_chain(&fm_notifier_list, FM_STOP,NULL);
	}

	return pcomip_codec_ops_dev->enable_fm(nValue);
}

int codec_enable_bluetooth(int nType,int nValue)
{
	if(pintdata->pcm_switch) {
		if(nValue == 1)
			pintdata->pcm_switch(0);
		else
			pintdata->pcm_switch(1);
	}
	return 0;
}
int codec_mp_powersave(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->mp_powersave))
		return 0;
	return pcomip_codec_ops_dev->mp_powersave(nType,nValue);
}
int codec_fm_out_gain(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->fm_out_gain))
		return 0;
	return pcomip_codec_ops_dev->fm_out_gain(nValue);
}

int codec_set_double_mics_mode(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->set_double_mics_mode))
		return 0;
	return pcomip_codec_ops_dev->set_double_mics_mode(nValue);
}

int codec_enable_alc(int nType,int nValue)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->enable_alc))
		return 0;
	return pcomip_codec_ops_dev->enable_alc(nValue);
}

int snd_ctl_leadcore_mixer(struct snd_ctl_file *file,
			struct snd_leadcore_mixer __user *_control)
{
	struct snd_leadcore_mixer *info;
	int result=0;
	int i;
	info = memdup_user(_control, sizeof(*info));

	if (IS_ERR(info))
		return PTR_ERR(info);

	for (i=0; i<sizeof(codec_core_func_TBL)/sizeof(codec_core_func_TBL[0]); i++) {
		if((info->nCmd == codec_core_func_TBL[i].szFuncName)){
			printk("snd_ctl_leadcore_mixer: cmd:%x,nType=%d,nValue=%d,i=%d\n",info->nCmd,info->nType,info->nValue,i);
			result = codec_core_func_TBL[i].pfunc(info->nType,info->nValue);
		}
	}
	kfree(info);
	printk(KERN_DEBUG "CTL_MIXER: END\n");
	return result;
}

int snd_ctl_leadcore_get_info(struct snd_ctl_file *file,
			struct snd_leadcore_mixer_info* _userInfo)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->codec_get_mixer_info))
		return 0;
	
	printk(KERN_ERR "snd_ctl_leadcore_get_info: END\n");
	//return 0;
	return pcomip_codec_ops_dev->codec_get_mixer_info(_userInfo);
}

static int snd_control_ioctl_leadcore(struct snd_card *card,
				   struct snd_ctl_file * control,
				   unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case SNDRV_CTL_IOCTL_LEADCORE_WRITE:
			return snd_ctl_leadcore_mixer(control, (struct snd_leadcore_mixer __user *)arg);
	case SNDRV_CTL_IOCTL_LEADCORE_READ:
			return snd_ctl_leadcore_get_info(control, (struct snd_leadcore_mixer_info*)arg);
	}
	return -ENOIOCTLCMD;
}


int leadcore_register_codec_info(struct snd_soc_codec_driver *soc_codec_dev,
				struct snd_soc_dai_driver  *soc_dai_dev, struct comip_codec_ops *comip_codec_ops_dev)
{
	int nRst=0;
	psoc_dai_dev = soc_dai_dev;
	pcomip_codec_ops_dev = comip_codec_ops_dev;
	psoc_codec_dev = soc_codec_dev;
	return nRst;
}
EXPORT_SYMBOL_GPL(leadcore_register_codec_info);

static ssize_t comip_register_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->comip_register_show))
		return 0;
	return pcomip_codec_ops_dev->comip_register_show(buf);
}

static ssize_t comip_register_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->comip_register_store))
		return 0;
	return pcomip_codec_ops_dev->comip_register_store(buf,size);
}

static ssize_t codec_calibrate_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->calibrate_data_show))
		return 0;
	return pcomip_codec_ops_dev->calibrate_data_show(buf);
}

static ssize_t codec_calibrate_data_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->calibrate_data_store))
		return 0;
	return pcomip_codec_ops_dev->calibrate_data_store(buf,size);
}

#ifdef CONFIG_PROC_FS
#ifdef CODEC_SOC_PROC
static ssize_t codec_proc_soc_read(struct file *file, char __user *buffer,
                        size_t count, loff_t *ppos)
{

	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->codec_proc_read))
		return 0;
	return pcomip_codec_ops_dev->codec_proc_read(buffer, count, ppos);

}

static int codec_proc_soc_write(struct file *file, const char __user *buffer,
                        size_t count, loff_t *ppos)
{

	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->codec_proc_write))
		return 0;
	return pcomip_codec_ops_dev->codec_proc_write(buffer, count);

}

static const struct file_operations codec_operations = {
        .owner = THIS_MODULE,
        .read  = codec_proc_soc_read,
        .write = codec_proc_soc_write,
};

static ssize_t codec_audio_proc_soc_read(struct file *file, char __user *buffer,
                        size_t count, loff_t *ppos)
{

	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->codec_audio_proc_read))
		return 0;
	return pcomip_codec_ops_dev->codec_audio_proc_read(buffer, count, ppos);
}

static int codec_audio_proc_soc_write(struct file *file, const char __user *buffer,
                        size_t count, loff_t *ppos)
{

	if((NULL == pcomip_codec_ops_dev)||(NULL == pcomip_codec_ops_dev->codec_audio_proc_write))
		return 0;
	return pcomip_codec_ops_dev->codec_audio_proc_write(buffer, count);
}

static const struct file_operations codec_audio_operations = {
        .owner = THIS_MODULE,
        .read  = codec_audio_proc_soc_read,
        .write = codec_audio_proc_soc_write,
};

static int codec_audio_eq_proc_soc_read(struct file *file, char __user *buffer,
                        size_t count, loff_t *ppos)
{
	if(NULL==pcomip_codec_ops_dev||NULL==pcomip_codec_ops_dev->codec_eq_proc_read)
		return 0;
	return pcomip_codec_ops_dev->codec_eq_proc_read(buffer, count, ppos);
}

static int codec_audio_eq_proc_soc_write(struct file *file, const char __user *buffer,
                        size_t count, loff_t *ppos)
{
	if(NULL==pcomip_codec_ops_dev||NULL==pcomip_codec_ops_dev->codec_eq_proc_write)
		return 0;
	return pcomip_codec_ops_dev->codec_eq_proc_write(buffer, count);
}

static const struct file_operations codec_audio_eq_operations = {
        .owner = THIS_MODULE,
        .read  = codec_audio_eq_proc_soc_read,
        .write = codec_audio_eq_proc_soc_write,
};

#endif
#endif

static DEVICE_ATTR(code_debug_register_set, 0644, comip_register_show, comip_register_store);
static DEVICE_ATTR(code_calibrate_data_set, 0644, codec_calibrate_data_show, codec_calibrate_data_store);

static int codec_voice_calibrate_probe(struct platform_device *pdev)
{
	int ret;
	printk(KERN_INFO "codec_voice_calibrate_probe \n");
	ret = device_create_file(&pdev->dev, &dev_attr_code_debug_register_set);
	if(ret != 0)
		printk(KERN_ERR "dev_attr_code_debug_register_set failed \n");
	ret = device_create_file(&pdev->dev, &dev_attr_code_calibrate_data_set);
	if(ret != 0)
		printk(KERN_ERR "dev_attr_code_calibrate_data_set failed \n");
	return ret;
}

static int codec_voice_calibrate_remove(struct platform_device *pdev)
{
	printk(KERN_INFO "codec_voice_calibrate_remove \n");
	device_remove_file(&pdev->dev, &dev_attr_code_debug_register_set);
	device_remove_file(&pdev->dev, &dev_attr_code_calibrate_data_set);

	return 0;
}

struct platform_driver codec_voice_calibrate_driver = {
	.probe  = codec_voice_calibrate_probe,
	.remove = codec_voice_calibrate_remove,
	.driver = {
		.name  = "codec_voice_calibration",
		.owner = THIS_MODULE,
	},
};

static struct platform_device codec_voice_calibrate_device = {
	.name = "codec_voice_calibration",
	.id = -1,
};

static int __init comip_codec_interface_probe(struct platform_device *pdev)
{
	struct comip_codec_interface_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;
#ifdef CODEC_SOC_PROC
	struct proc_dir_entry *codec_proc_entry;
	struct proc_dir_entry *codec_audio_proc_entry;
	struct proc_dir_entry *codec_audio_eq_proc_entry;
#endif

	printk(KERN_INFO "comip_codec_interface_probe \n");
	if (!pdata)
		return -EBUSY;
	pintdata = pdata;

#ifdef CONFIG_PROC_FS
#ifdef CODEC_SOC_PROC
	codec_proc_entry = proc_create_data("driver/codec", 0, NULL,
					&codec_operations, NULL);
	codec_audio_proc_entry = proc_create_data("driver/codec_audio", 0, NULL,
					&codec_audio_operations, NULL);
	codec_audio_eq_proc_entry = proc_create_data("driver/codec_eq", 0, NULL,
					&codec_audio_eq_operations, NULL);
#endif
#endif

	ret = platform_device_register(&codec_voice_calibrate_device);
	if (ret) {
		printk(KERN_ERR "comip_codec_interface_probe: register codec voice calibrate device failed \n");
		return ret;
	}
	snd_ctl_register_ioctl(snd_control_ioctl_leadcore);

	return snd_soc_register_codec(&pdev->dev,psoc_codec_dev, psoc_dai_dev, 1);
}

static int __exit comip_codec_interface_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	snd_ctl_unregister_ioctl(snd_control_ioctl_leadcore);
	return 0;
}

static struct platform_driver comip_codec_interface_driver = {
	.driver = {
		.name = "comip_codec",
		.owner = THIS_MODULE,
	},
	.probe = comip_codec_interface_probe,
	.remove = __exit_p(comip_codec_interface_remove),
};

static int __init comip_codec_interface_init(void)
{
	int nRst=0;
	nRst = platform_driver_register(&codec_voice_calibrate_driver);
	if(nRst != 0) {
		printk(KERN_ERR "Reg codec_voice_calibrate_driver failed\n");
	}
	return platform_driver_register(&comip_codec_interface_driver);
}


static void __exit comip_codec_interface_exit(void)
{
	platform_driver_unregister(&codec_voice_calibrate_driver);
	platform_driver_unregister(&comip_codec_interface_driver);
}

module_init(comip_codec_interface_init);
module_exit(comip_codec_interface_exit);

MODULE_AUTHOR("Peter Tang <tangyong@leadcoretech.com>");
MODULE_DESCRIPTION("comip codec driver");
MODULE_LICENSE("GPL");
