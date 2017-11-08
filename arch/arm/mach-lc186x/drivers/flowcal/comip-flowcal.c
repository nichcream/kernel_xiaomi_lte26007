#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/mutex.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <mach/comip-regs.h>
#include <plat/hardware.h>

enum {
	APHA7_MASTER_FLOW,
	APSA7_MASTER_FLOW,
	ON2_MASTER_FLOW,
	DISPLAY_MASTER_FLOW,
	ISP_MASTER_FLOW,
	USB_MASTER_FLOW,
	GPU_MASTER_FLOW,
	ACC_MASTER_FLOW,
	AP_DMAS_MASTER_FLOW,
	AP_SDIO_MASTER_FLOW,
	AP_MASTER_FLOW_ALL,
	TOP_DMAS_MASTER_FLOW,
	MAX_MASTER_FLOW = 100,
};


struct comip_flowcal_info {
	int item;
};

static struct comip_flowcal_info *ms_info;

static int comip_flowcal_group_config(int item)
{
	//int ret = -EINVAL;
	unsigned long int val;

	/*config flowcal group*/
	//if (item >= MAX_MASTER_FLOW)
	//	return ret;

	switch (item) {
		case APHA7_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF0));
			val |= (1<<28)|(1<<24)|(1<<12)|(1<<8);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF0));
			break;
		case APSA7_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF0));
			val |= (1<<20)|(1<<16)|(1<<4)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF0));
			break;
		case ON2_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF1));
			val |= (1<<28)|(1<<24)|(1<<12)|(1<<8)|(1<<20)|(1<<16)|(1<<4)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF1));
			break;
		case DISPLAY_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF2));
			val |= (1<<28)|(1<<24)|(1<<12)|(1<<8);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF2));
			break;
		case ISP_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF2));
			val |= (1<<20)|(1<<16)|(1<<4)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF2));
			break;
		case USB_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF10));
			val |= (1<<24)|(1<<8);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF10));
			break;
		case GPU_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF10));
			val |= (1<<20)|(1<<16)|(1<<4)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF10));
			break;
		case ACC_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF11));
			val |= (1<<20)|(1<<16)|(1<<4)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF11));
			break;
		case AP_DMAS_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF13));
			val |= (1<<16)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF13));
			break;
		case AP_SDIO_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF14));
			val |= (1<<20)|(1<<16)|(1<<4)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF14));

			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF15));
			val |= (1<<16)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF15));
			break;
		case AP_MASTER_FLOW_ALL:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF0));
			val |= (1<<20)|(1<<26)|(1<<4)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF0));
			break;
		case TOP_DMAS_MASTER_FLOW:
			val = __raw_readl(io_p2v(TOP_MAIL_MASTER_GRP_DEF0));
			val |= (1<<20)|(1<<26)|(1<<4)|(1<<0);
			__raw_writel(val, io_p2v(TOP_MAIL_MASTER_GRP_DEF0));
			break;
		case MAX_MASTER_FLOW:
			printk("flowcal group all in!!\n");
			break;
		default:
			printk("flowcal group default config!!\n");
			break;
	}
	return 0;
}

static int comip_flowcal_intr_en(int en)
{
	unsigned long int val;

	if(!!en) {
		val = __raw_readl(io_p2v(DDR_PWR_INTR_EN_APA7));
		val |= 1 << 0;
		__raw_writel(val, io_p2v(DDR_PWR_INTR_EN_APA7));
	}
	else {
		val = __raw_readl(io_p2v(DDR_PWR_INTR_EN_APA7));
		val &= ~(1 << 0);
		__raw_writel(val, io_p2v(DDR_PWR_INTR_EN_APA7));
	}
	return 0;
}

static int comip_flowcal_timer_set(int short_timer, int long_timer)
{
	__raw_writel(short_timer, io_p2v(DDR_PWR_FC_SHORT_PERIOD));
	__raw_writel(long_timer, io_p2v(DDR_PWR_FC_LONG_PERIOD));
	printk("flowcal:short timer = 0x%x, long timer = 0x%x\n\r", \
		__raw_readl(io_p2v(DDR_PWR_FC_SHORT_PERIOD)), __raw_readl(io_p2v(DDR_PWR_FC_LONG_PERIOD)));
	return 0;
}

static int comip_flowcal_group_rw_config(int ritem, int witem)
{
	__raw_writel(ritem, io_p2v(DDR_PWR_FC_AWUSER));
	__raw_writel(witem, io_p2v(DDR_PWR_FC_ARUSR));
	return 0;
}

static __inline void comip_flowcal_enable(int en)
{
	if(!!en)
		__raw_writel(0x101, io_p2v(DDR_PWR_FC_CTL));
	else
		__raw_writel(0x0, io_p2v(DDR_PWR_FC_CTL));
}

static ssize_t comip_flowcal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	//int ret = -EINVAL;
	char *buff = buf;
	unsigned long int val;
	comip_flowcal_enable(1);
	msleep(1100);
	val = __raw_readl(io_p2v(DDR_PWR_FC_LONG_BYTESCAL0));
	comip_flowcal_enable(0);

	buff += sprintf(buff, "MASTER_FLOW_ALL: %d Bytes, %d KByte\n", (int)val,  (int)(val / 1024));

	return (buff - buf);
#if 0
	switch (ms_info->item) {
		case APHA7_MASTER_FLOW:
			return sprintf(buf, "APHA7_MASTER_FLOW: %d Byte\n", __raw_readl(io_p2v(DDR_PWR_FC_LONG_BYTESCAL0)));
	}
#endif
	return 0;
}

static ssize_t comip_flowcal_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = -EINVAL;
	int len = 0;
	int i = 0;
	int j = 0;
	unsigned int para[4];

	para[0] = simple_strtoul(buf, NULL, 10);
	//para[0] is group config
	printk("********para[0] = %d ***********\n", para[0]);
	while(buf[len] != 0x20)
		len++;
	for (i = 1; i < 4; i++) {
		para[i] = simple_strtoul((buf + len + j + 1), NULL, 10);
		//para[1] is long period
		printk("********para[%d] = %d ***********\n",i, para[i]);
		j++;
		while(buf[len + j] != 0x20)
			j++;
	}

	ms_info->item = para[0];

	//printk("comip_flowcal_group_config\n");
	ret = comip_flowcal_group_config(para[0]);
	if(ret != 0){
		printk("comip_flowcal_group_config ret = %d\n", ret);
	}

	//printk("comip_flowcal_intr_en\n");
	comip_flowcal_intr_en(0);

	//printk("comip_flowcal_timer_set\n");
	//short timer
	//0xa28:	2600, 	100us,	1s para[1]=10,000
	//0x104:	260,	10us,	1s para[1]=100,000
	//0x01a:	26,	1us,	1s para[1]=1,000,000
	comip_flowcal_timer_set(0x1a, para[1]);

	//printk("comip_flowcal_group_rw_config\n");
	comip_flowcal_group_rw_config(0, 0);

	//printk("comip_flowcal_enable\n");
	comip_flowcal_enable(0);

	return size;
}

static DEVICE_ATTR(comip_flowcal, 0644, comip_flowcal_show, comip_flowcal_store);

static int comip_flowcal_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;

	ms_info = kzalloc(sizeof(struct comip_flowcal_info), GFP_KERNEL);
	if (!ms_info) {
		dev_err(&pdev->dev, "memctl statistic kzalloc failed\n");
		return -ENOMEM;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_comip_flowcal);
	if (ret) {
		dev_err(&pdev->dev, "comip flowcal device create failed.\n");
	}
	printk("comip flowcal device create!\n");


/**************set default config start**************/
	comip_flowcal_group_config(MAX_MASTER_FLOW);
	comip_flowcal_intr_en(1);
	//short timer
	//0xa28:	2600, 	100us,	1s para[1]=10,000
	//0x104:	260,	10us,	1s para[1]=100,000
	//0x01a:	26,	1us,	1s para[1]=1,000,000
	comip_flowcal_timer_set(0x1a, 1000000); // 1 second
	comip_flowcal_group_rw_config(0, 0);
	comip_flowcal_enable(0);
/***********end*********************/
	return 0;
}

static int comip_flowcal_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_comip_flowcal);
	if (ms_info) {
		kfree(ms_info);
	}
	return 0;
}

static struct platform_driver comip_flowcal_driver = {
	.probe = comip_flowcal_probe,
	.remove = comip_flowcal_remove,
	.driver = {
		.name = "comip_flowcal",
	},
};

static int __init flowcal_init(void)
{
	return platform_driver_register(&comip_flowcal_driver);
}

static void __exit flowcal_exit(void)
{
        platform_driver_unregister(&comip_flowcal_driver);
}

module_init(flowcal_init);
module_exit(flowcal_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("system flow calculation");
MODULE_AUTHOR("taoran <taoran@leadcoretech.com>");
