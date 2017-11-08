#ifndef __COMIP_SETUP_H__
#define __COMIP_SETUP_H__

#define ATAG_MEM_EXT		0x54410100

struct tag_mem_ext {
	u64 dram_base;
	u64 dram_size;
	u64 dram2_base;
	u64 dram2_size;
	u64 on2_base;
	u64 on2_size;
	u64 fb_base;
	u64 fb_size;
	u8 fb_num;
	u8 fb_fix;
	u8 on2_fix;
};

#define ATAG_BUCK2_VOLTAGE	0x54410101

struct tag_buck2_voltage {
	u32 vout0;
	u32 vout1;
	u32 vout2;
	u32 vout3;
};

#define ATAG_BOOT_INFO		0x54410102

struct tag_boot_info {
	u32 startup_reason;
	u32 shutdown_reason;
	u32 pu_reason;
	u32 reboot_reason;
	u32 boot_mode;
	u32 reserved[3];
};

#define ATAG_BUCK1_VOLTAGE	0x54410103

struct tag_buck1_voltage {
	u32 vout0;
	u32 vout1;
	u32 reserved[2];
};

#define ATAG_CHIP_ID		0x54410104

struct tag_chip_id {
	u32 chip_id;
	u32 rom_id;
	u32 bb_id;
	u32 sn_id;
	u32 reserved;
};

#define ATAG_MMC_TUNING_ADDR		0x54410105

struct tag_mmc_tuning_addr {
       u32 tuning_addr;
};

#endif/* __COMIP_SETUP_H__ */
