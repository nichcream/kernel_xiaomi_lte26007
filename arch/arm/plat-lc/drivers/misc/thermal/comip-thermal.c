/* 
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: temperature monitor for dbb/battery
 *
 *	CHANGE HISTORY:
 *
 *	Version Date		Author		Description
 *	1.0.0	2013-06-29	xuxuefeng 	created
 *	1.0.1	2013-12-06	xuxuefeng 	updated
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <plat/comip-battery.h>
#include <plat/comip-thermal.h>
#include <linux/notifier.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <plat/mets.h>

/*
 * Tracing flags.
 */
#define MONITOR_TRACE_INFO		0x00000001
#define MONITOR_TRACE_DEBUG		0x00000002
#define MONITOR_TRACE_ERROR		0x00000004

#define TEMP_SMOOTH_ARRAY_NUM		   (10)
#define TEMP_SMOOTH_ABANDON_MULTIPLE   (2)
#define TEMP_LEVEL_CHANGE_DIF		   (5)
#define TEMP_FEEDBACK_BOOTOM		   (60)
#define TEMP_FEEDBACK_TOP		       (100)
#define TEMP_FEEDBACK_DEFAULT		   (65)
#define TEMP_DEFAULT_HEAD_LEN		   (4)

#define THERMAL_LEN			(30)

#define thermal_trace(msk, fmt, ...) do { \
	if (monitor_trace_mask & (msk)) \
		printk(KERN_INFO "thermal: " fmt, ##__VA_ARGS__); \
} while (0)

/*
* struct definition
*
*/
struct comip_thermal_data{
	struct delayed_work monitor_work;
	struct notifier_block	nb;
	struct thermal_data notifer_data;
	unsigned int sample_duration_time;  /*frequecy of sample data*/
	struct comip_thermal_platform_data *pdata;
	struct platform_device *pdev;
};

struct temperature_data{
	int temperature;	/*adc temperature*/
	int temp_dif; 		/*adc temperature - average temperature*/
	bool valid;
};


/*
* local - global value  definition
*
*/
static int thermal_proc = 0;
static int _test_flag = 0;
static int temperature_max = 0;
static int temperature_min = 100;
static int temperature_last = 0;
static unsigned int monitor_trace_mask;
static struct comip_thermal_data *g_data;

static BLOCKING_NOTIFIER_HEAD(thermal_notifier_list);


/*
* function definition
*
*/
static inline int get_level_by_temp(int temperature, enum thermal_level * level, enum sample_rate * rate)
{
	struct sample_rate_member * table;
	static struct sample_rate_member * history_table = NULL;
	static enum thermal_level history_level = THERMAL_END;
	enum thermal_level current_level = THERMAL_END;
	int i = 0 ;

	for (i = 0; i < g_data->pdata->sample_table_size; i++) {
		table = &g_data->pdata->sample_table[i];

		if ((temperature > table->temp_level_bottom)
			&& (temperature <= table->temp_level_top)){

			current_level = table->level;
			*rate = table->rate;

			if(history_level == THERMAL_END)
				history_level = current_level;

			break;
		}

	}

	if(current_level == THERMAL_END){
		*level = THERMAL_HOT;
		*rate = SAMPLE_RATE_FAST;
		thermal_trace(MONITOR_TRACE_INFO, "%s no level found\n", __func__);
		return -1;
	}
	/*decrease*/
	if(history_level > current_level){
		if(history_table == NULL)
			history_table = table;

		if(temperature <= history_table->temp_level_bottom - TEMP_LEVEL_CHANGE_DIF){
			*level = current_level;
			history_table = table;
			history_level = current_level;
		}
		else{/*nothing changed*/
			*level = history_level;
		}

	/*rise*/
	}else{
		*level = current_level;
		history_table = table;
		history_level = current_level;
	}

	thermal_trace(MONITOR_TRACE_DEBUG, \
				"history_level is %d, current_level is %d\n",\
				history_level, current_level);
	return 0;
}


/* external function*/
int thermal_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&thermal_notifier_list, nb);
}
EXPORT_SYMBOL(thermal_notifier_register);

int thermal_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&thermal_notifier_list, nb);
}
EXPORT_SYMBOL(thermal_notifier_unregister);

int thermal_notify(struct thermal_data *data, enum thermal_level level)
{
	return blocking_notifier_call_chain(&thermal_notifier_list, level, (void *)data);
}
EXPORT_SYMBOL(thermal_notify);

/* local function*/
static int smooth_temperature(struct comip_thermal_data *data, int *temperature)
{
	struct temperature_data temp_array[TEMP_SMOOTH_ARRAY_NUM];
	int i , ret, temp_average, temp_diff_average , valid_num;

	memset(temp_array, 0, TEMP_SMOOTH_ARRAY_NUM * sizeof(struct temperature_data));

	for (i = 0; i < TEMP_SMOOTH_ARRAY_NUM; i++) {

		/*get DBB temp from soc*/
		temp_array[i].temperature = comip_mets_get_temperature();
		if (temp_array[i].temperature < 0) {
			/*get DBB temp from pmu*/
			printk("thermal: get temperature from mets error!\n");
			ret = comip_get_thermal_temperature(data->pdata->channel, &temp_array[i].temperature);
			if(ret)
				return	ret ;
		}
		thermal_trace(MONITOR_TRACE_DEBUG, "temperatue value[%d] is %d\n",\
					i , temp_array[i].temperature);
	}

	/*calculate the average of temperature*/
	temp_average = 0 ;
	for (i = 0; i < TEMP_SMOOTH_ARRAY_NUM ; i++)
		temp_average += temp_array[i].temperature;

	temp_average /= TEMP_SMOOTH_ARRAY_NUM;
	thermal_trace(MONITOR_TRACE_DEBUG, "temp_average is %d\n", temp_average);

	/*calculate how much difference
	between every temperatue to average*/
	for (i = 0; i < TEMP_SMOOTH_ARRAY_NUM ; i++) {
		temp_array[i].temp_dif = temp_array[i].temperature - temp_average;
		if(temp_array[i].temp_dif < 0)
			temp_array[i].temp_dif = 0 - temp_array[i].temp_dif ;
	}

	/*calculate the average of temperature diff*/
	temp_diff_average = 0 ;
	for (i = 0; i < TEMP_SMOOTH_ARRAY_NUM ; i++)
		temp_diff_average += temp_array[i].temp_dif;

	temp_diff_average /= TEMP_SMOOTH_ARRAY_NUM;
	thermal_trace(MONITOR_TRACE_DEBUG, "temp_diff_average is %d\n", temp_diff_average);

	/*mark some un correlative value,  for example
	   temp 40 25 26 24 23 , temp_average  28
	   temp_diff 12 -3 -2 -4 -5 ,temp_diff_average 5
	   12 > temp_diff_average*2 , 40 is invalid,marked
	   calulate average again: (25+26+23+24)/4=25
	*/
	valid_num = 0;
	for (i = 0; i < TEMP_SMOOTH_ARRAY_NUM ; i++) {
		if(temp_array[i].temp_dif > TEMP_SMOOTH_ABANDON_MULTIPLE * temp_diff_average)
			temp_array[i].valid = false;
		else{
			temp_array[i].valid = true;
			valid_num++;
		}
	}

	thermal_trace(MONITOR_TRACE_DEBUG, "valid_num is %d\n", valid_num);

	/*calculate average of temperature again*/
	temp_average = 0 ;
	for (i = 0; i < TEMP_SMOOTH_ARRAY_NUM ; i++) {
		if(true == temp_array[i].valid)
			temp_average += temp_array[i].temperature;
	}

	temp_average /= valid_num;
	*temperature = temp_average ;

	return 0;
}
static inline void _temperature_record(int temperature)
{
	temperature_last = temperature;

	if(temperature > temperature_max)
		temperature_max = temperature;

	if(temperature < temperature_min)
		temperature_min = temperature;
}
void thermal_uevent(struct platform_device *pdev, int temperature)
{
	char thermal_data[THERMAL_LEN];
	char *thermal[] = { thermal_data, NULL };

	sprintf(thermal_data, "%s=%d", "THERMAL", temperature);
	kobject_uevent_env(&pdev->dev.kobj, KOBJ_CHANGE, thermal);
}

static int uevent_notifier_call(struct notifier_block *nb,
                unsigned long event, void *data)
{
	struct comip_thermal_data *thermal_data =
	container_of(nb, struct comip_thermal_data, nb);

	thermal_uevent(thermal_data->pdev, thermal_data->notifer_data.temperature);

	return 0;
}


static void monitor_work_fn(struct work_struct *work)
{
	struct timespec ts;
	struct rtc_time tm;

	enum sample_rate next_rate;
	enum thermal_level level = THERMAL_NORMAL;

	int temperature = 0 , ret = 0;
	struct comip_thermal_data *data
		= container_of((struct delayed_work *)work, struct comip_thermal_data, monitor_work);

	next_rate = 0;

	if(!_test_flag)
		ret = smooth_temperature(data , &temperature); /*get DBB temp from pmu*/
	else if (1 == _test_flag)
		temperature = thermal_proc;
	else if (2 == _test_flag) {
		int adc_temp;
		temperature = comip_mets_get_temperature();
		comip_get_thermal_temperature(data->pdata->channel, &adc_temp);
		thermal_trace(MONITOR_TRACE_INFO,
			"mets %d adc %d\n", temperature, adc_temp);
	}

	_temperature_record(temperature);

	/* Print UTC time for debug. */
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	if (ret)
		thermal_trace(MONITOR_TRACE_ERROR,
			"can't get DBB temperature (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	else {
		thermal_trace(MONITOR_TRACE_INFO,
			"temperature is %d (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			temperature,
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		data->notifer_data.temperature = temperature;
		ret = get_level_by_temp(temperature, &level, &next_rate);
		if(!next_rate) /*change rate*/
			data->sample_duration_time = next_rate;

		thermal_trace(MONITOR_TRACE_DEBUG, "level is %d\n", level);
		thermal_notify(&data->notifer_data, level);

	}

	/*set timer again*/
	thermal_trace(MONITOR_TRACE_DEBUG, "next time rate is %d\n", next_rate);
	data->sample_duration_time = next_rate;
	schedule_delayed_work(&data->monitor_work, data->sample_duration_time * 100);
}

#ifdef CONFIG_PROC_FS
static ssize_t comip_thermal_proc_read(struct file *file, char __user *buf,
		  size_t size, loff_t *ppos)
{
	int i ;
	char temp_table[512] = {0};
	size_t ret = EFAULT;
	char * buff_temp = NULL;

	buff_temp = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if(!buff_temp)
		goto out;

	memset(buff_temp, 0, PAGE_SIZE);

	snprintf(buff_temp, PAGE_SIZE, \
		"record of max temperature is %d,\
		record of min temperature is %d, \
		last is %d feedback enable is %d\n\n",\
		temperature_max, temperature_min, temperature_last, \
		g_data->notifer_data.dfb_cpu.enable);

	memset(temp_table, 0 , sizeof(temp_table));
	for (i = 0; i < g_data->pdata->sample_table_size; i++) {
		memset(temp_table, 0 , sizeof(temp_table));
		sprintf(temp_table, "level %d bootom : %d top : %d\n", \
			i, g_data->pdata->sample_table[i].temp_level_bottom, \
			g_data->pdata->sample_table[i].temp_level_top);
		strcat(buff_temp, temp_table);
	}

	strcat(buff_temp, "\n");
	strcat(buff_temp, "echo \"cur:777\" > comip-thermal ->disable proc thermal\n");
	strcat(buff_temp, "echo \"cur:77\" > comip-thermal ->set temperature 77 from procfs, \
						enable proc thermal, 77 is a sample\n");
	strcat(buff_temp, "echo \"lim:0\" > comip-thermal ->disable feedback method\n");
	strcat(buff_temp, "echo \"lim:65\" > comip-thermal ->enable feedback method,\
						set feedback level is 65\n");
	strcat(buff_temp, "echo \"lev:1:10:65\" > comip-thermal ->change level 1 table ,\
						bootom 10 ,top 65\n\n\n");
	strcat(buff_temp, "echo \"met:50:60\" > comip-thermal ->use mets irq notify temperature, blow 50 or up 60 irq trigger\n");

	strcat(buff_temp, "echo \"mets\" > comip-thermal -> print pmu temperature and mets temperature\n");

	ret = simple_read_from_buffer(buf, size, ppos, buff_temp,
					strlen(buff_temp));
out:
	kfree(buff_temp);
	return ret;
}

static int _user_input_check(const char __user *buffer)
{
	char *num = NULL;
	char head[10] = {0};
	int ret = 0;

	strncpy(head, buffer, TEMP_DEFAULT_HEAD_LEN);

	if (!strcmp(head, "cur:")){
		_test_flag = 1;
		num = (char *)buffer + TEMP_DEFAULT_HEAD_LEN;
		if(kstrtol(num, 10, (long*)&thermal_proc) < 0)
			goto error;
		if (thermal_proc == 777)
			_test_flag = 0;
	} else if (!strcmp(head, "lim:")){
		long temperature = 0;
		_test_flag = 0;
		num = (char *)buffer + TEMP_DEFAULT_HEAD_LEN;
		if(kstrtol(num, 10, &temperature) < 0)
			goto error;

		if (temperature == 0){
			g_data->notifer_data.dfb_cpu.enable = 0;

		} else {

			g_data->notifer_data.dfb_cpu.enable = 1;
			if (temperature >= TEMP_FEEDBACK_BOOTOM && temperature <= TEMP_FEEDBACK_TOP){
				if(kstrtol(num, 10, (long *)&g_data->notifer_data.dfb_cpu.target_temperature) < 0)
					goto error;
			}
			else
				thermal_trace(MONITOR_TRACE_INFO, \
				"target temperature is %d, out of control\n", (int)temperature);

		}
	} else if (!strcmp(head, "lev:")){

		char *start = (char *)buffer;
		long level, bootom, top;
		start = memchr(start, ':', strlen(start));
		if (!start)
			goto error;

		start++;
		if(kstrtol(start, 10, &level) < 0)
			goto error;
		start = memchr(start, ':', strlen(start));
		if (!start)
			goto error;

		start++;
		if(kstrtol(start, 10, &bootom) < 0)
			goto error;
		start = memchr(start, ':', strlen(start));
		if (!start)
			goto error;

		start++;
		if(kstrtol(start, 10, &top) < 0)
			goto error;

		g_data->pdata->sample_table[level].temp_level_bottom = bootom;
		g_data->pdata->sample_table[level].temp_level_top = top;

	} else if (!strcmp(head, "met:")){
		char *start = (char *)buffer;
		char *end;
		char temp[10] = {0};
		long temp_low, temp_high;
		start = memchr(start, ':', strlen(start));
		if (!start) {
			printk("thermal a start %s\n", start);
			goto error;
		}
		start++;
		end = memchr(start, ':', strlen(start));
		strncpy(temp, start, end - start);

		printk("thermal temp %s\n", temp);
		if(kstrtol(temp, 10, &temp_low) < 0) {
			goto error;
		}

		start = memchr(start, ':', strlen(start));
		if (!start) {
			goto error;
		}

		start++;
		if(kstrtol(start, 10, &temp_high) < 0) {
			goto error;
		}

		comip_mets_config((int)temp_low, (int)temp_high);
	} else if (!strcmp(head, "mets")){
		_test_flag = 2;

	} else {
		_test_flag = 0;
		ret = -1;
	}

	thermal_trace(MONITOR_TRACE_INFO, \
		"_test_flag is %d, thermal_proc is %d, \
		\ntarget_temperature is %d, feedback enable is %d\n", \
		_test_flag, thermal_proc, \
		g_data->notifer_data.dfb_cpu.target_temperature,\
		g_data->notifer_data.dfb_cpu.enable);
	return ret;
error:
	thermal_trace(MONITOR_TRACE_INFO, "command error!!\n");
	return -1;
}

static int comip_thermal_proc_write(struct file *file, const char __user *buffer,
                        size_t count, loff_t *ppos)
{
	char *buf;

	if (count < 1)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}

	thermal_trace(MONITOR_TRACE_INFO, "%s\n", buf);

	_user_input_check(buf);

	kfree(buf);
	return count;
}

static const struct file_operations comip_thermal_operations = {
        .owner          = THIS_MODULE,
        .read           = comip_thermal_proc_read,
        .write          = comip_thermal_proc_write,
};

#endif

static void comip_thermal_proc_init(void *data)
{
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *comip_thermal_proc_entry = NULL;

	comip_thermal_proc_entry = proc_create_data("driver/comip-thermal", 0, NULL,
			&comip_thermal_operations, data);
#endif
}

static int comip_thermal_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct comip_thermal_data *data;

	monitor_trace_mask = MONITOR_TRACE_INFO | MONITOR_TRACE_ERROR;
	thermal_trace(MONITOR_TRACE_INFO, "%s called\n", __func__);

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "Platform data not set.\n");
		return -EINVAL;
	}

	if (!(data = kzalloc(sizeof(struct comip_thermal_data), GFP_KERNEL))) {
		dev_err(&pdev->dev, "Temperature monitor kzalloc error\n");
		ret = -ENOMEM;
		goto exit;
	}

	data->pdata = pdev->dev.platform_data;
	data->pdev = pdev;

	data->notifer_data.dfb_cpu.enable = 0;
	data->notifer_data.dfb_cpu.limit_percent = (10000*8)/16;
	data->notifer_data.dfb_cpu.target_temperature = 65;

	data->notifer_data.dfb_gpu.enable = 0;
	data->notifer_data.dfb_gpu.limit_percent = (10000*4)/16;
	data->notifer_data.dfb_gpu.target_temperature = 65;

	INIT_DELAYED_WORK(&data->monitor_work, monitor_work_fn);
	data->sample_duration_time = SAMPLE_RATE_FAST;

	/* First update after 15s*/
	schedule_delayed_work(&data->monitor_work, 15 * HZ);

	platform_set_drvdata(pdev, data);

	comip_thermal_proc_init(data);

	data->nb.notifier_call = uevent_notifier_call;

	g_data = data;

	thermal_notifier_register(&data->nb);

	return 0;
exit:
	return ret;
}

static int comip_thermal_remove(struct platform_device *pdev)
{
	struct comip_thermal_data *data = platform_get_drvdata(pdev);
	if(data){
		kfree(data);
	}
	return 0;
}

#ifdef CONFIG_PM
static int comip_thermal_suspend(struct device *dev)
{
	return 0;
}

static int comip_thermal_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops comip_thermal_pm_ops = {
	.suspend = comip_thermal_suspend,
	.resume	= comip_thermal_resume,
};
#endif

struct platform_driver comip_thermal_driver = {
	.probe = comip_thermal_probe,
	.remove = comip_thermal_remove,
	.driver = {
		.name = "comip-thermal",
#ifdef CONFIG_PM
		.pm = &comip_thermal_pm_ops,
#endif
	},
};

static int __init comip_thermal_init(void)
{
	return platform_driver_register(&comip_thermal_driver);
}

static void __exit comip_thermal_exit(void)
{
	platform_driver_unregister(&comip_thermal_driver);
}

module_init(comip_thermal_init);
module_exit(comip_thermal_exit);

MODULE_AUTHOR("xuxuefeng <xuxuefeng@leadcoretech.com>");
MODULE_DESCRIPTION("thermal driver");
MODULE_LICENSE("GPL");
