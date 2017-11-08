/*
 * LC186X modem driver
 *
 * Copyright (C) 2013 Leadcore Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <plat/hardware.h>
#include <plat/cpu.h>
#include <plat/comip-memory.h>
#include <mach/comip-modem.h>
#include <mach/comip-irq.h>
#include <mach/comip-bridge.h>

#define COMIP_MODEM_DEBUG	1
#ifdef COMIP_MODEM_DEBUG
#define MODEM_PRT(format, args...)  printk(KERN_DEBUG "modem: " format, ## args)
#define MODEM_INFO(format, args...) printk(KERN_INFO "modem: " format, ## args)
#define MODEM_ERR(format, args...)	printk(KERN_ERR "modem: " format, ## args)
#else
#define MODEM_PRT(x...)  do{} while(0)
#define MODEM_INFO(x...)  do{} while(0)
#define MODEM_ERR(x...)  do{} while(0)
#endif

static struct comip_modem_data * modem_data = NULL;

static void	Set_modem_data(struct comip_modem_data *modem_data)
{
	modem_data = modem_data;
	return ;
}

struct comip_modem_data *Get_modem_data(void)
{
	return modem_data;
}

static void add_bridge_devices(struct comip_modem_data *modem_data)
{
	struct modem_bridge_info *pos = NULL,*n = NULL;
	struct modem_base_info* modem_base_info = &modem_data->modem_base_info;

	list_for_each_entry_safe(pos, n,  &modem_base_info->bridge_head, list){

		memset(modem_data->mem_base + pos->rx_addr, 0, pos->rx_len);
		memset(modem_data->mem_base + pos->tx_addr, 0, pos->tx_len);
		memcpy(&pos->dbg_info, &modem_data->bridge_dbg, sizeof(struct info));

		MODEM_PRT("add bridge %s \n",pos->name);
		register_comip_bridge(pos, modem_data->miscdev.parent);
	}
}

static inline int image_head_parser(void *source,
						struct modem_base_info* modem_base_info, unsigned long source_len)
{
	struct dtag_header * header = NULL;
	struct set_info *set_info = NULL;
	struct get_info *get_info = NULL;
	struct modem_image_info *img_info = NULL;
	struct modem_bridge_info *bridge_info = NULL;
	struct comip_modem_data *modem_data = NULL;
	u32 tag_len = 0;

	header = (struct dtag_header *)source;
	modem_data = container_of(modem_base_info, struct comip_modem_data, modem_base_info);

	for (; (header->tag != TAG_NONE); header = tag_next(header)) {
		tag_len += header->size;
		if (tag_len > source_len) {
			MODEM_ERR("image head parser ERROR. tag_len = %d \n", tag_len);
			return -1;
		}

		if (((u32)header - (u32)source) > source_len) {
			MODEM_ERR("image head parser ERROR. (header - source) > source_len \n");
			return -1;
		}

		switch (header->tag) {
			case TAG_VER:
				modem_base_info->tag_version = ((struct tag_ver *)header)->version;
				break;

			case TAG_INFO:
				modem_base_info->modem_version = ((struct dtag_info *)header)->modem_version;
				modem_base_info->modem_ram_size = ((struct dtag_info *)header)->modem_ram_size;
				modem_base_info->nvram_addr = ((struct dtag_info *)header)->nvram_addr;
				modem_base_info->nvram_size = ((struct dtag_info *)header)->nvram_size;
				break;

			case TAG_IMAGE:
				img_info = kzalloc(sizeof(struct modem_image_info), GFP_KERNEL);
				if (!img_info){
					MODEM_ERR("img_info alloc fail \n");
					return -ENOMEM;
				}

				img_info->bin_offset = ((struct dtag_image *)header)->bin_offset;
				img_info->ram_offset = ((struct dtag_image *)header)->ram_addr;
				img_info->len = ((struct dtag_image *)header)->len;
				img_info->img_id = ((struct dtag_image *)header)->img_id;
				list_add_tail(&img_info->list, &modem_base_info->img_info_head);
				modem_base_info->img_num++;
				break;

			case TAG_BRIDGE:
				bridge_info = kzalloc(sizeof(struct modem_bridge_info), GFP_KERNEL);
				if (!bridge_info){
					MODEM_ERR("bridge_info alloc fail \n");
					return -ENOMEM;
				}
				strcpy(bridge_info->name, ((struct dtag_bridge_common*)header)->name);
				bridge_info->baseaddr = (u32)modem_data->mem_base;
				bridge_info->type = (Bridge_Type)((struct dtag_bridge_common *)header)->type;
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
				bridge_info->rx_id = ((struct dtag_bridge_common *)header)->a2m_notify_id;
#else
				bridge_info->rx_id = ((struct dtag_bridge_common *)header)->m2a_notify_id;
#endif
				bridge_info->rx_len = ((struct dtag_bridge_common *)header)->m2a_buf_len;
				bridge_info->rx_addr = ((struct dtag_bridge_common *)header)->m2a_buf_addr;
				bridge_info->tx_id = ((struct dtag_bridge_common *)header)->a2m_notify_id;
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
				bridge_info->tx_len = ((struct dtag_bridge_common *)header)->m2a_buf_len;
				bridge_info->tx_addr = ((struct dtag_bridge_common *)header)->m2a_buf_addr;
#else
				bridge_info->tx_len = ((struct dtag_bridge_common *)header)->a2m_buf_len;
				bridge_info->tx_addr = ((struct dtag_bridge_common *)header)->a2m_buf_addr;
#endif
				bridge_info->property.pro = ((struct dtag_bridge_common *)header)->property;
				if (bridge_info->property.pro_b.flow_ctl && bridge_info->property.pro_b.packet) {
					bridge_info->rx_pkt_maxnum =
								((struct dtag_hs_bridge *)header)->packet_ctl.m2a_packet_max_num;
					bridge_info->rx_pkt_maxsize =
								((struct dtag_hs_bridge *)header)->packet_ctl.m2a_packet_max_size;
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
					bridge_info->tx_pkt_maxnum =
								((struct dtag_hs_bridge *)header)->packet_ctl.m2a_packet_max_num;
					bridge_info->tx_pkt_maxsize =
								((struct dtag_hs_bridge *)header)->packet_ctl.m2a_packet_max_size;
					bridge_info->rx_ctl_id = ((struct dtag_hs_bridge *)header)->flow_ctl.a2m_ctl_id;
#else
					bridge_info->tx_pkt_maxnum =
								((struct dtag_hs_bridge *)header)->packet_ctl.a2m_packet_max_num;
					bridge_info->tx_pkt_maxsize =
								((struct dtag_hs_bridge *)header)->packet_ctl.a2m_packet_max_size;
					bridge_info->rx_ctl_id = ((struct dtag_hs_bridge *)header)->flow_ctl.m2a_ctl_id;
#endif
					bridge_info->tx_ctl_id = ((struct dtag_hs_bridge *)header)->flow_ctl.a2m_ctl_id;
				}
				list_add_tail(&bridge_info->list, &modem_base_info->bridge_head);
				modem_base_info->bridge_num++;
				break;

			case TAG_READ:
				get_info = kzalloc(sizeof(struct get_info), GFP_KERNEL);
				if (!get_info) {
					MODEM_ERR("get_info alloc fail \n");
					return -ENOMEM;
				}
				get_info->info_id =  ((struct dtag_read_info *)header)->info_id;
				get_info->info.type = ((struct dtag_read_info *)header)->addr_type;
				get_info->info.addr = ((struct dtag_read_info *)header)->info_addr;
				get_info->info.len = ((struct dtag_read_info *)header)->info_size;
				if (get_info->info_id == READ_INFO_CP_EXC_STEP_INFO_ADDR)
					memcpy(&modem_data->bridge_dbg, &get_info->info, sizeof(struct info));
				if (get_info->info_id == READ_INFO_DBBID_BASE_ADDR) {
					set_info = kzalloc(sizeof(struct set_info), GFP_KERNEL);
					if (!set_info){
						MODEM_ERR("set_bbid_info alloc fail \n");
						return -ENOMEM;
					}
					set_info->addr_type = get_info->info.type;
					set_info->addr = get_info->info.addr;
					set_info->value = modem_data->bbid;
					list_add_tail(&set_info->list, &modem_base_info->set_info_head);
					modem_base_info->set_info_num++;
				}
				list_add_tail(&get_info->list, &modem_base_info->get_info_head);
				modem_base_info->get_info_num++;
				break;

			case TAG_WRITE:
				set_info = kzalloc(sizeof(struct set_info), GFP_KERNEL);
				if (!set_info){
					MODEM_ERR("set_info alloc fail \n");
					return -ENOMEM;
				}
				set_info->addr_type = ((struct dtag_write_value *)header)->addr_type;
				set_info->addr = ((struct dtag_write_value *)header)->addr;
				set_info->value = ((struct dtag_write_value *)header)->value;
				list_add_tail(&set_info->list, &modem_base_info->set_info_head);
				modem_base_info->set_info_num++;
				break;

			default:
				MODEM_INFO("Ignoring unrecognised tag (0x%08x) %s size = %x.\n",
			       				header->tag,header->tag_name,header->size);
				if(header->size == 0){
					MODEM_ERR("headr size is NULL \n");
					return -1;
				}
		}
	}

	return (source_len - tag_len);
}


#ifdef CONFIG_MTD
static int load_nandflash_modem_image(struct comip_modem_data *modem_data)
{
	/*pay attention to img load sequence .refer to modem space definition.
	if arm0_target_offset>zsp1_target_offset>zsp0_target_offset
	then ===> load sequence zsp0_img->zsp1_img->arm0_img*/
	/*fix me. care about bbt.*/
	size_t len = 0;
	struct modem_image_info *pos = NULL,*n = NULL;
	struct modem_base_info* base_info = NULL;

	if (!modem_data) {
		MODEM_ERR("modem_data == 0 !\n");
		return -1;
	}

	if (modem_data->modem_base_info.img_num == 0) {
		MODEM_ERR("modem_data->modem_base_info.img_num == 0 !\n");
		return -1;
	}

	base_info = &modem_data->modem_base_info;

	list_for_each_entry_safe(pos, n, &base_info->img_info_head, list){
		switch (pos->img_id) {
			case MODEM_IMG_ID:
				if (!modem_data->arm_mtd) {
					MODEM_INFO("not find arm mtd parttion.\n");
					break;
				}
				mtd_read(modem_data->arm_mtd, modem_tag_len, pos->len, &len,
							modem_data->mem_base + pos->ram_offset);
				if (pos->len != len)
					MODEM_INFO("failed to load ARM IMG.img len = %d,but we load %d ", pos->len, len);
				break;

			case PL_IMG_ID:
				if (!modem_data->zsp_mtd) {
					MODEM_INFO("not find zsp0 mtd parttion.\n");
					break;
				}
				mtd_read(modem_data->zsp_mtd, 0, pos->len, &len,
							modem_data->mem_base + pos->ram_offset);
				if (pos->len != len)
					MODEM_INFO("failed to load ZSP IMG.img len = %d,but we load %d ", pos->len, len);
				break;

			default:
				break;
		}
	}
	return 0;
}

static int parser_mtd_modem_img_head(struct comip_modem_data *modem_data)
{
	/*pay attention to img load sequence .refer to modem space definition.if arm0_target_offset>zsp1_target_offset>zsp0_target_offset  then ===> load sequence zsp0_img->zsp1_img->arm0_img*/
	/*fix me. care about bbt.*/
	size_t len = 0;
	char *head_tag = NULL;
	struct modem_base_info* base_info = NULL;

	if (!modem_data) {
		MODEM_ERR("modem_data = 0 !\n");
		return -1;
	}
	if (!modem_data->arm_mtd) {
		MODEM_ERR("arm_mtd = 0 !\n");
		return -1;
	}

	base_info = &(modem_data->modem_base_info);
	if (base_info->atag_version != 0 || base_info->modem_version != 0) {
		MODEM_PRT("parser modem head twice.\n");
		return 0;
	}

	head_tag = kzalloc(modem_tag_len, GFP_KERNEL);
	/*parser image head.	*/
	mtd_read(modem_data->arm_mtd, 0, modem_tag_len, &len, head_tag);
	MODEM_PRT("read modem tag head %d bytes.\n", len);
	if (modem_tag_len != len) {
		MODEM_ERR("failed to load modem tag head.img len = %d,but we load %d ",modem_tag_len,len);
		return -1;
	}

	image_head_parser(head_tag, base_info, modem_tag_len);
	kfree(head_tag);

	return 0;
}
#endif


static int parser_emmc_modem_img_head(struct comip_modem_data *modem_data)
{
	char path[100] = {0};
	char *head_tag = NULL;
	struct file *img_file = NULL;
	struct modem_base_info* base_info = NULL;
	unsigned int len = 0;
	mm_segment_t old_fs;

	if (!modem_data) {
		MODEM_ERR("modem_data = 0 !\n");
		return -1;
	}
	if (!strcmp(modem_data->arm_partition_name , "")) {
		MODEM_ERR("arm_partition_name not defined.failed to load arm image.\n");
		return -1;
	}

	base_info = &(modem_data->modem_base_info);
	if (base_info->tag_version != 0 || base_info->modem_version != 0) {
		MODEM_PRT("parser modem head twice.\n");
		return 0;
	}

	sprintf(path, "/dev/block/platform/comip-mmc.1/by-name/%s", modem_data->arm_partition_name);
	img_file = filp_open(path, O_RDONLY, 0);
	if (img_file > 0) {
		head_tag = kzalloc(modem_tag_len, GFP_KERNEL);
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		len = img_file->f_op->read(img_file,head_tag, modem_tag_len,&img_file->f_pos);
		set_fs(old_fs);
		MODEM_PRT("modem tag head load 0x%x byte.\n",len);
		image_head_parser(head_tag, base_info, modem_tag_len);
		kfree(head_tag);
		filp_close(img_file, NULL);
	}else{
		MODEM_ERR("open %s failed.\n",path);
	}

	if (modem_tag_len != len) {
		MODEM_ERR("failed to load modem tag head.img len = %d,but we load %d ",
					modem_tag_len, len);
		return -1;
	}

	return 0;
}

static int load_emmc_modem_image(struct comip_modem_data *modem_data)
{
	char path[100] = {0};
	struct file *img_file = NULL;
	unsigned int len = 0;
	mm_segment_t old_fs;
	struct modem_base_info* base_info = NULL;
	struct modem_image_info * pos = NULL;
	struct modem_image_info *n = NULL;

	base_info = &(modem_data->modem_base_info);

	if(base_info->img_num == 0){
		MODEM_ERR("modem_data->modem_base_info.img_num = 0 !\n");
		return -1;
	}

	list_for_each_entry_safe(pos, n,  &base_info->img_info_head, list){
		switch (pos->img_id) {
			case MODEM_IMG_ID:
				len = 0;
				if (!strcmp(modem_data->arm_partition_name, "")) {
					MODEM_ERR("arm_partition_name not defined.failed to load arm image.\n");
					break;
				}

				sprintf(path, "/dev/block/platform/comip-mmc.1/by-name/%s", modem_data->arm_partition_name);
				img_file = filp_open(path, O_RDONLY, 0);
				if (img_file) {
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					img_file->f_op->llseek(img_file, modem_tag_len,0);
					len = img_file->f_op->read(img_file, modem_data->mem_base + pos->ram_offset,
							pos->len, &img_file->f_pos);
					MODEM_PRT("arm img load 0x%x byte.\n",len);
					set_fs(old_fs);
					filp_close(img_file,NULL);
				}

				if (pos->len != len)
					MODEM_ERR("failed to load ARM IMG.img len = 0x%x,but we load 0x%x \n",pos->len,len);
				break;

			case PL_IMG_ID:
				len = 0;
				if(!strcmp(modem_data->zsp_partition_name , "")){
					MODEM_ERR("zsp_partition_name not defined.failed to load zsp image.\n");
					break;
				}
				sprintf(path, "/dev/block/platform/comip-mmc.1/by-name/%s", modem_data->zsp_partition_name);
				img_file = filp_open(path, O_RDONLY, 0);
				if (img_file) {
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					len = img_file->f_op->read(img_file, modem_data->mem_base + pos->ram_offset,
												pos->len, &img_file->f_pos);
					MODEM_PRT("zsp img load 0x%x byte.\n",len);
					set_fs(old_fs);
					filp_close(img_file,NULL);
				}

				if(pos->len != len)
					MODEM_ERR("failed to load ZSP IMG.img len = %d,but we load %d \n", pos->len, len);
				break;

			default:
				break;
		}
	}
	return 0;
}

void modem_parameter_setting(struct comip_modem_data *modem_data)
{
	struct modem_base_info* modem_base_info = &modem_data->modem_base_info;
	struct set_info *pos = NULL,*n = NULL;
	unsigned int __iomem *membase = NULL;

	list_for_each_entry_safe(pos, n, &modem_base_info->set_info_head, list){
		if (pos->addr_type == ADDR_MEM) {
			membase = ioremap(pos->addr, 0x4);
			if (!membase) {
				MODEM_ERR("ioremap failed in set parameter\n");
				return;
			}
			*membase = pos->value;
			iounmap(membase);
		}
		if (pos->addr_type == ADDR_IO) {
			writel(pos->value, io_p2v(pos->addr));
		}
	}
}

static int reset_modem(struct comip_modem_data *modem_data)
{
	struct modem_bridge_info *pos = NULL,*n = NULL;
	struct modem_base_info* modem_base_info = &modem_data->modem_base_info;

	MODEM_PRT("reset_modem (%d)\n",modem_data->run_count);
	if (modem_data->run_count == 0)
		return 0;

	modem_data->status = MODEM_IN_RESET;
	msleep(1000);
	writel(1, io_p2v(AP_PWR_CP_RSTCTL));
	msleep(100);

	list_for_each_entry_safe(pos, n,  &modem_base_info->bridge_head, list){
		save_rxbuf_comip_bridge(pos);
	}
	memset(modem_data->mem_base, 0, modem_data->mem_len);

	list_for_each_entry_safe(pos, n,  &modem_base_info->bridge_head, list){
		restore_rxbuf_comip_bridge(pos);
	}
	return 0;
}

static int run_modem(struct comip_modem_data *modem_data)
{
	if(modem_data->status == MODEM_RUNNING  || modem_data->status == MODEM_RELEASE){
		MODEM_ERR("modem status(%d) == MODEM_RELEASE ||MODEM_RUNNING .run modem error! \n",modem_data->status);
		return -EIO;
	}

	if (modem_data->run_count == 0) {
		add_bridge_devices(modem_data);

		if (!(modem_data->img_flag & FISRT_LOAD_IN_BOOTLOADER)) {
			if (modem_data->img_flag&IN_MTD_NANDFLASH) {
				#ifdef CONFIG_MTD
				if (load_nandflash_modem_image(modem_data)) {
					MODEM_ERR("load_nandflash_modem_image error.\n");
					return -EAGAIN;
				}
				#else
				MODEM_ERR("board config error.\n");
				return -EIO;
				#endif
			}else if (modem_data->img_flag&IN_BLOCK_EMMC) {
				if (load_emmc_modem_image(modem_data)) {
					MODEM_ERR("load_emmc_modem_image error.\n");
					return -EAGAIN;
				}
			}else{
				MODEM_ERR("get unknow modem img_flag(%d),when load img.\n",
							modem_data->img_flag);
				return -EIO;
			}
		}
		modem_parameter_setting(modem_data);
		msleep(10);

		comip_cp_irq_enable();/*enabe begin irq*/
		comip_trigger_cp_irq(modem_data->irq_begin);/*trigger irq to ARM0*/
		msleep(10);
	}else{

		if(modem_data->img_flag&IN_MTD_NANDFLASH){
			#ifdef CONFIG_MTD
			if(load_nandflash_modem_image(modem_data))
				return -EAGAIN;
			#else
			printk(KERN_ERR "board config error.\n");
			return -EIO;
			#endif
		}else if(modem_data->img_flag&IN_BLOCK_EMMC){
			if(load_emmc_modem_image(modem_data))
				return -EAGAIN;
		}else{
			MODEM_PRT("get unknow modem img_flag,when load img.\n");
			return -EIO;
		}

		modem_parameter_setting(modem_data);
		msleep(10);
		writel(0, io_p2v(AP_PWR_CP_RSTCTL));
		msleep(10);

		comip_cp_irq_enable();/*enabe begin irq*/
		comip_trigger_cp_irq(modem_data->irq_begin);/*trigger irq to ARM0*/
		msleep(10);
	}
	modem_data->status = MODEM_RUNNING;

	modem_data->run_count++;
	MODEM_PRT("run modem %d time(s).\n",modem_data->run_count);
	return 0;
}

static int modem_open(struct inode *ip, struct file *fp)
{
	struct miscdevice  * misc = (struct miscdevice *)(fp->private_data);

	struct comip_modem_data *modem_data = container_of(misc,
					     struct comip_modem_data, miscdev);
	int ret = 0;
	mutex_lock(&modem_data->muxlock);
	if (modem_data->run_count == 0) {
		if (modem_data->img_flag & IN_MTD_NANDFLASH) {
			#ifdef CONFIG_MTD
			if (parser_mtd_modem_img_head(modem_data)) {
				MODEM_ERR("parser_mtd_modem_img_head error! \n");
				ret = -EAGAIN;
				goto out;
			}
			#else
			MODEM_ERR("board config error.\n");
			ret = -EIO;
			goto out;
			#endif
		}else if (modem_data->img_flag&IN_BLOCK_EMMC) {
			if (parser_emmc_modem_img_head(modem_data)) {
				MODEM_ERR("parser_emmc_modem_img_head error!\n");
				ret = -EAGAIN;
				goto out;
			}
		}else{
			MODEM_ERR("unknow modem img_flag.\n");
		}
	}

	if ((modem_data->status >= MODEM_OPEN) && (modem_data->ref_flag > 0)) {
		MODEM_INFO("modem status(%d) != MODEM_RELEASE.open modem again! \n",modem_data->status);
	}else
		modem_data->status = MODEM_OPEN;

	modem_data->ref_flag++;
	MODEM_PRT("modem_open ,ref(%d)\n",modem_data->ref_flag);
out:
	mutex_unlock(&modem_data->muxlock);
	return ret;
}

static int modem_release(struct inode *ip, struct file* fp)
{
	struct miscdevice  * misc = (struct miscdevice *)(fp->private_data);
	struct comip_modem_data *modem_data = container_of(misc,
					     		struct comip_modem_data, miscdev);

	mutex_lock(&modem_data->muxlock);
	MODEM_PRT("relese ref(%d)\n", modem_data->ref_flag);
	if (modem_data->ref_flag <= 1) {
		reset_modem(modem_data);
		modem_data->status = MODEM_RELEASE;
	}

	modem_data->ref_flag--;
	mutex_unlock(&modem_data->muxlock);
	return 0;
}

static long modem_ioctl(struct file* fp, unsigned int cmd, unsigned long arg)
{
	int left = 0;
	int ret = -1;
	void __user *argp = (void __user *)arg;
	struct modem_usr_info usr_info;
	struct modem_nvram_info nvram_info;
	struct modem_base_info* modem_base_info = NULL;
	struct get_info *pos = NULL,*n = NULL;
	struct miscdevice  * misc = (struct miscdevice *)(fp->private_data);
	struct comip_modem_data *modem_data = container_of(misc,
					     			struct comip_modem_data, miscdev);

	modem_base_info = &modem_data->modem_base_info;

	switch (cmd) {
		case GET_NVRAM_INFO:
			nvram_info.addr = modem_data->modem_base_info.nvram_addr;
			nvram_info.size = modem_data->modem_base_info.nvram_size;
			left = copy_to_user(argp, &nvram_info, sizeof(struct modem_nvram_info));
			if (left > 0) {
				MODEM_ERR("GET_NVRAM_INFO error.left %d bytes.\n",left);
			}else
				ret = 0;
			break;

		case TRIGGER_MODEM_RUN:
			/*set modem out of reset mode and run.*/
			ret = run_modem(modem_data);
			break;

		case TRIGGER_MODEM_RESET:
			ret = reset_modem(modem_data);
			break;

		case GET_INFO:
			left = copy_from_user(&usr_info, argp, sizeof(struct modem_usr_info));
			if (left > 0)
				MODEM_ERR("GET_INFO error.left %d bytes.\n",left);
			else{
				list_for_each_entry_safe(pos, n, &modem_base_info->get_info_head, list){
					if (pos->info_id == usr_info.info_id) {
						if (pos->info.type == ADDR_MEM) {
							usr_info.addr = pos->info.addr - MODEM_MEMORY_BEGIN;
						}else
							MODEM_ERR("GET_INFO type error: %d\n",pos->info.type);
						usr_info.len = pos->info.len;
						left = copy_to_user(argp, &usr_info, sizeof(struct modem_usr_info));
						if (left > 0)
							MODEM_ERR("GET_INFO error.left1 %d bytes.\n",left);
						else
							ret = 0;
						break;
					}
				}
			}
			break;

		default:
			MODEM_ERR("Unknow ioctl cmd : %d\n",cmd);
			break;
	}
	return ret;
}


static ssize_t modem_read(struct file *fp, char __user *buf,
									size_t count, loff_t *pos)
{
	unsigned long left_count = 0;
	struct miscdevice *misc = (struct miscdevice *)(fp->private_data);
	struct comip_modem_data *modem_data = container_of(misc,
					     			struct comip_modem_data, miscdev);
	if ((*pos + count) > modem_data->mem_len)
		count = modem_data->mem_len - *pos;

	if (count <= 0) {
		MODEM_ERR("read count < 0\n");
		return -EACCES;
	}
	left_count = copy_to_user(buf, modem_data->mem_base + *pos, count);
	*pos = *pos + count - left_count;
	return (count - left_count);
}

static ssize_t modem_write(struct file *fp, const char __user *buf,
								size_t count, loff_t *pos)
{
	unsigned long left_count = 0;
	struct miscdevice *misc = (struct miscdevice *)(fp->private_data);
	struct comip_modem_data *modem_data = container_of(misc,
					     			struct comip_modem_data, miscdev);

	if ((*pos + count) > modem_data->mem_len){
		MODEM_ERR("write len more than mem_len\n");
		return -EACCES;
	}

	left_count = copy_from_user(modem_data->mem_base + *pos, buf, count);
	*pos = *pos + count - left_count;
	return (count-left_count);
}

static int modem_mmap (struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;
	struct miscdevice *misc = (struct miscdevice *)(file->private_data);
	struct comip_modem_data *modem_data = container_of(misc,
					     			struct comip_modem_data, miscdev);
	unsigned long map_size = vma->vm_end - vma->vm_start;

	if (((vma->vm_pgoff << PAGE_SHIFT) + map_size) > modem_data->mem_len) {
		MODEM_ERR("mmap offset = 0x%lx ,len = 0x%lx  > modem_data->mem_len !\n",
						vma->vm_pgoff<<PAGE_SHIFT, map_size);
		return -ENOMEM;
	}

	ret = remap_pfn_range(vma, (unsigned long)vma->vm_start,
								vma->vm_pgoff,map_size, pgprot_noncached(PAGE_SHARED));

	if (ret)
		MODEM_ERR( "mmap offset = 0x%lx ,len = 0x%lx failed!\n",
						vma->vm_pgoff << PAGE_SHIFT, map_size);
	else
		MODEM_PRT( "mmap offset = 0x%lx ,len = 0x%lx  ok!\n",
						vma->vm_pgoff << PAGE_SHIFT, map_size);
	return ret;
}

static loff_t modem_llseek(struct file *file, loff_t offset, int origin)
{
	switch (origin) {
		case SEEK_END:
			MODEM_ERR("llseek origin SEEK_END!\n");
			return -EINVAL;
		case SEEK_CUR:
			file->f_pos += file->f_pos + offset;
			break;
		default:
			file->f_pos = offset;
	}
	return file->f_pos;
}

static struct file_operations modem_fops = {
	.owner = THIS_MODULE,
	.open = modem_open,
	.release = modem_release,
	.unlocked_ioctl = modem_ioctl,
	.read = modem_read,
	.write = modem_write,
	.mmap = modem_mmap,
	.llseek = modem_llseek,
};

#ifdef CONFIG_MTD
static void mtd_modem_notify_add(struct mtd_info *mtd)
{
	struct comip_modem_data *modem_data = Get_modem_data();
	if (!modem_data) {
		MODEM_ERR("Get_modem_data error.\n");
		return;
	}

	if (!strcmp(mtd->name,modem_data->arm0_partition_name))
		modem_data->arm0_mtd = mtd;
	else if (!strcmp(mtd->name,modem_data->zsp0_partition_name))
		modem_data->zsp0_mtd = mtd;
	else if (!strcmp(mtd->name,modem_data->zsp1_partition_name))
		modem_data->zsp1_mtd = mtd;

	return;
}

static void mtd_modem_notify_remove(struct mtd_info *mtd)
{
	struct comip_modem_data *modem_data = Get_modem_data();
	if (mtd == modem_data->arm0_mtd) {
		modem_data->arm0_mtd = NULL;
		MODEM_INFO("Unbound from %s\n", mtd->name);
	}
}

static struct mtd_notifier mtd_modem_notifier = {
	.add = mtd_modem_notify_add,
	.remove	= mtd_modem_notify_remove,
};
#endif

static ssize_t modem_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice  * misc;
	struct comip_modem_data *modem_data = NULL;

	misc = dev_get_drvdata(dev);
	modem_data = container_of(misc, struct comip_modem_data, miscdev);

	return sprintf(buf, "modem_status %d\n", modem_data->status);
}

static ssize_t modem_status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long state;
	struct miscdevice  * misc;
	struct comip_modem_data *modem_data = NULL;
	struct modem_bridge_info *bridge_info = NULL;

	state = simple_strtoul(buf, NULL, 10);

	misc = dev_get_drvdata(dev);
	modem_data = container_of(misc, struct comip_modem_data, miscdev);

	switch (state) {
		case MODEM_RUNNING:
			if (modem_data->status >= MODEM_OPEN) {
				printk(KERN_ERR "modem status(%d) != MODEM_RELEASE.open modem error! \n",modem_data->status);
				return -EIO;
			}
			/*if(modem_data->run_modem_count == 0){*/
			if (modem_data->img_flag&IN_MTD_NANDFLASH) {
				#ifdef CONFIG_MTD
				if (parser_mtd_modem_img_head(modem_data))
					return -EAGAIN;
				#else
				printk(KERN_ERR "board config error.\n");
				return -EIO;
				#endif
			}else if (modem_data->img_flag&IN_BLOCK_EMMC) {
				if (parser_emmc_modem_img_head(modem_data))
					return -EAGAIN;
			}else{
				printk("unknow modem img_flag.\n");
			}

			/*add a selfdefined  bridge for test.use bridge 2 tx buf.*/
			bridge_info = kzalloc(sizeof(struct modem_bridge_info), GFP_KERNEL);
			if (!bridge_info) {
				return -ENOMEM;
			}
			strcpy(bridge_info->name,"bg_test");
			bridge_info->rx_id = bridge_info->tx_id = 7;
			bridge_info->rx_addr = 0x18B0000;
			bridge_info->tx_addr = bridge_info->rx_addr ;
			bridge_info->rx_len = 0x10000;
			bridge_info->tx_len = 0x10000;
			list_add_tail(&bridge_info->list, &modem_data->modem_base_info.bridge_head);
			modem_data->modem_base_info.bridge_num++;
			modem_data->status = MODEM_OPEN;

			run_modem(modem_data);
			break;

       case MODEM_IN_RESET:
	   		reset_modem(modem_data);
	   		break;

       default:
	   		printk(KERN_ERR "Invalid argument!\n");
			break;
	}
	return size;
}

static ssize_t modem_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}


static ssize_t modem_info_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	struct comip_modem_data *modem_data = NULL;

	modem_data = dev_get_drvdata(dev);
	return sprintf(buf, "======================\n"
					"modem mem len = 0x%x\n"
					"modem irq begin = 0x%x\n"
					"modem irq end = 0x%x\n"
					"run modem count = 0x%x\n"
					"modem_status = 0x%x\n"
					"modem bbid = 0x%x\n"
					"tag_version = 0x%x\n"
					"modem_version = 0x%x\n"
					"modem used len = 0x%x\n"
					"nvram_addr = 0x%x\n"
					"nvram_size = 0x%x\n"
					"bridge_num = 0x%x\n"
					"img_num =  0x%x\n"
					"set_info_num = 0x%x\n"
					"get_info_num = 0x%x\n"
					"======================\n",
					modem_data->mem_len,
					modem_data->irq_begin,
					modem_data->irq_end,
					modem_data->run_count,
					modem_data->status,
					modem_data->bbid,
					modem_data->modem_base_info.tag_version,
					modem_data->modem_base_info.modem_version,
					modem_data->modem_base_info.modem_ram_size,
					modem_data->modem_base_info.nvram_addr,
					modem_data->modem_base_info.nvram_size,
					modem_data->modem_base_info.bridge_num,
					modem_data->modem_base_info.img_num,
					modem_data->modem_base_info.set_info_num,
					modem_data->modem_base_info.get_info_num
	   				);
}

static ssize_t modem_bridge_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}


static ssize_t modem_bridge_info_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	struct comip_modem_data *modem_data = NULL;
	struct modem_base_info* modem_base_info = NULL;
	struct modem_bridge_info *pos,*n;
	char *buff = buf;

	modem_data = dev_get_drvdata(dev);
	modem_base_info = &modem_data->modem_base_info;

	buff += sprintf(buff, "modem_bridge_info_show begin:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->bridge_head, list){
		buff += sprintf(buff, "bridge name: %s\n",pos->name);
		buff += sprintf(buff, "bridge type: 0x%x\n",pos->type);
		buff += sprintf(buff, "bridge tx_id: 0x%x\n",pos->tx_id);
		buff += sprintf(buff, "bridge tx_addr: 0x%x\n",pos->tx_addr);
		buff += sprintf(buff, "bridge tx_len: 0x%x\n",pos->tx_len);
		buff += sprintf(buff, "bridge rx_id: 0x%x\n",pos->rx_id);
		buff += sprintf(buff, "bridge rx_addr: 0x%x\n",pos->rx_addr);
		buff += sprintf(buff, "bridge rx_len: 0x%x\n",pos->rx_len);
		if (pos->property.pro_b.packet) {
			buff += sprintf(buff, "bridge rx_pkt_maxnum: 0x%x\n",pos->rx_pkt_maxnum);
			buff += sprintf(buff, "bridge rx_pkt_maxsize: 0x%x\n",pos->rx_pkt_maxsize);
			buff += sprintf(buff, "bridge tx_pkt_maxnum: 0x%x\n",pos->tx_pkt_maxnum);
			buff += sprintf(buff, "bridge tx_pkt_maxsize: 0x%x\n",pos->tx_pkt_maxsize);
		}
		if (pos->property.pro_b.flow_ctl) {
			buff += sprintf(buff, "bridge rx_ctl_id: 0x%x\n",pos->rx_ctl_id);
			buff += sprintf(buff, "bridge tx_ctl_id: 0x%x\n",pos->tx_ctl_id);
		}
		buff += sprintf(buff, "\n");
	}
	buff += sprintf(buff, "modem_bridge_info_show end:\n");
	return (buff - buf);
}

static ssize_t modem_img_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}


static ssize_t modem_img_info_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	struct comip_modem_data *modem_data = NULL;
	struct modem_base_info* modem_base_info = NULL;
	struct modem_image_info *pos,*n;
	char *buff = buf;

	modem_data = dev_get_drvdata(dev);
	modem_base_info = &modem_data->modem_base_info;

	buff += sprintf(buff, "modem_img_info_show begin:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->img_info_head, list){
		buff += sprintf(buff, "modem_img img_id: 0x%x\n",pos->img_id);
		buff += sprintf(buff, "modem_img bin_offset: 0x%x\n",pos->bin_offset);
		buff += sprintf(buff, "modem_img ram_offset: 0x%x\n",pos->ram_offset);
		buff += sprintf(buff, "modem_img len: 0x%x\n",pos->len);
		buff += sprintf(buff, "\n");
	}
	buff += sprintf(buff, "modem_img_info_show end:\n");
	return (buff - buf);
}

static ssize_t modem_set_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}


static ssize_t modem_set_info_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	struct comip_modem_data *modem_data;
	struct modem_base_info* modem_base_info;
	struct set_info *pos,*n;
	char *buff = buf;

	modem_data = dev_get_drvdata(dev);
	modem_base_info = &modem_data->modem_base_info;

	buff += sprintf(buff, "modem_set_info_show begin:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->set_info_head, list)
		buff += sprintf(buff, "modem_set_info addr:0x%x, addr_type:0x%x, value:0x%x\n",
					pos->addr, pos->addr_type, pos->value);
	buff += sprintf(buff, "modem_set_info_show end:\n");
	return (buff - buf);
}

static ssize_t modem_get_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t modem_get_info_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	struct comip_modem_data *modem_data;
	struct modem_base_info* modem_base_info;
	struct get_info *pos,*n;
	char *buff = buf;

	modem_data = dev_get_drvdata(dev);
	modem_base_info = &modem_data->modem_base_info;

	buff += sprintf(buff, "modem_get_info_show:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->get_info_head, list)
		buff += sprintf(buff, "modem_get_info info_id:0x%x, addr_type:0x%x, addr:0x%x, len:0x%x\n",
					pos->info_id, pos->info.type, pos->info.addr, pos->info.len);
	buff += sprintf(buff, "modem_get_info_show end\n");
	return (buff - buf);
}

static ssize_t add_test_bridge_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct comip_modem_data *modem_data = NULL;
	struct modem_bridge_info *bridge_info = NULL;
	unsigned char __iomem *membase = NULL;
	char buff[256] = {0}, *b = NULL;
	char *set = NULL;
	u32 i = 0;

	modem_data = dev_get_drvdata(dev);

	/*add a selfdefined  bridge for test.*/
	bridge_info = kzalloc(sizeof(struct modem_bridge_info), GFP_KERNEL);
	if (!bridge_info){
		MODEM_ERR("malloc bridge_info err!\n");
		return -ENOMEM;
	}
	strlcpy(buff, buf, sizeof(buff));
	b = strim(buff);
	strcpy(bridge_info->name,"bg_test");
	while (b) {
		set = strsep(&b, ",");
		if (!set)
			continue;
		MODEM_INFO("set:%s!\n", set);
		switch(i) {
			case 0:
				bridge_info->type = (Bridge_Type)simple_strtoul(set, NULL, 0);
				break;
			case 1:
				bridge_info->rx_id = simple_strtoul(set, NULL, 0);
				bridge_info->tx_id = bridge_info->rx_id;
				break;
			case 2:
				bridge_info->rx_addr = simple_strtoul(set, NULL, 0);
				bridge_info->tx_addr = bridge_info->rx_addr;
				break;
			case 3:
				bridge_info->rx_len = simple_strtoul(set, NULL, 0);;
				bridge_info->tx_len = bridge_info->rx_len;
				break;
			case 4:
				bridge_info->rx_ctl_id = simple_strtoul(set, NULL, 0);
				bridge_info->tx_ctl_id = bridge_info->rx_ctl_id;
				break;
			case 5:
				bridge_info->rx_pkt_maxnum = simple_strtoul(set, NULL, 0);
				bridge_info->tx_pkt_maxnum = bridge_info->rx_pkt_maxnum;
				break;
			case 6:
				bridge_info->rx_pkt_maxsize = simple_strtoul(set, NULL, 0);;
				bridge_info->tx_pkt_maxsize = bridge_info->rx_pkt_maxsize;
				break;
			default:
				break;
			}
		i++;
	}
	if(bridge_info->type != LS_CHAR)
		bridge_info->property.pro = (1 << 0) | (1 << 1);
	list_add_tail(&bridge_info->list, &modem_data->modem_base_info.bridge_head);
	modem_data->modem_base_info.bridge_num++;

	membase = ioremap(bridge_info->rx_addr, bridge_info->rx_len);
	memset(membase, 0, bridge_info->rx_len);
	iounmap(membase);

	register_comip_bridge(bridge_info, modem_data->miscdev.parent);

	return size;
}

static ssize_t add_test_bridge_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(modem_status, 0600, modem_status_show, modem_status_store);
static DEVICE_ATTR(modem_info, 0600, modem_info_show, modem_info_store);

static DEVICE_ATTR(bridge_info, 0600, modem_bridge_info_show,
							modem_bridge_info_store);
static DEVICE_ATTR(img_info, 0600, modem_img_info_show, modem_img_info_store);
static DEVICE_ATTR(set_info, 0600, modem_set_info_show,
							modem_set_info_store);
static DEVICE_ATTR(get_info, 0600, modem_get_info_show, modem_get_info_store);

static DEVICE_ATTR(add_test_bridge, 0600, add_test_bridge_show, add_test_bridge_store);


static int __init comip_modem_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res = NULL;
	struct modem_base_info* base_info = NULL;
	struct comip_modem_data *modem_data = NULL;
	struct modem_platform_data *pdata = NULL;

	if (pdev->id != -1) {
		MODEM_ERR("device id(%d) is invalid \n", pdev->id);
		return -ENXIO;
	}
	pdata = pdev->dev.platform_data;
	if (!pdata) {
		MODEM_ERR("platform is null \n");
		return -EIO;
	}

	modem_data = kzalloc(sizeof(struct comip_modem_data) , GFP_KERNEL);
	if (!modem_data) {
		MODEM_ERR("kzalloc fail \n");
		return -ENOMEM;
	}
	dev_set_drvdata(&pdev->dev, (void *)modem_data);

	modem_data->img_flag = pdata->img_flag;
	modem_data->arm_partition_name = pdata->arm_partition_name;
	modem_data->zsp_partition_name = pdata->zsp_partition_name;

	base_info = &modem_data->modem_base_info;
	memset(base_info, 0, sizeof(struct modem_base_info));
	INIT_LIST_HEAD(&base_info->bridge_head);
	INIT_LIST_HEAD(&base_info->img_info_head);
	INIT_LIST_HEAD(&base_info->set_info_head);
	INIT_LIST_HEAD(&base_info->get_info_head);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res){
		MODEM_ERR("get memory resource failed\n");
		ret = -EIO;
		goto  res_err;
	}
	modem_data->mem_len = res->end - res->start + 1;
	modem_data->mem_base = ioremap(res->start, modem_data->mem_len);
	if (!modem_data->mem_base){
		MODEM_ERR("memory address is invalid \n");
		ret = -ENOMEM;
		goto  remap_err;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res){
		MODEM_ERR("get irq resource failed\n");
		ret = -EIO;
		goto  res_err;
	}
	modem_data->irq_begin = res->start;
	modem_data->irq_end = res->end;

	modem_data->status = MODEM_RELEASE;
	modem_data->run_count = 0;
	modem_data->ref_flag = 0;
	modem_data->bbid = COMIP_BB_ID;
	modem_data->miscdev.minor = MISC_DYNAMIC_MINOR;
	modem_data->miscdev.parent = &pdev->dev;
	modem_data->miscdev.name = "modem";
	modem_data->miscdev.fops = &modem_fops;

	ret = misc_register(&modem_data->miscdev);
	if (ret) {
		MODEM_ERR("register modem misc dev failed!\n");
		goto misc_err;
	}

	ret = device_create_file(modem_data->miscdev.this_device, &dev_attr_modem_status);
	if (ret) {
		MODEM_ERR("creat modem_status device file failed!\n");
		goto require_dev_attr_modem_status_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_modem_info);
	if (ret) {
		MODEM_ERR("creat dev_attr_modem_info device file failed!\n");
		goto require_dev_attr_modem_info_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_bridge_info);
	if (ret) {
		MODEM_ERR("creat dev_attr_bridge_info device file failed!\n");
		goto require_dev_attr_bridge_info_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_img_info);
	if (ret) {
		MODEM_ERR("creat dev_attr_img_info device file failed!\n");
		goto require_dev_attr_img_info_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_set_info);
	if (ret) {
		MODEM_ERR("creat dev_attr_set_info device file failed!\n");
		goto require_dev_attr_set_info_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_get_info);
	if (ret) {
		MODEM_ERR("creat dev_attr_get_info device file failed!\n");
		goto require_dev_attr_get_info_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_add_test_bridge);
	if (ret) {
		MODEM_ERR("creat dev_attr_add_test_bridge device file failed!\n");
		goto require_dev_attr_add_test_bridge;
	}

	mutex_init(&modem_data->muxlock);
	Set_modem_data(modem_data);

#ifdef CONFIG_MTD
	modem_data->arm0_mtd = NULL;
	modem_data->zsp0_mtd = NULL;
	modem_data->zsp1_mtd = NULL;
	register_mtd_user(&mtd_modem_notifier);
#endif

	MODEM_PRT("devices driver up.\n");
	return ret;

require_dev_attr_add_test_bridge:
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_get_info);
require_dev_attr_get_info_err:
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_set_info);
require_dev_attr_set_info_creat_err:
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_img_info);
require_dev_attr_img_info_creat_err:
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_bridge_info);
require_dev_attr_bridge_info_creat_err:
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_modem_info);
require_dev_attr_modem_info_creat_err:
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_modem_status);
require_dev_attr_modem_status_creat_err:
	misc_deregister(&modem_data->miscdev);
misc_err:
remap_err:
res_err:
	kfree(modem_data);

	return ret;
}

static int __exit comip_modem_remove(struct platform_device *pdev)
{
	struct modem_bridge_info * bridge_info = NULL;
	struct modem_image_info * img_info = NULL;
	struct set_info	* set_info = NULL;
	struct get_info	* get_info = NULL;
	struct comip_modem_data *modem_data = NULL;

	modem_data= (struct comip_modem_data *)dev_get_drvdata(&pdev->dev);
#ifdef CONFIG_MTD
	unregister_mtd_user(&mtd_modem_notifier);
#endif
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_add_test_bridge);
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_get_info);
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_set_info);
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_img_info);
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_bridge_info);
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_modem_info);
	device_remove_file(modem_data->miscdev.this_device, &dev_attr_modem_status);
	misc_deregister(&modem_data->miscdev);


	while (!list_empty(&modem_data->modem_base_info.bridge_head)) {
		bridge_info = list_first_entry(&modem_data->modem_base_info.bridge_head, struct modem_bridge_info, list);
		list_del(&bridge_info->list);
		kfree(bridge_info);
	}

	while (!list_empty(&modem_data->modem_base_info.img_info_head)) {
		img_info = list_first_entry(&modem_data->modem_base_info.img_info_head, struct modem_image_info, list);
		list_del(&img_info->list);
		kfree(img_info);
	}

	while (!list_empty(&modem_data->modem_base_info.set_info_head)) {
		set_info = list_first_entry(&modem_data->modem_base_info.set_info_head, struct set_info, list);
		list_del(&set_info->list);
		kfree(set_info);
	}

	while (!list_empty(&modem_data->modem_base_info.get_info_head)) {
		get_info = list_first_entry(&modem_data->modem_base_info.get_info_head, struct get_info, list);
		list_del(&get_info->list);
		kfree(get_info);
	}

	mutex_destroy(&modem_data->muxlock);
	kfree(modem_data);
	MODEM_PRT("devices driver remove.\n");
	return 0;
}


static struct platform_driver comip_modem_driver = {
        .remove = __exit_p(comip_modem_remove),
        .driver = {
                .name = "comip-modem",
        },
};

static int __init modem_init(void)
{
	return platform_driver_probe(&comip_modem_driver, comip_modem_probe);
}

static void __exit modem_exit(void)
{
	platform_driver_unregister(&comip_modem_driver);
}

late_initcall(modem_init);
module_exit(modem_exit);

MODULE_AUTHOR(ken xue <xuekun@leadcoretech.com>);
MODULE_DESCRIPTION("modem&&bridge driver for lc186X");
MODULE_LICENSE("GPL");
MODULE_ALIAS("comip modem&&bridge");

