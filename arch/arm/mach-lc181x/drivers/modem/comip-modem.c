/*
 * LC1810 modem driver
 *
 * Copyright (C) 2011 Leadcore Corporation
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
#include <plat/tpz.h>


#define COMIP_MODEM_DEBUG	1
#ifdef COMIP_MODEM_DEBUG
#define MODEM_PRT(format, args...)  printk(KERN_DEBUG "[COMIP Modem] " format, ## args)
#else
#define MODEM_PRT(x...)  do{} while(0)
#endif

#define ROM_LEN		0x10000
#define CHIP_ID_OFFSET	0xFFFC
#define SECURITY_OFFSET 0x80
#define CHIP_ID_ECO1_2	0x24211000
static Comip_Modem_Data * comip_modem_data=NULL;


static void
Set_modem_data(Comip_Modem_Data *modem_data){
	comip_modem_data = modem_data;
	return ;
}

Comip_Modem_Data * 	Get_modem_data( void ){
	return comip_modem_data;
}

static void
add_bridge_devices(Comip_Modem_Data *modem_data){
	Modem_Bridge_Info *pos,*n;
	unsigned char __iomem *membase;
	Modem_Base_Info* modem_base_info = &modem_data->modem_base_info;

	list_for_each_entry_safe(pos, n,  &modem_base_info->bridge_head, list){

		membase = ioremap( pos->rx_addr,pos->rx_len);
		memset(membase,0,pos->rx_len);
		iounmap(membase);

		membase = ioremap( pos->tx_addr,pos->tx_len);
		memset(membase,0,pos->tx_len);
		iounmap(membase);

		pos->debug_info = (u32)modem_base_info->bridge_debug_info;

		MODEM_PRT("add bridge %s \n",pos->name);
		register_comip_bridge(pos,modem_data->modem_miscdev.parent,NULL);
	}
}

#if 0
static void
delete_bridge_devices(Comip_Modem_Data *modem_data){
	Modem_Base_Info* modem_base_info = &modem_data->modem_base_info;
	Modem_Bridge_Info *pos,*n;

	list_for_each_entry_safe(pos, n,  &modem_base_info->bridge_head, list){
		MODEM_PRT("delete bridge %s \n",pos->name);
		deregister_comip_bridge(pos);
	}
}
#endif
static inline void get_tpz_config(Modem_Base_Info* modem_base_info, u32 size)
{
	MODEM_PRT("want ap monitor start 0x%x, size 0x%x\n",
		modem_base_info->ap_monitor_flag, size);

	if ((modem_base_info->ap_monitor_base_addr >= MODEM_MEMORY_BASE) &&
		((modem_base_info->ap_monitor_base_addr + size)
		<= (MODEM_MEMORY_BASE + MODEM_MEMORY_SIZE))){
			if (modem_base_info->ap_monitor_base_addr % 0x100000){
				modem_base_info->ap_monitor_base_addr = (modem_base_info->ap_monitor_base_addr
				+ 0x100000) & 0xFFF00000;
			}
			modem_base_info->ap_monitor_end_addr = modem_base_info->ap_monitor_base_addr +
				size;
			if (modem_base_info->ap_monitor_end_addr / 0x100000 == 0){
				printk(KERN_WARNING "[COMIP Modem] ap monitor size too small!\n");
				modem_base_info->ap_monitor_end_addr = modem_base_info->ap_monitor_base_addr;
				return;
			}
			if ((modem_base_info->ap_monitor_end_addr / 0x100000 == 1) &&
					(modem_base_info->ap_monitor_base_addr == 0x100000)){
				printk(KERN_WARNING "[COMIP Modem] ap monitor can not start!\n");
				modem_base_info->ap_monitor_end_addr = modem_base_info->ap_monitor_base_addr;
				return;
			}
			if (modem_base_info->ap_monitor_end_addr % 0x100000){
				modem_base_info->ap_monitor_end_addr = ( modem_base_info->ap_monitor_end_addr
					- 0x100000 ) & 0xFFF00000;
			}
	}else{
		modem_base_info->ap_monitor_end_addr = modem_base_info->ap_monitor_base_addr = 0;
	}
	MODEM_PRT("ap monitor start 0x%x, end 0x%x\n",
		modem_base_info->ap_monitor_base_addr, modem_base_info->ap_monitor_end_addr);
}
static inline int image_head_parser(void *source,Modem_Base_Info* modem_base_info,unsigned long source_len){
	tag_header * header;
	Setting_Info *setting;
	Extern_Info *ext_info;
	Modem_Image_Info *img_info;
	Modem_Bridge_Info *bridge_info;
	Comip_Modem_Data *modem_data;
	u32 tag_len=0;
	bool  no_offset_setting = false;
	bool  no_abs_setting = false;
	bool  no_offset_ext_info = false;
	bool  no_abs_ext_info = false;
	u32 tpz_ap_monitor_size = 0;
	header = (tag_header *)source;

	modem_data = container_of(modem_base_info,Comip_Modem_Data, modem_base_info);

	for (; (header->tag != ATAG_NONE); header = tag_next(header)){
		tag_len +=header->size;
		if(tag_len > source_len){
			printk(KERN_ERR "MODEM image head parser ERROR. tag_len = %d \n",tag_len);
			return -1;
		}

		if(((u32)header - (u32)source) > source_len){
			printk(KERN_ERR "MODEM image head parser ERROR. (header - source) > source_len \n");
			return -1;
		}

		switch(header->tag){
			case ATAG_VER:
				modem_base_info->atag_version = ((tag_ver *)header)->version;
				break;

			case ATAG_ID:
				modem_data->td_rfid = ((tag_modem_id *)header)->td_rfid;
				modem_data->gsm_rfid = ((tag_modem_id *)header)->gsm_rfid;
				modem_data->gsm_dual_rfid = ((tag_modem_id *)header)->second_gsm_rfid;
				modem_data->bbid = ((tag_modem_id *)header)->bbid;
				break;

			case ATAG_INFO:
				modem_base_info->modem_version = ((tag_info *)header)->modem_version;
				modem_base_info->modem_ram_size = ((tag_info *)header)->modem_ram_size;
				modem_base_info->nvram_addr = ((tag_info *)header)->nvram_addr;
				modem_base_info->nvram_size = ((tag_info *)header)->nvram_size;
				break;

			case ATAG_IMAGE:
				img_info = kzalloc(sizeof(Modem_Image_Info), GFP_KERNEL);
				if (!img_info){
					return -ENOMEM;
				}

				img_info->bin_offset = ((tag_image *)header)->bin_offset;
				img_info->ram_offset = ((tag_image *)header)->ram_addr;
				img_info->len = ((tag_image *)header)->len;
				#if 0
				if(strstr(header->tag_name,"arm0")){
					img_info->img_id = ARM0_IMG_ID;
				}else if(strstr(header->tag_name,"zsp0")){
					img_info->img_id = ZSP0_IMG_ID;
				}else if(strstr(header->tag_name,"zsp1")){
					img_info->img_id = ZSP1_IMG_ID;
				}
				#endif
				img_info->img_id = ((tag_image *)header)->img_id;
				list_add_tail(&img_info->list, &modem_base_info->img_info_head);
				modem_base_info->img_num++;
				break;

			case ATAG_BRIDGE:
				bridge_info = kzalloc(sizeof(Modem_Bridge_Info), GFP_KERNEL);
				if (!bridge_info){
					return -ENOMEM;
				}
				strcpy(bridge_info->name, ((tag_bridge *)header)->name);
				bridge_info->irq_id = ((tag_bridge *)header)->fiq_id;
				bridge_info->rx_len = ((tag_bridge *)header)->rx_len;
				bridge_info->rx_addr =((tag_bridge *)header)->rx_addr;
				bridge_info->tx_len = ((tag_bridge *)header)->tx_len;
				bridge_info->tx_addr =((tag_bridge *)header)->tx_addr;
				list_add_tail(&bridge_info->list, &modem_base_info->bridge_head);
				modem_base_info->bridge_num++;
				break;

			case ATAG_OFFSET_SETTING:
				setting = kzalloc(sizeof(Setting_Info), GFP_KERNEL);
				if (!setting){
					return -ENOMEM;
				}
				setting->offset =  ((tag_value *)header)->addr;
				setting->value =  ((tag_value *)header)->value;
				list_add_tail(&setting->list, &modem_base_info->offset_setting_head);
				modem_base_info->offset_setting_num++;
				break;

			case ATAG_ABS_SETTING:
				setting = kzalloc(sizeof(Setting_Info), GFP_KERNEL);
				if (!setting){
					return -ENOMEM;
				}
				setting->offset =  ((tag_value *)header)->addr;
				setting->value =  ((tag_value *)header)->value;
				list_add_tail(&setting->list, &modem_base_info->abs_setting_head);
				modem_base_info->abs_setting_num++;
				break;

			case ATAG_OFFSET_EXT_INFO:
				ext_info = kzalloc(sizeof(Extern_Info), GFP_KERNEL);
				if (!ext_info){
					return -ENOMEM;
				}
				ext_info->info_id =  ((tag_ext_info *)header)->info_id;
				ext_info->offset =  ((tag_ext_info *)header)->ext_info_addr;
				ext_info->len = ((tag_ext_info *)header)->ext_info_size;
				list_add_tail(&ext_info->list, &modem_base_info->offset_ext_info_head);
				modem_base_info->offset_ext_info_num++;

				if(ext_info->info_id == TD_RFID_EXT_INFO_ID 
					|| ext_info->info_id == GSM_RFID_EXT_INFO_ID
					|| ext_info->info_id == DBB_ID_EXT_INFO_ID
					|| ext_info->info_id == GSM_DUAL_RFID_EXT_INFO_ID){
					setting = kzalloc(sizeof(Setting_Info), GFP_KERNEL);
					if (!setting){
						return -ENOMEM;
					}
					setting->offset = ext_info->offset;
					switch(ext_info->info_id){
						case TD_RFID_EXT_INFO_ID:
							setting->value = modem_data->td_rfid ;
							break;

						case GSM_RFID_EXT_INFO_ID:
							setting->value = modem_data->gsm_rfid;
							break;

						case DBB_ID_EXT_INFO_ID:
							setting->value = modem_data->bbid;
							break;

						case GSM_DUAL_RFID_EXT_INFO_ID:
							setting->value = modem_data->gsm_dual_rfid;
							break;

						default:
							break;
					}
					list_add_tail(&setting->list, &modem_base_info->offset_setting_head);
					modem_base_info->offset_setting_num++;
				}

				break;

			case ATAG_ABS_EXT_INFO:
				ext_info = kzalloc(sizeof(Extern_Info), GFP_KERNEL);
				if (!ext_info){
					return -ENOMEM;
				}
				ext_info->info_id = ((tag_ext_info *)header)->info_id;
				ext_info->offset =  ((tag_ext_info *)header)->ext_info_addr;
				ext_info->len =  ((tag_ext_info *)header)->ext_info_size;
				list_add_tail(&ext_info->list, &modem_base_info->abs_ext_info_head);
				modem_base_info->abs_ext_info_num++;

				if(ext_info->info_id == MODEM_RESET_GENERAL_REG)
					modem_base_info->reset_addr = ext_info->offset;
				if(ext_info->info_id == MODEM_RESET_GENERAL_VALUE)
					modem_base_info->reset_val = ext_info->offset;
				if(ext_info->info_id == TC_EXC_STEP_INFO_ID){
					if(ext_info->offset > modem_data->mem_len){
						MODEM_PRT("invalid bridge debug info %x.\n",ext_info->offset);
					}else{
						modem_base_info->bridge_debug_info = modem_data->mem_base+ext_info->offset;
					}
				}
				if (ext_info->info_id == TPZ_AP_MONITOR_FLAG)
					modem_base_info->ap_monitor_flag = ext_info->offset;
				if (ext_info->info_id == TPZ_AP_MONITOR_BASE_ADDR)
					modem_base_info->ap_monitor_base_addr = ext_info->offset;
				if (ext_info->info_id == TPZ_AP_MONITOR_SIZE)
					tpz_ap_monitor_size = ext_info->offset;
				if (tpz_ap_monitor_size)
					get_tpz_config(modem_base_info, tpz_ap_monitor_size);

				if(ext_info->info_id == TD_RFID_EXT_INFO_ID
					|| ext_info->info_id == GSM_RFID_EXT_INFO_ID
					|| ext_info->info_id == DBB_ID_EXT_INFO_ID
					|| ext_info->info_id == GSM_DUAL_RFID_EXT_INFO_ID){
					setting = kzalloc(sizeof(Setting_Info), GFP_KERNEL);
					if (!setting){
						return -ENOMEM;
					}
					setting->offset = ext_info->offset;
					switch(ext_info->info_id){
						case TD_RFID_EXT_INFO_ID:
							setting->value = modem_data->td_rfid ;
							break;

						case GSM_RFID_EXT_INFO_ID:
							setting->value = modem_data->gsm_rfid;
							break;

						case DBB_ID_EXT_INFO_ID:
							setting->value = modem_data->bbid;
							break;

						case GSM_DUAL_RFID_EXT_INFO_ID:
							setting->value = modem_data->gsm_dual_rfid;
							break;

						default:
							break;
					}
					list_add_tail(&setting->list, &modem_base_info->abs_setting_head);
					modem_base_info->abs_setting_num++;
				}
				break;

			default:
				printk(KERN_WARNING
			       "Ignoring unrecognised tag (0x%08x) %s size = %x.\n",
			       header->tag,header->tag_name,header->size);
				if(header->size == 0){
					return -1;
				}
		}
	}
	no_offset_setting = modem_base_info->offset_setting_num == 0 ? true : false;
	no_abs_setting = modem_base_info->abs_setting_num == 0 ? true : false;
	no_offset_ext_info = modem_base_info->offset_ext_info_num == 0 ? true : false;
	no_abs_ext_info =  modem_base_info->abs_ext_info_num == 0 ? true : false;

	if ( no_offset_setting ||no_abs_setting ||no_offset_ext_info ||no_abs_ext_info )
	{
		header = (tag_header *)source;
		for (; (header->tag != ATAG_NONE); header = tag_next(header)){
		switch(header->tag){
			case ATAG_OFFSET_SETTING:
				if( no_abs_setting )
				{
					setting = kzalloc(sizeof(Setting_Info), GFP_KERNEL);
					if (!setting){
						return -ENOMEM;
					}
					setting->offset =  MODEM_MEMORY_BEGIN +((tag_value *)header)->addr;
					setting->value = ((tag_value *)header)->value;
					list_add_tail(&setting->list, &modem_base_info->abs_setting_head);
					modem_base_info->abs_setting_num++;
				}
				break;

			case ATAG_ABS_SETTING:
				if( no_offset_setting )
				{
					if( ((tag_value *)header)->addr >= MODEM_MEMORY_BEGIN )
					{
						setting = kzalloc(sizeof(Setting_Info), GFP_KERNEL);
						if (!setting){
							return -ENOMEM;
						}
						setting->offset =  ((tag_value *)header)->addr - MODEM_MEMORY_BEGIN;
						setting->value =	((tag_value *)header)->value;
						list_add_tail(&setting->list, &modem_base_info->offset_setting_head);
						modem_base_info->offset_setting_num++;
					}
					else
					{
						printk(KERN_ERR "error: ABS_SETTING addr less than begin!\n");
						return -EFAULT;
					}
				}
				break;

			case ATAG_OFFSET_EXT_INFO:
				if( no_abs_ext_info )
				{
					ext_info = kzalloc(sizeof(Extern_Info), GFP_KERNEL);
					if (!ext_info){
						return -ENOMEM;
					}
					ext_info->info_id = ((tag_ext_info *)header)->info_id;
					ext_info->offset =  MODEM_MEMORY_BEGIN + ((tag_ext_info *)header)->ext_info_addr;
					ext_info->len =  ((tag_ext_info *)header)->ext_info_size;
					list_add_tail(&ext_info->list, &modem_base_info->abs_ext_info_head);
					modem_base_info->abs_ext_info_num++;

					if(ext_info->info_id == TD_RFID_EXT_INFO_ID
						|| ext_info->info_id == GSM_RFID_EXT_INFO_ID
						|| ext_info->info_id == DBB_ID_EXT_INFO_ID
						|| ext_info->info_id == GSM_DUAL_RFID_EXT_INFO_ID){
						setting = kzalloc(sizeof(Setting_Info), GFP_KERNEL);
						if (!setting){
							return -ENOMEM;
						}
						setting->offset = ext_info->offset;
						switch(ext_info->info_id){
							case TD_RFID_EXT_INFO_ID:
								setting->value = modem_data->td_rfid;
								break;

							case GSM_RFID_EXT_INFO_ID:
								setting->value = modem_data->gsm_rfid;
								break;

							case DBB_ID_EXT_INFO_ID:
								setting->value = modem_data->bbid;
								break;

							case GSM_DUAL_RFID_EXT_INFO_ID:
								setting->value = modem_data->gsm_dual_rfid;
								break;

							default:
								break;
						}
						list_add_tail(&setting->list, &modem_base_info->abs_setting_head);
						modem_base_info->abs_setting_num++;
					}
				}
				break;

			case ATAG_ABS_EXT_INFO:
				if( no_offset_ext_info )
				{
					if( ((tag_ext_info *)header)->ext_info_addr >= MODEM_MEMORY_BEGIN )
					{
						ext_info = kzalloc(sizeof(Extern_Info), GFP_KERNEL);
						if (!ext_info){
							return -ENOMEM;
						}
						ext_info->info_id =  ((tag_ext_info *)header)->info_id;
						ext_info->offset =  ((tag_ext_info *)header)->ext_info_addr - MODEM_MEMORY_BEGIN;
						ext_info->len = ((tag_ext_info *)header)->ext_info_size;
						list_add_tail(&ext_info->list, &modem_base_info->offset_ext_info_head);
						modem_base_info->offset_ext_info_num++;

						if(ext_info->info_id == TD_RFID_EXT_INFO_ID
							|| ext_info->info_id == GSM_RFID_EXT_INFO_ID
							|| ext_info->info_id == DBB_ID_EXT_INFO_ID
							|| ext_info->info_id == GSM_DUAL_RFID_EXT_INFO_ID){
							setting = kzalloc(sizeof(Setting_Info), GFP_KERNEL);
							if (!setting){
								return -ENOMEM;
							}
							setting->offset = ext_info->offset;
							switch(ext_info->info_id){
								case TD_RFID_EXT_INFO_ID:
									setting->value = modem_data->td_rfid;
									break;

								case GSM_RFID_EXT_INFO_ID:
									setting->value = modem_data->gsm_rfid;
									break;

								case DBB_ID_EXT_INFO_ID:
									setting->value = modem_data->bbid;
									break;

								case GSM_DUAL_RFID_EXT_INFO_ID:
									setting->value = modem_data->gsm_dual_rfid;
									break;

								default:
									break;
							}
							list_add_tail(&setting->list, &modem_base_info->offset_setting_head);
							modem_base_info->offset_setting_num++;
							}
						}
						else
						{
							printk(KERN_ERR "error: ABS_EXT addr less than begin!\n");
							return -EFAULT;
						}
					}
				break;

			default:
				break;
			}
		}
	}
	return (source_len - tag_len);
}


#ifdef CONFIG_MTD
static int load_nandflash_modem_image(Comip_Modem_Data *modem_data){
	/*pay attention to img load sequence .refer to modem space definition.
	if arm0_target_offset>zsp1_target_offset>zsp0_target_offset  
	then ===> load sequence zsp0_img->zsp1_img->arm0_img*/
	/*fix me. care about bbt.*/
	size_t len=0;
	Modem_Image_Info *pos,*n;
	Modem_Base_Info* base_info;

	if(!modem_data)
		return -1;
	
	if(modem_data->modem_base_info.img_num == 0){
			printk(KERN_ERR "error: modem_data->modem_base_info.img_num == 0 !\n");
			return -1;
	}

	base_info = &modem_data->modem_base_info;

	list_for_each_entry_safe(pos, n,  &base_info->img_info_head, list){
		switch(pos->img_id){
			case ARM0_IMG_ID:
				if(!modem_data->arm0_mtd){
					printk(KERN_WARNING"not find arm0 mtd parttion.\n");
					break;
				}
				mtd_read(modem_data->arm0_mtd,modem_tag_len,
						pos->len,&len,modem_data->mem_base+pos->ram_offset);
				if(pos->len != len)
					printk(KERN_WARNING"failed to load ARM0 IMG.img len = %d,but we load %d ",pos->len,len);
				break;

			case ZSP0_IMG_ID:
				if(!modem_data->zsp0_mtd){
					printk(KERN_WARNING"not find zsp0 mtd parttion.\n");
					break;
				}
				mtd_read(modem_data->zsp0_mtd,0,
						pos->len,&len,modem_data->mem_base+pos->ram_offset);
				if(pos->len != len)
					printk(KERN_WARNING"failed to load ZSP0 IMG.img len = %d,but we load %d ",pos->len,len);
				break;

			case ZSP1_IMG_ID:
				if(!modem_data->zsp1_mtd){
					printk(KERN_WARNING"not find zsp1 mtd parttion.\n");
					break;
				}
				mtd_read(modem_data->zsp1_mtd,0,
						pos->len,&len,modem_data->mem_base+pos->ram_offset);
				if(pos->len != len)
					printk(KERN_WARNING"failed to load zsp1 IMG.img len = %d,but we load %d ",pos->len,len);
				break;

			default:
				break;
		}
	}

	return 0;
}


static int
parser_mtd_modem_img_head(Comip_Modem_Data *modem_data){
	/*pay attention to img load sequence .refer to modem space definition.if arm0_target_offset>zsp1_target_offset>zsp0_target_offset  then ===> load sequence zsp0_img->zsp1_img->arm0_img*/
	/*fix me. care about bbt.*/
	size_t len;
	char *head_tag=NULL;
	Modem_Base_Info* base_info=NULL;

	if(!modem_data)
		return -1;

	if(!modem_data->arm0_mtd)
		return -1;

	base_info = &(modem_data->modem_base_info);
	if( base_info->atag_version!=0 || base_info->modem_version != 0){
		MODEM_PRT("parser modem head twice.\n");
		return 0;
	}

	head_tag = kzalloc(modem_tag_len,GFP_KERNEL);
	/*parser image head.	*/
	mtd_read(modem_data->arm0_mtd,0,modem_tag_len,  &len, head_tag);
	MODEM_PRT("read modem tag head %d bytes.\n",len);
	if(modem_tag_len != len){
		printk(KERN_WARNING"failed to load modem tag head.img len = %d,but we load %d ",modem_tag_len,len);
		return -1;
	}

	image_head_parser(head_tag, base_info,modem_tag_len);
	kfree(head_tag);

	return 0;
}
#endif


static int
parser_emmc_modem_img_head(Comip_Modem_Data *modem_data){
	char path[100];
	char *head_tag=NULL;
	struct file *img_file=NULL;
	Modem_Base_Info* base_info=NULL;
	unsigned int len = 0;
	mm_segment_t old_fs;

	if(!modem_data)
		return -1;

	if(!strcmp(modem_data->arm0_partition_name , "")){
		printk(KERN_ERR"arm0_partition_name not defined.failed to load arm0 image.\n");
		return -1;
	}

	base_info = &(modem_data->modem_base_info);
	if( base_info->atag_version!=0 || base_info->modem_version != 0){
		MODEM_PRT("parser modem head twice.\n");
		return 0;
	}

	sprintf(path, "/dev/block/platform/comip-mmc.1/by-name/%s", modem_data->arm0_partition_name);
	img_file = filp_open(path, O_RDONLY, 0);
	if(img_file>0){
		head_tag = kzalloc(modem_tag_len,GFP_KERNEL);
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		len = img_file->f_op->read(img_file,head_tag, modem_tag_len,&img_file->f_pos);
		set_fs(old_fs);
		MODEM_PRT("modem tag head load 0x%x byte.\n",len);
		image_head_parser(head_tag,base_info,modem_tag_len);
		kfree(head_tag);
		filp_close(img_file,NULL);
	}else{
		printk("open %s failed.\n",path);
	}

	if(modem_tag_len != len){
		printk(KERN_WARNING"failed to load modem tag head.img len = %d,but we load %d ",modem_tag_len,len);
		return -1;
	}

	return 0;
}

static int
load_emmc_modem_image(Comip_Modem_Data *modem_data){
	char path[100];
	struct file *img_file = NULL;
	unsigned int len = 0;
	mm_segment_t old_fs;

	Modem_Base_Info* base_info = NULL;
	Modem_Image_Info * pos = NULL;
	Modem_Image_Info *n = NULL;

	base_info = &(modem_data->modem_base_info);

	if(base_info->img_num == 0){
			printk(KERN_ERR "error: modem_data->modem_base_info.img_num == 0 !\n");
			return -1;
	}

	list_for_each_entry_safe(pos, n,  &base_info->img_info_head, list){
		switch(pos->img_id){
			case ARM0_IMG_ID:
				len = 0;
				if(!strcmp(modem_data->arm0_partition_name , "")){
					printk(KERN_ERR"arm0_partition_name not defined.failed to load arm0 image.\n");
					break;
				}

				sprintf(path, "/dev/block/platform/comip-mmc.1/by-name/%s", modem_data->arm0_partition_name);
				img_file = filp_open(path, O_RDONLY, 0);
				if(img_file){
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					img_file->f_op->llseek(img_file,modem_tag_len,0);
					len = img_file->f_op->read(img_file,modem_data->mem_base+pos->ram_offset, pos->len,&img_file->f_pos);
					MODEM_PRT("arm0 img load 0x%x byte.\n",len);
					set_fs(old_fs);
					filp_close(img_file,NULL);
				}

				if(pos->len != len)
					printk(KERN_WARNING"failed to load ARM0 IMG.img len = %d,but we load %d \n",pos->len,len);
				break;

			case ZSP0_IMG_ID:
				len = 0;
				if(!strcmp(modem_data->zsp0_partition_name , "")){
					printk(KERN_ERR"zsp0_partition_name not defined.failed to load zsp0 image.\n");
					break;
				}
				sprintf(path, "/dev/block/platform/comip-mmc.1/by-name/%s", modem_data->zsp0_partition_name);
				img_file = filp_open(path, O_RDONLY, 0);
				if(img_file){
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					len = img_file->f_op->read(img_file,modem_data->mem_base+pos->ram_offset, pos->len,&img_file->f_pos);
					MODEM_PRT("zsp0 img load 0x%x byte.\n",len);
					set_fs(old_fs);
					filp_close(img_file,NULL);
				}

				if(pos->len != len)
					printk(KERN_WARNING"failed to load ZSP0 IMG.img len = %d,but we load %d \n",pos->len,len);
				break;

			case ZSP1_IMG_ID:
				len = 0;
				if(!strcmp(modem_data->zsp1_partition_name , "")){
					printk(KERN_ERR"zsp1_partition_name not defined.failed to load zsp1 image.\n");
					break;
				}
				sprintf(path, "/dev/block/platform/comip-mmc.1/by-name/%s", modem_data->zsp1_partition_name);
				img_file = filp_open(path, O_RDONLY, 0);
				if(img_file){
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					len = img_file->f_op->read(img_file,modem_data->mem_base+pos->ram_offset, pos->len,&img_file->f_pos);
					MODEM_PRT("zsp1 img load 0x%x byte.\n",len);
					set_fs(old_fs);
					filp_close(img_file,NULL);
				}
				if(pos->len != len)
					printk(KERN_WARNING"failed to load zsp1 IMG.img len = %d,but we load %d \n",pos->len,len);
				break;

			default:
				break;
		}
	}

	return 0;
}

void modem_parameter_setting(Comip_Modem_Data *modem_data){
	Modem_Base_Info* modem_base_info = &modem_data->modem_base_info;
	Setting_Info *pos,*n;
	unsigned int  __iomem * membase;

	list_for_each_entry_safe(pos, n,  &modem_base_info->offset_setting_head, list){
		*(u32 *)(modem_data->mem_base+pos->offset) = pos->value;
	}

	list_for_each_entry_safe(pos, n,  &modem_base_info->abs_setting_head, list){
		membase=  ioremap(pos->offset, 0x4);
		*membase = pos->value;
		iounmap(membase);
	}
}

static int reset_modem(Comip_Modem_Data *modem_data){
	MODEM_PRT("reset_modem (%d)\n",modem_data->run_modem_count);
	if( modem_data->run_modem_count == 0)
		return 0;

	//if(modem_data->modem_status != MODEM_RUNNING){
	//	printk(KERN_ERR "modem status(%d) != MODEM_RUNNING.\n reset modem error! \n",modem_data->modem_status);
	//	return -EIO;
	//}
	modem_data->modem_status = MODEM_IN_RESET;
	msleep(1000);
#if defined(CONFIG_CPU_LC1810)
	writel(2, io_p2v(AP_PWR_CP_RSTCTL));
#else
	writel(1, io_p2v(AP_PWR_CP_RSTCTL));
#endif
	msleep(100);

	memset(modem_data->mem_base,0,modem_data->mem_len);

	return 0;
}

static int run_modem(Comip_Modem_Data *modem_data){
	if(modem_data->modem_status == MODEM_RUNNING  || modem_data->modem_status == MODEM_RELEASE){
		printk(KERN_ERR "modem status(%d) == MODEM_RELEASE ||MODEM_RUNNING .run modem error! \n",modem_data->modem_status);
		return -EIO;
	}
	if (modem_data->modem_base_info.ap_monitor_base_addr !=
			modem_data->modem_base_info.ap_monitor_end_addr){
		MODEM_PRT("modem tpz disable\n");
		comip_tpz_enabe(TPZ_AP0, 0);
	}
	if( modem_data->run_modem_count == 0){
		add_bridge_devices(modem_data);
#if 0
		comip_trigger_cp_irq(modem_data->irq_begin);/*trigger irq to ARM0*/
		msleep(10);
		goto out;//xk debug tmp
#endif

		if(!(modem_data->img_flag&FISRT_LOAD_IN_BOOTLOADER)){
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
		}
		modem_parameter_setting(modem_data);
		msleep(10);
#if defined(CONFIG_CPU_LC1813)
		writel(0x00, io_p2v(CTL_CP_FUNCTION_TEST));
#endif
		if( modem_data->modem_base_info.reset_addr!= 0)
		{
			writel(modem_data->modem_base_info.reset_val, io_p2v(modem_data->modem_base_info.reset_addr));
			printk("modem:set %x val %x\n",modem_data->modem_base_info.reset_addr,readl(io_p2v(modem_data->modem_base_info.reset_addr)));
		}
		else
		{
			printk(KERN_ERR "modem:reset_addr is null,modem version has something wrong!\n");
		}
		comip_trigger_cp_irq(modem_data->irq_begin);/*trigger irq to ARM0*/
		msleep(10);
	}else{
#if 0
		comip_trigger_cp_irq(modem_data->irq_begin);/*trigger irq to ARM0*/
		msleep(10);
		
		goto out;//xk debug tmp
#endif

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
#if defined(CONFIG_CPU_LC1813)
		writel(0x00, io_p2v(CTL_CP_FUNCTION_TEST));
#endif
		if( modem_data->modem_base_info.reset_addr!= 0)
		{
			writel(modem_data->modem_base_info.reset_val, io_p2v(modem_data->modem_base_info.reset_addr));
			printk("modem:set %x val %x\n",modem_data->modem_base_info.reset_addr,readl(io_p2v(modem_data->modem_base_info.reset_addr)));
		}
		else
		{
			printk(KERN_ERR "modem:reset_addr is null,modem version has something wrong!\n");
		}
		writel(0, io_p2v(AP_PWR_CP_RSTCTL));
		msleep(10);

		comip_trigger_cp_irq(modem_data->irq_begin);/*trigger irq to ARM0*/
		msleep(10);
	}
//out:
	if (modem_data->modem_base_info.ap_monitor_base_addr !=
			modem_data->modem_base_info.ap_monitor_end_addr){
		MODEM_PRT("modem tpz enable flag 0x%x start 0x%x, end 0x%x\n",
			modem_data->modem_base_info.ap_monitor_flag,
			modem_data->modem_base_info.ap_monitor_base_addr/0x100000,
			modem_data->modem_base_info.ap_monitor_end_addr/0x100000);
		comip_tpz_config(TPZ_AP0, modem_data->modem_base_info.ap_monitor_flag,
			modem_data->modem_base_info.ap_monitor_base_addr/0x100000,
			modem_data->modem_base_info.ap_monitor_end_addr/0x100000,
			-1);
		comip_tpz_enabe(TPZ_AP0, 1);
	}
	modem_data->modem_status = MODEM_RUNNING;

	modem_data->run_modem_count++;
	MODEM_PRT("run modem %d time(s).\n",modem_data->run_modem_count);
	return 0;
}

static int modem_open(struct inode *ip, struct file *fp){
	struct miscdevice  * misc= (struct miscdevice *)(fp->private_data);
	Comip_Modem_Data *modem_data = container_of(misc,
					     Comip_Modem_Data, modem_miscdev);

	mutex_lock(&modem_data->muxlock);
	if(modem_data->run_modem_count == 0){
		if(modem_data->img_flag&IN_MTD_NANDFLASH){
			#ifdef CONFIG_MTD
			if(parser_mtd_modem_img_head(modem_data)){
				printk(KERN_ERR "parser_mtd_modem_img_head error! \n");
				mutex_unlock(&modem_data->muxlock);
				return -EAGAIN;
			}
			#else
			printk(KERN_ERR "board config error.\n");
			mutex_unlock(&modem_data->muxlock);
			return -EIO;
			#endif
		}else if(modem_data->img_flag&IN_BLOCK_EMMC){
			if(parser_emmc_modem_img_head(modem_data)){
				printk(KERN_ERR "parser_emmc_modem_img_head error!\n");
				mutex_unlock(&modem_data->muxlock);
				return -EAGAIN;
			}
		}else{
			MODEM_PRT("unknow modem img_flag.\n");
		}
	}

	if(modem_data->modem_status >= MODEM_OPEN && (modem_data->modem_ref > 0)){
		printk(KERN_WARNING "modem status(%d) != MODEM_RELEASE.open modem again! \n",modem_data->modem_status);
		//return -EIO;
	}else
		modem_data->modem_status = MODEM_OPEN;

	modem_data->modem_ref++;
	MODEM_PRT("modem_open ,ref(%d)\n",modem_data->modem_ref);
	mutex_unlock(&modem_data->muxlock);
	return 0;
}

static int modem_release(struct inode *ip, struct file* fp){
	struct miscdevice  * misc= (struct miscdevice *)(fp->private_data);
	Comip_Modem_Data *modem_data = container_of(misc,
					     Comip_Modem_Data, modem_miscdev);
	mutex_lock(&modem_data->muxlock);
	MODEM_PRT("relese modem ref(%d)\n",modem_data->modem_ref);
	if(modem_data->modem_ref <= 1){
		reset_modem(modem_data);
		modem_data->modem_status = MODEM_RELEASE;
	}

	modem_data->modem_ref--;
	mutex_unlock(&modem_data->muxlock);
	return 0;
}

static long modem_ioctl(struct file* fp, unsigned int cmd, unsigned long arg){
	int left=0;
	int ret =0;
	void __user *argp = (void __user *)arg;
	Dump_Memory_Arg dump_arg;
	Set_Memory_Arg set_arg;
	Modem_Ext_Usr_Info ext_usr_info;
	Modem_Base_Info* modem_base_info;
	Extern_Info *pos,*n;

	struct miscdevice  * misc= (struct miscdevice *)(fp->private_data);
	Comip_Modem_Data *modem_data = container_of(misc,
					     Comip_Modem_Data, modem_miscdev);
	modem_base_info = &modem_data->modem_base_info;

	switch (cmd) {
		case GET_MODEM_BASE_INFO:
			left = copy_to_user(argp,(unsigned long  *)(&(modem_data->modem_base_info)),sizeof(Modem_Base_Usr_Info));
			if(left>0){
				printk(KERN_ERR "GET_MODEM_MODEM_BASE_INFO error.left %d bytes.\n",left);
			}
			break;

		case DUMP_MODEM_MEMORY:
			left = copy_from_user(&dump_arg,argp,sizeof(Dump_Memory_Arg));
			printk("dump modem memory from  0x%x to 0x%x. ",dump_arg.offset ,dump_arg.len);
			if( (dump_arg.offset +  dump_arg.len) >  (modem_data->mem_len)){
				printk(" is not allowed \n");
				break;
			}
			left = copy_to_user(dump_arg.target_addr,(char  *)(modem_data->mem_base+dump_arg.offset),dump_arg.len);
			if(left>0){
				printk(KERN_ERR "DUMP_MODEM_MEMORY error.left %d bytes.\n",left);
			}
			printk(" is ok \n");
			break;

		case SET_MODEM_MEMORY:
			left = copy_from_user(&set_arg,argp,sizeof(Set_Memory_Arg));
			MODEM_PRT("set modem memory from  0x%x to 0x%x. ",set_arg.offset ,set_arg.len);
			if( (set_arg.offset +  set_arg.len) >  (modem_data->mem_len)){
				printk(" is not allowed \n");
				break;
			}
			left = copy_from_user((char  *)(modem_data->mem_base+set_arg.offset),set_arg.source_addr,set_arg.len);
			if(left>0){
				printk(KERN_ERR "Set_Memory_Arg error.left %d bytes.\n",left);
			}
			MODEM_PRT(" is ok \n");
			break;

		case TRIGGER_MODEM_RUN:
			/*set modem out of reset mode and run.*/
			ret = run_modem(modem_data);
			break;

		case TRIGGER_MODEM_RESET:
			ret =reset_modem(modem_data);
			break;

		case GET_MODEM_STATUS:
			left = copy_to_user(argp,(char  *)(&modem_data->modem_status),sizeof(modem_data->modem_status));
			if(left>0){
					printk(KERN_ERR "GET_MODEM_STATUS error.left %d bytes.\n",left);
			}
			break;

		case GET_OFFSET_EXT_INFO:
			list_for_each_entry_safe(pos, n,  &modem_base_info->offset_ext_info_head, list){
				ext_usr_info.info_id=pos->info_id;
				ext_usr_info.len=pos->len;
				ext_usr_info.addr.offset=pos->offset;
				left += copy_to_user(argp,(char  *)(&ext_usr_info),sizeof(Modem_Ext_Usr_Info));
				argp += sizeof(Modem_Ext_Usr_Info);
			}
			if(left>0){
					printk(KERN_ERR "GET_OFFSET_EXT_INFO error.left %d bytes.\n",left);
			}
			break;

		case GET_ABS_EXT_INFO:
			list_for_each_entry_safe(pos, n,  &modem_base_info->abs_ext_info_head, list){
				ext_usr_info.info_id=pos->info_id;
				ext_usr_info.len=pos->len;
				ext_usr_info.addr.abs=pos->offset;
				left += copy_to_user(argp,(char  *)(&ext_usr_info),sizeof(Modem_Ext_Usr_Info));
				argp += sizeof(Modem_Ext_Usr_Info);
			}
			if(left>0){
					printk(KERN_ERR "GET_OFFSET_EXT_INFO error.left %d bytes.\n",left);
			}
			break;

		default:
			MODEM_PRT("Comip-modem : Unknow ioctl cmd : %d\n",cmd);
			break;
	}
	return ret;
}


static ssize_t modem_read(struct file *fp, char __user *buf,size_t count, loff_t *pos){
	unsigned long left_count;

	struct miscdevice  * misc= (struct miscdevice *)(fp->private_data);
	Comip_Modem_Data *modem_data = container_of(misc,
					     Comip_Modem_Data, modem_miscdev);
	if((*pos+count) > modem_data->mem_len)
		count = modem_data->mem_len-*pos;

	if(count <= 0){
		printk(KERN_ERR "modem_read count < 0\n");
		return -EACCES;
	}
	left_count = copy_to_user(buf,modem_data->mem_base+*pos,count);
	*pos=*pos+count-left_count;
	return (count-left_count);
}

ssize_t modem_write(struct file *fp, const char __user *buf,size_t count, loff_t *pos){
	unsigned long left_count;

	struct miscdevice  * misc= (struct miscdevice *)(fp->private_data);
	Comip_Modem_Data *modem_data = container_of(misc,
					     Comip_Modem_Data, modem_miscdev);
	if((*pos+count) > modem_data->mem_len){
		printk(KERN_ERR "modem_write len more than mem_len\n");
		return -EACCES;
	}

	left_count = copy_from_user(modem_data->mem_base+*pos,buf,count);
	*pos=*pos+count-left_count;
	return (count-left_count);
}

int modem_mmap (struct file *file, struct vm_area_struct *vma){
	int ret;
	struct miscdevice  * misc= (struct miscdevice *)(file->private_data);
	Comip_Modem_Data *modem_data = container_of(misc,
					     Comip_Modem_Data, modem_miscdev);

	unsigned long map_size = vma->vm_end - vma->vm_start;
	if(((vma->vm_pgoff<<PAGE_SHIFT)+map_size) > modem_data->mem_len ){
		printk( "error: modem_mmap offset = 0x%lx ,len = 0x%lx  > modem_data->mem_len !\n",vma->vm_pgoff<<PAGE_SHIFT,map_size);
		return -ENOMEM;
	}

	ret = remap_pfn_range(vma, (unsigned long)vma->vm_start,
		vma->vm_pgoff,map_size, pgprot_noncached(PAGE_SHARED));

	if (ret)
		printk( "modem_mmap offset = 0x%lx ,len = 0x%lx failed!\n",vma->vm_pgoff<<PAGE_SHIFT,map_size);
	else
		MODEM_PRT( "modem_mmap offset = 0x%lx ,len = 0x%lx  ok!\n",vma->vm_pgoff<<PAGE_SHIFT,map_size);
	return ret;
}

loff_t  modem_llseek(struct file *file,loff_t offset, int origin){
	switch (origin) {
		case SEEK_END:
			printk(KERN_ERR "modem_llseek origin SEEK_END!\n");
			return -EINVAL;
			break;

		case SEEK_CUR:
			file->f_pos+=file->f_pos+offset;
			break;

		default:
			file->f_pos = offset;
	}
	//MODEM_PRT("modem_llseek f_pos = 0x%llx ,offset = 0x%llx, origin=0x%x \n",file->f_pos,offset,origin);
	return file->f_pos;
}

static struct file_operations modem_fops = {
	.owner = THIS_MODULE,
	.open =    modem_open,
	.release = modem_release,
	.unlocked_ioctl = modem_ioctl,
	.read = modem_read,
	.write = modem_write,
	.mmap = modem_mmap,
	.llseek = modem_llseek,
};

#ifdef CONFIG_MTD
static void mtd_modem_notify_add(struct mtd_info *mtd){
	Comip_Modem_Data * 	modem_data=Get_modem_data();
	if(!modem_data){
		printk(KERN_ERR "Get_modem_data error.\n");
		return;
	}

	if (!strcmp(mtd->name,modem_data->arm0_partition_name)){
		modem_data->arm0_mtd = mtd;
	}else if(!strcmp(mtd->name,modem_data->zsp0_partition_name))
		modem_data->zsp0_mtd = mtd;
	else if(!strcmp(mtd->name,modem_data->zsp1_partition_name))
		modem_data->zsp1_mtd = mtd;

	return;
}

static void mtd_modem_notify_remove(struct mtd_info *mtd){
	Comip_Modem_Data * 	modem_data=Get_modem_data();
	if (mtd == modem_data->arm0_mtd) {
		modem_data->arm0_mtd = NULL;
		printk(KERN_INFO "modem: Unbound from %s\n", mtd->name);
	}
}

static struct mtd_notifier mtd_modem_notifier = {
	.add	= mtd_modem_notify_add,
	.remove	= mtd_modem_notify_remove,
};
#endif

static ssize_t modem_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice  * misc;
	Comip_Modem_Data *modem_data;

	misc = dev_get_drvdata(dev);
	modem_data = container_of(misc,Comip_Modem_Data, modem_miscdev);

       return sprintf(buf, "modem_status %d\n", modem_data->modem_status);
}

static ssize_t modem_status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long state;
       struct miscdevice  * misc;
	Comip_Modem_Data *modem_data;
	Modem_Bridge_Info *bridge_info;

	state = simple_strtoul(buf, NULL, 10);

	misc = dev_get_drvdata(dev);
	modem_data = container_of(misc,Comip_Modem_Data, modem_miscdev);

       switch(state){
   	case MODEM_RUNNING:
		if(modem_data->modem_status >= MODEM_OPEN){
			printk(KERN_ERR "modem status(%d) != MODEM_RELEASE.open modem error! \n",modem_data->modem_status);
			return -EIO;
		}
		/*if(modem_data->run_modem_count == 0){*/
			if(modem_data->img_flag&IN_MTD_NANDFLASH){
				#ifdef CONFIG_MTD
				if(parser_mtd_modem_img_head(modem_data))
					return -EAGAIN;
				#else
				printk(KERN_ERR "board config error.\n");
				return -EIO;
				#endif
			}else if(modem_data->img_flag&IN_BLOCK_EMMC){
				if(parser_emmc_modem_img_head(modem_data))
					return -EAGAIN;
			}else{
				printk("unknow modem img_flag.\n");
			}

			/*add a selfdefined  bridge for test.use bridge 2 tx buf.*/
			bridge_info = kzalloc(sizeof(Modem_Bridge_Info), GFP_KERNEL);
			if (!bridge_info){
				return -ENOMEM;
			}
			strcpy(bridge_info->name,"bg_test");
			bridge_info->irq_id =7;
			bridge_info->rx_addr = 0x18B0000;
			bridge_info->tx_addr = bridge_info->rx_addr ;
			bridge_info->rx_len = 0x10000;
			bridge_info->tx_len = 0x10000;
			list_add_tail(&bridge_info->list, &modem_data->modem_base_info.bridge_head);
			modem_data->modem_base_info.bridge_num++;

		/*}*/

		modem_data->modem_status = MODEM_OPEN;

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
	Comip_Modem_Data *modem_data;

	modem_data = dev_get_drvdata(dev);
       return sprintf(buf, "======================\n"
					"modem mem len = 0x%x\n"
					"modem td_rfid = 0x%x\n"
					"modem gsm_rfid = 0x%x\n"
					"modem bbid = 0x%x\n"
					"modem gsm_dual_rfid = 0x%x\n"
					"modem irq begin = 0x%x\n"
					"modem irq end = 0x%x\n"
					"run modem count = 0x%x\n"
					"modem_status = 0x%x\n"
					"atag_version = 0x%x\n"
					"modem_version = 0x%x\n"
					"modem used len = 0x%x\n"
					"nvram_addr = 0x%x\n"
					"nvram_size = 0x%x\n"
					"bridge_num = 0x%x\n"
					"img_num =  0x%x\n"
					"offset_setting_num = 0x%x\n"
					"abs_setting_num = 0x%x\n"
					"offset_ext_info_num = 0x%x\n"
					"abs_ext_info_num = 0x%x\n"
					"reset_addr = 0x%x\n"
					"reset_val = 0x%x\n"
					"bridge_debug_info = 0x%x\n"
					"======================\n",
					modem_data->mem_len,
					modem_data->td_rfid,
					modem_data->gsm_rfid,
					modem_data->bbid,
					modem_data->gsm_dual_rfid,
					modem_data->irq_begin,
					modem_data->irq_end,
					modem_data->run_modem_count,
					modem_data->modem_status,
					modem_data->modem_base_info.atag_version,
					modem_data->modem_base_info.modem_version,
					modem_data->modem_base_info.modem_ram_size,
					modem_data->modem_base_info.nvram_addr,
					modem_data->modem_base_info.nvram_size,
					modem_data->modem_base_info.bridge_num,
					modem_data->modem_base_info.img_num,
					modem_data->modem_base_info.offset_setting_num,
					modem_data->modem_base_info.abs_setting_num,
					modem_data->modem_base_info.offset_ext_info_num,
					modem_data->modem_base_info.abs_ext_info_num,
					modem_data->modem_base_info.reset_addr,
					modem_data->modem_base_info.reset_val,
					(u32)modem_data->modem_base_info.bridge_debug_info
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
	Comip_Modem_Data *modem_data;
	Modem_Base_Info* modem_base_info;
	Modem_Bridge_Info *pos,*n;

	modem_data = dev_get_drvdata(dev);
	modem_base_info = &modem_data->modem_base_info;

	printk("modem_bridge_info_show begin:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->bridge_head, list){
		printk("bridge name: %s\n",pos->name);
		printk("bridge irq_id: 0x%x\n",pos->irq_id);
		printk("bridge tx_addr: 0x%x\n",pos->tx_addr);
		printk("bridge tx_len: 0x%x\n",pos->tx_len);
		printk("bridge rx_addr: 0x%x\n",pos->rx_addr);
		printk("bridge rx_len: 0x%x\n",pos->rx_len);
		printk("\n");
	}
	printk("modem_bridge_info_show end:\n");
	return 0;
}

static ssize_t modem_img_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}


static ssize_t modem_img_info_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	Comip_Modem_Data *modem_data;
	Modem_Base_Info* modem_base_info;
	Modem_Image_Info *pos,*n;

	modem_data = dev_get_drvdata(dev);
	modem_base_info = &modem_data->modem_base_info;

	printk("modem_img_info_show begin:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->img_info_head, list){
		printk("modem_img img_id: 0x%x\n",pos->img_id);
		printk("modem_img bin_offset: 0x%x\n",pos->bin_offset);
		printk("modem_img ram_offset: 0x%x\n",pos->ram_offset);
		printk("modem_img len: 0x%x\n",pos->len);
		printk("\n");
	}
	printk("modem_img_info_show end:\n");
	return 0;
}

static ssize_t modem_setting_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}


static ssize_t modem_setting_info_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	Comip_Modem_Data *modem_data;
	Modem_Base_Info* modem_base_info;
	Setting_Info *pos,*n;

	modem_data = dev_get_drvdata(dev);
	modem_base_info = &modem_data->modem_base_info;

	printk("modem_offset_setting_info_show begin:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->offset_setting_head, list){
		printk("modem_setting offset: 0x%x\n",pos->offset);
		printk("modem_setting value: 0x%x\n",pos->value);
		printk("\n");
	}
	printk("modem_offset_setting_info_show end:\n");

	printk("modem_abs_setting_info_show  begin:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->abs_setting_head, list){
		printk("modem_setting abs addr: 0x%x\n",pos->offset);
		printk("modem_setting value: 0x%x\n",pos->value);
		printk("\n");
	}
	printk("modem_abs_setting_info_show end\n");
	return 0;
}

static ssize_t modem_ext_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t modem_ext_info_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	Comip_Modem_Data *modem_data;
	Modem_Base_Info* modem_base_info;
	Extern_Info *pos,*n;

	modem_data = dev_get_drvdata(dev);
	modem_base_info = &modem_data->modem_base_info;

	printk("modem_offset_ext_info_show:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->offset_ext_info_head, list){
		printk("modem_setting info_id: 0x%x\n",pos->info_id);
		printk("modem_setting offset: 0x%x\n",pos->offset);
		printk("modem_setting value: 0x%x\n",pos->len);
		printk("\n");
	}
	printk("modem_offset_ext_info_show end\n");

	printk("modem_abs_ext_info_show:\n");
	list_for_each_entry_safe(pos, n,  &modem_base_info->abs_ext_info_head, list){
		printk("modem_setting offset: 0x%x\n",pos->info_id);
		printk("modem_setting abs addr: 0x%x\n",pos->offset);
		printk("modem_setting value: 0x%x\n",pos->len);
		printk("\n");
	}
	printk("modem_abs_ext_info_show end\n");
	return 0;
}

static ssize_t add_test_bridge_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
       struct miscdevice  * misc;
	Comip_Modem_Data *modem_data;
	Modem_Bridge_Info *bridge_info;
	unsigned char __iomem *membase;


	misc = dev_get_drvdata(dev);
	modem_data = container_of(misc,Comip_Modem_Data, modem_miscdev);

	/*add a selfdefined  bridge for test.use bridge 2 tx buf.*/
	bridge_info = kzalloc(sizeof(Modem_Bridge_Info), GFP_KERNEL);
	if (!bridge_info){
		return -ENOMEM;
	}
	strcpy(bridge_info->name,"bg_test");
	bridge_info->irq_id =7;
	bridge_info->rx_addr = 0x18B0000;
	bridge_info->tx_addr = bridge_info->rx_addr ;
	bridge_info->rx_len = 0x10000;
	bridge_info->tx_len = 0x10000;
	list_add_tail(&bridge_info->list, &modem_data->modem_base_info.bridge_head);
	modem_data->modem_base_info.bridge_num++;

	membase = ioremap( bridge_info->rx_addr,bridge_info->rx_len);
	memset(membase,0,bridge_info->rx_len);
	iounmap(membase);


	membase = ioremap( bridge_info->tx_addr,bridge_info->tx_len);
	memset(membase,0,bridge_info->tx_len);
	iounmap(membase);

	register_comip_bridge(bridge_info,modem_data->modem_miscdev.parent,NULL);

	return size;
}

static ssize_t add_test_bridge_show(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(modem_status, 0600, modem_status_show, modem_status_store);
static DEVICE_ATTR(modem_info, 0600, modem_info_show, modem_info_store);

static DEVICE_ATTR(bridge_info, 0600, modem_bridge_info_show, modem_bridge_info_store);
static DEVICE_ATTR(img_info, 0600, modem_img_info_show, modem_img_info_store);
static DEVICE_ATTR(setting_info, 0600, modem_setting_info_show, modem_setting_info_store);
static DEVICE_ATTR(ext_info, 0600, modem_ext_info_show, modem_ext_info_store);

static DEVICE_ATTR(add_test_bridge, 0600, add_test_bridge_show, add_test_bridge_store);

static void fix_bbid(u32 *bbid)
{
#if defined(CONFIG_CPU_LC1810)
	void __iomem *rom_base = NULL;
	void __iomem *security_mem = NULL;
	u32 chip_id = 0;
	u32 security_id = 0;
	u32 security = 0;

	rom_base = ioremap(ROM_BASE, ROM_LEN);
	if(!rom_base){
		printk("ioremap ROM memory error!\n");
		return;
	}

	chip_id = *(u32 *)(rom_base + CHIP_ID_OFFSET);
	if(chip_id){
		security_mem = ioremap((SECURITY_BASE + SECURITY_OFFSET), 0x4);
		security = (*(u32 *)security_mem);
		security_id = (security & 0xC0000000) >> 30;
		if(chip_id == CHIP_ID_ECO1_2){
			if(security_id != 0)
				security_id = 0x01;//eco2
		}
		*bbid = chip_id + security_id;
		iounmap(security_mem);
	}
	iounmap(rom_base);

	printk("chip id:0x%x, security:0x%x, security id:0x%x, bb id:0x%x\n",
		chip_id, security, security_id, *bbid);
#else
	*bbid = COMIP_BB_ID;
#endif
}

static int  __init comip_modem_probe(struct platform_device *pdev)
{
	int ret=0;
	struct resource *res;
	Modem_Base_Info* base_info;
	Comip_Modem_Data *modem_data=NULL;
	struct comip_modem_platform_data *pdata;

	if(pdev->id !=-1)
		return -ENXIO;

	pdata = pdev->dev.platform_data;
	if(!pdata){
		return -EIO;
	}

	modem_data = kzalloc(sizeof(Comip_Modem_Data) , GFP_KERNEL);
	if (!modem_data)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev,(void *)modem_data);

	modem_data->td_rfid = pdata->td_rfid;
	modem_data->gsm_rfid = pdata->gsm_rfid;
	modem_data->bbid = pdata->bbid;
	modem_data->gsm_dual_rfid = pdata->gsm_dual_rfid;
	modem_data->img_flag = pdata->img_flag;
#if defined(CONFIG_NAND_COMIP)
	modem_data->img_flag = IN_MTD_NANDFLASH;
#elif defined(CONFIG_MMC_COMIP)
	modem_data->img_flag = IN_BLOCK_EMMC;
#endif
	modem_data->arm0_partition_name = pdata->arm0_partition_name;
	modem_data->zsp0_partition_name = pdata->zsp0_partition_name;
	modem_data->zsp1_partition_name = pdata->zsp1_partition_name;
	//#ifdef CONFIG_BBID_FROM_REG
	fix_bbid(&modem_data->bbid);
	//#endif
	base_info = &modem_data->modem_base_info;
	memset(base_info,0,sizeof(Modem_Base_Info));
	INIT_LIST_HEAD(&base_info->bridge_head);
	INIT_LIST_HEAD(&base_info->img_info_head);
	INIT_LIST_HEAD(&base_info->offset_setting_head);
	INIT_LIST_HEAD(&base_info->abs_setting_head);
	INIT_LIST_HEAD(&base_info->offset_ext_info_head);
	INIT_LIST_HEAD(&base_info->abs_ext_info_head);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res){
		ret = -EIO;
		goto  res_err;
	}
	modem_data->mem_len = res->end - res->start + 1;
	modem_data->mem_base = ioremap(res->start, modem_data->mem_len);
	if (!modem_data->mem_base){
		printk(KERN_ERR "modem memory address is invalid \n");
		ret = -ENOMEM;
		goto  remap_err;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res){
		ret = -EIO;
		goto  res_err;
	}
	modem_data->irq_begin = res->start;
	modem_data->irq_end = res->end;

	modem_data->run_modem_count = 0;

#ifdef CONFIG_MTD
	modem_data->arm0_mtd = NULL;
	modem_data->zsp0_mtd = NULL;
	modem_data->zsp1_mtd = NULL;
#endif

	modem_data->modem_miscdev.minor = MISC_DYNAMIC_MINOR;
	modem_data->modem_miscdev.parent = &pdev->dev;
	modem_data->modem_miscdev.name = "modem";
	modem_data->modem_miscdev.fops = &modem_fops;

	ret = misc_register(&modem_data->modem_miscdev);
	if(ret){
		printk(KERN_ERR "register modem misc dev failed!\n");
		goto misc_err;
	}

	ret = device_create_file(modem_data->modem_miscdev.this_device, &dev_attr_modem_status);
	if(ret){
		printk(KERN_ERR "creat modem_status device file failed!\n");
		goto require_dev_attr_modem_status_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_modem_info);
	if(ret){
		printk(KERN_ERR "creat dev_attr_modem_info device file failed!\n");
		goto require_dev_attr_modem_info_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_bridge_info);
	if(ret){
		printk(KERN_ERR "creat dev_attr_bridge_info device file failed!\n");
		goto require_dev_attr_bridge_info_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_img_info);
	if(ret){
		printk(KERN_ERR "creat dev_attr_img_info device file failed!\n");
		goto require_dev_attr_img_info_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_setting_info);
	if(ret){
		printk(KERN_ERR "creat dev_attr_setting_info device file failed!\n");
		goto require_dev_attr_setting_info_creat_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_ext_info);
	if(ret){
		printk(KERN_ERR "creat dev_attr_ext_info device file failed!\n");
		goto require_dev_attr_ext_info_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_add_test_bridge);
	if(ret){
		printk(KERN_ERR "creat dev_attr_ext_info device file failed!\n");
		goto require_dev_attr_add_test_bridge;
	}

	mutex_init(&modem_data->muxlock);
	Set_modem_data(modem_data);
#ifdef CONFIG_MTD
	register_mtd_user(&mtd_modem_notifier);
#endif

	MODEM_PRT("modem devices driver up.\n");
	return ret ;

require_dev_attr_add_test_bridge:
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_ext_info);
require_dev_attr_ext_info_err:
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_setting_info);
require_dev_attr_setting_info_creat_err:
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_img_info);
require_dev_attr_img_info_creat_err:
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_bridge_info);
require_dev_attr_bridge_info_creat_err:
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_modem_info);
require_dev_attr_modem_info_creat_err:
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_modem_status);
require_dev_attr_modem_status_creat_err:
	misc_deregister(&modem_data->modem_miscdev);
misc_err:
remap_err:
res_err:
	kfree(modem_data);
	
	return ret;
}

static int comip_modem_remove(struct platform_device *pdev)
{
	Modem_Bridge_Info * bridge_info=NULL;
	Modem_Image_Info * img_info;
	Setting_Info	* setting_info;
	Extern_Info	* extern_info;
	Comip_Modem_Data *modem_data;

	modem_data= (Comip_Modem_Data *)dev_get_drvdata(&pdev->dev);
#ifdef CONFIG_MTD
	unregister_mtd_user(&mtd_modem_notifier);
#endif
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_add_test_bridge);
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_ext_info);
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_setting_info);
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_img_info);
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_bridge_info);
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_modem_info);
	device_remove_file(modem_data->modem_miscdev.this_device, &dev_attr_modem_status);
	misc_deregister(&modem_data->modem_miscdev);

	while(!list_empty(&modem_data->modem_base_info.bridge_head)) {
		bridge_info = list_first_entry(&modem_data->modem_base_info.bridge_head, Modem_Bridge_Info, list);
		list_del(&bridge_info->list);
		kfree(bridge_info);
	}

	while(!list_empty(&modem_data->modem_base_info.img_info_head)) {
		img_info = list_first_entry(&modem_data->modem_base_info.img_info_head, Modem_Image_Info, list);
		list_del(&img_info->list);
		kfree(img_info);
	}

	while(!list_empty(&modem_data->modem_base_info.offset_setting_head)) {
		setting_info = list_first_entry(&modem_data->modem_base_info.offset_setting_head, Setting_Info, list);
		list_del(&setting_info->list);
		kfree(setting_info);
	}

	while(!list_empty(&modem_data->modem_base_info.abs_setting_head)) {
		setting_info = list_first_entry(&modem_data->modem_base_info.offset_setting_head, Setting_Info, list);
		list_del(&setting_info->list);
		kfree(setting_info);
	}

	while(!list_empty(&modem_data->modem_base_info.offset_ext_info_head)) {
		extern_info = list_first_entry(&modem_data->modem_base_info.offset_ext_info_head, Extern_Info, list);
		list_del(&extern_info->list);
		kfree(extern_info);
	}

	while(!list_empty(&modem_data->modem_base_info.abs_ext_info_head)) {
		extern_info = list_first_entry(&modem_data->modem_base_info.abs_ext_info_head, Extern_Info, list);
		list_del(&extern_info->list);
		kfree(extern_info);
	}
	mutex_destroy(&modem_data->muxlock);
	kfree(modem_data);
	return 0;
}


static struct platform_driver comip_modem_driver = {
        .probe = comip_modem_probe,
        .remove = comip_modem_remove,
        .driver = {
                .name = "comip-modem",
        },
};

static int __init modem_init(void)
{
	return platform_driver_register(&comip_modem_driver);
}

static void __exit modem_exit(void)
{
	platform_driver_unregister(&comip_modem_driver);
}

late_initcall(modem_init);
module_exit(modem_exit);

MODULE_AUTHOR(ken xue <xuekun@leadcoretech.com>);
MODULE_DESCRIPTION("modem&&bridge driver for lc1810");
MODULE_LICENSE("GPL");
MODULE_ALIAS("comip modem&&bridge");

