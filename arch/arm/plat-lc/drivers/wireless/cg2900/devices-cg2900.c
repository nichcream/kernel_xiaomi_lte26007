/*
 * Copyright (C) ST-Ericsson SA 2011
 * Authors:
 * Par-Gunnar Hjalmdahl (par-gunnar.p.hjalmdahl@stericsson.com) for ST-Ericsson.
 * Henrik Possung (henrik.possung@stericsson.com) for ST-Ericsson.
 * Josef Kindberg (josef.kindberg@stericsson.com) for ST-Ericsson.
 * Dariusz Szymszak (dariusz.xd.szymczak@stericsson.com) for ST-Ericsson.
 * Kjell Andersson (kjell.k.andersson@stericsson.com) for ST-Ericsson.
 * Hemant Gupta (hemant.gupta@stericsson.com) for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 *
 * Board specific device support for the Linux Bluetooth HCI H:4 Driver
 * for ST-Ericsson connectivity controller.
 */

#define NAME			"devices-cg2900"
#define pr_fmt(fmt)		NAME ": " fmt "\n"

#include <asm/byteorder.h>
#include <asm-generic/errno-base.h>
#include <asm/mach-types.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include "include/cg2900.h"
#include "devices-cg2900.h"
#include "board_non_ste.h"

#define BT_VS_POWER_SWITCH_OFF		0xFD40

#define H4_HEADER_LENGTH		0x01
#define BT_HEADER_LENGTH		0x03

struct vs_power_sw_off_cmd {
	__le16	op_code;
	u8	len;
	u8	gpio_0_7_pull_up;
	u8	gpio_8_15_pull_up;
	u8	gpio_16_22_pull_up;
	u8	gpio_0_7_pull_down;
	u8	gpio_8_15_pull_down;
	u8	gpio_16_22_pull_down;
} __packed;

void dcg2900_enable_chip(struct cg2900_chip_dev *dev)
{
	struct dcg2900_info *info = dev->b_data;


	if (info->gbf_gpio == -1)
		return;

	/*
	 * - SET PMU_EN to high
	 * - Wait for 300usec
	 * - Set PDB to high.
	 */

	if (info->pmuen_gpio != -1) {
		/*
		 * We must first set PMU_EN pin high and then wait 300 us before
		 * setting the GBF_EN high.
		 */
		printk("%s : pmuen_gpio HIGH \n",__func__);
		gpio_set_value(info->pmuen_gpio, 1);
		udelay(CHIP_ENABLE_PMU_EN_TIMEOUT);
	}
	printk("%s : gbf_gpio HIGH \n",__func__);
	gpio_set_value(info->gbf_gpio, 1);
}

void dcg2900_disable_chip(struct cg2900_chip_dev *dev)
{
	struct dcg2900_info *info = dev->b_data;

	if (info->gbf_gpio != -1){
		printk("%s : gbf_gpio LOW \n",__func__);
		gpio_set_value(info->gbf_gpio, 0);
		}
	if (info->pmuen_gpio != -1){
		printk("%s : pmuen_gpio LOW \n",__func__);
		gpio_set_value(info->pmuen_gpio, 0);
		}
	schedule_timeout_killable(
			msecs_to_jiffies(CHIP_ENABLE_PDB_LOW_TIMEOUT));
}

static struct sk_buff *dcg2900_get_power_switch_off_cmd
				(struct cg2900_chip_dev *dev, u16 *op_code)
{
	struct sk_buff *skb;
	struct vs_power_sw_off_cmd *cmd;
	struct dcg2900_info *info;
	int i;
	int max_gpio = 23;
	printk("++ %s \n",__func__);

	/* If connected chip does not support the command return NULL */
	if (!check_chip_revision_support(dev->chip.hci_revision))
		return NULL;

	dev_dbg(dev->dev, "Generating PowerSwitchOff command\n");

	info = dev->b_data;

	skb = alloc_skb(sizeof(*cmd) + H4_HEADER_LENGTH, GFP_KERNEL);
	if (!skb) {
		dev_err(dev->dev, "Could not allocate skb\n");
		return NULL;
	}

	skb_reserve(skb, H4_HEADER_LENGTH);
	cmd = (struct vs_power_sw_off_cmd *)skb_put(skb, sizeof(*cmd));
	cmd->op_code = cpu_to_le16(BT_VS_POWER_SWITCH_OFF);
	cmd->len = sizeof(*cmd) - BT_HEADER_LENGTH;
	/*
	 * Enter system specific GPIO settings here:
	 * Section data[3-5] is GPIO pull-up selection
	 * Section data[6-8] is GPIO pull-down selection
	 * Each section is a bitfield where
	 * - byte 0 bit 0 is GPIO 0
	 * - byte 0 bit 1 is GPIO 1
	 * - up to
	 * - byte 2 bit 6 which is GPIO 22
	 * where each bit means:
	 * - 0: No pull-up / no pull-down
	 * - 1: Pull-up / pull-down
	 * All GPIOs are set as input.
	 */
	if(dev->chip.hci_revision == CG2900_PG1_SPECIAL_REV ||
		dev->chip.hci_revision == CG2900_PG1_REV ||
		dev->chip.hci_revision == CG2900_PG2_REV)
		max_gpio = 21;

	if (!info->sleep_gpio_set) {
		struct cg2900_platform_data *pf_data;

		pf_data = dev_get_platdata(dev->dev);
		for (i = 0; i < 8; i++) {
			if (pf_data->gpio_sleep[i] == CG2900_PULL_UP)
				info->gpio_0_7_pull_up |= (1 << i);
			else if (pf_data->gpio_sleep[i] == CG2900_PULL_DN)
				info->gpio_0_7_pull_down |= (1 << i);
		}
		for (i = 8; i < 16; i++) {
			if (pf_data->gpio_sleep[i] == CG2900_PULL_UP)
				info->gpio_8_15_pull_up |= (1 << (i - 8));
			else if (pf_data->gpio_sleep[i] == CG2900_PULL_DN)
				info->gpio_8_15_pull_down |= (1 << (i - 8));
		}
		for (i = 16; i < max_gpio; i++) {
			if (pf_data->gpio_sleep[i] == CG2900_PULL_UP)
				info->gpio_16_22_pull_up |= (1 << (i - 16));
			else if (pf_data->gpio_sleep[i] == CG2900_PULL_DN)
				info->gpio_16_22_pull_down |= (1 << (i - 16));
		}
		info->sleep_gpio_set = true;
	}
	cmd->gpio_0_7_pull_up = info->gpio_0_7_pull_up;
	cmd->gpio_8_15_pull_up = info->gpio_8_15_pull_up;
	cmd->gpio_16_22_pull_up = info->gpio_16_22_pull_up;
	cmd->gpio_0_7_pull_down = info->gpio_0_7_pull_down;
	cmd->gpio_8_15_pull_down = info->gpio_8_15_pull_down;
	cmd->gpio_16_22_pull_down = info->gpio_16_22_pull_down;


	if (op_code)
		*op_code = BT_VS_POWER_SWITCH_OFF;
	printk("-- %s \n",__func__);

	return skb;
}

int dcg2900_resource_setup(struct cg2900_chip_dev *dev,
					struct dcg2900_info *info)
{
	int err = 0;
	const char *gbf_name;
	const char *bt_name = NULL;
	const char *pmuen_name = NULL;
	struct cg2900_platform_data *pf_data = dev_get_platdata(dev->dev);

	if (pf_data->gpios.gbf_ena_reset == -1) {
		dev_err(dev->dev, "GBF GPIO does not exist\n");
		err = -EINVAL;
		goto err_handling;
	}
	info->gbf_gpio = pf_data->gpios.gbf_ena_reset;
	gbf_name = "gbf_ena_reset";

	/* BT Enable GPIO may not exist */
	if (pf_data->gpios.bt_enable != -1) {
		info->bt_gpio = pf_data->gpios.bt_enable;
		bt_name = "bt_enable";
	}

	/* PMU_EN GPIO may not exist */
	if (pf_data->gpios.pmu_en != -1) {
		info->pmuen_gpio = pf_data->gpios.pmu_en;
		pmuen_name = "pmu_en";
	}

	/* Now setup the GPIOs */
	err = gpio_request(info->gbf_gpio, gbf_name);
	if (err < 0) {
		dev_err(dev->dev, "gpio_request %s failed with err: %d\n",
			gbf_name, err);
		goto err_handling;
	}

	err = gpio_direction_output(info->gbf_gpio, 0);
	if (err < 0) {
		dev_err(dev->dev,
			"gpio_direction_output %s failed with err: %d\n",
			gbf_name, err);
		goto err_handling_free_gpio_gbf;
	}

	if (!pmuen_name)
		goto set_bt_gpio;

	err = gpio_request(info->pmuen_gpio, pmuen_name);
	if (err < 0) {
		dev_err(dev->dev, "gpio_request %s failed with err: %d\n",
			pmuen_name, err);
		goto err_handling_free_gpio_gbf;
	}

	err = gpio_direction_output(info->pmuen_gpio, 0);
	if (err < 0) {
		dev_err(dev->dev,
			"gpio_direction_output %s failed with err: %d\n",
			pmuen_name, err);
		goto err_handling_free_gpio_pmuen;
	}

set_bt_gpio:
	if (!bt_name)
		goto finished;

	err = gpio_request(info->bt_gpio, bt_name);
	if (err < 0) {
		dev_err(dev->dev, "gpio_request %s failed with err: %d\n",
			bt_name, err);
		goto err_handling_free_gpio_pmuen;
	}

	err = gpio_direction_output(info->bt_gpio, 1);
	if (err < 0) {
		dev_err(dev->dev,
			"gpio_direction_output %s failed with err: %d\n",
			bt_name, err);
		goto err_handling_free_gpio_bt;
	}

finished:

	return 0;

err_handling_free_gpio_bt:
	gpio_free(info->bt_gpio);
	info->bt_gpio = -1;
err_handling_free_gpio_pmuen:
	if (info->pmuen_gpio != -1) {
		gpio_free(info->pmuen_gpio);
		info->pmuen_gpio = -1;
	}
err_handling_free_gpio_gbf:
	gpio_free(info->gbf_gpio);
	info->gbf_gpio = -1;
err_handling:

	return err;
}

static int dcg2900_init(struct cg2900_chip_dev *dev)
{
	int err = 0;
	struct dcg2900_info *info;

	/* First retrieve and save the resources */
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(dev->dev, "Could not allocate dcg2900_info\n");
		return -ENOMEM;
	}

	info->gbf_gpio = -1;
	info->pmuen_gpio = -1;
	info->bt_gpio = -1;

	if (!dev->pdev->num_resources) {
		dev_dbg(dev->dev, "No resources available\n");
		goto finished;
	}

	err = dcg2900_resource_setup(dev, info);
	if (err)
		goto err_handling;

	info->regulator_enabled = false;
	info->regulator_wlan_enabled = false;
	info->regulator = NULL;
	info->regulator_wlan = NULL;


finished:
	dev->b_data = info;
	return 0;
err_handling:
	kfree(info);
	return err;
}

static void dcg2900_exit(struct cg2900_chip_dev *dev)
{
	struct dcg2900_info *info = dev->b_data;


	if (info->bt_gpio != -1)
		gpio_free(info->bt_gpio);
	if (info->pmuen_gpio != -1)
		gpio_free(info->pmuen_gpio);
	if (info->gbf_gpio != -1)
		gpio_free(info->gbf_gpio);
	kfree(info);
	dev->b_data = NULL;
}
void dcg2900_init_platdata(struct cg2900_platform_data *data)
{
	data->init = dcg2900_init;
	data->exit = dcg2900_exit;

	data->enable_chip = dcg2900_enable_chip;
	data->disable_chip = dcg2900_disable_chip;

	data->get_power_switch_off_cmd = dcg2900_get_power_switch_off_cmd;

	data->uart.enable_uart = dcg2900_enable_uart;
	data->uart.disable_uart = dcg2900_disable_uart;
}

