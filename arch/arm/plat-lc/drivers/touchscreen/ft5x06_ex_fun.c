/*
 *drivers/input/touchscreen/ft5x06_ex_fun.c
 *
 *FocalTech ft5x0x expand function for debug.
 *
 *Copyright (c) 2010  Focal tech Ltd.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *Note:the error code of EIO is the general error in this file.
 */

#include "ft5x06_ex_fun.h"
#include <plat/ft5x06_ts.h>
#include <linux/mount.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <plat/ft5x06_ts.h>

#ifdef FT5X06_DEBUG
#define FT5X06_EX_PRINT(fmt, args...) printk(KERN_ERR "FT5X06_EX: " fmt, ##args)
#else
#define FT5X06_EX_PRINT(fmt, args...) printk(KERN_DEBUG "FT5X06_EX: " fmt, ##args)
#endif

extern struct Upgrade_Info fts_updateinfo_curr;
int fts_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
		u32 dw_lenth);

/*
static unsigned char CTPM_FW[] = {
#if defined(CONFIG_TOUCHSCREEN_FT5316_AUTO_UPG)
#include "ft5316_firmware.h"
#elif defined(CONFIG_TOUCHSCREEN_FT5406_AUTO_UPG)
#include "ft5406_firmware.h"
#elif defined(CONFIG_TOUCHSCREEN_FT6206_AUTO_UPG)
#include "ft6206_firmware.h"
#else
#include "ft5406_firmware.h"
#endif
};
*/
static struct mutex g_device_mutex;
static u8 is_ic_update_crash = 0;

/*
 *ft5x0x_i2c_Read-read data and write data by i2c
 *@client: handle of i2c
 *@writebuf: Data that will be written to the slave
 *@writelen: How many bytes to write
 *@readbuf: Where to store data read from slave
 *@readlen: How many bytes to read
 *
 *Returns negative errno, else the number of messages executed
 *
 *
 */
int ft5x0x_i2c_Read(struct i2c_client *client, char *writebuf,
		int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				.addr = client->addr,
				.flags = 0,
				.len = writelen,
				.buf = writebuf,
			},
			{
				.addr = client->addr,
				.flags = I2C_M_RD,
				.len = readlen,
				.buf = readbuf,
			},
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error.\n",
					__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				.addr = client->addr,
				.flags = I2C_M_RD,
				.len = readlen,
				.buf = readbuf,
			},
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}
/*write data by i2c*/
int ft5x0x_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = writelen,
			.buf = writebuf,
		},
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error.\n", __func__);

	return ret;
}



int ft5x0x_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	unsigned char buf[2] = {0};
	buf[0] = regaddr;
	buf[1] = regvalue;

	return ft5x0x_i2c_Write(client, buf, sizeof(buf));
}


int ft5x0x_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
	return ft5x0x_i2c_Read(client, &regaddr, 1, regvalue, 1);
}

static void fts_reset_to_upgrade(struct i2c_client *client)
{
        struct ft5x06_info* info = client->dev.platform_data;
        int ft5x06_rst = info->reset_gpio;

        gpio_request(ft5x06_rst, "ft5x06 Reset");
        gpio_direction_output(ft5x06_rst, 0);
        msleep(2);
        gpio_direction_output(ft5x06_rst, 1);
        gpio_free(ft5x06_rst);
}

int fts_ctpm_auto_clb(struct i2c_client *client)
{
	unsigned char uc_temp = 0x00;
	unsigned char i = 0;

	/*start auto CLB */
	msleep(200);
	ft5x0x_write_reg(client, 0, FTS_FACTORYMODE_VALUE);
	/*make sure already enter factory mode */
	msleep(100);
	/*write command to start calibration */
	ft5x0x_write_reg(client, 2, 0x4);
	msleep(300);
	if ((fts_updateinfo_curr.CHIP_ID==0x11) ||(fts_updateinfo_curr.CHIP_ID==0x12) ||
			(fts_updateinfo_curr.CHIP_ID==0x13) ||(fts_updateinfo_curr.CHIP_ID==0x14)) {//5x36,5x36i
		for (i = 0; i < 100; i++) {
			ft5x0x_read_reg(client, 0x02, &uc_temp);
			if (0x02 == uc_temp || 0xFF == uc_temp) {
				/*if 0x02, then auto clb ok, else 0xff, auto clb failure*/
				break;
			}
			msleep(20);	    
		}
	} else {
		for (i = 0; i < 100; i++) {
			ft5x0x_read_reg(client, 0, &uc_temp);
			if (0x0 == ((uc_temp&0x70)>>4)) { /*return to normal mode, calibration finish*/
				break;
			}
			msleep(20);	    
		}
	}
	/*calibration OK */
	msleep(300);
	ft5x0x_write_reg(client, 0, FTS_FACTORYMODE_VALUE);	/*goto factory mode for store */
	msleep(200);	/*make sure already enter factory mode */
	ft5x0x_write_reg(client, 2, 0x5);	/*store CLB result */
	msleep(300);
	ft5x0x_write_reg(client, 0, FTS_WORKMODE_VALUE);	/*return to normal mode */
	msleep(300);

	/*store CLB result OK */
	return 0;
}

#ifdef CFG_SUPPORT_AUTO_UPG
static unsigned char CTPM_O_FILM[] = {
#include "ft_firmware_o_film.h"
};

static unsigned char CTPM_BIEL[] = {
#include "ft_firmware_biel.h"
};

static unsigned char CTPM_GIS[] = {
#include "ft_firmware_gis.h"
};
#endif

/*
   upgrade with *.i file
 */
int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client)
{
	u8 *pbt_buf = NULL;
        int flag_TPID = 0;
	int i_ret;
	int fw_len;
        u8 uc_host_fm_ver, uc_tp_fm_ver, vendor_id, ic_type;
	struct ft5x06_ts_data *ts_data;

        ts_data = i2c_get_clientdata(client);

	/*judge the fw that will be upgraded
	 * if illegal, then stop upgrade and return. */
	ft5x0x_read_reg(client, FT5X06_REG_FIRMID, &uc_tp_fm_ver);
	ft5x0x_read_reg(client, FT5X06_REG_VENDOR_ID, &vendor_id);
	ft5x0x_read_reg(client, FT5X06_REG_CIPHER, &ic_type);

	if (vendor_id == FT5X06_REG_VENDOR_ID || vendor_id == 0x00 || ic_type == FT5X06_REG_CIPHER || ic_type == 0x00) {
		FT5X06_EX_PRINT("vend_id read error, need project\n");
		vendor_id = fts_ctpm_update_project_setting(client);
		flag_TPID = 1;
	}

	if (vendor_id == VENDOR_O_FILM) {
		pbt_buf = CTPM_O_FILM;
		fw_len = sizeof(CTPM_O_FILM);
		FT5X06_EX_PRINT("update firmware size:%d", fw_len);
	} else if (vendor_id == VENDOR_BIEL) {
		pbt_buf = CTPM_BIEL;
		fw_len = sizeof(CTPM_BIEL);
		FT5X06_EX_PRINT("update firmware size:%d", fw_len);
	} else if(vendor_id == VENDOR_GIS) {
		pbt_buf = CTPM_GIS;
		fw_len = sizeof(CTPM_GIS);
		FT5X06_EX_PRINT("update firmware size:%d", fw_len);
        } else {
		FT5X06_EX_PRINT("read vendor_id fail");
		return -1;
	}

	if (fw_len < 8 || fw_len > 32 * 1024) {
		dev_err(&client->dev, "%s:FW length error\n", __func__);
		return -EIO;
	}

	if ((pbt_buf[fw_len - 8] ^ pbt_buf[fw_len - 6]) == 0xFF
                && (pbt_buf[fw_len - 7] ^ pbt_buf[fw_len - 5]) == 0xFF
                && (pbt_buf[fw_len - 3] ^ pbt_buf[fw_len - 4]) == 0xFF) {

                uc_host_fm_ver = pbt_buf[fw_len - 2];

                if ((uc_tp_fm_ver < uc_host_fm_ver) || (is_ic_update_crash == 1)) {
                        FT5X06_EX_PRINT("uc_tp_fm_ver is %d, uc_host_fm_ver is %d\n",uc_tp_fm_ver, uc_host_fm_ver);
                        i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, fw_len);
                        if (i_ret != 0)
                                dev_err(&client->dev, "%s:upgrade failed. err.\n", __func__);
                        else {
                                FT5X06_EX_PRINT("upgrade successfully.\n");
                                if (ft5x0x_read_reg(client, FT5x0x_REG_FW_VER, &uc_tp_fm_ver) >= 0)
                                        FT5X06_EX_PRINT("the new fw ver is 0x%02x\n", uc_tp_fm_ver);
                                //fts_ctpm_auto_clb();  //start auto CLB
                        }
                } else
                        FT5X06_EX_PRINT("current firmware is the newest, No need to upgrade\n");

	} else {
		dev_err(&client->dev, "%s:FW format error\n", __func__);
		return -EBADFD;
	}

	return i_ret;
}

u8 fts_ctpm_get_i_file_ver(void)
{
/*
	u16 ui_sz;

	ui_sz = sizeof(CTPM_FW);
	if (ui_sz > 2)
		return CTPM_FW[ui_sz - 2];
*/
	return 0x00;	/*default value */
}

/*update project setting
 *only update these settings for COB project, or for some special case
 */
int fts_ctpm_update_project_setting(struct i2c_client *client)
{
	u8 buf[FTS_SETTING_BUF_LEN];
	u8 reg_val[2] = {0};
	u8 auc_i2c_write_buf[10] = {0};
	u32 i = 0, j;

        for (i = 0, j = 0; i < FTS_UPGRADE_LOOP; i++) {
                /* Step 1:Reset  CTPM */
                fts_reset_to_upgrade(client);
                if (i<=15)
                        msleep(fts_updateinfo_curr.delay_55+i*3);
                else
                        msleep(fts_updateinfo_curr.delay_55-(i-15)*2);

                /* Step 2:Enter upgrade mode */
                auc_i2c_write_buf[0] = FT_UPGRADE_55;
                ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);
                mdelay(2);
                auc_i2c_write_buf[0] = FT_UPGRADE_AA;
                ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);

                /* Step 3:check READ-ID */
                msleep(fts_updateinfo_curr.delay_readid);
                auc_i2c_write_buf[0] = 0x90;
                auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
                ft5x0x_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

		if (reg_val[0] == fts_updateinfo_curr.upgrade_id_1
				&& reg_val[1] == fts_updateinfo_curr.upgrade_id_2) {

			FT5X06_EX_PRINT("read id okay ,ID1 = 0x%x, ID2 = 0x%x\n",
					reg_val[0], reg_val[1]);
			break;
		} else {
			dev_err(&client->dev, "read id failed: ID1 = 0x%x, ID2 = 0x%x\n",
					reg_val[0], reg_val[1]);
		}

        }

	/*set read start address */
	buf[0] = 0x3;
	buf[1] = 0x0;
#if defined(CONFIG_TOUCHSCREEN_FT5336_AUTO_UPG)
        buf[2] = (u8)(0x07b0 >> 8);
        buf[3] = (u8)(0x07b0);
#else
        buf[2] = 0x78;
        buf[3] = 0x0;
#endif

	ft5x0x_i2c_Read(client, buf, 4, buf, FTS_SETTING_BUF_LEN);
        msleep(10);
        FT5X06_EX_PRINT("uc_panel_factory_id = 0x%x\n", buf[4]);

        /* reset the new FW */
        ft5x0x_write_reg(client, 0xfc, 0xaa);
        msleep(30);
        ft5x0x_write_reg(client, 0xfc, 0x55);
        msleep(200);
        is_ic_update_crash = 1;

	return buf[4];
}

int fts_ctpm_auto_upgrade(struct i2c_client *client)
{
	u8 uc_host_fm_ver = FT5x0x_REG_FW_VER;
	int i_ret;

	i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
	if (i_ret == 0)	{
		msleep(300);
		uc_host_fm_ver = fts_ctpm_get_i_file_ver();
		dev_dbg(&client->dev, "upgrade to new version 0x%x\n",
				uc_host_fm_ver);
	} else {
		FT5X06_EX_PRINT("upgrade failed ret=%d.\n", i_ret);
		return -EIO;
	}

	return 0;
}

#if 0//dennis delete
/*
 *get upgrade information depend on the ic type
 */
static void fts_get_upgrade_info(struct Upgrade_Info *upgrade_info)
{
	switch (DEVICE_IC_TYPE) {
		case IC_FT5X06:
			upgrade_info->delay_55 = FT5X06_UPGRADE_55_DELAY;
			upgrade_info->delay_aa = FT5X06_UPGRADE_AA_DELAY;
			upgrade_info->upgrade_id_1 = FT5X06_UPGRADE_ID_1;
			upgrade_info->upgrade_id_2 = FT5X06_UPGRADE_ID_2;
			upgrade_info->delay_readid = FT5X06_UPGRADE_READID_DELAY;
			upgrade_info->delay_earse_flash = FT5X06_UPGRADE_EARSE_DELAY;
			break;
		case IC_FT5606:
			upgrade_info->delay_55 = FT5606_UPGRADE_55_DELAY;
			upgrade_info->delay_aa = FT5606_UPGRADE_AA_DELAY;
			upgrade_info->upgrade_id_1 = FT5606_UPGRADE_ID_1;
			upgrade_info->upgrade_id_2 = FT5606_UPGRADE_ID_2;
			upgrade_info->delay_readid = FT5606_UPGRADE_READID_DELAY;
			upgrade_info->delay_earse_flash = FT5606_UPGRADE_EARSE_DELAY;
			break;
		case IC_FT5316:
			upgrade_info->delay_55 = FT5316_UPGRADE_55_DELAY;
			upgrade_info->delay_aa = FT5316_UPGRADE_AA_DELAY;
			upgrade_info->upgrade_id_1 = FT5316_UPGRADE_ID_1;
			upgrade_info->upgrade_id_2 = FT5316_UPGRADE_ID_2;
			upgrade_info->delay_readid = FT5316_UPGRADE_READID_DELAY;
			upgrade_info->delay_earse_flash = FT5316_UPGRADE_EARSE_DELAY;
			break;
		case IC_FT6208:
			upgrade_info->delay_55 = FT6208_UPGRADE_55_DELAY;
			upgrade_info->delay_aa = FT6208_UPGRADE_AA_DELAY;
			upgrade_info->upgrade_id_1 = FT6208_UPGRADE_ID_1;
			upgrade_info->upgrade_id_2 = FT6208_UPGRADE_ID_2;
			upgrade_info->delay_readid = FT6208_UPGRADE_READID_DELAY;
			upgrade_info->delay_earse_flash = FT6208_UPGRADE_EARSE_DELAY;
			break;
		case IC_FT6x06:
			upgrade_info->delay_55 = FT6X06_UPGRADE_55_DELAY;
			upgrade_info->delay_aa = FT6X06_UPGRADE_AA_DELAY;
			upgrade_info->upgrade_id_1 = FT6X06_UPGRADE_ID_1;
			upgrade_info->upgrade_id_2 = FT6X06_UPGRADE_ID_2;
			upgrade_info->delay_readid = FT6X06_UPGRADE_READID_DELAY;
			upgrade_info->delay_earse_flash = FT6X06_UPGRADE_EARSE_DELAY;
			break;
		case IC_FT5x06i:
			upgrade_info->delay_55 = FT5X06_UPGRADE_55_DELAY;
			upgrade_info->delay_aa = FT5X06_UPGRADE_AA_DELAY;
			upgrade_info->upgrade_id_1 = FT5X06_UPGRADE_ID_1;
			upgrade_info->upgrade_id_2 = FT5X06_UPGRADE_ID_2;
			upgrade_info->delay_readid = FT5X06_UPGRADE_READID_DELAY;
			upgrade_info->delay_earse_flash = FT5X06_UPGRADE_EARSE_DELAY;
			break;
		case IC_FT5x36:
			upgrade_info->delay_55 = FT5X36_UPGRADE_55_DELAY;
			upgrade_info->delay_aa = FT5X36_UPGRADE_AA_DELAY;
			upgrade_info->upgrade_id_1 = FT5X36_UPGRADE_ID_1;
			upgrade_info->upgrade_id_2 = FT5X36_UPGRADE_ID_2;
			upgrade_info->delay_readid = FT5X36_UPGRADE_READID_DELAY;
			upgrade_info->delay_earse_flash = FT5X36_UPGRADE_EARSE_DELAY;
			break;
		default:
			break;
	}
}
#endif

void delay_qt_ms(unsigned long  w_ms)
{
	unsigned long i;
	unsigned long j;

	for (i = 0; i < w_ms; i++) {
		for (j = 0; j < 1000; j++) {
			udelay(1);
		}
	}
}


int fts_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
		u32 dw_lenth)
{
	u8 reg_val[2] = {0};
	u32 i = 0;
	u8 is_5336_new_bootloader = 0;
	u8 is_5336_fwsize_30 = 0;
	u32 packet_number;
	u32 j=0;
	u32 temp;
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
	// struct Upgrade_Info upgradeinfo;

	//fts_get_upgrade_info(&upgradeinfo);
	if (*(pbt_buf+dw_lenth-12) == 30)
		is_5336_fwsize_30 = 1;
	else
		is_5336_fwsize_30 = 0;

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/* Step 1:Reset  CTPM */
                fts_reset_to_upgrade(client);

		if (i<=15)
			msleep(fts_updateinfo_curr.delay_55+i*3);
		else
			msleep(fts_updateinfo_curr.delay_55-(i-15)*2);
		/* Step 2:Enter upgrade mode */
		auc_i2c_write_buf[0] = FT_UPGRADE_55;
		ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);
		mdelay(2);
		auc_i2c_write_buf[0] = FT_UPGRADE_AA;
		ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);

		/* Step 3:check READ-ID */
		msleep(fts_updateinfo_curr.delay_readid);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
		ft5x0x_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

		if (reg_val[0] == fts_updateinfo_curr.upgrade_id_1
				&& reg_val[1] == fts_updateinfo_curr.upgrade_id_2) {
			//dev_dbg(&client->dev, "Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
			//reg_val[0], reg_val[1]);
			FT5X06_EX_PRINT("read id okay ,ID1 = 0x%x, ID2 = 0x%x\n",
					reg_val[0], reg_val[1]);
			break;
		} else {
			dev_err(&client->dev, "read id failed: ID1 = 0x%x, ID2 = 0x%x\n",
					reg_val[0], reg_val[1]);
		}
		msleep(100);

	}
	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;
	auc_i2c_write_buf[0] = 0xcd;

	ft5x0x_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
	/*if (reg_val[0] > 4)
		is_5336_new_bootloader = 1;*/

	if (reg_val[0] <= 4) {
		is_5336_new_bootloader = BL_VERSION_LZ4 ;
	}
	else if(reg_val[0] == 7) {
		is_5336_new_bootloader = BL_VERSION_Z7 ;
	}
	else if(reg_val[0] >= 0x0f &&((fts_updateinfo_curr.CHIP_ID==0x11) ||
				(fts_updateinfo_curr.CHIP_ID==0x12) ||
				(fts_updateinfo_curr.CHIP_ID==0x13) ||
				(fts_updateinfo_curr.CHIP_ID==0x14))) {
		is_5336_new_bootloader = BL_VERSION_GZF ;
	}
	else {
		is_5336_new_bootloader = BL_VERSION_LZ4 ;
	}
	/*Step 4:erase app and panel paramenter area*/
	FT5X06_EX_PRINT("Step 4:erase app and panel paramenter area\n");
	auc_i2c_write_buf[0] = 0x61;
	ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);	/*erase app area */
	msleep(fts_updateinfo_curr.delay_earse_flash);
	/*erase panel parameter area */
	if(is_5336_fwsize_30) {
		auc_i2c_write_buf[0] = 0x63;
		ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);
	}
	msleep(100);

	/* Step 5:write firmware(FW) to ctpm flash */
	bt_ecc = 0;
	FT5X06_EX_PRINT("Step 5:write firmware(FW) to ctpm flash\n");

	//dw_lenth = dw_lenth - 8;
	if(is_5336_new_bootloader == BL_VERSION_LZ4 ||
			is_5336_new_bootloader == BL_VERSION_Z7 ) {
		dw_lenth = dw_lenth - 8;
	}
	else if(is_5336_new_bootloader == BL_VERSION_GZF) {
	      dw_lenth = dw_lenth - 14;
	}
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++) {
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (lenght >> 8);
		packet_buf[5] = (u8) lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++) {
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}

		ft5x0x_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH + 6);
		msleep(FTS_PACKET_LENGTH / 6 + 1);
		//FT5X06_EX_PRINT("write bytes:0x%04x\n", (j+1) * FTS_PACKET_LENGTH);
		//delay_qt_ms(FTS_PACKET_LENGTH / 6 + 1);
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++) {
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}

		ft5x0x_i2c_Write(client, packet_buf, temp + 6);
		msleep(20);
	}
#if 0

	/*send the last six byte */
	for (i = 0; i < 6; i++) {
		if (is_5336_new_bootloader && ((fts_updateinfo_curr.CHIP_ID==0x11) ||
					(fts_updateinfo_curr.CHIP_ID==0x12) ||(fts_updateinfo_curr.CHIP_ID==0x13) ||
					(fts_updateinfo_curr.CHIP_ID==0x14)))//5x36,5x36i
			temp = 0x7bfa + i;
		else
			temp = 0x6ffa + i;

		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = 1;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;
		packet_buf[6] = pbt_buf[dw_lenth + i];
		bt_ecc ^= packet_buf[6];
		ft5x0x_i2c_Write(client, packet_buf, 7);
		msleep(20);
	}
#else
	/*send the last six byte*/
	if(is_5336_new_bootloader == BL_VERSION_LZ4 ||
			is_5336_new_bootloader == BL_VERSION_Z7 ) {
		for (i = 0; i<6; i++) {
			if (is_5336_new_bootloader  == BL_VERSION_Z7 ) {
				temp = 0x7bfa + i;
			}
			else if(is_5336_new_bootloader == BL_VERSION_LZ4) {
				temp = 0x6ffa + i;
			}
			packet_buf[2] = (u8)(temp>>8);
			packet_buf[3] = (u8)temp;
			temp = 1;
			packet_buf[4] = (u8)(temp>>8);
			packet_buf[5] = (u8)temp;
			packet_buf[6] = pbt_buf[ dw_lenth + i];
			bt_ecc ^= packet_buf[6];

			ft5x0x_i2c_Write(client, packet_buf, 7);
			msleep(20);
		}
	}
	else if(is_5336_new_bootloader == BL_VERSION_GZF){
		for (i = 0; i<12; i++) {
			if (is_5336_fwsize_30) {
				temp = 0x7ff4 + i;
			}
			else {
				temp = 0x7bf4 + i;
			}
			packet_buf[2] = (u8)(temp>>8);
			packet_buf[3] = (u8)temp;
			temp = 1;
			packet_buf[4] = (u8)(temp>>8);
			packet_buf[5] = (u8)temp;
			packet_buf[6] = pbt_buf[ dw_lenth + i];
			bt_ecc ^= packet_buf[6];

			ft5x0x_i2c_Write(client, packet_buf, 7);
			msleep(20);

		}
	}

#endif
	/* Step 6: read out checksum */
	/* send the opration head */
	FT5X06_EX_PRINT("Step 6: read out checksum\n");
	auc_i2c_write_buf[0] = 0xcc;
	ft5x0x_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc) {
		dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",
				reg_val[0],
				bt_ecc);
		return -EIO;
	}

	/* Step 7: reset the new FW */
	FT5X06_EX_PRINT("Step 7: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(300);	//make sure CTP startup normally

	return 0;
}

/*sysfs debug*/

/*
 *get firmware size

 @firmware_name:firmware name
 *note:the firmware default path is sdcard.
 if you want to change the dir, please modify by yourself.
 */
static int ft5x0x_GetFirmwareSize(char *firmware_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	char filepath[128];
	memset(filepath, 0, sizeof(filepath));

	sprintf(filepath, "%s", firmware_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}



/*
 *read firmware buf for .bin file.

 @firmware_name: fireware name
 @firmware_buf: data buf of fireware

note:the firmware default path is sdcard.
if you want to change the dir, please modify by yourself.
 */
static int ft5x0x_ReadFirmware(char *firmware_name,
		unsigned char *firmware_buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s", firmware_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, firmware_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}



/*
   upgrade with *.bin file
 */

int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client,
		char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int i_ret=0;
	int retry = 5; 
	int fwsize = ft5x0x_GetFirmwareSize(firmware_name);

	if (fwsize <= 0) {
		dev_err(&client->dev, "%s ERROR:Get firmware size failed\n",
				__func__);
		return -EIO;
	}

	if (fwsize < 8 || fwsize > 32 * 1024) {
		dev_dbg(&client->dev, "%s:FW length error\n", __func__);
		return -EIO;
	}

	/*=========FW upgrade========================*/
	pbt_buf = kmalloc(fwsize + 1, GFP_ATOMIC);

	if (ft5x0x_ReadFirmware(firmware_name, pbt_buf)) {
		dev_err(&client->dev, "%s() - ERROR: request_firmware failed\n",
				__func__);
		kfree(pbt_buf);
		return -EIO;
	}

	if ((pbt_buf[fwsize - 8] ^ pbt_buf[fwsize - 6]) == 0xFF
			&& (pbt_buf[fwsize - 7] ^ pbt_buf[fwsize - 5]) == 0xFF
			&& (pbt_buf[fwsize - 3] ^ pbt_buf[fwsize - 4]) == 0xFF) {
		/*call the upgrade function */
		do {
			i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, fwsize);
			if (i_ret != 0)
				dev_dbg(&client->dev, "%s() - ERROR:[FTS] upgrade failed.. retry %d \n",
						__func__,retry);
			else {
#ifdef AUTO_CLB
				fts_ctpm_auto_clb(client);	/*start auto CLB*/
#endif
			}
		}while(i_ret && retry--);	/*retry for 5 times*/
		kfree(pbt_buf);
	} else {
		dev_dbg(&client->dev, "%s:FW format error\n", __func__);
		kfree(pbt_buf);
		return -EIO;
	}

	return i_ret;
}

static ssize_t ft5x0x_tpfwver_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&g_device_mutex);

	if (ft5x0x_read_reg(client, FT5x0x_REG_FW_VER, &fwver) < 0)
		num_read_chars = snprintf(buf, PAGE_SIZE,
				"get tp fw version fail!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02X\n", fwver);

	mutex_unlock(&g_device_mutex);

	return num_read_chars;
}

static ssize_t ft5x0x_tpfwver_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	/*place holder for future use*/
	return -EPERM;
}



static ssize_t ft5x0x_tprwreg_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}

static ssize_t ft5x0x_tprwreg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	ssize_t num_read_chars = 0;
	int retval;
	long unsigned int wmreg = 0;
	u8 regaddr = 0xff, regvalue = 0xff;
	u8 valbuf[5] = {0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&g_device_mutex);
	num_read_chars = count - 1;

	if (num_read_chars != 2) {
		if (num_read_chars != 4) {
			pr_info("please input 2 or 4 character\n");
			goto error_return;
		}
	}

	memcpy(valbuf, buf, num_read_chars);
	retval = strict_strtoul(valbuf, 16, &wmreg);

	if (0 != retval) {
		dev_err(&client->dev, "%s() - ERROR: Could not convert the "\
				"given input to a number." \
				"The given input was: \"%s\"\n",
				__func__, buf);
		goto error_return;
	}

	if (2 == num_read_chars) {
		/*read register*/
		regaddr = wmreg;
		if (ft5x0x_read_reg(client, regaddr, &regvalue) < 0)
			dev_err(&client->dev, "Could not read the register(0x%02x)\n",
					regaddr);
		else
			pr_info("the register(0x%02x) is 0x%02x\n",
					regaddr, regvalue);
	} else {
		regaddr = wmreg >> 8;
		regvalue = wmreg;
		if (ft5x0x_write_reg(client, regaddr, regvalue) < 0)
			dev_err(&client->dev, "Could not write the register(0x%02x)\n",
					regaddr);
		else
			dev_err(&client->dev, "Write 0x%02x into register(0x%02x) successful\n",
					regvalue, regaddr);
	}

error_return:
	mutex_unlock(&g_device_mutex);

	return count;
}

static ssize_t ft5x0x_fwupdate_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/*upgrade from *.i*/
static ssize_t ft5x0x_fwupdate_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ft5x0x_ts_data *data = NULL;
	u8 uc_host_fm_ver;
	int i_ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	data = (struct ft5x0x_ts_data *)i2c_get_clientdata(client);

	mutex_lock(&g_device_mutex);

	disable_irq(client->irq);
	i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
	if (i_ret == 0) {
		msleep(300);
		uc_host_fm_ver = fts_ctpm_get_i_file_ver();
		FT5X06_EX_PRINT("upgrade to new version 0x%x\n", uc_host_fm_ver);
	} else
		dev_err(&client->dev, "%s ERROR:[FTS] upgrade failed.\n",
				__func__);

	enable_irq(client->irq);
	mutex_unlock(&g_device_mutex);

	return count;
}

static ssize_t ft5x0x_fwupgradeapp_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}


/*upgrade from app.bin*/
static ssize_t ft5x0x_fwupgradeapp_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	char fwname[128];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count - 1] = '\0';

	mutex_lock(&g_device_mutex);
	disable_irq(client->irq);

	fts_ctpm_fw_upgrade_with_app_file(client, fwname);

	enable_irq(client->irq);
	mutex_unlock(&g_device_mutex);

	return count;
}

/*sysfs */
/*get the fw version
 *example:cat ftstpfwver
 */
static DEVICE_ATTR(ftstpfwver, S_IRUGO | S_IWUSR, ft5x0x_tpfwver_show,
		ft5x0x_tpfwver_store);

/*upgrade from *.i
 *example: echo 1 > ftsfwupdate
 */
static DEVICE_ATTR(ftsfwupdate, S_IRUGO | S_IWUSR, ft5x0x_fwupdate_show,
		ft5x0x_fwupdate_store);

/*read and write register
 *read example: echo 88 > ftstprwreg ---read register 0x88
 *write example:echo 8807 > ftstprwreg ---write 0x07 into register 0x88
 *
 *note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
 */
static DEVICE_ATTR(ftstprwreg, S_IRUGO | S_IWUSR, ft5x0x_tprwreg_show,
		ft5x0x_tprwreg_store);


/*upgrade from app.bin
 *example:echo "*_app.bin" > ftsfwupgradeapp
 */
static DEVICE_ATTR(ftsfwupgradeapp, S_IRUGO | S_IWUSR, ft5x0x_fwupgradeapp_show,
		ft5x0x_fwupgradeapp_store);


/*add your attr in here*/
static struct attribute *ft5x0x_attributes[] = {
	&dev_attr_ftstpfwver.attr,
	&dev_attr_ftsfwupdate.attr,
	&dev_attr_ftstprwreg.attr,
	&dev_attr_ftsfwupgradeapp.attr,
	NULL
};

static struct attribute_group ft5x0x_attribute_group = {
	.attrs = ft5x0x_attributes
};

/*create sysfs for debug*/
int ft5x0x_create_sysfs(struct i2c_client *client)
{
	int err;
	err = sysfs_create_group(&client->dev.kobj, &ft5x0x_attribute_group);
	if (0 != err) {
		dev_err(&client->dev,
				"%s() - ERROR: sysfs_create_group() failed.\n",
				__func__);
		sysfs_remove_group(&client->dev.kobj, &ft5x0x_attribute_group);
		return -EIO;
	} else {
		mutex_init(&g_device_mutex);
		pr_info("ft5x0x:%s() - sysfs_create_group() succeeded.\n",
				__func__);
	}
	return err;
}

void ft5x0x_release_sysfs(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &ft5x0x_attribute_group);
	mutex_destroy(&g_device_mutex);
}

#ifdef FTS_APK_DEBUG
/*create apk debug channel*/

#define PROC_UPGRADE			0
#define PROC_READ_REGISTER		1
#define PROC_WRITE_REGISTER	        2
#define PROC_RAWDATA			3
#define PROC_AUTOCLB			4

#define PROC_NAME	"ft5x0x-debug"
static unsigned char proc_operate_mode = PROC_RAWDATA;

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *ft5x0x_proc_entry;
/*interface of write proc*/
static int ft5x0x_debug_write(struct file *file, const char __user *buffer,
                        size_t count, loff_t *ppos)
{
	struct i2c_client *client = PDE_DATA(file_inode(file));
	unsigned char writebuf[FTS_PACKET_LENGTH];
	int buflen = count;
	int writelen = 0;
	int ret = 0;

	if (copy_from_user(&writebuf, buffer, buflen)) {
		dev_err(&client->dev, "%s:copy from user error\n", __func__);
		return -EFAULT;
	}
	proc_operate_mode = writebuf[0];

	switch (proc_operate_mode) {
		case PROC_UPGRADE:
			{
				char upgrade_file_path[64];
				memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
				sprintf(upgrade_file_path, "%s", writebuf + 1);
				upgrade_file_path[buflen-1] = '\0';
				FT5X06_EX_PRINT("%s\n", upgrade_file_path);
				disable_irq(client->irq);

				ret = fts_ctpm_fw_upgrade_with_app_file(client, upgrade_file_path);

				enable_irq(client->irq);
				if (ret < 0) {
					dev_err(&client->dev, "%s:upgrade failed.\n", __func__);
					return ret;
				}
			}
			break;
		case PROC_READ_REGISTER:
			writelen = 1;
			FT5X06_EX_PRINT("%s:register addr=0x%02x\n", __func__, writebuf[1]);
			ret = ft5x0x_i2c_Write(client, writebuf + 1, writelen);
			if (ret < 0) {
				dev_err(&client->dev, "%s:write iic error\n", __func__);
				return ret;
			}
			break;
		case PROC_WRITE_REGISTER:
			writelen = 2;
			ret = ft5x0x_i2c_Write(client, writebuf + 1, writelen);
			if (ret < 0) {
				dev_err(&client->dev, "%s:write iic error\n", __func__);
				return ret;
			}
			break;
		case PROC_RAWDATA:
			break;
		case PROC_AUTOCLB:
			fts_ctpm_auto_clb(client);
			break;
		default:
			break;
	}

	return count;
}

/*interface of read proc*/
static int ft5x0x_debug_read(struct file *file, char __user *buffer,
                        size_t count, loff_t *ppos)
{
	struct i2c_client *client = PDE_DATA(file_inode(file));
	int ret = 0;
	char *buf = buffer;
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;
	switch (proc_operate_mode) {
		case PROC_UPGRADE:
			/*after calling ft5x0x_debug_write to upgrade*/
			regaddr = 0xA6;
			ret = ft5x0x_read_reg(client, regaddr, &regvalue);
			if (ret < 0)
				num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
			else
				num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
			break;
		case PROC_READ_REGISTER:
			readlen = 1;
			ret = ft5x0x_i2c_Read(client, NULL, 0, buf, readlen);
			if (ret < 0) {
				dev_err(&client->dev, "%s:read iic error\n", __func__);
				return ret;
			} else
				FT5X06_EX_PRINT("%s:value=0x%02x\n", __func__, buf[0]);
			num_read_chars = 1;
			break;
		case PROC_RAWDATA:
			break;
		default:
			break;
	}

	return num_read_chars;
}

static const struct file_operations ft5x0x_debug_operations = {
        .owner = THIS_MODULE,
        .read  = ft5x0x_debug_read,
        .write = ft5x0x_debug_write,
};
#endif

int ft5x0x_create_apk_debug_channel(struct i2c_client * client)
{
#ifdef CONFIG_PROC_FS
	ft5x0x_proc_entry = proc_create_data(PROC_NAME, 0666, NULL,
					&ft5x0x_debug_operations, client);
	if (!ft5x0x_proc_entry) {
		dev_err(&client->dev, "Couldn't create proc entry!\n");
		return -ENOMEM;
	}
#endif

	return 0;
}

void ft5x0x_release_apk_debug_channel(void)
{
#ifdef CONFIG_PROC_FS
	if (ft5x0x_proc_entry)
		remove_proc_entry(PROC_NAME, NULL);
#endif
}
#endif

