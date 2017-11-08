/*
*
*
* Use of source code is subject to the terms of the LeadCore license agreement
* under which you licensed source code. If you did not accept the terms of the
* license agreement, you are not authorized to use source code. For the terms
* of the license, please see the license agreement between you and LeadCore.
*
* Copyright (c) 2010-2019 LeadCoreTech Corp.
*
* PURPOSE: This file contains the comip hardware plarform's mmc driver.
*
* CHANGE HISTORY:
*
*	Version	   Date			Author		Description
*	1.0.0	   2012-03-19		xuxuefeng		created
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/suspend.h>
#include <asm/setup.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/sizes.h>

#include <plat/mmc.h>
#include <plat/clock.h>
#include <plat/hardware.h>
#include <plat/comip-setup.h>
#include <mach/comip-regs.h>
#include <plat/comip-battery.h>

#include <plat/comip-snd-notifier.h>

#define DRIVER_NAME	"comip-mmc"

//#define CONFIG_MMC_COMIP_DEBUG
#ifdef CONFIG_MMC_COMIP_DEBUG
#define MMC_PRINTK(fmt, args...) printk(KERN_INFO fmt, ##args)
#else
#define MMC_PRINTK(fmt, args...) printk(KERN_DEBUG fmt, ##args)
#endif

#define MMC_SWITCH_VOLTAGE					(11)
#define COMIP_MCI_ERR_RECOVERY_CNT				(10)
#define COMIP_MCI_SEND_CMD_TIMEOUT				(2500)	/* ms. */
#define COMIP_MCI_SEND_CMD_TIMEOUT_IRQ				(25)	/* ms. */
#define COMIP_MCI_CMD_DONE_TIMEOUT				(5 * HZ)
#define COMIP_MCI_DATA_DONE_TIMEOUT				(500 * HZ)
#define COMIP_MCI_WAIT_IDLE_TIMEOUT				(10000)	/* ms. */
#define COMIP_MCI_WAIT_IDLE_TIMEOUT_IRQ			(500)	/* ms. */

#define ERR_STS_SBE				(1 << SDMMC_SBE_INTR)

#ifdef CONFIG_MMC_COMIP_TUNING
#define MMC_CLKIN_SAMPLE_DELAY_MAX					(16)
#define COMIP_MCI_CMD21_TUNING_BLOCK				(128)
#define MMC_TRANSFER_TIMEOUT						(2000) /* us. */
#define MMC_RETRY_CNT								(60000)
#define MMC_SEND_CMD_TIMEOUT						(5000000)
#define MMC_WAIT_IDLE_TIMEOUT						(5000000)
//#define BLOCK_SIZE									(512)
#define BLOCK_TESTNUM								(8)

#define DMA_BACKUP_BUFFER_NUM_MAX	(256)
#define MMC_IDMAC_SHIFT			(32)
#define MMC_IDMAC_ADDR_MAX		(0x100000000ULL)
#define MMC_IDMAC_PFN_MAX		(1 << (MMC_IDMAC_SHIFT - PAGE_SHIFT))

struct mmc_provider {
	u32 mid;
	int rdelay;
	char *brand;
	int extra_flags;
};

static u32 g_mid = 0;
static struct mmc_provider providers[] = {
	{0x150100, 4, "Sumsung", 0},
	{0xfe014e, 1, "Micron", 0},
	{0x90014a, 5, "Hynix", 0},
	{0x13014e, 0, "Micron2G", MMCF_SUSPEND_DEVICE}
};
#endif

struct mmc_dma_des {
        unsigned int DES0;
        unsigned int DES1;
        unsigned int DES2;
        unsigned int DES3;
};

struct comip_mmc_host {
	struct mmc_host *mmc;
	spinlock_t lock;
	struct resource *res;
	void __iomem *base;
	int id;
	int irq;
	int dma;
	int dma_rx_hsno;
	int dma_tx_hsno;

	struct notifier_block audio_notify;

	struct clk *clk;
	int clk_enabled;
	int clk_low_power;
	unsigned int clock1;		/* input clock to sdmmc. */
	unsigned int clock2;		/* output clock to mmc card. */
	unsigned int clocklimit;	/* output clock limit. */
	unsigned int clockdiv;

	unsigned int err_sts;
	unsigned int err_cnt;
	unsigned int err_recovery;
	unsigned int flag_cmdseq; /* cmd<<16 | next_cmd, to handle special cmd */
	struct timer_list timer;
	struct tasklet_struct cmd_tasklet;
	struct workqueue_struct *cmd_workqueue;
	struct work_struct cmd_work;
	unsigned int cmd_use_workqueue;
	unsigned int cmdat_init;
	unsigned int ienable;
	unsigned int iused;
	unsigned int power_mode;
	unsigned int bus_width;
	unsigned short vdd;
	struct comip_mmc_platform_data *pdata;

	unsigned int cmdat;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_data *data;

	dma_addr_t sg_dma;	/* phsical address of dma description list. */
	struct mmc_dma_des *sg_cpu;
	unsigned int dma_len;
	unsigned int dma_dir;
#ifdef CONFIG_ARCH_PHYS_ADDR_T_64BIT
	unsigned long dma_backup[DMA_BACKUP_BUFFER_NUM_MAX];
	unsigned long page_link_backup[DMA_BACKUP_BUFFER_NUM_MAX];
#endif
};

#define COMIP_MCI_WRITE_REG32(value, offset)		\
	writel(value, host->base + offset)

#define COMIP_MCI_READ_REG32(offset)				\
	readl(host->base + offset)

static struct workqueue_struct *workqueue_detect;

static int comip_mmc_wait_for_bb(struct comip_mmc_host *host)
{
	unsigned long timeout;

	if (!in_interrupt())
		timeout = jiffies + msecs_to_jiffies(COMIP_MCI_WAIT_IDLE_TIMEOUT);
	else
		timeout = jiffies + msecs_to_jiffies(COMIP_MCI_WAIT_IDLE_TIMEOUT_IRQ);

	while (COMIP_MCI_READ_REG32(SDMMC_STATUS) & (1 << SDMMC_STATUS_DATA_BUSY)) {
		if (time_after(jiffies, timeout)) {
			MMC_PRINTK("mmc-%d: timeout waiting for idle %s.\n",
					host->id, in_interrupt() ? "in interrupt" : "");
			if (!in_interrupt())
				msleep(10);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int comip_mmc_send_cmd(struct comip_mmc_host *host,
				unsigned int cmdat)
{
	unsigned long timeout;

	COMIP_MCI_WRITE_REG32(cmdat, SDMMC_CMD);

	if (!in_interrupt())
		timeout = jiffies + msecs_to_jiffies(COMIP_MCI_SEND_CMD_TIMEOUT);
	else
		timeout = jiffies + msecs_to_jiffies(COMIP_MCI_SEND_CMD_TIMEOUT_IRQ);

	while (COMIP_MCI_READ_REG32(SDMMC_CMD) & (1 << SDMMC_CMD_START_CMD)) {
		if (time_after(jiffies, timeout)) {
			MMC_PRINTK("mmc-%d: timeout waiting for send command(0x%08x) %s.\n",
					host->id, cmdat, in_interrupt() ? "in interrupt" : "");
			if (!in_interrupt())
				msleep(10);
			return -ETIMEDOUT;
		}
	}

	return 0;
}
static void comip_mmc_stop_clock_uhs(struct comip_mmc_host *host)
{
	unsigned int reg32;
	unsigned int cmdat = (1 << SDMMC_CMD_START_CMD)
				| (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE)
				| (1 << SDMMC_CMD_UPDATE_CLOCK_REGISTERS_ONLY)
				|(1 << SDMMC_CMD_VOLT_SWITCH);

	reg32 = COMIP_MCI_READ_REG32(SDMMC_CLKENA);
	reg32 &= ~(1 << SDMMC_CLKENA_CCLK_EN);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CLKENA);

	/* Update clk setting to controller. */
	comip_mmc_send_cmd(host, cmdat);
}

static void comip_mmc_stop_clock(struct comip_mmc_host *host)
{
	unsigned int reg32;
	unsigned int cmdat = (1 << SDMMC_CMD_START_CMD)
				| (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE)
				| (1 << SDMMC_CMD_UPDATE_CLOCK_REGISTERS_ONLY);

	reg32 = COMIP_MCI_READ_REG32(SDMMC_CLKENA);
	reg32 &= ~(1 << SDMMC_CLKENA_CCLK_EN);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CLKENA);

	/* Update clk setting to controller. */
	comip_mmc_send_cmd(host, cmdat);
}
static void comip_mmc_start_clock_uhs(struct comip_mmc_host *host)
{
	unsigned int reg32;
	unsigned int cmdat = (1 << SDMMC_CMD_START_CMD)
				| (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE)
				| (1 << SDMMC_CMD_UPDATE_CLOCK_REGISTERS_ONLY)
				|(1 << SDMMC_CMD_VOLT_SWITCH);

	reg32 = COMIP_MCI_READ_REG32(SDMMC_CLKENA);
	reg32 |= (1 << SDMMC_CLKENA_CCLK_EN);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CLKENA);

	/* Update clk setting to controller. */
	comip_mmc_send_cmd(host, cmdat);
}

static void comip_mmc_start_clock(struct comip_mmc_host *host)
{
	unsigned int reg32;
	unsigned int cmdat = (1 << SDMMC_CMD_START_CMD)
				| (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE)
				| (1 << SDMMC_CMD_UPDATE_CLOCK_REGISTERS_ONLY);

	reg32 = COMIP_MCI_READ_REG32(SDMMC_CLKENA);
	reg32 |= (1 << SDMMC_CLKENA_CCLK_EN);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CLKENA);

	/* Update clk setting to controller. */
	comip_mmc_send_cmd(host, cmdat);
}

static void comip_mmc_set_clock(struct comip_mmc_host *host)
{
	unsigned int cmdat = (1 << SDMMC_CMD_START_CMD)
		| (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE)
				| (1 << SDMMC_CMD_UPDATE_CLOCK_REGISTERS_ONLY);

	COMIP_MCI_WRITE_REG32(host->clockdiv / 2, SDMMC_CLKDIV);

	/* Update clk setting to controller. */
	comip_mmc_send_cmd(host, cmdat);
}

static void comip_mmc_set_lower_power(struct comip_mmc_host *host)
{
	unsigned int reg32;
	unsigned int cmdat = (1 << SDMMC_CMD_START_CMD)
		| (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE)
				| (1 << SDMMC_CMD_UPDATE_CLOCK_REGISTERS_ONLY);

	reg32 = COMIP_MCI_READ_REG32(SDMMC_CLKENA);
	reg32 |= (1 << SDMMC_CLKENA_CCLK_LOW_POWER);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CLKENA);

	/* Update clk setting to controller. */
	comip_mmc_send_cmd(host, cmdat);
}

static void comip_mmc_enable_clock(struct comip_mmc_host *host)
{
	if (!host->clk_enabled) {
		comip_mmc_start_clock(host);
		host->clk_enabled = 1;
	}
}

static void comip_mmc_disable_clock(struct comip_mmc_host *host)
{
	if (host->clk_enabled) {
		comip_mmc_stop_clock(host);
		host->clk_enabled = 0;
	}
}

static void comip_mmc_update_clock(struct comip_mmc_host *host)
{
	/* Make sure the mmc card is not busy. */
	comip_mmc_wait_for_bb(host);

	/* Update MCI output clock. */
	comip_mmc_disable_clock(host);
	comip_mmc_set_clock(host);
	comip_mmc_enable_clock(host);
}

static void comip_mmc_low_power_clock(struct comip_mmc_host *host)
{
	if (host->mmc->card && !mmc_card_sdio(host->mmc->card)) {
		if (!host->clk_low_power) {
			/* Clock low power mode. */
			host->clk_low_power = 1;
			comip_mmc_set_lower_power(host);
		}
	}
}

static void comip_mmc_enable_irq(struct comip_mmc_host *host,
				unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->ienable |= val;
	COMIP_MCI_WRITE_REG32(host->ienable, SDMMC_INTEN);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void comip_mmc_disable_irq(struct comip_mmc_host *host,
				unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->ienable &= ~val;
	COMIP_MCI_WRITE_REG32(host->ienable, SDMMC_INTEN);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void comip_mmc_error_recovery(struct comip_mmc_host *host)
{
	if (!(host->pdata->flags & MMCF_ERROR_RECOVERY))
		return;

	if (host->err_recovery)
		return;

	if (host->pdata && host->pdata->get_cd
		&& !(host->pdata->get_cd(mmc_dev(host->mmc))))
		return;

	host->err_recovery = 1;
}

static void comip_mmc_cmd_error(struct comip_mmc_host *host, struct mmc_command *cmd)
{
	if (!cmd->retries && (host->pdata->flags & MMCF_ERROR_RECOVERY)) {
		comip_mmc_error_recovery(host);
		cmd->retries = 1;
	}
}

static void comip_mmc_data_error(struct comip_mmc_host *host)
{
	if (host->pdata->flags & MMCF_ERROR_RECOVERY) {
		comip_mmc_error_recovery(host);
	}
}

static int comip_mmc_get_cd_none(struct device *dev) {
	return 0;
}

static void comip_mmc_check_recovery(struct comip_mmc_host *host)
{
	int ret = 0;
	int max = COMIP_MCI_ERR_RECOVERY_CNT;

	if (!host->err_recovery)
		return;

	/* set recovery times for SD card */
	if (0 == host->id)
		max = 3;

	host->err_recovery = 0;
	host->err_cnt++;
	if (host->err_cnt >= max) {
		MMC_PRINTK("mmc-%d: err_cnt >= max\n", host->id);
		host->err_cnt = 0;
		if (host->pdata) {
			if (host->pdata->get_cd && !(host->pdata->get_cd(mmc_dev(host->mmc)))) {
				mmc_card_set_removed(host->mmc->card);
				MMC_PRINTK("mmc-%d: mmc_card_set_removed\n", host->id);
				return;
			} else if (0 == host->id) {
				/* for no hotplug SD card */
				host->pdata->get_cd = comip_mmc_get_cd_none;
				if(host->mmc->card)
					mmc_card_set_removed(host->mmc->card);
				MMC_PRINTK("mmc-%d: set SD card removed\n", host->id);
				return;
			}
		}
	} else
		MMC_PRINTK("mmc-%d: error count (%d/%d)\n", host->id, host->err_cnt, max);

	if (host->mmc->card) {
		ret = mmc_power_save_host(host->mmc);
		MMC_PRINTK("mmc-%d: mmc_power_save_host %d\n", host->id, ret);

		if (mmc_card_sd(host->mmc->card))
			host->mmc->f_max = host->clocklimit / 2;

		if (mmc_card_mmc(host->mmc->card)) {
			host->mmc->f_max = host->clocklimit / 2;
			if(host->pdata->ocr_mask & MMC_VDD_165_195)
				host->mmc->ocr |= MMC_VDD_165_195;
		}

		msleep(20);
		ret = mmc_power_restore_host(host->mmc);
		MMC_PRINTK("mmc-%d: mmc_power_restore_host %d\n", host->id, ret);
	}
}

static void comip_mmc_init_hw(struct comip_mmc_host *host)
{
	unsigned int reg32;
	struct clk *mmc_clk = host->clk;

	/* Set clock delay. */
	reg32 = readl(mmc_clk->divclk_reg);
	reg32 &= ~(0xf << CPR_SDMMCCLKCTL_SDMMC_CLK2_DELAY);
	reg32 |= (host->pdata->clk_write_delay << CPR_SDMMCCLKCTL_SDMMC_CLK2_DELAY);
	reg32 &= ~(0xf << CPR_SDMMCCLKCTL_SDMMC_CLK1_DELAY);
	reg32 |= (host->pdata->clk_read_delay << CPR_SDMMCCLKCTL_SDMMC_CLK1_DELAY);
	writel(reg32, mmc_clk->divclk_reg);

	/* Reset. */
	reg32 = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	reg32 |= (1 << SDMMC_CTRL_CONTROLLER_RESET);
	reg32 |= (1 << SDMMC_CTRL_FIFO_RESET);
	reg32 |= (1 << SDMMC_CTRL_DMA_RESET);
	reg32 |= (1 << SDMMC_CTRL_DMA_ENABLE);
	reg32 |= (1 << SDMMC_CTRL_INT_ENABLE);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CTRL);

	/* Disable all interrupts. */
	comip_mmc_disable_irq(host, 0xffffffff);

	/* Clear all intr. */
	COMIP_MCI_WRITE_REG32(0xffffffff, SDMMC_RINTSTS);

	/* Set the timeout. */
	COMIP_MCI_WRITE_REG32(0xffffffff, SDMMC_TMOUT);

	/* Set debounce time. */
	COMIP_MCI_WRITE_REG32(0x94C5F, SDMMC_DEBNCE);

	if (2 == host->id) {
		/* For wifi, fifo threshold: 4 transfer, rx_mark=3, tx mark=4.*/
		COMIP_MCI_WRITE_REG32(0x10030004, SDMMC_FIFOTH);
	} else {
		/* Set fifo threshold to 16 transfer rx_mark=127, tx mark=128. */
		COMIP_MCI_WRITE_REG32(0x307f0080, SDMMC_FIFOTH);
		/* Set card thresholde blksize = 512. */
		COMIP_MCI_WRITE_REG32(0x02000001, SDMMC_CARDTHRCTL);
	}

	/* 1 bit card. */
	COMIP_MCI_WRITE_REG32(0, SDMMC_CTYPE);

	/* Set data num that will be sent. */
	COMIP_MCI_WRITE_REG32(0, SDMMC_BYTCNT);

	/* RI and TI int */
	COMIP_MCI_WRITE_REG32(0, SDMMC_IDINTEN);
	wmb();
}

static void comip_mmc_setup_data(struct comip_mmc_host *host,
				struct mmc_data *data)
{
	unsigned int reg32;
	unsigned int nob = data->blocks;
	unsigned int length;
	int i;

	host->data = data;

	if (data->flags & MMC_DATA_STREAM)
		nob = 0xffff;

	COMIP_MCI_WRITE_REG32(0xffffffff, SDMMC_RINTSTS);

	COMIP_MCI_WRITE_REG32(nob * data->blksz, SDMMC_BYTCNT);
	COMIP_MCI_WRITE_REG32(data->blksz, SDMMC_BLKSIZ);

	/* Reset SDMMC FIFO.  */
	reg32 = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	reg32 |= (1 << SDMMC_CTRL_FIFO_RESET | 1<< SDMMC_CTRL_DMA_RESET);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CTRL);
	while(COMIP_MCI_READ_REG32(SDMMC_CTRL)&((1 << SDMMC_CTRL_FIFO_RESET | 1<< SDMMC_CTRL_DMA_RESET)));

	if (data->flags & MMC_DATA_READ)
		host->dma_dir = DMA_FROM_DEVICE;
	else
		host->dma_dir = DMA_TO_DEVICE;

	if (!(host->pdata->flags & MMCF_DMA_COHERENT)) {
#ifdef CONFIG_ARCH_PHYS_ADDR_T_64BIT
		for (i = 0; i < data->sg_len; i++) {
			if (page_to_pfn((struct page *)(data->sg[i].page_link & ~0x3)) >= MMC_IDMAC_PFN_MAX) {
				if (!(data->flags & MMC_DATA_READ)) {
					struct page *page = (struct page *)(data->sg[i].page_link & ~0x3);
					void *backup_vaddr = (void *)host->dma_backup[i] +
						data->sg[i].offset;
					unsigned long pfn = page_to_pfn(page) +
						data->sg[i].offset / PAGE_SIZE;
					unsigned int offset = data->sg[i].offset % PAGE_SIZE;
					unsigned int left = sg_dma_len(&data->sg[i]);
					do {
						unsigned int len = left;
						void *vaddr = NULL;
						page = pfn_to_page(pfn);
						if (len + offset > PAGE_SIZE)
							len = PAGE_SIZE - offset;
						vaddr = kmap(page);
						if (!vaddr) {
							MMC_PRINTK("mmc-%d: kmap failed\n", host->id);
							return;
						}
						memcpy(backup_vaddr, vaddr + offset, len);
						kunmap(page);
						offset = 0;
						pfn++;
						left -= len;
						backup_vaddr += len;
					} while (left);
				}
				host->page_link_backup[i] = data->sg[i].page_link;
				data->sg[i].page_link = (unsigned long)phys_to_page(virt_to_phys((void *)(host->dma_backup[i])));
			}
		}
#endif
		host->dma_len = dma_map_sg(mmc_dev(host->mmc), data->sg, data->sg_len, host->dma_dir);
	} else {
		for(i = 0; i< data->sg_len; i++)
			sg_dma_address(&data->sg[i]) = page_to_phys((struct page*)(data->sg[i].page_link & ~0x3)) + data->sg[i].offset;
		host->dma_len = data->sg_len;
	}

	for (i = 0; i < host->dma_len; i++) {
		length = sg_dma_len(&data->sg[i]);
		host->sg_cpu[i].DES1 = length;
		if (sg_dma_address(&data->sg[i]) >= MMC_IDMAC_ADDR_MAX) {
			printk("mmc-%d: sg_dma_address is 0x%llx\n", host->id, (u64)sg_dma_address(&data->sg[i]));
			BUG();
		}
		host->sg_cpu[i].DES2 = (u32)sg_dma_address(&data->sg[i]);
		host->sg_cpu[i].DES0 = (1 << 31) | (1 << 4);	//bit 31:own,bit 4:sg mode
		if(0 == i)
			host->sg_cpu[i].DES0 |= 1 << 3;	//fisrt des
		if((host->dma_len - 1) == i)
			host->sg_cpu[i].DES0 |= 1 << 2;	//last des
	}
	wmb();

	COMIP_MCI_WRITE_REG32(host->sg_dma, SDMMC_DBADDR);

	/* start idmac */
	COMIP_MCI_WRITE_REG32((COMIP_MCI_READ_REG32(SDMMC_CTRL) | 1<<25), SDMMC_CTRL);
	wmb();
	COMIP_MCI_WRITE_REG32((COMIP_MCI_READ_REG32(SDMMC_BMOD)|(1<<7 | 1<<1)), SDMMC_BMOD);

	COMIP_MCI_WRITE_REG32(1, SDMMC_PLDMND);
}

static void comip_mmc_finish_request(struct comip_mmc_host *host,
					struct mmc_request *mrq)
{
	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;
	smp_wmb();

	if (mrq)
		mmc_request_done(host->mmc, mrq);
	comip_mmc_low_power_clock(host);
}

static inline void comip_mmc_special_cmd(struct comip_mmc_host *host)
{
	if(host->cmd->opcode == MMC_ERASE) {
		host->flag_cmdseq = (MMC_ERASE)<<16 | MMC_SEND_STATUS;
	}
}

static inline void comip_mmc_special_handle(struct comip_mmc_host *host)
{
	if (NULL == host || NULL == host->cmd || !(host->flag_cmdseq))
		return;

	if ((host->flag_cmdseq & 0xffff) == host->cmd->opcode) {
		MMC_PRINTK("%s: CMD%d -> CMD%d\n", __func__, (host->flag_cmdseq>>16), host->cmd->opcode);
		host->flag_cmdseq = 0;

		if (host->cmd->retries)
			host->cmd->retries = 100;
	}
}

static int comip_mmc_start_cmd(struct comip_mmc_host *host)
{
	struct mmc_command cmd_copy;
	u32 reg32 = 0;
	u32 retry = 0;

	if (!host->cmd || !host->mrq) {
		MMC_PRINTK("mmc-%d: host command is null.\n", host->id);
		return -EINVAL;
	}

	if (ERR_STS_SBE == host->err_sts) {
		/* Reset SDMMC FIFO.  */
		reg32 = COMIP_MCI_READ_REG32(SDMMC_CTRL);
		reg32 |= (1 << SDMMC_CTRL_FIFO_RESET);
		COMIP_MCI_WRITE_REG32(reg32, SDMMC_CTRL);
		while ((COMIP_MCI_READ_REG32(SDMMC_CTRL) & (1 << SDMMC_CTRL_FIFO_RESET))
			&& (retry++ < MMC_RETRY_CNT));
			udelay(1);
	}

	/* For mmc, don't wait for DATA_BUSY bit clear. */
	if (host->mmc->card && mmc_card_mmc(host->mmc->card))
		goto skip_bb;

	/* Make sure the mmc card is not busy. */
	if (comip_mmc_wait_for_bb(host) < 0) {
		if (!in_interrupt()) {
			MMC_PRINTK("mmc-%d: device is busy, command %d, retries %d.\n",
					host->id, host->cmd->opcode, host->cmd->retries);
			comip_mmc_special_handle(host);
			memcpy(&cmd_copy, host->cmd, sizeof(struct mmc_command));
			if (host->mrq->cmd)
				host->mrq->cmd->error = -ETIMEDOUT;
			comip_mmc_finish_request(host, host->mrq);
			comip_mmc_cmd_error(host, &cmd_copy);
		} else {
			printk(KERN_DEBUG "mmc-%d: device is busy(in interrupt), command %d.\n",
					host->id, host->cmd->opcode);
			queue_work(host->cmd_workqueue, &host->cmd_work);
		}
		return -ETIMEDOUT;
	}

skip_bb:
	COMIP_MCI_WRITE_REG32(host->cmd->arg, SDMMC_CMDARG);

	if (host->cmd->flags & MMC_RSP_PRESENT) {
		if (host->cmd->flags & MMC_RSP_136)
			host->cmdat |= ((1 << SDMMC_CMD_RESPONSE_EXPECT)
				| (1 << SDMMC_CMD_RESPONSE_LENGTH));
		else
			host->cmdat |= (1 << SDMMC_CMD_RESPONSE_EXPECT);
	}

	host->cmdat |= (1 << SDMMC_CMD_START_CMD);
	host->cmdat |= (1 << SDMMC_CMD_USE_HOLD_REG);
	host->cmdat |= host->cmd->opcode;

	if (host->pdata->flags & MMCF_MONITOR_TIMER)
		mod_timer(&host->timer, jiffies + COMIP_MCI_CMD_DONE_TIMEOUT);

	if(host->cmd->opcode == MMC_SWITCH_VOLTAGE) {
		comip_mmc_start_clock_uhs(host);
		host->cmdat |= (1 << SDMMC_CMD_VOLT_SWITCH);
		comip_mmc_enable_irq(host, (1 << SDMMC_HTO_INTR));
	} else {
		comip_mmc_enable_irq(host, (1 << SDMMC_CD_INTR));
	}

	/* some special cmd like MMC_ERASE, need to set flag_cmdseq */
	comip_mmc_special_cmd(host);

	wmb();

	if (ERR_STS_SBE == host->err_sts) {
		comip_mmc_send_cmd(host, (host->cmdat & ~(1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE))
									| (1 << SDMMC_CMD_STOP_ABORT_CMD));
		host->err_sts = 0;
	} else {
		comip_mmc_send_cmd(host, host->cmdat);
	}

	return 0;
}

static void comip_mmc_dma_reset(struct comip_mmc_host *host)
{
	u32 v;

	v = COMIP_MCI_READ_REG32(SDMMC_BMOD);
	v &= ~(1 << 7);
	COMIP_MCI_WRITE_REG32(v, SDMMC_BMOD);
	v |= (1 << 0);
	COMIP_MCI_WRITE_REG32(v, SDMMC_BMOD);

	v = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	COMIP_MCI_WRITE_REG32((v & (~1 << 25)), SDMMC_CTRL);
	COMIP_MCI_WRITE_REG32((v | (1 << SDMMC_CTRL_DMA_RESET)), SDMMC_CTRL);
	while (COMIP_MCI_READ_REG32(SDMMC_CTRL) & (1 << SDMMC_CTRL_DMA_RESET));
}
static int comip_mmc_data_done(struct comip_mmc_host *host,
				unsigned int stat)
{
	struct mmc_data *data = host->data;
#ifdef CONFIG_ARCH_PHYS_ADDR_T_64BIT
	unsigned int i;
#endif

	if (host->pdata->flags & MMCF_MONITOR_TIMER)
		del_timer(&host->timer);

	if (!data)
		return 0;

	comip_mmc_dma_reset(host);
	if (!(host->pdata->flags & MMCF_DMA_COHERENT)) {
		dma_unmap_sg(mmc_dev(host->mmc), data->sg, host->dma_len, host->dma_dir);
#ifdef CONFIG_ARCH_PHYS_ADDR_T_64BIT
		for (i = 0; i < data->sg_len; i++) {
			if (host->page_link_backup[i] &&
					(page_to_pfn((struct page *)(host->page_link_backup[i] & ~0x3)) >= MMC_IDMAC_PFN_MAX)) {
				if (data->flags & MMC_DATA_READ) {
					struct page *page = (struct page *)(host->page_link_backup[i] & ~0x3);
					void *backup_vaddr = (void *)host->dma_backup[i] +
						data->sg[i].offset;
					unsigned long pfn = page_to_pfn(page) +
						data->sg[i].offset / PAGE_SIZE;
					unsigned int offset = data->sg[i].offset % PAGE_SIZE;
					unsigned int left = sg_dma_len(&data->sg[i]);
					do {
						unsigned int len = left;
						void *vaddr = NULL;
						page = pfn_to_page(pfn);
						if (len + offset > PAGE_SIZE)
							len = PAGE_SIZE - offset;
						vaddr = kmap_atomic(page);
						if (!vaddr) {
							MMC_PRINTK("mmc-%d: kmap atomic failed\n", host->id);
							return 0;
						}
						memcpy(vaddr + offset, backup_vaddr, len);
						kunmap_atomic(vaddr);
						offset = 0;
						pfn++;
						left -= len;
						backup_vaddr += len;
					} while (left);
				}
			}
			data->sg[i].page_link = host->page_link_backup[i];
		}
#endif
	}

#ifdef CONFIG_ARCH_PHYS_ADDR_T_64BIT
	memset(&host->page_link_backup, 0x0, sizeof(host->page_link_backup[0]) * host->mmc->max_segs);
#endif

	if (host->mrq->data->flags & MMC_DATA_WRITE)
		stat &= ~(1 << SDMMC_DCRC_INTR);

	if (stat & (1 << SDMMC_DRTO_INTR)) {
		data->error = -ETIMEDOUT;
		MMC_PRINTK("mmc-%d: send data timeout. 0x%08x \n", host->id, stat);

		//for SD and mmc, DRTO int needn't recovery.
		if (host->mmc->card && (mmc_card_sd(host->mmc->card) || mmc_card_mmc(host->mmc->card)))
			MMC_PRINTK("mmc-%d: DRTO int but on recovery\n", host->id);
		else
			comip_mmc_data_error(host);

	} else if (stat & (1 << SDMMC_DCRC_INTR)) {
		data->error = -EINVAL;
		MMC_PRINTK("mmc-%d: send data crc error. 0x%08x \n", host->id, stat);
		comip_mmc_data_error(host);
	} else if (stat & (1 << SDMMC_SBE_INTR)) {
		data->error = -EIO;
		MMC_PRINTK("mmc-%d: send data st bit error. 0x%08x \n", host->id, stat);
		host->err_sts = ERR_STS_SBE;
		comip_mmc_data_error(host);
	}

	/*
	 * There appears to be a hardware design bug here. There seems to
	 * be no way to find out how much data was transferred to the card.
	 * This means that if there was an error on any block, we mark all
	 * data blocks as being in error.
	 */
	if (!data->error)
		data->bytes_xfered = data->blocks * data->blksz;
	else
		data->bytes_xfered = 0;

	comip_mmc_disable_irq(host, (1 << SDMMC_DTO_INTR));
	smp_wmb();

	host->data = NULL;
	if (host->mrq->stop && (data->error || !host->mrq->sbc)) {
		host->cmd = host->mrq->stop;
		host->cmdat = host->cmdat_init;
		if (host->cmd_use_workqueue)
			queue_work(host->cmd_workqueue, &host->cmd_work);
		else
			tasklet_schedule(&host->cmd_tasklet);
	} else
		comip_mmc_finish_request(host, host->mrq);

	return 1;
}

static int comip_mmc_cmd_done(struct comip_mmc_host *host,
				unsigned int stat)
{
	u32 reg32 = 0;
	struct mmc_command *cmd = host->cmd;

	if (host->pdata->flags & MMCF_MONITOR_TIMER)
		del_timer(&host->timer);

	if (!cmd)
		return 0;

	host->cmd = NULL;

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[0] = COMIP_MCI_READ_REG32(SDMMC_RESP3);
			cmd->resp[1] = COMIP_MCI_READ_REG32(SDMMC_RESP2);
			cmd->resp[2] = COMIP_MCI_READ_REG32(SDMMC_RESP1);
			cmd->resp[3] = COMIP_MCI_READ_REG32(SDMMC_RESP0);
			if (MMC_ALL_SEND_CID == cmd->opcode)
				g_mid = cmd->resp[0] >> 8;
		} else {
			cmd->resp[0] = COMIP_MCI_READ_REG32(SDMMC_RESP0);
		}
	}

	if (stat & (1 << SDMMC_RTO_INTR)) {
		cmd->error = -ETIMEDOUT;
		if (host->mmc->card && mmc_card_present(host->mmc->card)) {
			MMC_PRINTK("mmc-%d: send command timeout. "
				"status: 0x%08x, command: %d, response: 0x%08x\n",
				host->id, stat, cmd->opcode, cmd->resp[0]);
			if ((0 == host->id) && (SD_SEND_IF_COND == cmd->opcode)) {
				/* CMD8 is harmless for low than SD 2.0 */
			} else {
				comip_mmc_cmd_error(host, cmd);
			}
		}
	} else if ((stat & (1 << SDMMC_RCRC_INTR))
		   && (cmd->flags & (1 << MMC_RSP_CRC))) {
		cmd->error = -EINVAL;
		if (host->mmc->card && mmc_card_present(host->mmc->card)) {
			MMC_PRINTK("mmc-%d: send cmmand crc error. "
				"status: 0x%08x, command: %d, response: 0x%08x\n",
				host->id, stat, cmd->opcode, cmd->resp[0]);
			comip_mmc_cmd_error(host, cmd);
		}
	}

	/* Finished CMD23, now send actual command. */
	if (cmd == host->mrq->sbc) {
		host->cmdat = host->cmdat_init;
		host->cmdat_init &= ~(1 << SDMMC_CMD_SEND_INITIALIZATION);
		if (host->mrq->data) {
			host->cmdat |= (1 << SDMMC_CMD_DATA_TRANSFER_EXPECTED);
			if (host->mrq->data->flags & MMC_DATA_WRITE)
				host->cmdat |= (1 << SDMMC_CMD_READ_WRITE);
			else
				host->cmdat |= (1 << SDMMC_CMD_CHECK_RESPONSE_CRC);
			if (host->mrq->data->flags & MMC_DATA_STREAM)
				host->cmdat |= (1 << SDMMC_CMD_TRANSFER_MODE);
		}
		host->cmd = host->mrq->cmd;
		if (host->cmd_use_workqueue)
			queue_work(host->cmd_workqueue, &host->cmd_work);
		else
			tasklet_schedule(&host->cmd_tasklet);
	} else {
		if ((MMC_SWITCH_VOLTAGE == cmd->opcode) && (stat & (1 << SDMMC_HTO_INTR))) {
			MMC_PRINTK("mmc-%d: send cmmand 11. "
					"status: 0x%08x, response: 0x%08x\n",
					host->id, stat, cmd->resp[0]);
			reg32 = COMIP_MCI_READ_REG32(SDMMC_RINTSTS);
			reg32 |= (1<<SDMMC_HTO_INTR);
			COMIP_MCI_WRITE_REG32(reg32, SDMMC_RINTSTS);
			comip_mmc_disable_irq(host, (1 << SDMMC_HTO_INTR));

		} else {
			comip_mmc_disable_irq(host, (1 << SDMMC_CD_INTR));
		}

		if (host->data && !cmd->error) {
			if (host->pdata->flags & MMCF_MONITOR_TIMER)
				mod_timer(&host->timer, jiffies + COMIP_MCI_DATA_DONE_TIMEOUT);

			if (0 == host->id)
				comip_mmc_enable_irq(host, (1 << SDMMC_DTO_INTR) | (1 << SDMMC_SBE_INTR)
						| (1 << SDMMC_DRTO_INTR));
			else if (1 == host->id)
				comip_mmc_enable_irq(host, (1 << SDMMC_DTO_INTR) | (1 << SDMMC_DRTO_INTR));
			else
				comip_mmc_enable_irq(host, (1 << SDMMC_DTO_INTR));
		} else {
			comip_mmc_finish_request(host, host->mrq);
		}
	}
	return 1;
}

static void comip_mmc_timer_func(unsigned long data)
{
	struct comip_mmc_host *host = (struct comip_mmc_host *)data;

	del_timer(&host->timer);
	MMC_PRINTK("mmc-%d: send %s%stimeout(timer expired). "
		"command: %d\n",
		host->id,
		(host->ienable & (1 << SDMMC_CD_INTR)) ? "command " : "",
		(host->ienable & (1 << SDMMC_DTO_INTR)) ? "data " : "",
		host->cmd ? host->cmd->opcode : -1);

	MMC_PRINTK("mmc-%d: SDMMC_MINTSTS(0x%x) SDMMC_RINTSTS(0x%x) SDMMC_STATUS(0x%x)\n",
		host->id, COMIP_MCI_READ_REG32(SDMMC_MINTSTS),
		COMIP_MCI_READ_REG32(SDMMC_RINTSTS), COMIP_MCI_READ_REG32(SDMMC_STATUS));

	if (host->cmd) {
		comip_mmc_cmd_error(host, host->cmd);
		host->cmd->error = -ETIMEDOUT;
		host->cmd = NULL;
	}

	if (host->mrq) {
		comip_mmc_disable_irq(host, (1 << SDMMC_CD_INTR) | (1 << SDMMC_DTO_INTR));
		comip_mmc_finish_request(host, host->mrq);
	}
}

static void comip_mmc_cmd_tasklet(unsigned long data)
{
	struct comip_mmc_host *host = (struct comip_mmc_host *)data;

	comip_mmc_start_cmd(host);
}

static void comip_mmc_cmd_wq(struct work_struct *work)
{
	struct comip_mmc_host *host =
		container_of(work, struct comip_mmc_host, cmd_work);

	comip_mmc_start_cmd(host);
}

static irqreturn_t comip_mmc_irq(int irq, void *devid)
{
	struct comip_mmc_host *host = devid;
	unsigned int int_status;
	unsigned int int_status_raw;

	if (host->power_mode == MMC_POWER_OFF)
		return IRQ_HANDLED;

	int_status = COMIP_MCI_READ_REG32(SDMMC_MINTSTS);
	if (int_status) {
		int_status_raw = COMIP_MCI_READ_REG32(SDMMC_RINTSTS);
		COMIP_MCI_WRITE_REG32((int_status_raw & ~host->iused) | int_status, SDMMC_RINTSTS);

		if ((int_status & (1 << SDMMC_CD_INTR)) || (int_status & (1 << SDMMC_HTO_INTR)))
			comip_mmc_cmd_done(host, int_status_raw);
		if ((int_status & (1 << SDMMC_DTO_INTR)) || (int_status & (1 << SDMMC_SBE_INTR))
				|| (int_status & (1 << SDMMC_DRTO_INTR)))
			comip_mmc_data_done(host, int_status_raw);
		if (int_status & (1 << SDMMC_SDIO_INTR))
			mmc_signal_sdio_irq(host->mmc);

		if (int_status & (1 << SDMMC_CDT_INTR))
			mmc_detect_change(host->mmc, host->pdata->detect_delay);
	}

	return IRQ_HANDLED;
}

static int comip_mmc_get_cd(struct mmc_host *mmc)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	int cd = 1;

	if (host->pdata && host->pdata->get_cd)
		cd = host->pdata->get_cd(mmc_dev(mmc));

	return cd;
}

static void comip_mmc_request(struct mmc_host *mmc,
				struct mmc_request *mrq)
{
	struct comip_mmc_host *host = mmc_priv(mmc);

	if(host->clk_low_power && !(comip_mmc_get_cd(host->mmc))) {
		MMC_PRINTK("mmc-%d: no sdcard. command %d.\n",
					host->id, mrq->cmd->opcode);
		mrq->cmd->error = -ETIMEDOUT;
		comip_mmc_finish_request(host, mrq);
		return;
	}

	if (host->mrq)
		MMC_PRINTK("mmc-%d: host request is not null.\n", host->id);

	/* Try to recover sd card. */
	comip_mmc_check_recovery(host);

	host->mrq = mrq;
	COMIP_MCI_WRITE_REG32(0x0000ffff, SDMMC_RINTSTS);
	if(host->id == 0)
		COMIP_MCI_WRITE_REG32(0xffffffff, SDMMC_RINTSTS);
	host->cmdat = host->cmdat_init;
	host->cmdat_init &= ~(1 << SDMMC_CMD_SEND_INITIALIZATION);

	if (mrq->data && !mrq->sbc) {
		host->cmdat |= (1 << SDMMC_CMD_DATA_TRANSFER_EXPECTED);
		if (mrq->data->flags & MMC_DATA_WRITE)
			host->cmdat |= (1 << SDMMC_CMD_READ_WRITE);
		else
			host->cmdat |= (1 << SDMMC_CMD_CHECK_RESPONSE_CRC);
		if (mrq->data->flags & MMC_DATA_STREAM)
			host->cmdat |= (1 << SDMMC_CMD_TRANSFER_MODE);
	}

	if (mrq->data)
		comip_mmc_setup_data(host, mrq->data);

	if (mrq->sbc)
		host->cmd = mrq->sbc;
	else
		host->cmd = mrq->cmd;

	comip_mmc_start_cmd(host);
}

static int comip_mmc_get_ro(struct mmc_host *mmc)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	if (host->pdata && host->pdata->get_ro)
		return host->pdata->get_ro(mmc_dev(mmc));
	/* Host doesn't support read only detection so assume writeable */
	return 0;
}

static void comip_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	unsigned int update_power_mode = 0;
	unsigned int tmpclock;

	if (host->power_mode != ios->power_mode) {
		host->power_mode = ios->power_mode;
		host->vdd = ios->vdd;
		update_power_mode = 1;
	}

	if (update_power_mode && (ios->power_mode == MMC_POWER_UP)) {
		if (host->pdata && host->pdata->setpower)
			host->pdata->setpower(mmc_dev(mmc), ios->vdd);
		clk_enable(host->clk);
		comip_mmc_init_hw(host);
		comip_mmc_dma_reset(host);
		host->cmdat_init |= (1 << SDMMC_CMD_SEND_INITIALIZATION);
	}

	if (ios->clock) {
		if (ios->clock == 400000)
			tmpclock = 400000;
		else if (ios->clock == host->clocklimit)
			tmpclock = host->clocklimit;
		else if (ios->clock == (host->clocklimit / 2))
			tmpclock = host->clocklimit / 2;
		else if (ios->clock == 25000000)
			tmpclock = host->clocklimit;
		else {
			if ((host->clock1 % (ios->clock * 2)))
				tmpclock = host->clocklimit;
			else
				tmpclock = ios->clock;
			MMC_PRINTK("mmc-%d: we don't support this clock rate %d, "
				"use the default clock rate %d!\n",
				host->id, ios->clock, tmpclock);
		}

		if (host->clock2 != tmpclock) {
			MMC_PRINTK("mmc-%d: set mmc clock rate to %d\n",
				host->id, tmpclock);

			/* Update mmc bus frequency. */
			host->clock2 = tmpclock;
			host->clockdiv = host->clock1 / tmpclock;
			comip_mmc_update_clock(host);
		}
	} else {
		if (host->clock2)
			host->clock2 = 0;
	}

	if (host->bus_width != ios->bus_width) {
		host->bus_width = ios->bus_width;
		if (ios->bus_width == MMC_BUS_WIDTH_8)
			COMIP_MCI_WRITE_REG32(0x10000, SDMMC_CTYPE);
		else if (ios->bus_width == MMC_BUS_WIDTH_4)
			COMIP_MCI_WRITE_REG32(1, SDMMC_CTYPE);
		else
			COMIP_MCI_WRITE_REG32(0, SDMMC_CTYPE);
	}

	if (update_power_mode && (ios->power_mode == MMC_POWER_OFF)) {
		comip_mmc_disable_irq(host, 0xffffffff);
		comip_mmc_disable_clock(host);
		clk_disable(host->clk);
		if (host->pdata && host->pdata->setpower)
			host->pdata->setpower(mmc_dev(mmc), ios->vdd);

		if (!(host->mmc->card) && !(host->pdata->get_cd) &&
				!(host->pdata->flags & MMCF_IGNORE_PM)) {
			unregister_pm_notifier(&(host->mmc->pm_notify));
			MMC_PRINTK("mmc-%d, removed its pm_notify!\n", host->id);
		}
	}
}

static void comip_mmc_enable_sdio_irq(struct mmc_host *mmc,
					int enable)
{
	struct comip_mmc_host *host = mmc_priv(mmc);

	if (enable)
		comip_mmc_enable_irq(host, (1 << SDMMC_SDIO_INTR));
	else
		comip_mmc_disable_irq(host, (1 << SDMMC_SDIO_INTR));
}

static int comip_mmc_start_signal_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	u32 reg32 = 0;
	u32 timeout = 20000;
	u32 mask = ((1<<SDMMC_CD_INTR)|(1<<SDMMC_HTO_INTR));

	if (host->id != 0) {
		return 0;
	}

	if ((!(host->pdata)) || (!(host->pdata->setpower))) {
		return 0;
	}

	switch (ios->signal_voltage) {

		case MMC_SIGNAL_VOLTAGE_330:
#if defined(CONFIG_MACH_LC186X)
			reg32 = readl(io_p2v(MUXPIN_PVDD8_VOL_CTRL));
			reg32 |= 0x02;
			writel(reg32, io_p2v(MUXPIN_PVDD8_VOL_CTRL));
#endif
			reg32 = COMIP_MCI_READ_REG32(SDMMC_UHSREG);
			reg32 &= ~(1 << SDMMC_UHSREG_VOLT_REG);
			COMIP_MCI_WRITE_REG32(reg32, SDMMC_UHSREG);
			wmb();
			host->pdata->setpower(mmc_dev(mmc), 2);
			break;
		case MMC_SIGNAL_VOLTAGE_180:

			comip_mmc_stop_clock_uhs(host);
#if defined(CONFIG_MACH_LC186X)
			reg32 = readl(io_p2v(MUXPIN_PVDD8_VOL_CTRL));
			reg32 &= ~0x03;
			writel(reg32, io_p2v(MUXPIN_PVDD8_VOL_CTRL));
#endif
			reg32 = COMIP_MCI_READ_REG32(SDMMC_UHSREG);
			reg32|= (1 << SDMMC_UHSREG_VOLT_REG);
			COMIP_MCI_WRITE_REG32(reg32, SDMMC_UHSREG);
			wmb();
			host->pdata->setpower(mmc_dev(mmc), 1);

			usleep_range(5000, 5500);
			comip_mmc_start_clock_uhs(host);
			MMC_PRINTK("mmc-%d: %s start!\n", host->id, __func__);
			usleep_range(1000, 1500);

			while (timeout) {
				udelay(1);
				timeout--;
				reg32 = COMIP_MCI_READ_REG32(SDMMC_RINTSTS);
				if ((reg32 & mask) == mask) {
					MMC_PRINTK("mmc-%d: %s reg32 = %x", host->id, __func__, reg32);
					COMIP_MCI_WRITE_REG32(reg32,SDMMC_RINTSTS);
					return 0;
				}
			}
			if(timeout <= 0) {
				MMC_PRINTK("mmc-%d: %s err!\n", host->id, __func__);
				return -1;
			}
			break;
		default:
			reg32 = COMIP_MCI_READ_REG32(SDMMC_UHSREG);
			reg32 &= ~(1 << SDMMC_UHSREG_VOLT_REG);
			COMIP_MCI_WRITE_REG32(reg32, SDMMC_UHSREG);

			MMC_PRINTK("mmc-%d: ios->signal_voltage = 0x%08x\n",
				host->id, ios->signal_voltage);
			break;
	}
	return 0;
}

#ifdef CONFIG_MMC_COMIP_TUNING
static int comip_mmc_read_fifo(struct comip_mmc_host* host,
				unsigned char *data, unsigned int len)
{
	int i;
	unsigned int retry = 0;
	unsigned int status;
	unsigned int empty;

	for(i = len / 4; i > 0; i--, data +=4) {
		do {
			status = COMIP_MCI_READ_REG32(SDMMC_STATUS);
			empty = (status & (1 << SDMMC_STATUS_FIFO_EMPTY)) >>SDMMC_STATUS_FIFO_EMPTY;
			if (empty)
				udelay(1);
		} while (empty && (retry++ < MMC_TRANSFER_TIMEOUT));

		if (retry >= MMC_TRANSFER_TIMEOUT) {
			MMC_PRINTK("mmc-%d: read fifo timeout\n", host->id);
			goto out;
		}

		*((u32 *)data) = COMIP_MCI_READ_REG32(SDMMC_FIFO);
	}

out:
	status = COMIP_MCI_READ_REG32(SDMMC_RINTSTS);
	COMIP_MCI_WRITE_REG32(status, SDMMC_RINTSTS);

	if (retry >= MMC_TRANSFER_TIMEOUT)
		return -ETIMEDOUT;

	if (status & ((1 << SDMMC_DRTO_INTR) | (1 << SDMMC_DCRC_INTR)
		|(1 << SDMMC_SBE_INTR) | (1 << SDMMC_HLE_INTR)))
		return status;
	else
		return 0;

}

static int comip_mmc_cmd(struct mmc_host *mmc, struct mmc_command *cmd, int wait)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	unsigned int flags = 0;
	unsigned int reg32;
	unsigned int status;
	int i;

	flags |= (1 << SDMMC_CMD_START_CMD);
	flags |= (1 << SDMMC_CMD_USE_HOLD_REG);
	flags |= host->cmdat;

	if(host->pdata->flags & MMCF_UHS_I)
		mdelay(5);

	if (wait)
		flags |= (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE);

	/* Enable cpu mode in MMC_CTL register. */
	reg32 = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	reg32 &= ~(1 << SDMMC_CTRL_DMA_ENABLE);
	reg32 &= ~(1 << SDMMC_CTRL_USE_INTERNAL_DMAC);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CTRL);

	/* Reset SDMMC FIFO.  */
	reg32 = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	reg32 |= (1 << SDMMC_CTRL_FIFO_RESET);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CTRL);

	while ((COMIP_MCI_READ_REG32(SDMMC_CTRL) & (1 << SDMMC_CTRL_FIFO_RESET))
			&& (i++ < MMC_RETRY_CNT));

	if (i >= MMC_RETRY_CNT)
		MMC_PRINTK("mmc-%d: DMA_RESET reset timeout:0x%x\n",
		host->id, COMIP_MCI_READ_REG32(SDMMC_CTRL));

	COMIP_MCI_WRITE_REG32(0xffffffff, SDMMC_RINTSTS);
	COMIP_MCI_WRITE_REG32(cmd->arg, SDMMC_CMDARG);

	COMIP_MCI_WRITE_REG32((cmd->opcode | flags), SDMMC_CMD);

	do {
		status = COMIP_MCI_READ_REG32(SDMMC_RINTSTS);
		udelay(1);
	} while ( ((status & ((1 << SDMMC_CRCERR_INTR) | (1 << SDMMC_RCRC_INTR)
				| (1 << SDMMC_RTO_INTR) | (1 << SDMMC_CD_INTR) )) == 0)
			 && (i++ < MMC_RETRY_CNT) );

	if((status & ((1 << SDMMC_CRCERR_INTR) | (1 << SDMMC_RCRC_INTR) | (1 << SDMMC_RTO_INTR) ))) {
		MMC_PRINTK("mmc-%d: error :0x%x\n", host->id, status);
		return -ETIMEDOUT;
	}

	if (i >= MMC_RETRY_CNT) {
		MMC_PRINTK("mmc-%d: time out %d, status_word :0x%x\n", host->id, i, status);
		return -ETIMEDOUT;
	}

	COMIP_MCI_WRITE_REG32(status, SDMMC_RINTSTS);

	return 0;
}

static int comip_mmc_ctrl_reset(struct mmc_host *mmc)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	unsigned int reg32;
	unsigned int retry = 0;

	/* Reset. */
	reg32 = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	reg32 |= (1 << SDMMC_CTRL_CONTROLLER_RESET);
	reg32 |= (1 << SDMMC_CTRL_FIFO_RESET);
	COMIP_MCI_WRITE_REG32(reg32, SDMMC_CTRL);

	while ((COMIP_MCI_READ_REG32(SDMMC_CTRL) &
		(1 << SDMMC_CTRL_CONTROLLER_RESET | SDMMC_CTRL_FIFO_RESET)) &&
		(retry++ < MMC_SEND_CMD_TIMEOUT))
		udelay(1);

	if (retry >= MMC_SEND_CMD_TIMEOUT) {
		MMC_PRINTK("mmc-%d: mmc send command timeout:0x%x\n",
			host->id, COMIP_MCI_READ_REG32(SDMMC_RINTSTS));
		return -ETIMEDOUT;
	}

	return 0;
}

static int comip_mmc_tuning_stop(struct mmc_host *mmc)
{
	int ret = -1;

	/* Controller hang up while bad phase. */
	ret = comip_mmc_ctrl_reset(mmc);
	return ret;
}

/*
 *	If EMMC support HS200 or advance, send cmd21 to card then
 *	receive 128Bytes special data, phase invalid if fail else phase optional.
 *	CMD18 multi block read to confirm phase. No error for valid phase
 */
static int comip_mmc_tuning_cmd21(struct mmc_host *mmc, u32 opcode)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	struct mmc_command cmd = {0};
	unsigned int data_len;
	unsigned char data[128];
	int ret = -1;

	if (mmc->ios.bus_width == MMC_BUS_WIDTH_8) {
		data_len = 128;
		COMIP_MCI_WRITE_REG32(data_len, SDMMC_BLKSIZ);
		COMIP_MCI_WRITE_REG32(data_len, SDMMC_BYTCNT);
	} else if (mmc->ios.bus_width == MMC_BUS_WIDTH_4) {
		data_len = 64;
		COMIP_MCI_WRITE_REG32(data_len, SDMMC_BLKSIZ);
		COMIP_MCI_WRITE_REG32(data_len, SDMMC_BYTCNT);
	}

	cmd.opcode = opcode;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1;
	cmd.retries = 0;
	cmd.data = NULL;
	cmd.error = 0;
	host->cmdat = 0;
	host->cmdat |= (1 << SDMMC_CMD_DATA_TRANSFER_EXPECTED);
	host->cmdat |= (1 << SDMMC_CMD_CHECK_RESPONSE_CRC);

	comip_mmc_cmd(mmc, &cmd, 0);

	ret = comip_mmc_read_fifo(host, data, data_len);
	if (ret) {
		MMC_PRINTK("mmc-%d:tuning CMD21 phase %d recv data fail 0x%x\n",
			host->id, host->pdata->clk_read_delay, ret);
		comip_mmc_tuning_stop(mmc);
		goto out;
	}

out:
	return ret;
}

static int comip_mmc_phase_select(int *phase, int num)
{
	int valid_delay = -1;
	int valid_begin = 0;
	int total_max = 0;

	int idx, st = 0;
	int total = 0;

	for (idx = 0; idx < num; idx++) {
		MMC_PRINTK("[%02d]---%d\n", idx, phase[idx]);
	}

	do {
		total = 0, idx = st;
		do {
			if(!phase[idx%num])
				break;
			else
				total++;
			idx++;
		}while( (idx%num) != st);

		MMC_PRINTK("loop end: st(%d) total(%d) \n", st, total);
		if (total > total_max) {
			total_max = total;
			valid_begin = st;
		}

		if (!phase[idx%num])
			st = (++idx) % num;
	}while(idx < num);

	if (total_max)
		valid_delay = (valid_begin + (total_max * 3) / 4 ) % num;

	return valid_delay;
}

static int comip_mmc_set_phase(struct mmc_host *mmc, unsigned read_delay,
						unsigned int write_delay, int flag)
{
	struct comip_mmc_host *host = mmc_priv(mmc);

	unsigned int cmdat = (1 << SDMMC_CMD_START_CMD) | (1 << SDMMC_CMD_USE_HOLD_REG)
				| (1 << SDMMC_CMD_UPDATE_CLOCK_REGISTERS_ONLY)
				| (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE);

	unsigned int reg32;
	unsigned int v;
	int ret = -1;

	struct clk *mmc_clk = host->clk;

	ret = comip_mmc_wait_for_bb(host);
	if (ret)
		goto out;

	v = COMIP_MCI_READ_REG32(SDMMC_CLKENA);
	v &= ~(1 << SDMMC_CLKENA_CCLK_EN);
	COMIP_MCI_WRITE_REG32(v, SDMMC_CLKENA);

	/* Update clk setting to controller. */
	ret = comip_mmc_send_cmd(host, cmdat);
	if (ret)
		goto out;

	/* Disable clock. */
	clk_disable(mmc_clk);

	/* Set clock delay. */
	reg32 = readl(mmc_clk->divclk_reg);
	if (flag == 1) {
		reg32 &= ~(0xf << CPR_SDMMCCLKCTL_SDMMC_CLK2_DELAY);
		reg32 |= (write_delay << CPR_SDMMCCLKCTL_SDMMC_CLK2_DELAY);
	}
	reg32 &= ~(0xf << CPR_SDMMCCLKCTL_SDMMC_CLK1_DELAY);
	reg32 |= (read_delay << CPR_SDMMCCLKCTL_SDMMC_CLK1_DELAY);
	writel(reg32, mmc_clk->divclk_reg);

	/* Enable clock. */
	clk_enable(mmc_clk);

	v = COMIP_MCI_READ_REG32(SDMMC_CLKENA);
	v |= (1 << SDMMC_CLKENA_CCLK_EN);
	COMIP_MCI_WRITE_REG32(v, SDMMC_CLKENA);

	COMIP_MCI_WRITE_REG32(host->clockdiv / 2, SDMMC_CLKDIV);

	/* Update clk setting to controller. */
	ret = comip_mmc_send_cmd(host, cmdat);
	COMIP_MCI_WRITE_REG32(0xffffffff, SDMMC_RINTSTS);

	host->pdata->clk_read_delay = read_delay;
	if (flag == 1)
		host->pdata->clk_write_delay = write_delay;

	return ret;
out:
	printk("set mmc phase read(%d)fail\n", read_delay);
	return ret;
}

static int comip_mmc_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	int result = -1;
	int tunning_times, i;
	int phase[MMC_CLKIN_SAMPLE_DELAY_MAX] = {0};
	int phase_num;
	struct clk *mmc_clk = host->clk;

	for(i = 0; i < sizeof(providers)/sizeof(providers[0]); i++) {
		if (g_mid == providers[i].mid) {
			MMC_PRINTK("found (%x, %s), use read_delay %d.\n",
					providers[i].mid, providers[i].brand, providers[i].rdelay);
			host->pdata->clk_read_delay = providers[i].rdelay;

			if(providers[i].extra_flags) {
				MMC_PRINTK("add extra_flags: 0x%x.\n", providers[i].extra_flags);
				host->pdata->flags |= providers[i].extra_flags;
			}

			goto skip_tuning;
		}
	}
	MMC_PRINTK("miss EMMC mid: 0x%x.\n", g_mid);

	phase_num = mmc_clk->parent->rate / (mmc_clk->grclk_val + 1) / host->clock2;
	tunning_times = phase_num;
	host->pdata->clk_read_delay = 0;

	do {

		result = comip_mmc_set_phase(mmc, host->pdata->clk_read_delay,
						host->pdata->clk_write_delay, 0);
		if (result) {
			MMC_PRINTK("mmc-%d: phase %d change err!\n",
				host->id, host->pdata->clk_read_delay);
			phase[host->pdata->clk_read_delay++] = 0;
			comip_mmc_tuning_stop(mmc);
			continue;
		}

		if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
			result = comip_mmc_tuning_cmd21(mmc, opcode);
			if (result) {
				printk("%s: %d failed result:%d +\n", __func__,
						host->pdata->clk_read_delay, result);
				phase[host->pdata->clk_read_delay] = 0;
			} else
				phase[host->pdata->clk_read_delay] = 1;
		}
		host->pdata->clk_read_delay++;
	} while(--tunning_times);
	host->pdata->clk_read_delay = comip_mmc_phase_select(phase, phase_num);

skip_tuning:
	if (host->pdata->clk_read_delay < 0) {
		/* Tuning fail, reduce clock. */
		host->pdata->clk_read_delay = 0;
		host->clockdiv = host->clockdiv << 1;
		result = comip_mmc_set_phase(mmc, host->pdata->clk_read_delay,
					host->pdata->clk_write_delay, 0);

		MMC_PRINTK("mmc-%d: tuning fail, reduce clock\n", host->id);
	} else
		/* Set phase & clock again after called comip_mmc_ctrl_reset. */
		result = comip_mmc_set_phase(mmc, host->pdata->clk_read_delay,
					host->pdata->clk_write_delay, 0);

	if (result) {
		MMC_PRINTK("mmc-%d: phase %d change err!\n",
			host->id, host->pdata->clk_read_delay);
		return -EIO;
	}

	MMC_PRINTK("mmc-%d: tuning result phase is %d\n",
		host->id, host->pdata->clk_read_delay);
	return result;
}

static int comip_mmc_tuning_cmd_uhs(struct mmc_host *mmc, u32 opcode)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	struct mmc_command cmd = {0};
	unsigned int data_len;
	unsigned char data[128];
	int ret;

	if (mmc->ios.bus_width == MMC_BUS_WIDTH_8) {
		data_len = 128;
		COMIP_MCI_WRITE_REG32(data_len, SDMMC_BLKSIZ);
		COMIP_MCI_WRITE_REG32(data_len, SDMMC_BYTCNT);
	} else if (mmc->ios.bus_width == MMC_BUS_WIDTH_4) {
		data_len = 64;
		COMIP_MCI_WRITE_REG32(data_len, SDMMC_BLKSIZ);
		COMIP_MCI_WRITE_REG32(data_len, SDMMC_BYTCNT);
	}

	cmd.opcode = opcode;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1 | MMC_RSP_CRC | MMC_CMD_ADTC;
	cmd.retries = 0;
	cmd.data = NULL;
	cmd.error = 0;
	host->cmdat = 0;
	host->cmdat |= (1 << SDMMC_CMD_DATA_TRANSFER_EXPECTED);
	host->cmdat |= (1 << SDMMC_CMD_CHECK_RESPONSE_CRC);

	ret = comip_mmc_cmd(mmc, &cmd, 0);
	if (ret)
		printk("cmd %d send fail\n", opcode);

	ret = comip_mmc_read_fifo(host, data, data_len);
	if (ret) {
		MMC_PRINTK("mmc-%d:tuning CMD%d phase %d recv data fail 0x%x\n",
			host->id, host->pdata->clk_read_delay, opcode, ret);
		comip_mmc_tuning_stop(mmc);
		goto out;
	}
out:
	return ret;
}

static int comip_execute_mmc_tuning_uhs(struct mmc_host *mmc, u32 opcode)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	struct mmc_ios *ios = &mmc->ios;
	int freq_table[] = {104000000};
	int ret = 0;
	int flag = 0;
	int org_clock1 = host->clock1;
	int org_clocklimit = host->clocklimit;
	int org_read_delay = host->pdata->clk_read_delay;
	int org_write_delay = host->pdata->clk_write_delay;
	int i;

	MMC_PRINTK("mmc-%d: comip_mmc_execute_tuning\n", host->id);
	for (i = 0; i < sizeof(freq_table)/sizeof(int);i++) {
		host->clock1 = freq_table[i];
		host->clocklimit = freq_table[i];
		//clk_disable(mmc_clk);
		//mdelay(5);
		clk_set_rate(host->clk, host->clock1);
		host->clock1 = clk_get_rate(host->clk);
		MMC_PRINTK("mmc-%d: host->clock1 = %d\n", host->id, host->clock1);
		//clk_enable(mmc_clk);

		comip_mmc_set_phase(mmc, 3, host->pdata->clk_write_delay, 0);
		ret = comip_mmc_tuning_cmd_uhs(mmc, opcode);
		if (ret) {
			MMC_PRINTK("mmc-%d: tuning processing %d delay read=3\n", host->id, ret);
			comip_mmc_tuning_stop(mmc);
		} else {
			MMC_PRINTK("mmc-%d: tuning result OK %d delay read=3\n", host->id, ret);
			flag = 1;
			break;
		}
	}

	if (flag == 0) {
		MMC_PRINTK("mmc-%d: proper delay has not been found by tuning process\n", host->id);
		host->clock1 = org_clock1;
		host->clocklimit = org_clocklimit;
		clk_set_rate(host->clk, host->clock1);
		host->clock1 = clk_get_rate(host->clk);
		MMC_PRINTK("mmc-%d: host->clock1 = %d\n",host->id, host->clock1);
		comip_mmc_set_phase(mmc, org_read_delay, org_write_delay, 0);
		comip_mmc_set_ios(mmc, ios);
		return ret;
	}

	// 2 is the best after testing many uhs-i cards in our system
	comip_mmc_set_phase(mmc, 3, 2, 1);
	comip_mmc_set_ios(mmc, ios);
	return 0;
}
static int comip_execute_mmc_tuning(struct mmc_host *mmc, u32 opcode)
{
	int ret = 0;

	switch(opcode) {
		case MMC_SEND_TUNING_BLOCK:
			ret = comip_execute_mmc_tuning_uhs(mmc, opcode);
			break;
		case MMC_SEND_TUNING_BLOCK_HS200:
			ret = comip_mmc_tuning(mmc, opcode);
			break;
		default:
			break;
	}
	return ret;
}

#else
static int comip_execute_mmc_tuning(struct mmc_host *mmc, u32 opcode)
{
	return 0;
}
#endif

static int comip_mmc_card_busy(struct mmc_host *mmc)
{
	struct comip_mmc_host *host = mmc_priv(mmc);
	u32 reg32 = 0;

	reg32 = (COMIP_MCI_READ_REG32(SDMMC_STATUS) & (1 << SDMMC_STATUS_DATA_BUSY));
	MMC_PRINTK("mmc-%d: comip_mmc_card_busy reg32 = %d (1=busy data low)(0=idle data high)\n",host->id, reg32);

	return !!reg32;
}

static const struct mmc_host_ops comip_mmc_ops = {
	.request			= comip_mmc_request,
	.get_ro 			= comip_mmc_get_ro,
	.set_ios			= comip_mmc_set_ios,
	.get_cd 			= comip_mmc_get_cd,
	.enable_sdio_irq	= comip_mmc_enable_sdio_irq,
	.start_signal_voltage_switch = comip_mmc_start_signal_voltage_switch,
	.execute_tuning		= comip_execute_mmc_tuning,
	.card_busy = comip_mmc_card_busy,
};

static ssize_t comip_mmc_debug_recovery(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct comip_mmc_host *host = mmc_priv(mmc);
	int ret = 0;

	if (!strncmp(buf, "recovery", strlen("recovery"))) {
		MMC_PRINTK("mmc-%d: comip_mmc_recovery(debug)\n", host->id);
		host->err_recovery = 1;
	} else if (!strncmp(buf, "suspend", strlen("suspend"))) {
		ret = mmc_suspend_host(host->mmc);
		MMC_PRINTK("mmc-%d: mmc_suspend_host %d(debug)\n", host->id, ret);
	} else if (!strncmp(buf, "resume", strlen("resume"))) {
		ret = mmc_resume_host(host->mmc);
		MMC_PRINTK("mmc-%d: mmc_resume_host %d(debug)\n", host->id, ret);
	}

	return count;
}

static struct device_attribute comip_mmc_debug_recovery_attr =
	__ATTR(debug_recovery, S_IWUSR, NULL, comip_mmc_debug_recovery);

static irqreturn_t comip_mmc_detect_irq(int irq, void *devid)
{
	struct comip_mmc_host *host = mmc_priv(devid);

	mmc_detect_change(devid, host->pdata->detect_delay);

	return IRQ_HANDLED;
}

static int host_active_notifier(struct notifier_block *blk,
			unsigned long code, void *_param)
{
	struct comip_mmc_host *host =
		container_of(blk, struct comip_mmc_host, audio_notify);

	switch (code){
		case AUDIO_START:
			host->pdata->flags |= MMCF_AUDIO_ON;
			break;
		case AUDIO_STOP:
			host->pdata->flags &= (~MMCF_AUDIO_ON);
			break;
		default:
			break;
	}

	return 0;
}

static int comip_mmc_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct comip_mmc_host *host = NULL;
	struct resource *r;
	int ret, irq;
	int i;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "Platform data not set.\n");
		return -EINVAL;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!r || irq < 0)
		return -ENXIO;

	r = request_mem_region(r->start, r->end - r->start + 1, DRIVER_NAME);
	if (!r)
		return -EBUSY;

	mmc = mmc_alloc_host(sizeof(struct comip_mmc_host), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto out_release_region;
	}
	mmc->ops = &comip_mmc_ops;
	/*
	 * We can do SG-DMA, but we don't because we never know how much
	 * data we successfully wrote to the card.
	 */
	host = mmc_priv(mmc);
	host->pdata = pdev->dev.platform_data;
	if(host->pdata->setflags)
		host->pdata->flags |= host->pdata->setflags();
	mmc->max_segs = 32;

        if (!(host->pdata->flags & MMCF_SET_SEG_BLK_SIZE)) {
            mmc->max_seg_size = 4096;
            mmc->max_blk_size = 4096;
        }

	mmc->max_blk_count = 65535;
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;
	host->mmc = mmc;
	host->clock1 = host->pdata->clk_rate_max;
	host->clock2 = 0;
	host->clocklimit = host->pdata->clk_rate;
	host->id = pdev->id;
	host->dma = -1;

	if (host->pdata->flags & MMCF_DETECT_WQ) {
		mmc->workqueue = alloc_ordered_workqueue("kmmcd%d detect", 0, host->id);
		if (!mmc->workqueue) {
			ret = -ENOMEM;
			goto out_wq;
		}
	} else
		mmc->workqueue = workqueue_detect;

	if (!host->pdata->clk_rate_max || !host->pdata->clk_rate) {
		dev_err(&pdev->dev, "Clock rate not set.\n");
		ret = -EINVAL;
		goto out_clk;
	}

	if (host->pdata->clk_rate_max % (host->pdata->clk_rate)) {
		dev_err(&pdev->dev, "Clock rate invalid.\n");
		ret = -EINVAL;
		goto out_clk;
	}

	host->clk_enabled = 0;
	host->clk = clk_get(&pdev->dev, "sdmmc_clk");
	if (IS_ERR(host->clk)) {
		ret = PTR_ERR(host->clk);
		host->clk = NULL;
		goto out_clk;
	}

	clk_set_rate(host->clk, host->clock1);
	host->clock1 = clk_get_rate(host->clk);

	/*
	 * Calculate minimum clock rate, rounding up.
	 */
	mmc->f_min = 400000;
	mmc->f_max = host->clocklimit;
	mmc->ocr_avail = host->pdata ?
		host->pdata->ocr_mask : MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	mmc->caps = 0;
	mmc->caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_SDIO_IRQ;

	if (host->pdata->flags & MMCF_CAP2_HS200)
		mmc->caps2 |= MMC_CAP2_HS200;

	if (host->pdata->flags & MMCF_CAP2_PACKED_CMD)
		mmc->caps2 |= MMC_CAP2_PACKED_CMD;

	if (host->pdata->flags & MMCF_USE_POLL)
		mmc->caps |= MMC_CAP_NEEDS_POLL;

	if (host->pdata->flags & MMCF_8BIT_DATA)
		mmc->caps |= MMC_CAP_8_BIT_DATA;

	if (host->pdata->flags & MMCF_USE_CMD23)
		mmc->caps |= MMC_CAP_CMD23;

	mmc->caps |= MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED | MMC_CAP_ERASE;
	mmc->pm_flags = 0;
	mmc->pm_caps = 0;
	host->cmdat_init = (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE);
	host->power_mode = MMC_POWER_OFF;
	host->vdd = 0;

	if (host->pdata->flags & MMCF_UHS_I) {
		mmc->caps |= MMC_CAP_UHS_SDR12| MMC_CAP_UHS_SDR25 | MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104 | MMC_CAP_DRIVER_TYPE_A;//| MMC_CAP_UHS_DDR50;
	}

	if (host->pdata->flags & MMCF_IGNORE_PM) {
		mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
		mmc->pm_caps |= MMC_PM_IGNORE_PM_NOTIFY;
	}

	if (host->pdata->flags & MMCF_KEEP_POWER) {
		mmc->pm_caps |= MMC_PM_KEEP_POWER;
		mmc->pm_flags |= MMC_PM_KEEP_POWER;
	}

	if (host->pdata->flags & MMCF_DMA_COHERENT)
		host->sg_cpu = kmalloc(PAGE_SIZE, GFP_KERNEL);	
	else
		host->sg_cpu = dma_alloc_coherent(&pdev->dev, PAGE_SIZE, &host->sg_dma, GFP_KERNEL);

	if (!host->sg_cpu) {
		ret = -ENOMEM;
		goto out_dma;
	}

	if (host->pdata->flags & MMCF_DMA_COHERENT)
		host->sg_dma = virt_to_phys(host->sg_cpu);

	/* Initialize the dmac_linktbl_item_t nod, max number 120. */
	for (i = 0; i < PAGE_SIZE / sizeof(struct mmc_dma_des); i++)
		host->sg_cpu[i].DES3 = (u32)(host->sg_dma + (i + 1) * sizeof(struct mmc_dma_des));

	spin_lock_init(&host->lock);

	if (host->pdata->flags & MMCF_CMD_WORKQUEUE)
		host->cmd_use_workqueue = 1;

	host->cmd_workqueue = alloc_ordered_workqueue("kmmcd%d cmd", 0, host->id);
	if (!host->cmd_workqueue) {
		ret = -ENOMEM;
		goto out_cmd;
	}
	INIT_WORK(&host->cmd_work, comip_mmc_cmd_wq);
	tasklet_init(&host->cmd_tasklet, comip_mmc_cmd_tasklet, (unsigned long)host);

	host->res = r;
	host->irq = irq;
	host->ienable = 0x0;
	host->iused = (1 << SDMMC_CD_INTR) | (1 << SDMMC_DTO_INTR) |
		(1 << SDMMC_SDIO_INTR) | (1 << SDMMC_HTO_INTR) | (1 << SDMMC_RTO_INTR);

	if (0 == host->id) // broken SD report start bit err int.
		host->iused |= (1 << SDMMC_SBE_INTR) | (1 << SDMMC_DRTO_INTR);

	if (1 == host->id) // use DRTO for mmc, too.
		host->iused |= (1 << SDMMC_DRTO_INTR);

	host->base = ioremap(r->start, r->end - r->start + 1);
	if (!host->base) {
		ret = -ENOMEM;
		goto out;
	}

	ret = request_irq(host->irq, comip_mmc_irq, IRQF_SHARED, DRIVER_NAME, host);
	if (ret)
		goto out;

	if (host->pdata->flags & MMCF_MONITOR_TIMER) {
		init_timer(&host->timer);
		host->timer.data = (unsigned long)host;
		host->timer.function = comip_mmc_timer_func;
	}

	if (host->pdata->flags & MMCF_ERROR_RECOVERY)
		device_create_file(&pdev->dev, &comip_mmc_debug_recovery_attr);

	platform_set_drvdata(pdev, mmc);

	if (host->pdata && host->pdata->init)
		host->pdata->init(&pdev->dev, comip_mmc_detect_irq, mmc);

	if (host->pdata->flags & MMCF_SUSPEND_HOST) {
		(host->audio_notify).notifier_call = host_active_notifier;
		register_audio_state_notifier(&(host->audio_notify));
	}

	host->flag_cmdseq = 0;

#ifdef CONFIG_ARCH_PHYS_ADDR_T_64BIT
	if (mmc->max_segs > DMA_BACKUP_BUFFER_NUM_MAX) {
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < mmc->max_segs; i++) {
		host->dma_backup[i] = __get_free_pages(GFP_KERNEL,
				__get_order(mmc->max_seg_size * 2));
		if (!host->dma_backup[i]) {
			ret = -ENOMEM;
			goto out;
		}
	}
#endif
	mmc_add_host(mmc);

	return 0;

out:
	destroy_workqueue(host->cmd_workqueue);
	tasklet_kill(&host->cmd_tasklet);
out_cmd:
	if (host->sg_cpu) {
		if (host->pdata->flags & MMCF_DMA_COHERENT) {
			kfree(host->sg_cpu);
			host->sg_dma = 0;
		} else
			dma_free_coherent(&pdev->dev, PAGE_SIZE, host->sg_cpu, host->sg_dma);
	}
out_dma:
	clk_disable(host->clk);
	clk_put(host->clk);
out_clk:
	if (host->base)
		iounmap(host->base);

	if (host->pdata->flags & MMCF_DETECT_WQ)
		destroy_workqueue(mmc->workqueue);
out_wq:
	if (mmc)
		mmc_free_host(mmc);
out_release_region:
	release_mem_region(r->start, r->end - r->start + 1);

	return ret;
}

static int comip_mmc_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct comip_mmc_host *host;
#ifdef CONFIG_ARCH_PHYS_ADDR_T_64BIT
	unsigned int i;
#endif

	platform_set_drvdata(pdev, NULL);

	if (mmc) {
		host = mmc_priv(mmc);
		if (host->pdata && host->pdata->exit)
			host->pdata->exit(&pdev->dev, mmc);

		if (host->pdata->flags & MMCF_ERROR_RECOVERY)
			device_remove_file(&pdev->dev, &comip_mmc_debug_recovery_attr);

		destroy_workqueue(host->cmd_workqueue);
		tasklet_kill(&host->cmd_tasklet);

		mmc_remove_host(mmc);

		free_irq(host->irq, host);
		if (host->pdata->flags & MMCF_DMA_COHERENT) {
			kfree(host->sg_cpu);
			host->sg_dma = 0;
		} else
			dma_free_coherent(&pdev->dev, PAGE_SIZE, host->sg_cpu, host->sg_dma);

		if (host->base)
			iounmap(host->base);

		clk_put(host->clk);

		release_mem_region(host->res->start, host->res->end - host->res->start + 1);

		if (host->pdata->flags & MMCF_DETECT_WQ)
			destroy_workqueue(mmc->workqueue);

#ifdef CONFIG_ARCH_PHYS_ADDR_T_64BIT
		for (i = 0; i < mmc->max_segs; i++)
			free_page(host->dma_backup[i]);
#endif
		mmc_free_host(mmc);
	}

	return 0;
}

#ifdef CONFIG_PM
static int comip_mmc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct comip_mmc_host *host = mmc_priv(mmc);
	int ret = 0;

	if ((mmc && (host->clk_low_power) && (host->power_mode != MMC_POWER_OFF))
			|| (mmc && (host->pdata->flags & MMCF_SUSPEND_SDIO))) {
		if (mmc->caps & MMC_CAP_NEEDS_POLL)
			mmc->rescan_disable = 1;

		if (host->cmd_use_workqueue)
			flush_workqueue(host->cmd_workqueue);

		if (host->pdata->flags & MMCF_KEEP_POWER)
			mmc->pm_flags |= MMC_PM_KEEP_POWER;

		if ((host->pdata->flags & MMCF_SUSPEND_HOST) &&
				!(host->pdata->flags & MMCF_AUDIO_ON))
			mmc_suspend_host(mmc);
		else {
			if ((host->pdata->flags & MMCF_SUSPEND_DEVICE)
					&& mmc_card_can_sleep(mmc)) {
				mmc_claim_host(mmc);
				mmc_card_sleep(mmc);
				mmc_release_host(mmc);
			}

			clk_disable(host->clk);
		}
	}

	return ret;
}

static int comip_mmc_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct comip_mmc_host *host = mmc_priv(mmc);
	int ret = 0;

	if ((mmc && (host->clk_low_power)) || (mmc && (host->pdata->flags & MMCF_SUSPEND_SDIO))) {
		if ((host->pdata->flags & MMCF_SUSPEND_HOST) &&
				!(host->pdata->flags & MMCF_AUDIO_ON)) {
			ret = mmc_resume_host(mmc);
		} else {
			if (host->power_mode != MMC_POWER_OFF)
				clk_enable(host->clk);

			if ((host->pdata->flags & MMCF_SUSPEND_DEVICE)
					&& mmc_card_can_sleep(mmc)) {
				mmc_claim_host(mmc);
				mmc_card_awake(mmc);
				mmc_release_host(mmc);
			}
		}

		if (mmc->caps & MMC_CAP_NEEDS_POLL)
			mmc->rescan_disable = 0;
	}

	return ret;
}

static struct dev_pm_ops comip_mmc_pm_ops = {
	.suspend = comip_mmc_suspend,
	.resume = comip_mmc_resume,
};
#endif

static struct platform_driver comip_mmc_driver = {
	.probe		= comip_mmc_probe,
	.remove 	= comip_mmc_remove,
	.driver 	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm 	= &comip_mmc_pm_ops,
#endif
	},
};

static int __init comip_mmc_init(void)
{
	workqueue_detect = alloc_ordered_workqueue("kmmcd detect", 0);
	if (!workqueue_detect)
		return -ENOMEM;

	return platform_driver_register(&comip_mmc_driver);
}

static void __exit comip_mmc_exit(void)
{
	platform_driver_unregister(&comip_mmc_driver);
	destroy_workqueue(workqueue_detect);
}

module_init(comip_mmc_init);
module_exit(comip_mmc_exit);

MODULE_DESCRIPTION("COMIP Multimedia Card Interface Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:comip_mmc");
