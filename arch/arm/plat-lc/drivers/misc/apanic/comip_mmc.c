/*
 * (C) Copyright Leadcore
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/io.h>

#include <plat/hardware.h>

#include "mmc.h"
#include "comip_mmc.h"


//#define debug(fmt, args...) printk(KERN_DEBUG fmt, ##args)
#define debug(fmt, args...)

/* support 4 mmc hosts */
struct mmc mmc_dev[4];
struct mmc_host mmc_host[4];

struct comip_mmc_context {
	struct mmc_host host_mmc;
	struct mmc mmc_dev;
	int index;
	int clk_read_delay;
	int clk_write_delay;
	int div;
};

static struct comip_mmc_context comip_mmc_context[4];

#define COMIP_MCI_WRITE_REG32(value, offset)		\
	writel(value, io_p2v(host->base + offset))

#define COMIP_MCI_READ_REG32(offset)				\
	readl(io_p2v(host->base + offset))

#define MMC_TRANSFER_TIMEOUT					(2000000) /* us. */
#define MMC_RETRY_CNT							(60000)
#define MMC_SEND_CMD_TIMEOUT					(5000000) /* us. */
#define MMC_WAIT_IDLE_TIMEOUT					(5000000) /* us. */

/**
 * Get the host address and peripheral ID for a device. Devices are numbered
 * from 0 to 3.
 *
 * @param host		Structure to fill in (base, reg, mmc_id)
 * @param dev_index	Device index (0-3)
 */
static void comip_get_setup(struct mmc_host *host, int dev_index)
{
	debug("comip_get_base_mmc: dev_index = %d\n", dev_index);

	switch (dev_index) {
	case 1:
		host->base = SDMMC1_BASE;
		break;
	case 2:
		host->base = SDMMC2_BASE;
		break;
#if defined(CONFIG_CPU_LC1810)
	case 3:
		host->base = SDMMC3_BASE;
		break;
#endif
	case 0:
	default:
		host->base = SDMMC0_BASE;
		break;
	}
}

static int comip_mmc_wait_for_bb(struct mmc_host *host)
{
	unsigned int retry = 0;

	while ((COMIP_MCI_READ_REG32(SDMMC_STATUS) & (1 << SDMMC_STATUS_DATA_BUSY))
			&& (retry++ < MMC_WAIT_IDLE_TIMEOUT))
		udelay(1);

	if (retry >= MMC_WAIT_IDLE_TIMEOUT) {
		printk("wait mmc idle timeout:0x%x\n", COMIP_MCI_READ_REG32(SDMMC_STATUS));
		return TIMEOUT;
	}

	return 0;
}

static int __comip_mmc_send_cmd(struct mmc_host* host, unsigned int cmdat)
{
	unsigned int retry = 0;

	COMIP_MCI_WRITE_REG32(cmdat, SDMMC_CMD);

	while ((COMIP_MCI_READ_REG32(SDMMC_CMD) & (1 << SDMMC_CMD_START_CMD))
			&& (retry++ < MMC_SEND_CMD_TIMEOUT))
		udelay(1);

	if (retry >= MMC_SEND_CMD_TIMEOUT) {
		printk("mmc send command timeout:0x%x\n", COMIP_MCI_READ_REG32(SDMMC_CMD));
		return TIMEOUT;
	}

	return 0;
}

static void comip_mmc_set_clock(struct mmc_host* host, u32 clock_rate)
{
	struct comip_mmc_context *comip_host = container_of(host, struct comip_mmc_context, host_mmc);
	unsigned int cmdat = (1 << SDMMC_CMD_START_CMD)
				| (1 << SDMMC_CMD_UPDATE_CLOCK_REGISTERS_ONLY);
	unsigned int v;
	int div;
	int ret;

	if (clock_rate == 0)
		div = 0;
	else if (clock_rate == 400000)
		div = 130;
	else {
		if (comip_host->index == 1)
			div = 1;
		else
			div = 2;
	}

	if (div == comip_host->div)
		return;

	ret = comip_mmc_wait_for_bb(host);
	if (ret)
		goto out;

	v = COMIP_MCI_READ_REG32(SDMMC_CLKENA);
	v &= ~(1 << SDMMC_CLKENA_CCLK_EN);
	COMIP_MCI_WRITE_REG32(v, SDMMC_CLKENA);

	if (div) {
		/* Update clk setting to controller. */
		ret = __comip_mmc_send_cmd(host, cmdat);
		if (ret)
			goto out;

		COMIP_MCI_WRITE_REG32(div, SDMMC_CLKDIV);

		/* Update clk setting to controller. */
		ret = __comip_mmc_send_cmd(host, cmdat);
		if (ret)
			goto out;

		v |= (1 << SDMMC_CLKENA_CCLK_EN);
		COMIP_MCI_WRITE_REG32(v, SDMMC_CLKENA);
	}

	/* Update clk setting to controller. */
	ret = __comip_mmc_send_cmd(host, cmdat);
	if (ret)
		goto out;

	comip_host->div = div;

out:
	if (ret) {
		printk("set mmc clock(%d) fail\n", clock_rate);
	}
}

static void comip_reset_fifo(struct mmc_host *host)
{
	unsigned int v;
	unsigned int i = 0;

	/* Reset SDMMC FIFO.  */
	v = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	v |= (1 << SDMMC_CTRL_FIFO_RESET);
	COMIP_MCI_WRITE_REG32(v, SDMMC_CTRL);

	while ((COMIP_MCI_READ_REG32(SDMMC_CTRL) & (1 << SDMMC_CTRL_FIFO_RESET))
			&& (i++ < MMC_RETRY_CNT));

	if (i >= MMC_RETRY_CNT)
		printk("DMA_RESET reset timeout:0x%x\n" , COMIP_MCI_READ_REG32(SDMMC_CTRL));
}

static void comip_mmc_prepare_data(struct mmc_host *host, struct mmc_data *data)
{
	unsigned int v;

	/* Enable cpu mode in MMC_CTL register. */
	v = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	v &= ~(1 << SDMMC_CTRL_DMA_ENABLE);
	COMIP_MCI_WRITE_REG32(v, SDMMC_CTRL);
	comip_reset_fifo(host);

	COMIP_MCI_WRITE_REG32(data->blocksize, SDMMC_BLKSIZ);
	COMIP_MCI_WRITE_REG32(data->blocks * data->blocksize, SDMMC_BYTCNT);
}

static int comip_mmc_transfer(struct mmc_host* host, struct mmc_data *data)
{
	int result = 0;
	u32 i = 0;
	unsigned long retry = 0;
	volatile unsigned int mmci_status;
	const char *data_buff_src = data->src;
	char *data_buff_dest = data->dest;
	unsigned int data_len = data->blocks * data->blocksize;

	if (MMC_DATA_WRITE == data->flags ) {
		while (data_len) {
			u32 fifo_data;

			i = 0;
			while (0 == (COMIP_MCI_READ_REG32(SDMMC_STATUS) & (1 << 1)) && (i++ < MMC_RETRY_CNT)) {
				udelay(1);
			}
			if (i >= MMC_RETRY_CNT) {
				result = TIMEOUT;
				goto out;
			}

			fifo_data = 0;
			if(data_len){
				fifo_data |= ((u32)data_buff_src[0]) << 0;
				data_buff_src++;
				data_len--;
			}
			if(data_len){
				fifo_data |= ((u32)data_buff_src[0]) << 8;
				data_buff_src++;
				data_len--;
			}
			if(data_len){
				fifo_data |= ((u32)data_buff_src[0]) << 16;
				data_buff_src++;
				data_len--;
			}
			if(data_len){
				fifo_data |= ((u32)data_buff_src[0]) << 24;
				data_buff_src++;
				data_len--;
			}

			COMIP_MCI_WRITE_REG32(fifo_data, SDMMC_FIFO);
		}
	} else {
		if (data_len) {
			u32 remain = data_len & 0x03;
			u32 fifo_data;
			if ((u32)data_buff_dest & 0x03) {
				debug("transferR:data_len:0x%x,data_buff:0x%x\n", data_len, (u32)data_buff_dest);
				goto copy_one_by_one;
			}

			while(data_len >= 4) {
				i = 0;
				while (0==(COMIP_MCI_READ_REG32(SDMMC_STATUS) & (1 << 0)) && (i++ < MMC_RETRY_CNT)) {
					udelay(1);
				}
				if (i >= MMC_RETRY_CNT) {
					result = TIMEOUT;
					goto out;
				}

				fifo_data = COMIP_MCI_READ_REG32(SDMMC_FIFO);
				*((u32 *)data_buff_dest) = fifo_data;
				data_buff_dest+=4;
				data_len -= 4;
			}
			if (remain) {
				i = 0;
				while (0==(COMIP_MCI_READ_REG32(SDMMC_STATUS) & (1 << 0)) && (i++ < MMC_RETRY_CNT)) {
					udelay(1);
				}
				if (i >= MMC_RETRY_CNT) {
					result = TIMEOUT;
					goto out;
				}

				fifo_data = COMIP_MCI_READ_REG32(SDMMC_FIFO);

				if(remain == 3) {
					*((u16 *)data_buff_dest) = (fifo_data & 0xffff);
					data_buff_dest += 2;
					*(u8 *)data_buff_dest = ( fifo_data >> 16) & 0xff;
				} else if (remain == 2)	{
					*((u16 *)data_buff_dest) = (fifo_data & 0xffff);
					data_buff_dest += 2;
				} else {
					*(u8 *)data_buff_dest = (fifo_data & 0xff);
				}
			}
			goto check_status;
		}

copy_one_by_one:
		while (data_len) {
			u32 fifo_data;

			i = 0;
			while (0 == (COMIP_MCI_READ_REG32(SDMMC_STATUS) & (1<<0)) && (i++ < MMC_RETRY_CNT)) {
				udelay(1);
			}
			if (i >= MMC_RETRY_CNT) {
				result = TIMEOUT;
				goto out;
			}

			fifo_data = COMIP_MCI_READ_REG32(SDMMC_FIFO);
			if(data_len){
				*data_buff_dest = (fifo_data >> 0) & 0xff;
				data_buff_dest++;
				data_len--;
			}
			if(data_len){
				*data_buff_dest = (fifo_data >> 8) & 0xff;
				data_buff_dest++;
				data_len--;
			}
			if(data_len){
				*data_buff_dest = (fifo_data >> 16) & 0xff;
				data_buff_dest++;
				data_len--;
			}
			if(data_len){
				*data_buff_dest = (fifo_data >> 24) & 0xff;
				data_buff_dest++;
				data_len--;
			}
		}
	}

check_status:

	mmci_status = COMIP_MCI_READ_REG32(SDMMC_RINTSTS);

	/* wait until data has been written */
	do {
		mmci_status = COMIP_MCI_READ_REG32(SDMMC_RINTSTS);
	} while ( ( (mmci_status & MMC_TRANSFER_MASK) == 0) &&
		  (retry++ < MMC_TRANSFER_TIMEOUT) );


	if(MMC_DATA_WRITE == data->flags)
		mmci_status &= ~(1<<SDMMC_DCRC_INTR);
	if (mmci_status & (MMC_TRANSFER_ERROR_MASK) ) {
		result = COMIP_MCI_READ_REG32(SDMMC_RINTSTS);
	}

	if (retry >= MMC_TRANSFER_TIMEOUT)
		result = TIMEOUT;

	COMIP_MCI_WRITE_REG32(0xffffffff, SDMMC_RINTSTS);
out:
	return result;
}


static int comip_mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd,
						struct mmc_data *data)
{
	struct mmc_host *host = (struct mmc_host *)mmc->priv;
	unsigned int flags = 0;
	unsigned int i = 0;
	unsigned int status_word;
	int result = 0;

	flags |= (1 << SDMMC_CMD_START_CMD);
	flags |= (1 << SDMMC_CMD_USE_HOLD_REG);
	flags |= (1 << SDMMC_CMD_WAIT_PRVDATA_COMPLETE);

	if (data) {
		comip_mmc_prepare_data(host, data);
	}

	COMIP_MCI_WRITE_REG32(0xffffffff, SDMMC_RINTSTS);
	COMIP_MCI_WRITE_REG32(cmd->cmdarg, SDMMC_CMDARG);

	if (cmd->resp_type & MMC_RSP_PRESENT)
		flags |= (1 << SDMMC_CMD_RESPONSE_EXPECT);
	if (cmd->resp_type & MMC_RSP_136)
		flags |= (1 << SDMMC_CMD_RESPONSE_LENGTH);

	if (data) {
		flags |= (1 << SDMMC_CMD_DATA_TRANSFER_EXPECTED);

		if (! ( (cmd->cmdidx == MMC_CMD_WRITE_MULTIPLE_BLOCK)
			|| (cmd->cmdidx == MMC_CMD_WRITE_SINGLE_BLOCK)) ) {
			flags |= (1 << SDMMC_CMD_CHECK_RESPONSE_CRC);
		}

		if (MMC_DATA_WRITE == data->flags)
			flags |= (1 << SDMMC_CMD_READ_WRITE);
	}

	COMIP_MCI_WRITE_REG32((cmd->cmdidx | flags), SDMMC_CMD);

	do {
		status_word = COMIP_MCI_READ_REG32(SDMMC_RINTSTS);
	} while ( ((status_word & ((1 << SDMMC_CRCERR_INTR) | (1 << SDMMC_RCRC_INTR)
				| (1 << SDMMC_RTO_INTR) | (1 << SDMMC_CD_INTR) )) == 0)
			 && (i++ < MMC_RETRY_CNT) );

	if((status_word & ((1 << SDMMC_CRCERR_INTR) | (1 << SDMMC_RCRC_INTR) | (1 << SDMMC_RTO_INTR) ))) {
		debug("error :0x%x\n", status_word);
		return TIMEOUT;
	}

	if (i >= MMC_RETRY_CNT) {
		debug("time out %d, status_word :0x%x\n", i, status_word);
		return TIMEOUT;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			cmd->response[0] = COMIP_MCI_READ_REG32(SDMMC_RESP3);
			cmd->response[1] = COMIP_MCI_READ_REG32(SDMMC_RESP2);
			cmd->response[2] = COMIP_MCI_READ_REG32(SDMMC_RESP1);
			cmd->response[3] = COMIP_MCI_READ_REG32(SDMMC_RESP0);
		} else {
			cmd->response[0] = COMIP_MCI_READ_REG32(SDMMC_RESP0);
		}
	}

	COMIP_MCI_WRITE_REG32((1 << SDMMC_CD_INTR), SDMMC_RINTSTS);

	if (data) {
		result = comip_mmc_transfer(host, data);
	}

	COMIP_MCI_WRITE_REG32(0xffffffff, SDMMC_RINTSTS);

	return result;
}

static void comip_mmc_set_ios(struct mmc *mmc)
{
	struct mmc_host *host = mmc->priv;

	if (8 == mmc->bus_width)
		COMIP_MCI_WRITE_REG32(0x10000, SDMMC_CTYPE);
	else if (4 == mmc->bus_width)
		COMIP_MCI_WRITE_REG32(1, SDMMC_CTYPE);
	else if (1 == mmc->bus_width)
		COMIP_MCI_WRITE_REG32(0, SDMMC_CTYPE);

	comip_mmc_set_clock(host, mmc->clock);
}

static int comip_mmc_core_init(struct mmc *mmc)
{
	struct mmc_host *host = (struct mmc_host *)mmc->priv;
	struct comip_mmc_context *comip_host = container_of(host, struct comip_mmc_context, host_mmc);
	unsigned int status;
	unsigned int i;

#if defined(CONFIG_CPU_LC1810) || defined(CONFIG_CPU_LC1813)
	/* Enable clock. */
	status = ((1 << (21 + comip_host->index)) | (1 << (5 + comip_host->index)));
	writel(status, io_p2v(AP_PWR_SECPCLK_EN));

	/* Set clock rate. */
	/* Clock rate = (pll1_out / (2 * (div + 1))) * (gr / 8) =  (1248M / 6) * (1 / 2) = 104M. */
	status = 2 << 8 | (comip_host->clk_read_delay << 4) | (comip_host->clk_write_delay << 0);
	writel(status, io_p2v(AP_PWR_SDMMC0CLKCTL + comip_host->index * 4));
	status = (1 << (16 + comip_host->index)) | (4 << (4 * comip_host->index));
	writel(status, io_p2v(AP_PWR_SDMMCCLKGR_CTL));

	/* Reset SDMMC. */
	status = (1 << (6 + comip_host->index));
	writel(status, io_p2v(AP_PWR_MOD_RSTCTL4));
	mdelay(5);

#elif defined(CONFIG_CPU_LC1860)
	/* Enable clock. */
	status = ((1 << (12 + comip_host->index)) | (1 << (28 + comip_host->index)));
	writel(status, io_p2v(AP_PWR_SDMMCCLK_CTL0));

	/* Set clock rate. */
	/* Clock rate = parent_rate / (div0 + 1) / (2 * (div1 + 1)) =  1248M  / (1 + 1) / (2 * (2 + 1))= 104M. */
	status = 2 << 8 | (comip_host->clk_read_delay << 4) | (comip_host->clk_write_delay << 0);
	writel(status, io_p2v(AP_PWR_SDMMC0CLK_CTL1 + comip_host->index * 4));
	status = (7 << (16 + comip_host->index * 4)) | (1 << (4 * comip_host->index));
	writel(status, io_p2v(AP_PWR_SDMMCCLK_CTL0));

	/* Reset SDMMC. */
	if (comip_host->index == 1)
		writel(0x08, io_p2v(AP_PWR_HBLK0_RSTCTL));
	else if (comip_host->index == 0) {
		status = 1 << 5;
		writel(status, io_p2v(AP_PWR_MOD_RSTCTL0));
	} else if (comip_host->index == 2) {
		status = 1 << 6;
		writel(status, io_p2v(AP_PWR_MOD_RSTCTL0));
	}
	mdelay(5);

#else
	#error nothing to mach
#endif

	/* Reset. */
	status = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	status |= (1 << SDMMC_CTRL_CONTROLLER_RESET);
	status |= (1 << SDMMC_CTRL_FIFO_RESET);
	status |= (1 << SDMMC_CTRL_DMA_RESET);
	status |= (1 << SDMMC_CTRL_INT_ENABLE);
	COMIP_MCI_WRITE_REG32(status, SDMMC_CTRL);

	/* Wait controller reset done. */
	i = 0;
	status = COMIP_MCI_READ_REG32(SDMMC_CTRL);
	while((status & (1 << SDMMC_CTRL_CONTROLLER_RESET))
		&& (i++ < MMC_RETRY_CNT));
	if (i >= MMC_RETRY_CNT)
		printk("CONTROLLER_RESET reset timeout:0x%x\n", status);

	//Wait no dma_req
	i = 0;
	status = COMIP_MCI_READ_REG32(SDMMC_STATUS);
	while((status & (1 << SDMMC_STATUS_DMA_REQ))
		&& (i++ < MMC_RETRY_CNT));
	if ( i >= MMC_RETRY_CNT)
		printk("CONTROLLER_RESET reset timeout:0x%x\n" ,COMIP_MCI_READ_REG32(SDMMC_CTRL));

	/* Clear all interrupt. */
	COMIP_MCI_WRITE_REG32(0xFFFFFFFF, SDMMC_RINTSTS);

	/* Set debounce time 25ms. */
	COMIP_MCI_WRITE_REG32(0x94C5F, SDMMC_DEBNCE);
	COMIP_MCI_WRITE_REG32(0xFFFFFFFF, SDMMC_TMOUT);

	/* Set fifo threshold. */
	COMIP_MCI_WRITE_REG32(0x00000001, SDMMC_FIFOTH);


	/* 1 bit card. */
	COMIP_MCI_WRITE_REG32(0, SDMMC_CTYPE);
	COMIP_MCI_WRITE_REG32(0, SDMMC_BYTCNT);

	/* 400K. */
	comip_mmc_set_clock(host, 400000);

	status = COMIP_MCI_READ_REG32(SDMMC_CMD);
	status |= 1 << SDMMC_CMD_SEND_INITIALIZATION;
	COMIP_MCI_WRITE_REG32(status, SDMMC_CMD);

	udelay(1);

	status = COMIP_MCI_READ_REG32(SDMMC_CMD);
	status &= ~(1 << SDMMC_CMD_SEND_INITIALIZATION);
	COMIP_MCI_WRITE_REG32(status, SDMMC_CMD);

	return 0;
}

int comip_mmc_init(int dev_index, int bus_width, int clk_write_delay, int clk_read_delay)
{
	struct comip_mmc_context *comip_host;
	struct mmc_host *host;
	struct mmc *mmc;

	memset(&comip_mmc_context, 0, sizeof(comip_mmc_context));
	comip_host = &comip_mmc_context[dev_index];
	comip_host->index = dev_index;
	comip_host->clk_read_delay = clk_read_delay;
	comip_host->clk_write_delay = clk_write_delay;
	comip_host->div = -1;

	host = &comip_host->host_mmc;
	host->clock = 0;
	comip_get_setup(host, dev_index);

	mmc = &comip_host->mmc_dev;
	sprintf(mmc->name, "SD/MMC%d", dev_index);
	mmc->priv = host;
	mmc->send_cmd = comip_mmc_send_cmd;
	mmc->set_ios = comip_mmc_set_ios;
	mmc->init = comip_mmc_core_init;

	mmc->voltages = MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31
			| MMC_VDD_31_32 | MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	if (bus_width == 8)
		mmc->host_caps = MMC_MODE_8BIT;
	else
		mmc->host_caps = MMC_MODE_4BIT;
	mmc->host_caps |= MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_HC;

	mmc->f_min = 400000;
	mmc->f_max = 13000000;

	mmc_register(mmc);

	return 0;
}
