#ifndef _COMIP_MODEM_H
#define _COMIP_MODEM_H

#include <linux/string.h>
#include <linux/list.h>
#include <linux/miscdevice.h>

#define modem_debug printk

/*SETTING*/
#define modem_tag_len			4096

#define trig_modem_run_irq		0

#define MODEM_MEMORY_BEGIN		MODEM_MEMORY_BASE

typedef enum _IMG_ID{
    MODEM_IMG_ID = 1,
    PL_IMG_ID
}Img_Id;

typedef enum _BRIDGE_TYPE{
    LS_CHAR = 0,
    HS_IP_PACKET,
    HS_RAW_DATA,
}Bridge_Type;

typedef enum _ADDR_TYPE{
	ADDR_MEM,
	ADDR_IO,
}Addr_Type;

typedef union _BRIDGE_PRO{
    u32	pro;
    struct {
		u32	flow_ctl:1;
		u32	packet:1;
		u32	reserved:30;
    }pro_b;
}Bridge_Pro;

typedef enum{
	READ_INFO_INVALID,
	READ_INFO_SIMCARD,
	READ_INFO_SHARM_BASE_ADDR,
	READ_INFO_PL_DUMP_FLAG_ADDR,
	READ_INFO_PL1_DUMP_FLAG_ADDR,
	READ_INFO_PL_DUMP_SWITCH_FLAG_ADDR,
	READ_INFO_BOOT_LOG_SWITCH_INFO_ADDR,
	READ_INFO_RESERVED1,
	READ_INFO_DD_PL_SRAM_TEP_ADDR,
	READ_INFO_CP_EXC_STEP_INFO_ADDR,
	READ_INFO_DBBID_BASE_ADDR,
	READ_INFO_MAX,
}AP_READ_INFO_ID;

/*============================================================
tag parser begin
*/
#define MAKE_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

/* The list ends with an ATAG_NONE node. */
#define TAG_NONE				0x54410000
struct dtag_header {
	u32 size;
	u32 tag;
	char tag_name[12];
};

#define TAG_VER					0x54410001
struct tag_ver {
	struct dtag_header hdr;
	u32 version;
};

#define TAG_INFO				0x54410002
struct dtag_info {
	struct dtag_header hdr;
	u32 modem_version;
	u32 modem_ram_size;
	u32 nvram_addr;
	u32 nvram_size;
};

#define TAG_IMAGE				0x54410003
struct dtag_image {
	struct dtag_header hdr;
	u32 img_id;
	u32 bin_offset;
	u32 ram_addr;
	u32 len;
};

#define TAG_BRIDGE				0x54410004
struct dtag_bridge_common {
	struct dtag_header hdr;
	char name[12];
	u32 type;
	u32 property;
	u32 m2a_buf_addr;
	u32 m2a_buf_len;
	u32 m2a_notify_id;
	u32 a2m_buf_addr;
	u32 a2m_buf_len;
	u32 a2m_notify_id;
};

struct dtag_bridge_pkt{
	u32 m2a_packet_max_num;
	u32 m2a_packet_max_size;
	u32 a2m_packet_max_num;
	u32 a2m_packet_max_size;
};

struct dtag_bridge_fc{
	u32 m2a_ctl_id;
	u32 a2m_ctl_id;
};

/* specify the low speed bridge channel.*/
struct dtag_ls_bridge{
	struct dtag_bridge_common	bridge_common_info;
};

/* specify the high speed bridge channel.*/
struct dtag_hs_bridge{
	struct dtag_bridge_common	bridge_common_info;
	struct dtag_bridge_pkt		packet_ctl;
	struct dtag_bridge_fc		flow_ctl;
};

#define TAG_READ				0x54410005
struct dtag_read_info {
	struct dtag_header			hdr;
	u32					info_id;
	u32					addr_type;
	u32					info_addr;
	u32					info_size;
};

#define TAG_WRITE				0x54410006
struct dtag_write_value {
	struct dtag_header			hdr;
	u32					addr_type;
	u32					addr;
	u32					value;
};

#define tag_next(t)	((struct dtag_header *)((char *)(t) + (t)->size))

/*============================================================
tag parser end
*/
#define FISRT_LOAD_IN_BOOTLOADER	(1<<0)
#define IN_MTD_NANDFLASH			(1<<1)
#define IN_BLOCK_EMMC				(1<<2)

struct modem_platform_data{
	unsigned int img_flag;/*modem image store IN_MTD_NANDFLASH or IN_BLOCK_EMMC*/
	char *arm_partition_name;
	char *zsp_partition_name;
};

struct info{
	u32	type;
	u32	addr;
	u32	len;
};

struct modem_bridge_info{
	struct list_head	list;
	char			name[12];
	u32			baseaddr;
	Bridge_Type		type;
	Bridge_Pro		property;
	u32			rx_addr;
	u32			rx_len;
	u32			rx_id;
	u32			tx_addr;
	u32			tx_len;
	u32			tx_id;
	u32			rx_ctl_id;
	u32			tx_ctl_id;
	u32			rx_pkt_maxnum;
	u32			rx_pkt_maxsize;
	u32			tx_pkt_maxnum;
	u32			tx_pkt_maxsize;
	void			*dev;
	struct info		dbg_info;
};

struct modem_image_info{
	struct list_head	list;
	 u32 bin_offset;
	 u32 ram_offset;
	 u32 len;
	 u32 img_id;
};

struct set_info{
	struct list_head	list;
	u32 addr_type;
	u32 addr;
	u32 value;
};

struct get_info{
	struct list_head	list;
	u32 info_id;
	struct info info;
};

struct modem_base_info{
	u32 tag_version;/*image head format version.*/
	u32 modem_version;
	u32 modem_ram_size;/*modem used memory*/
	u32 nvram_addr;/*GET_NVRAM_INFO cmd param1*/
	u32 nvram_size;/*GET_NVRAM_INFO cmd param2*/

	u32 bridge_num;
	u32 img_num;/*modem image number*/
	u32 set_info_num;
	u32 get_info_num;

	struct list_head 	bridge_head;
	struct list_head 	img_info_head;
	struct list_head 	set_info_head;
	struct list_head	get_info_head;

};

struct modem_nvram_info{
	u32 addr;
	u32 size;
};

struct modem_usr_info{
	u32 info_id;
	u32 addr;
	u32 len;
};

#define GET_NVRAM_INFO		_IO('M', 1)
#define TRIGGER_MODEM_RUN 	_IO('M', 2)
#define TRIGGER_MODEM_RESET	_IO('M', 3)
#define GET_INFO		_IO('M', 4)

typedef enum {
	MODEM_RELEASE,
	MODEM_OPEN,
	MODEM_IN_RESET,
	MODEM_RUNNING,
}Modem_Status;

struct comip_modem_data{
	unsigned int img_flag;
	char *arm_partition_name;
	char *zsp_partition_name;
	void __iomem *mem_base;/*modem used ddr begin address,after ioremaped. */
	u32 mem_len;/*kernel assigned for modem.should >= modem needs.*/
	u32 irq_begin;
	u32 irq_end;
	u32 run_count;
	volatile Modem_Status	status;
	u32 ref_flag;
	u32	bbid;
	struct info bridge_dbg;
	struct miscdevice miscdev;
	struct mutex muxlock;
#ifdef CONFIG_MTD
	struct mtd_info *arm_mtd;
	struct mtd_info *zsp_mtd;
#endif

	struct modem_base_info modem_base_info;
};

extern void comip_set_modem_info(struct modem_platform_data *info);
extern struct comip_modem_data * get_modem_data(void);
#endif
