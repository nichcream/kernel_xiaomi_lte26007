#ifndef _COMIP_MODEM_H
#define _COMIP_MODEM_H

#include <linux/string.h>
#include <linux/list.h>
#include <linux/miscdevice.h>

#define modem_debug printk

/*SETTING*/
#define modem_tag_len 4096

#define trig_modem_run_irq 0


#define MODEM_MEMORY_BEGIN	MODEM_MEMORY_BASE

/*RFID*/
#define SOC_RFIC_TD_ERROR   0x0
#define SOC_RFIC_TD_LC1206   0x00010000
#define SOC_RFIC_TD_LC1207   0x00020000
#define SOC_RFIC_TD_RS2012   0x00030000
#define SOC_RFIC_TD_RS3009   0x00040000
#define SOC_RFIC_TD_RDA8208   0x00050000

#define SOC_RFIC_GSM_ERROR   0x0
#define SOC_RFIC_GSM_PMB6277  0x00010000
#define SOC_RFIC_GSM_RDA6220  0x00020000
#define SOC_RFIC_GSM_RDA6220S   0x00030000
#define SOC_RFIC_GSM_RS3009   0x00040000
#define SOC_RFIC_GSM_RDA8208   0x00050000

/*BBID*/
#define SOC_PARTID_BASICDEV_1609           ((0x1821)<<16)
#define SOC_PARTID_BASICDEV_1809B      ((0x204C)<<16)
#define SOC_PARTID_BASICDEV_1810      ((0x2420)<<16)
#define SOC_PARTID_BASICDEV_1813      ((0x2461)<<16)

typedef enum{
	UNDIFINED_ID,
	SIMCARD_EXT_INFO_ID,
	SHARM_EXT_INFO_ID,
	TD_RFID_EXT_INFO_ID,
	GSM_RFID_EXT_INFO_ID,
	DBB_ID_EXT_INFO_ID,
	GSM_DUAL_RFID_EXT_INFO_ID,
	PL_DUMP_FLAG_ADDR_EXT_INFO_ID,
	PL_DUMP_FILE_ADDR_EXT_INFO_ID,
	PL_DUMP_SWITCH_FLAG_ADDR_EXT_INFO_ID,
	PL1_DUMP_FLAG_ADDR_EXT_INFO_ID,
	PL1_DUMP_FILE_ADDR_EXT_INFO_ID,
	PL1_DUMP_SWITCH_FLAG_ADDR_EXT_INFO_ID,
	RSCP_LEVEL_ADDR_EXT_INFO_ID,
	SINGAL_LEVEL_ADDR_EXT_INFO_ID,
	DD_PL_SRAM_TEP_ADD_EXT_INFO_ID,
	MODEM_RESET_GENERAL_REG,
	MODEM_RESET_GENERAL_VALUE,
	TC_EXC_STEP_INFO_ID,
	TPZ_AP_MONITOR_FLAG,
	TPZ_AP_MONITOR_BASE_ADDR,
	TPZ_AP_MONITOR_SIZE,
	END_EXT_INFO_ID,
}EXTERN_INFO_ID;

#define FISRT_LOAD_IN_BOOTLOADER 1<<0
#define IN_MTD_NANDFLASH	1<<1
#define IN_BLOCK_EMMC		1<<2
struct comip_modem_platform_data{
	unsigned int img_flag;
	u32 td_rfid;
	u32 gsm_rfid;
	u32 bbid;
	u32 gsm_dual_rfid;
	char *arm0_partition_name;
	char *zsp0_partition_name;
	char *zsp1_partition_name;
};

extern void comip_set_modem_info(struct comip_modem_platform_data *info);

/*============================================================
tag parser begine
*/
#define MAKE_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

/* The list ends with an ATAG_NONE node. */
#define ATAG_NONE		0x54410000
typedef struct _tag_header {
	u32 size;
	u32 tag;
	char tag_name[12];
}tag_header;

#define ATAG_VER		        0x54410001
typedef struct _tag_ver {
	tag_header hdr;
	u32 version;
}tag_ver;

#define ATAG_INFO		        0x54410002
typedef struct _tag_info {
	tag_header hdr;
	u32 modem_version;
	u32 modem_ram_size;
	u32 nvram_addr;
	u32 nvram_size;
}tag_info;

#define ATAG_IMAGE		      0x54410003
#define ARM0_IMG_ID	1
#define ZSP0_IMG_ID	2
#define ZSP1_IMG_ID	3
typedef struct _tag_image {
	tag_header hdr;
	u32 img_id;
	u32 bin_offset;
	u32 ram_addr;
	u32 len;
}tag_image;

#define ATAG_BRIDGE		      0x54410004
typedef struct _tag_bridge {
	tag_header hdr;
	char name[12];
	u32 fiq_id;
	u32 rx_addr;
	u32 rx_len;
	u32 tx_addr;
	u32 tx_len;
}tag_bridge;

#define ATAG_OFFSET_SETTING		       0x54410005
#define ATAG_ABS_SETTING		   0x54410006

typedef struct _tag_value {
	tag_header hdr;
	u32 addr;
	u32 value;
}tag_value;

#define ATAG_OFFSET_EXT_INFO		     0x54410007
#define ATAG_ABS_EXT_INFO		0x54410008

typedef struct _tag_ext_info {
	tag_header hdr;

#define SIMCARD_EXT_INFO_ID   1
#define SHARM_EXT_INFO_ID     2
	u32 info_id;
	u32 ext_info_addr;
	u32 ext_info_size;
}tag_ext_info;

#define ATAG_ID		        0x54420001
typedef struct _tag_modem_id {
	tag_header hdr;
	u32 td_rfid;
	u32 gsm_rfid;
	u32 second_gsm_rfid;
	u32 bbid;
}tag_modem_id;

#define tag_next(t)	((tag_header *)((char *)(t) + (t)->size))

/*============================================================
tag parser end
*/


typedef struct MODEM_BRIDGE_INFO{
	 struct list_head 	list;
	 char name[12];
	 u32 irq_id;
	 u32 tx_addr;
	 u32 tx_len;
	 u32 rx_addr;
	 u32 rx_len;
	 u32 debug_info;
	 struct miscdevice *bridge_miscdev;
}Modem_Bridge_Info;


typedef struct MODEM_IMAGE_INFO{
	struct list_head 	list;
	 u32 bin_offset;
	 u32 ram_offset;
	 u32 len;
	 u32 img_id;
}Modem_Image_Info;


typedef struct SETTING_INFO{
	struct list_head 	list;
	 u32 offset;
	 u32 value;
}Setting_Info;

typedef struct EXTERN_INFO{
	struct list_head 	list;
	u32 info_id;
	u32 offset;
	u32 len;
}Extern_Info;

typedef struct MODEM_EXT_USR_INFO{
	u32 info_id;
	union{
		u32 offset;
		u32 abs;
	}addr;
	u32 len;
}Modem_Ext_Usr_Info;

typedef struct MODEM_BASE_INFO{
	u32 atag_version;/*image head format version.*/
	u32 modem_version;/**/
	u32 modem_ram_size;/*modem used memory*/
	u32 nvram_addr;
	u32 nvram_size;

	u32 bridge_num;

	u32 img_num;/*modem image number*/

	u32 offset_setting_num;
	u32 abs_setting_num;
	u32 offset_ext_info_num;
	u32 abs_ext_info_num;

	struct list_head 	bridge_head;
	struct list_head 	img_info_head;
	struct list_head 	offset_setting_head;
	struct list_head 	abs_setting_head;
	struct list_head 	offset_ext_info_head;
	struct list_head 	abs_ext_info_head;

	u32 ap_monitor_flag;
	u32 ap_monitor_base_addr;
	u32 ap_monitor_end_addr;
	u32 reset_addr;
	u32 reset_val;
	unsigned int __iomem *bridge_debug_info;
}Modem_Base_Info;

typedef struct MODEM_BASE_USR_INFO{
	u32 atag_version;/*image head format version.*/
	u32 modem_version;
	u32 modem_ram_size;
	u32 nvram_addr;
	u32 nvram_size;

	u32 bridge_num;

	/*unsigned long head_len;*/
	u32 img_num;

	u32 offset_setting_num;
	u32 abs_setting_num;
	u32 offset_ext_info_num;
	u32 abs_ext_info_num;
}Modem_Base_Usr_Info;


typedef struct DUMP_MEMORY_ARG{
	void __user *target_addr;
	u32	offset;
	u32	len;
}Dump_Memory_Arg;

typedef struct SET_MEMORY_ARG{
	void __user *source_addr;
	u32	offset;
	u32	len;
}Set_Memory_Arg;

typedef enum MODEM_CMD{
	GET_MODEM_BASE_INFO,
	GET_MODEM_STATUS,
	DUMP_MODEM_MEMORY,
	SET_MODEM_MEMORY,
	TRIGGER_MODEM_RUN,
	TRIGGER_MODEM_RESET,
	GET_OFFSET_EXT_INFO,
	GET_ABS_EXT_INFO,
}Modem_Cmd;

typedef enum {
	MODEM_RELEASE,
	MODEM_OPEN,
	MODEM_IN_RESET,
	MODEM_RUNNING,
}Modem_Status;

typedef struct COMIP_MODEM_DATA{
	unsigned int img_flag;/*modem image store IN_MTD_NANDFLASH or IN_BLOCK_EMMC*/
	u32 td_rfid;
	u32 gsm_rfid;
	u32 bbid;
	u32 gsm_dual_rfid;
	char *arm0_partition_name;
	char *zsp0_partition_name;
	char *zsp1_partition_name;

	void __iomem *mem_base;/*modem used ddr begin address,after ioremaped. */
	u32 mem_len;/*kernel assigned for modem.should >= modem needs.*/
	u32 irq_begin;
	u32 irq_end;
	u32 run_modem_count;
	volatile Modem_Status	modem_status;
	u32 modem_ref;
	struct miscdevice modem_miscdev;
	struct mutex muxlock;

#ifdef CONFIG_MTD
	struct mtd_info *arm0_mtd;
	struct mtd_info *zsp0_mtd;
	struct mtd_info *zsp1_mtd;
#endif

	Modem_Base_Info modem_base_info;
}Comip_Modem_Data;

extern Comip_Modem_Data * get_modem_data(void);

#endif
