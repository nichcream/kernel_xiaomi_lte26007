/* 
 **
 ** Use of source code is subject to the terms of the LeadCore license agreement under
 ** which you licensed source code. If you did not accept the terms of the license agreement,
 ** you are not authorized to use source code. For the terms of the license, please see the
 ** license agreement between you and LeadCore.
 **
 ** Copyright (c) 2013-2020  LeadCoreTech Corp.
 **
 **  PURPOSE: nand main function, based on low level nand driver, implemented  bad block management
 **
 **  CHANGE HISTORY:
 **
 **  Version		Date		Author		Description
 **  1.0.0		2013-06-05	zouqiao		created
 **
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <asm/dma.h>
#include <asm/system.h>

#include <plat/hardware.h>
#include <plat/comip-bbm.h>
#include <plat/nand.h>

//#define CONFIG_NAND_COMIP_DEBUG
#ifdef CONFIG_NAND_COMIP_DEBUG
#define NAND_PRINT1(fmt,args...)	printk(KERN_INFO fmt, ##args)
#define NAND_PRINT2(fmt,args...)	printk(KERN_DEBUG fmt, ##args)
#define PRINT_BUF(buf, num)		print_buf(buf, num)
#else
#define NAND_PRINT1(fmt,args...)	printk(KERN_DEBUG fmt, ##args)
#define NAND_PRINT2(fmt,args...)
#define DPRINTK(fmt,args...)
#define PRINT_BUF(buf, num)
#endif

/* The Data Flash Controller Context structure */
struct comip_nfc_context {
	int use_irq;
	int use_dma;
	int irq;
	int txdma;
	int rxdma;
	uint8_t seq_num;
	uint8_t nop_clk_cnt;
	uint8_t* data_buf;
	uint8_t* oob_buf;
	unsigned int data_buf_len;
	dma_addr_t data_buf_addr;	/*phsical address of dma buffer */
	struct dmas_ch_cfg tx_dma_cfg;
	struct dmas_ch_cfg rx_dma_cfg;
	struct completion tx_dma_completion;
	struct completion rx_dma_completion;
	struct completion completion;

	/* Clock. */
	unsigned long clk_rate;
	struct clk *clk;

	/* NFC. */
	uint32_t ecc_start;
	uint32_t ecc_bytes;
	uint32_t sector_num;
	uint32_t sector_spare;
	uint32_t chip_select;
	uint32_t enable_ecc;

	struct comip_bbm *bbm;
	struct comip_nfc_flash_info *flash_info;	/* Flash Spec */
};

/*
 * This macro will be used in following NAND operation functions.
 * It is used to clear command buffer to ensure cmd buffer is empty
 * in case of operation is timeout
 */
#define ClearCMDBuf(ctx, timeout)	do { \
		comip_nfc_cmd_stop(ctx); \
		udelay(timeout); \
	} while (0)

#define WAIT_FOR_RX(data) do { \
		while (!(comip_nfc_read_reg32(NFC_INTR_RAW_STAT) & NFC_INT_RNE)){;} \
		(data) = comip_nfc_read_reg16(NFC_RX_BUF); \
	} while (0)

#define WAIT_FOR_TX(data) do { \
		while (!(comip_nfc_read_reg32(NFC_INTR_RAW_STAT) & NFC_INT_TNF)){;} \
		comip_nfc_write_reg16((data), NFC_TX_BUF); \
	} while (0)

static struct nand_ecclayout comip_ecc1_64_layout = {
	.eccbytes = 12,
	.eccpos = {8, 9, 10, 24, 25, 26,
		   40, 41, 42, 56, 57, 58},
	.oobfree = {{11, 13}, {27, 13}, {43, 13}, {59, 5}, {2, 6}}
};

static struct nand_ecclayout comip_ecc1_16_layout = {
	.eccbytes = 6,
	.eccpos = {8, 9, 10, 11, 12, 13},
	.oobfree = {{2, 6}}
};

static struct nand_ecclayout comip_ecc8_layout;

static struct comip_nfc_flash_info micron4GbX16 = {
	.timing = {
		.tWP = 15,		/* the nWE pulse width, ns */
		.tWC = 30,		/* write cycle, ns */
		.tCLS = 10,		/* control signal to WE setup, ns */
		.tCLH = 5,		/* control signal hold, ns */

		.tRP = 15,		/* the nRE pulse width, ns */
		.tRC = 30,		/* read cycle, ns */
		.tCS = 25,		/* CS setup, ns */
		.tCH = 5,		/* CS hold, ns */

		.nWB = 100,		/* nWE high to busy, ns */
		.nFEAT = 1,		/* the poll time of feature operation, us */
		.nREAD = 25,		/* the poll time of read, us */
		.nPROG = 600,		/* the poll time of program, us */
		.nERASE = 10000,		/* the poll time of erase, us */
		.nRESET = 500,		/* the poll time of reset, us */
	},
	.cmd = {
		.read_status = 0x70,
		.read1 = 0x00,
		.read2 = 0x30,
		.readx = 0,
		.reads = 0,
		.write1 = 0x80,
		.write2 = 0x10,
		.erase1 = 0x60,
		.erase2 = 0xD0,
		.random_in = 0x85,
		.random_out1 = 0x05,
		.random_out2 = 0xE0,
	},
	.page_per_block = 64,		/* Pages per block */
	.row = 3,			/* how many byte be needed to input page num */
	.col = 2,			/* how many byte be needed to input page offset */
	.read_id_bytes = 4,		/* Returned ID bytes */
	.page_size = 4096,		/* Page size in bytes */
	.oob_size = 224, 		/* OOB size in bytes */
	.flash_width = 16,		/* Width of Flash memory */
	.ecc_bit = 8,			/* ECC bits */
	.num_blocks = 2048,		/* Number of physical blocks in Flash */
	.chip_id = 0xbc2c,
};

static struct comip_nfc_flash_info micron8GbX16 = {
	.timing = {
		.tWP = 25,		/* the nWE pulse width, ns */
		.tWC = 45,		/* write cycle, ns */
		.tCLS = 35,		/* control signal to WE setup, ns */
		.tCLH = 10,		/* control signal hold, ns */

		.tRP = 25,		/* the nRE pulse width, ns */
		.tRC = 45,		/* read cycle, ns */
		.tCS = 35,		/* CS setup, ns */
		.tCH = 10,		/* CS hold, ns */

		.nWB = 100,		/* nWE high to busy, ns */
		.nFEAT = 10,		/* the poll time of feature operation, us */
		.nREAD = 70,		/* the poll time of read, us */
		.nPROG = 700,		/* the poll time of program, us */
		.nERASE = 5000,		/* the poll time of erase, us */
		.nRESET = 500,		/* the poll time of reset, us */
	},
	.cmd = {
		.read_status = 0x70,
		.read1 = 0x00,
		.read2 = 0x30,
		.readx = 0,
		.reads = 0,
		.write1 = 0x80,
		.write2 = 0x10,
		.erase1 = 0x60,
		.erase2 = 0xD0,
		.random_in = 0x85,
		.random_out1 = 0x05,
		.random_out2 = 0xE0,
	},
	.page_per_block = 64,		/* Pages per block */
	.row = 3,			/* how many byte be needed to input page num */
	.col = 2,			/* how many byte be needed to input page offset */
	.read_id_bytes = 4,		/* Returned ID bytes */
	.page_size = 4096,		/* Page size in bytes */
	.oob_size = 224, 		/* OOB size in bytes */
	.flash_width = 16,		/* Width of Flash memory */
	.ecc_bit = 8,			/* ECC bits */
	.num_blocks = 4096,		/* Number of physical blocks in Flash */
	.chip_id = 0xb32c,
};

struct comip_nfc_flash_info *comip_flash_info[] = {
	&micron4GbX16,
	&micron8GbX16,
};

static int comip_nfc_get_flash_info(int index,
			struct comip_nfc_flash_info **flash_info)
{
	if (index >= ARRAY_SIZE(comip_flash_info)) {
		*flash_info = NULL;
		return -EINVAL;
	}

	*flash_info = comip_flash_info[index];
	return 0;
}

static void comip_nfc_flush_fifo(struct comip_nfc_context *context,
				uint8_t is_rx)
{
	unsigned int reg;

	reg = comip_nfc_read_reg32(NFC_CTRLR0);
	if (is_rx)
		comip_nfc_write_reg32((reg | NFC_CTRLR0_RFLUSH), NFC_CTRLR0);
	else
		comip_nfc_write_reg32((reg | NFC_CTRLR0_TFLUSH), NFC_CTRLR0);
	comip_nfc_write_reg32(reg, NFC_CTRLR0);
}

static int comip_nfc_read_fifo(struct comip_nfc_context *context,
				uint8_t *buffer, int nbytes)
{
	struct comip_nfc_flash_info *flash_info = context->flash_info;
	int status = ERR_NONE;

	if (context->use_dma) {
		unsigned long timeout;
		struct dmas_ch_cfg *cfg = &context->rx_dma_cfg;

		cfg->flags = DMAS_CFG_BLOCK_SIZE | DMAS_CFG_DST_ADDR;
		cfg->dst_addr = (u32)(buffer - context->data_buf)
						+ context->data_buf_addr;
		cfg->block_size = nbytes;
		comip_dmas_config(context->rxdma, cfg);
		INIT_COMPLETION(context->rx_dma_completion);
		comip_dmas_start(context->rxdma);

		timeout = wait_for_completion_timeout(
				&context->rx_dma_completion,
				NAND_OPERATION_TIMEOUT);
		if (!timeout && !context->rx_dma_completion.done) {
			INIT_COMPLETION(context->rx_dma_completion);
			status = ERR_SENDCMD;
			NAND_PRINT1("Nand read fifo timeout!\n");
		}

		comip_dmas_stop(context->rxdma);
	} else {
		uint32_t cnt;
		uint16_t data;

		if (flash_info->flash_width == 8) {
			for(cnt = 0; cnt < nbytes; cnt++) {
				WAIT_FOR_RX(data);
				*buffer++ = (uint8_t)data;
			}
		} else {
			for (cnt = 0; cnt < nbytes / 2; cnt++) {
				WAIT_FOR_RX(*(uint16_t *)buffer);
				buffer += 2;
			}
		}
	}

	return status;
}

static int comip_nfc_write_fifo(struct comip_nfc_context *context,
				 uint8_t *buffer, int nbytes)
{
	int status = ERR_NONE;

	if (context->use_dma) {
		unsigned long timeout;
		struct dmas_ch_cfg *cfg = &context->tx_dma_cfg;

		cfg->flags = DMAS_CFG_BLOCK_SIZE | DMAS_CFG_SRC_ADDR;
		cfg->src_addr = (u32)(buffer - context->data_buf)
						+ context->data_buf_addr;
		cfg->block_size = nbytes;
		comip_dmas_config(context->txdma, cfg);
		INIT_COMPLETION(context->tx_dma_completion);
		comip_dmas_start(context->txdma);

		timeout = wait_for_completion_timeout(
				&context->tx_dma_completion,
				NAND_OPERATION_TIMEOUT);
		if (!timeout && !context->tx_dma_completion.done) {
			INIT_COMPLETION(context->tx_dma_completion);
			status = ERR_SENDCMD;
			NAND_PRINT1("Nand write fifo timeout!\n");
		}

		comip_dmas_stop(context->txdma);
	} else {
		uint32_t cnt;

		for (cnt = 0; cnt < nbytes / 2; cnt++) {
			WAIT_FOR_TX(*(uint16_t *)buffer);
			buffer += 2;
		}
	}

	return status;
}

static void comip_nfc_enable_irq(struct comip_nfc_context *context,
				 unsigned int flags)
{
	unsigned int val;

	val = comip_nfc_read_reg32(NFC_INTR_EN);
	val |= flags;
	comip_nfc_write_reg32(val, NFC_INTR_EN);
}

static void comip_nfc_disable_irq(struct comip_nfc_context *context,
				 unsigned int flags)
{
	unsigned int val;

	val = comip_nfc_read_reg32(NFC_INTR_EN);
	val &= ~flags;
	comip_nfc_write_reg32(val, NFC_INTR_EN);
}

static void comip_nfc_clear_irq(struct comip_nfc_context *context,
				 unsigned int flags)
{
	comip_nfc_write_reg32(flags, NFC_INTR_STAT);
}

static inline int is_buf_blank(u8 * buf, int size)
{
	int i = 0;
	while (i < size) {
		if (*((uint32_t *) (buf + i)) != 0xffffffff)
			return 0;
		i += 4;
	}
	if (i > size) {
		i -= 4;
		while (i < size) {
			if (*(buf + i) != 0xff)
				return 0;
			i++;
		}
	}
	return 1;
}

static void print_buf(char *buf, int num)
{
	int i = 0;

	while (i < num) {
		NAND_PRINT1("0x%08x: %02x %02x %02x %02x %02x %02x %02x"
		" %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		(unsigned int) (i),  buf[i], buf[i+1], buf[i+2],
		buf[i+3], buf[i+4], buf[i+5], buf[i+6], buf[i+7],
		buf[i+8], buf[i+9], buf[i+10],buf[i+11], buf[i+12],
		buf[i+13], buf[i+14], buf[i+15]);
		i += 16;
	}
}

static void inline dump_info(struct comip_nand_info *info)
{
	if (!info)
		return;

	NAND_PRINT1("cmd:0x%x; addr:0x%x; retcode:%d; state:%d \n",
		   info->cur_cmd, info->addr, info->retcode, info->state);
}

static int comip_ecc_gen(struct comip_nfc_context *context,
			uint8_t *dbuf, uint8_t *sbuf, uint32_t sector_num,
			 uint8_t width, uint8_t flag)
{
	uint32_t cnt;
	uint32_t pos;
	uint32_t sector;
	uint32_t hwecc;

	if (context->flash_info->ecc_bit != 1) {
		while(!(comip_nfc_read_reg32(NFC_INTR_RAW_STAT) & NFC_INT_BCH_ENC_DONE));
		comip_nfc_write_reg32(NFC_INT_BCH_ENC_DONE, NFC_INTR_STAT);

		for (pos = 0; pos < context->ecc_bytes; pos++)
			sbuf[context->ecc_start + pos] = comip_nfc_read_reg8(NFC_BCH_RAM + pos);
	} else {
		for (sector = 0; sector < sector_num; sector++) {
			/* Wait for hardware ecc ready. */
			while (!(comip_nfc_read_reg32(NFC_INTR_RAW_STAT) & (1 << (13 + sector))));

			hwecc = comip_nfc_read_reg32(NFC_ECC_CODE0 + 4 * sector);

			/* Clear the ready flag. */
			comip_nfc_write_reg32(1 << (13 + sector), NFC_INTR_STAT);

			for (pos = 0, cnt = 0; (pos < context->sector_spare) && (cnt < 3); pos++) {
				if (ECC_POSITION_MASK & (0x1 << pos)) {
					sbuf[pos] = (uint8_t)hwecc;
					hwecc = hwecc >> 8;
					cnt++;
				}
			}

			dbuf += SECTOR_SIZE;
			sbuf += context->sector_spare;
		}
	}

	return ERR_NONE;
}

static int comip_ecc_arithmetic(uint8_t *ecc1, uint8_t *ecc2, uint8_t *buf,
				   uint8_t bw)
{
	uint32_t result;
	uint32_t err_position;
	uint32_t err_bit;
	uint32_t err_byte;
	uint8_t *errAddr;
	uint8_t cnt;
	uint8_t tmp;

	if (!memcmp(ecc1, ecc2, 3))
		return ERR_NONE;

	cnt = 0;
	tmp = 0;
	result = 0;

	result |= (ecc1[0] ^ ecc2[0]);
	result |= ((ecc1[1] ^ ecc2[1]) << 8);
	result |= ((ecc1[2] ^ ecc2[2]) << 16);

	for (cnt = 0; cnt < 24; cnt++)
		if (result & (0x1 << cnt))
			tmp++;

	switch (tmp) {
	case 0:
		return ERR_NONE;

	case 1:
		return ERR_NONE;

	case 12:
		err_position = ((result & 0x02) >> 1) | ((result & 0x08) >> 2)
					| ((result & 0x20) >> 3) | ((result & 0x80) >> 4);
		err_position = err_position | ((result & 0x200) >> 5) | ((result & 0x800) >> 6)
					| ((result & 0x2000) >> 7) | ((result & 0x8000) >> 8);
		err_position = err_position | ((result & 0x20000) >> 9) | ((result & 0x80000) >> 10)
					| ((result & 0x200000) >> 11) | ((result & 0x800000) >> 12);
		err_bit = (err_position >> 9) & 0x7;
		err_byte = err_position & 0x1ff;
		errAddr = (uint8_t *)(buf + err_byte);

		/* If err_bit is 1, then clr this bit; If err_bit is 0, then set this bit. */
		tmp = *errAddr;
		if((tmp >> err_bit) & 0x01)
			tmp = tmp & (0xff & (~(1 << err_bit)));
		else
			tmp = tmp | (1 << err_bit);
		*errAddr = tmp;
		return ERR_ECC_CORRECTABLE;

	default:
		return ERR_ECC_UNCORRECTABLE;
	}

	return ERR_NONE;
}

static int comip_ecc_verify(struct comip_nfc_context *context,
				uint8_t *dbuf, uint8_t *sbuf, uint32_t sector_num,
				uint8_t width)
{
	uint32_t cnt;
	uint32_t pos;
	uint32_t sector;
	uint32_t hwecc;
	uint8_t swecc[3];	/* Calc for caculate, store is in sbuf. */
	int status = 0;

	if (context->flash_info->ecc_bit != 1) {
		while(!(comip_nfc_read_reg32(NFC_INTR_RAW_STAT) & NFC_INT_BCH_DEC_DONE));
		comip_nfc_write_reg32(NFC_INT_BCH_DEC_DONE, NFC_INTR_STAT);

		if (comip_nfc_read_reg32(NFC_BCH_ERR_MODE) & 0xff00)
			return ERR_ECC_UNCORRECTABLE;
	} else {
		/* Get the ecc-code from sbuf. */
		for (sector = 0; sector < sector_num; sector++) {
			for (pos = 0, cnt = 0; (pos < context->sector_spare) && (cnt < 3); pos++) {
				if (ECC_POSITION_MASK & (0x1 << pos))
					swecc[cnt++] = sbuf[pos];
			}

			/* Wait for hardware ecc ready. */
			while (!(comip_nfc_read_reg32(NFC_INTR_RAW_STAT) & (1 << (13 + sector))));

			hwecc = comip_nfc_read_reg32(NFC_ECC_CODE0 + 4 * sector);

			/* Clear the ready flag. */
			comip_nfc_write_reg32(1 << (13 + sector), NFC_INTR_STAT);

			status = comip_ecc_arithmetic((uint8_t *)&hwecc, swecc, dbuf, width);
			if ((status != ERR_NONE) && (status != ERR_ECC_CORRECTABLE))
				return ERR_ECC_UNCORRECTABLE;

			dbuf += SECTOR_SIZE;
			sbuf += context->sector_spare;
		}
	}

	return ERR_NONE;
}

static int comip_nfc_status(struct comip_nfc_context *context, uint8_t *status2)
{
	uint8_t seq_num;
	uint16_t seq_status;
	int status = ERR_NONE;

	do {
		seq_status = comip_nfc_read_reg16(NFC_STA_SEQ);
		seq_num = (uint8_t)((seq_status >> 8) & 0x7f);
		if (seq_num != context->seq_num)
			NAND_PRINT1("Invalid seq number. Wanted %d, actual %d. Try again!\n",
						context->seq_num, seq_num);
	} while (seq_num != context->seq_num);

	*status2 = (uint8_t)seq_status;

	context->seq_num++;
	if (context->seq_num > 16)
		context->seq_num = 1;

	return status;
}

static int comip_nfc_check_rb(struct comip_nfc_context *context,
				uint32_t timeout)
{
	/* Check ready status. */
	do {
		udelay(5);
	} while (timeout-- && !(comip_nfc_read_reg32(NFC_RB_STATUS) & NFC_RB_STATUS_RDY));

	if (!(comip_nfc_read_reg32(NFC_RB_STATUS) & NFC_RB_STATUS_RDY)) {
		NAND_PRINT1("Nand check rb timeout!\n");
		return ERR_SENDCMD;
	}

	return ERR_NONE;
}

static int comip_nfc_wait_status(struct comip_nfc_context *context,
				uint8_t *status2, uint32_t timeout, uint8_t check_rb)
{
	int status = ERR_NONE;
	int chip_num = context->chip_select;
	uint8_t seq_num = context->seq_num;

	if (!context->use_irq) {
		if (check_rb) {
			status = comip_nfc_check_rb(context, timeout);
			if (status)
				return status;
		}

		WRITE_CMD(STAT_CODE, chip_num, seq_num);

		WRITE_CMD(NOP_CODE, 0x1, 0x0);

		while (timeout-- && !(comip_nfc_read_reg32(NFC_INTR_RAW_STAT) & NFC_INT_NOP))
			udelay(1);

		if (!(comip_nfc_read_reg32(NFC_INTR_RAW_STAT) & NFC_INT_NOP)) {
			status = ERR_SENDCMD;
			NAND_PRINT1("Nand wait status timeout!\n");
		}

		comip_nfc_write_reg32(NFC_INT_NOP, NFC_INTR_STAT);
	} else {
		INIT_COMPLETION(context->completion);

		if (check_rb)
			seq_num |= NFC_CMD_STA_RB_EN;

		WRITE_CMD(STAT_CODE, chip_num, seq_num);

		WRITE_CMD(NOP_CODE, 0x1, 0x0);

		timeout = wait_for_completion_timeout(&context->completion, NAND_OPERATION_TIMEOUT);
		if (!timeout && !context->completion.done) {
			INIT_COMPLETION(context->completion);
			status = ERR_SENDCMD;
			NAND_PRINT1("Nand wait status irq timeout!\n");
		}
	}

	if (status)
		return status;

	return comip_nfc_status(context, status2);
}

static void comip_nfc_set_ecc_type(int type)
{
	unsigned int reg;
	unsigned int val;

	if (type == 8)
		val = 1;
	else if (type == 4)
		val = 2;
	else
		val = 0;

	reg = comip_nfc_read_reg32(NFC_CTRLR0);
	reg &= ~(3 << 8);
	reg |= val << 8;
	comip_nfc_write_reg32(reg, NFC_CTRLR0);
}

static void inline comip_nfc_enable_ecc(struct comip_nfc_context *context,
				int enable)
{
	uint32_t reg;

	if (context->flash_info->ecc_bit != 1)
		return;

	if (!enable) {
		reg = comip_nfc_read_reg32(NFC_CTRLR0);
		reg &= ~NFC_CTRLR0_ECC_EN;
		comip_nfc_write_reg32(reg, NFC_CTRLR0);
		context->enable_ecc = 0;
	} else {
		reg = comip_nfc_read_reg32(NFC_CTRLR0);
		reg |= NFC_CTRLR0_ECC_EN;
		comip_nfc_write_reg32(reg, NFC_CTRLR0);
		context->enable_ecc = 1;
	}
}

static void comip_nfc_enable_auto_ecc(int enable, unsigned int address)
{
	unsigned int reg;

	reg = comip_nfc_read_reg32(NFC_CTRLR0);
	if (!enable)
		reg &= ~NFC_CTRLR0_ECC_FIX_ERROR_EN;
	else
		reg |= NFC_CTRLR0_ECC_FIX_ERROR_EN;

	comip_nfc_write_reg32(reg, NFC_CTRLR0);
	comip_nfc_write_reg32(address, NFC_AUTO_EC_BASE_ADDR);
}

static int comip_nfc_ll_erase(struct comip_nfc_context *context, uint32_t block)
{
	struct comip_nfc_flash_info *flash_info = context->flash_info;
	uint32_t chip_num = context->chip_select;
	uint32_t temp;

	WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.erase1);

	WRITE_CMD(ADD_CODE, chip_num, flash_info->row - 1);
	if (flash_info->row > 2) {
		temp = flash_info->page_per_block * block;
		WRITE_DATA(temp);
		WRITE_DATA(temp >> 16);
	} else
		WRITE_DATA(flash_info->page_per_block * block);

	WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.erase2);

	WRITE_CMD(NOP_CODE, 0x0, context->nop_clk_cnt);

	return ERR_NONE;
}

static int comip_nfc_ll_read_page(struct comip_nfc_context *context, uint32_t page,
				uint8_t *dbuf, uint8_t *sbuf)
{
	struct comip_nfc_flash_info *flash_info = context->flash_info;
	uint32_t chip_num = context->chip_select;
	uint32_t width = (flash_info->flash_width == 8) ? 0 : 1;
	uint8_t* sptr;
	int status = ERR_NONE;

	comip_nfc_flush_fifo(context, 1);

	if (dbuf) {
		if (flash_info->ecc_bit != 1) {
			comip_nfc_enable_auto_ecc(1, (ulong)(dbuf - context->data_buf)
						+ context->data_buf_addr);
			comip_nfc_set_ecc_type(flash_info->ecc_bit);

			comip_nfc_write_reg32(0x01, NFC_PARITY_EN);

			WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.read1);

			WRITE_CMD(ADD_CODE, chip_num, flash_info->col + flash_info->row - 1);
			WRITE_DATA((flash_info->page_size + context->ecc_start) >> width);
			if (flash_info->row > 2) {
				WRITE_DATA(page);
				WRITE_DATA(page >> 16);
			} else
				WRITE_DATA(page);

			WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.read2);

			WRITE_CMD(NOP_CODE, 0x0, context->nop_clk_cnt);
			WRITE_CMD(READ_CODE, chip_num, 0x2);
			WRITE_DATA(context->ecc_bytes - 1);

			WRITE_CMD(NOP_CODE, 0x1, 0x00);
			comip_nfc_read_fifo(context, context->oob_buf + context->ecc_start, context->ecc_bytes);

			comip_nfc_write_reg32(0x00, NFC_PARITY_EN);
		}

		WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.read1);

		WRITE_CMD(ADD_CODE, chip_num, flash_info->col + flash_info->row - 1);
		WRITE_DATA(0);
		if (flash_info->row > 2) {
			WRITE_DATA(page);
			WRITE_DATA(page >> 16);
		} else
			WRITE_DATA(page);

		WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.read2);

		WRITE_CMD(NOP_CODE, 0x0, context->nop_clk_cnt);

		if (flash_info->ecc_bit == 1)
			WRITE_CMD(READ_CODE, chip_num, 1);
		else
			WRITE_CMD(READ_CODE, chip_num, 0);
		WRITE_DATA(context->sector_num * SECTOR_SIZE - 1);

		WRITE_CMD(NOP_CODE, 0x1, 0x0);
		status = comip_nfc_read_fifo(context, dbuf, context->sector_num * SECTOR_SIZE);

		/* Do ecc-verify. */
		if (!status && (flash_info->ecc_bit != 1)) {
			status = comip_ecc_verify(context, dbuf, NULL, context->sector_num, width);
			comip_nfc_enable_auto_ecc(0, 0xffffffff);
			comip_nfc_set_ecc_type(1);
		}

		/* Demand for ECC-verify. */
		if (!status) {
			if (sbuf == NULL)
				sptr = context->oob_buf;
			else
				sptr = sbuf;

			/* Using random-output way to read spare data. */
			if (flash_info->cmd.random_out1 != 0) {
				WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.random_out1);

				WRITE_CMD(ADD_CODE, chip_num, flash_info->col - 1);
				WRITE_DATA(flash_info->page_size >> width);

				WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.random_out2);

				WRITE_CMD(READ_CODE, chip_num, 0x0);
				WRITE_DATA(context->sector_spare * context->sector_num - 1);

				WRITE_CMD(NOP_CODE, 0x1, 0x00);
				status = comip_nfc_read_fifo(context, sptr, context->sector_num * context->sector_spare);
			}

			/* Do ecc-verify. */
			if (flash_info->ecc_bit == 1)
				status = comip_ecc_verify(context, dbuf, sptr, context->sector_num, width);
		}
	} else if (sbuf) {
		WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.read1);

		WRITE_CMD(ADD_CODE, chip_num,
			  flash_info->col + flash_info->row - 1);
		WRITE_DATA(flash_info->page_size >> width);
		if (flash_info->row > 2) {
			WRITE_DATA(page);
			WRITE_DATA(page >> 16);
		} else
			WRITE_DATA(page);

		WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.read2);

		WRITE_CMD(NOP_CODE, 0x0, context->nop_clk_cnt);

		WRITE_CMD(READ_CODE, chip_num, 0);
		WRITE_DATA(context->sector_num * context->sector_spare - 1);

		WRITE_CMD(NOP_CODE, 0x1, 0x0);
		status = comip_nfc_read_fifo(context, sbuf, context->sector_num * context->sector_spare);
	}

	return status;
}

static int comip_nfc_ll_read_random(struct comip_nfc_context *context, uint32_t page,
				uint32_t offset, uint32_t size, uint8_t *buf)
{
	struct comip_nfc_flash_info *flash_info = context->flash_info;
	uint32_t chip_num = context->chip_select;
	uint32_t width = (flash_info->flash_width == 8) ? 0 : 1;

	comip_nfc_flush_fifo(context, 1);

	WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.read1);

	WRITE_CMD(ADD_CODE, chip_num, flash_info->col + flash_info->row - 1);
	/* Send col address. */
	WRITE_DATA(offset >> width);
	/* Send row address. */
	if (flash_info->row > 2) {
		WRITE_DATA(page);
		WRITE_DATA(page >> 16);
	} else
		WRITE_DATA(page);

	WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.read2);

	WRITE_CMD(NOP_CODE, 0x0, context->nop_clk_cnt);

	WRITE_CMD(READ_CODE, chip_num, 0x0);
	WRITE_DATA(size - 1);

	return comip_nfc_read_fifo(context, buf, size);
}

static int comip_nfc_ll_write_page(struct comip_nfc_context *context, uint32_t page,
				uint8_t *dbuf, uint8_t *sbuf)
{
	struct comip_nfc_flash_info *flash_info = context->flash_info;
	uint32_t chip_num = context->chip_select;
	uint32_t width = (flash_info->flash_width == 8) ? 0 : 1;
	uint8_t* sptr;
	int status = ERR_NONE;

	if (dbuf) {
		if (flash_info->ecc_bit != 1)
			comip_nfc_set_ecc_type(flash_info->ecc_bit);

		WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.write1);

		WRITE_CMD(ADD_CODE, chip_num, flash_info->col + flash_info->row - 1);
		WRITE_DATA(0);
		if (flash_info->row > 2) {
			WRITE_DATA(page);
			WRITE_DATA(page >> 16);
		} else
			WRITE_DATA(page);

		if (flash_info->ecc_bit == 1)
			WRITE_CMD(WRITE_CODE, chip_num, 0x1);
		else
			WRITE_CMD(WRITE_CODE, chip_num, 0x0);
		WRITE_DATA(context->sector_num * SECTOR_SIZE - 1);

		status = comip_nfc_write_fifo(context, dbuf, context->sector_num * SECTOR_SIZE);

		if (!status) {
			if (sbuf == NULL) {
				sptr = context->oob_buf;
				memset(sptr, 0xff, context->flash_info->oob_size);
			} else
				sptr = sbuf;

			comip_ecc_gen(context, dbuf, sptr, context->sector_num, width, 1);

			if (flash_info->ecc_bit != 1)
				comip_nfc_set_ecc_type(1);
			WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.random_in);

			WRITE_CMD(ADD_CODE, chip_num, flash_info->col - 1);
			WRITE_DATA(flash_info->page_size >> width);

			WRITE_CMD(WRITE_CODE, chip_num, 0x0);
			WRITE_DATA(context->sector_spare * context->sector_num - 1);

			status = comip_nfc_write_fifo(context, sptr, context->sector_spare * context->sector_num);
		}

		WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.write2);

		WRITE_CMD(NOP_CODE, 0x0, context->nop_clk_cnt);
	} else if (sbuf) {
		WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.write1);

		WRITE_CMD(ADD_CODE, chip_num, flash_info->col + flash_info->row - 1);
		WRITE_DATA(flash_info->page_size >> width);
		if (flash_info->row > 2) {
			WRITE_DATA(page);
			WRITE_DATA(page >> 16);
		} else
			WRITE_DATA(page);

		WRITE_CMD(WRITE_CODE, chip_num, 0x0);
		WRITE_DATA(context->sector_num * context->sector_spare - 1);

		status = comip_nfc_write_fifo(context, sbuf, context->sector_num * context->sector_spare);

		WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.write2);

		WRITE_CMD(NOP_CODE, 0x0, context->nop_clk_cnt);
	}

	return status;
}

static int comip_nfc_ll_write_random(struct comip_nfc_context *context, uint32_t page,
				uint32_t offset, uint32_t size, uint8_t *buf)
{
	struct comip_nfc_flash_info *flash_info = context->flash_info;
	uint32_t chip_num = context->chip_select;
	uint32_t width = (flash_info->flash_width == 8) ? 0 : 1;
	int status = ERR_NONE;

	WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.write1);

	WRITE_CMD(ADD_CODE, chip_num, flash_info->col + flash_info->row - 1);
	WRITE_DATA(offset >> width);
	if (flash_info->row > 2) {
		WRITE_DATA(page);
		WRITE_DATA(page >> 16);
	} else
		WRITE_DATA(page);

	WRITE_CMD(WRITE_CODE, chip_num, 0x0);
	WRITE_DATA(size - 1);

	status = comip_nfc_write_fifo(context, buf, size);

	WRITE_CMD(CMD_CODE, chip_num, flash_info->cmd.write2);

	WRITE_CMD(NOP_CODE, 0x0, context->nop_clk_cnt);

	return status;
}

static unsigned int comip_nfc_ns2clkcnt(struct comip_nfc_context *context,
				int ns)
{
	unsigned int clkcnt = (context->clk_rate / 100000) * ns;

	do_div(clkcnt, 10000);

	return (clkcnt + 1);
}

static unsigned int comip_nfc_check_timing(struct comip_nfc_context *context,
				unsigned int *range, int ns)
{
	unsigned int clkcnt = comip_nfc_ns2clkcnt(context, ns);
	unsigned int index;

	for (index = 0; index < 4; index++)
		if (clkcnt <= range[index])
			break;

	if (index >= 4)
		index = 3;

	return index;
}

static void comip_nfc_set_timing(struct comip_nfc_context *context)
{
	struct comip_nfc_flash_timing* timing = &context->flash_info->timing;
	unsigned int range[4][4] = {{0, 1, 2, 3}, {1, 3, 4, 5},
				{1, 2, 3, 4}, {3, 4, 5, 6}};
	unsigned int value;
	unsigned int temp;

	/* Configure the timming. */
	value = 0;
	temp = comip_nfc_check_timing(context, &range[3][0], timing->tWP);
	value |= (temp << 0);
	temp = comip_nfc_check_timing(context, &range[2][0], timing->tWC - timing->tWP);
	value |= (temp << 2);
	temp = comip_nfc_check_timing(context, &range[1][0], timing->tCLS - timing->tWP);
	value |= (temp << 4);
	temp = comip_nfc_check_timing(context, &range[0][0], timing->tCLH);
	value |= (temp << 6);
	comip_nfc_write_reg32(value, NFC_TIMR0);

	value = 0;
	temp = comip_nfc_check_timing(context, &range[3][0], timing->tRP);
	value |= (temp << 0);
	temp = comip_nfc_check_timing(context, &range[2][0], timing->tRC - timing->tRP);
	value |= (temp << 2);
	temp = comip_nfc_check_timing(context, &range[1][0], timing->tCS - timing->tRP);
	value |= (temp << 4);
	temp = 0x3;
	value |= (temp << 6);
	comip_nfc_write_reg32(value, NFC_TIMR1);

	context->nop_clk_cnt = comip_nfc_ns2clkcnt(context, timing->nWB);
	if (context->nop_clk_cnt > 127)
		context->nop_clk_cnt = 127;
}

static void comip_nfc_cmd_stop(struct comip_nfc_context *context)
{
	unsigned int reg;

	reg = comip_nfc_read_reg32(NFC_CTRLR0);
	comip_nfc_write_reg32(reg & (~NFC_CTRLR0_EN), NFC_CTRLR0);
	comip_nfc_write_reg32((reg | NFC_CTRLR0_EN), NFC_CTRLR0);

	/* Clear ecc status. */
	comip_nfc_write_reg32(NFC_INT_ECC, NFC_INTR_STAT);

	/* Clear nop interrupt. */
	comip_nfc_write_reg32(NFC_INT_NOP, NFC_INTR_STAT);
}

/*
 * high level primitives
 */
static int comip_nfc_init(struct comip_nfc_context *context, int index)
{
	unsigned int val = NFC_CTRLR0_ECC_EN | NFC_CTRLR0_NFC_WIDTH
				| NFC_CTRLR0_EN | NFC_CTRLR0_WP;
	unsigned int page_size;
	unsigned int parity;
	unsigned int tmp;
	int status;

	comip_nfc_write_reg32(0x00, NFC_CTRLR0);
	udelay(10);
	comip_nfc_write_reg32(val | NFC_CTRLR0_TFLUSH | NFC_CTRLR0_RFLUSH, NFC_CTRLR0);
	udelay(10);
	comip_nfc_write_reg32(val, NFC_CTRLR0);
	udelay(10);

	comip_nfc_write_reg32(context->chip_select, NFC_CS);

	WRITE_CMD(RST_CODE, context->chip_select, 0x00);
	WRITE_CMD(NOP_CODE, 0x0, 0x3);

	mdelay(1);

	status = comip_nfc_get_flash_info(index, &(context->flash_info));
	if (status)
		return status;

	if (context->flash_info->flash_width == 8)
		val &= ~NFC_CTRLR0_NFC_WIDTH;

	if (context->flash_info->ecc_bit == 1) {
		/* Disable auto ECC. */
		val &= ~NFC_CTRLR0_ECC_FIX_ERROR_EN;
	} else {
		/* Enable auto ECC. */
		if (context->flash_info->ecc_bit == 8)
			val |= (1 << 8) | NFC_CTRLR0_ECC_FIX_ERROR_EN;
		else
			val |= (2 << 8) | NFC_CTRLR0_ECC_FIX_ERROR_EN;
	
		tmp = (context->flash_info->page_size / SECTOR_SIZE);
		parity = ((1 << tmp) - 1);
		page_size = 0;
		while (tmp /= 2)
			page_size++;

		comip_nfc_write_reg32(1, NFC_BCH_RST);
		comip_nfc_write_reg32(((page_size << 8) | parity), NFC_BCH_CFG);
	}

	comip_nfc_write_reg32(val, NFC_CTRLR0);

	comip_nfc_set_timing(context);
	comip_nfc_enable_ecc(context, 1);

	return ERR_NONE;
}

static int comip_nfc_reset_flash(struct comip_nfc_context *context)
{
	struct comip_nfc_flash_info *flash_info = context->flash_info;
	uint8_t status2;
	int status;

	/* Send command */
	WRITE_CMD(RST_CODE, context->chip_select, 0x00);
	WRITE_CMD(NOP_CODE, 0x0, context->nop_clk_cnt);

	status = comip_nfc_wait_status(context, &status2, flash_info->timing.nRESET, 1);
	if (status) {
		ClearCMDBuf(context, NAND_OTHER_TIMEOUT);
		return status;
	}

	if (!(status2 & NFC_STA_SEQ_STA_READY1)) {
		NAND_PRINT1("Nand reset failed!\n");
		return ERR_RESET;
	}

	return ERR_NONE;
}

static int comip_nfc_read_id(struct comip_nfc_context *context, uint32_t *id)
{
	uint32_t i;
	uint32_t chip_num = context->chip_select;
	uint8_t* buf = (uint8_t*)id;

	WRITE_CMD(CMD_CODE, chip_num, CMD_READ_ID);

	WRITE_CMD(ADD_CODE, chip_num, 0x00);
	WRITE_DATA(0x00);

	WRITE_CMD(READ_CODE, chip_num, 0x0);
	WRITE_DATA(3);

	for (i = 0; i < 4; i++) {
		while (!(comip_nfc_read_reg32(NFC_INTR_RAW_STAT) & NFC_INT_RNE));
		buf[i] = (uint8_t)comip_nfc_read_reg16(NFC_RX_BUF);
	}

	return ERR_NONE;
}

static int comip_nfc_erase(struct comip_nfc_context *context, uint32_t block)
{
	int status;
	uint8_t status2;
	uint8_t tries = 3;

erase_again:
	status = comip_nfc_ll_erase(context, block);
	if (status)
		return status;

	status = comip_nfc_wait_status(context, &status2, context->flash_info->timing.nERASE, 1);
	if (status) {
		tries--;
		if (tries){
			NAND_PRINT1("Nand erase timeout at block %d, try it again, tries=0x%x\n", block, tries);
			ClearCMDBuf(context, NAND_OTHER_TIMEOUT);
			goto erase_again;
		}

		return status;
	}

	if (status2 & NFC_STA_SEQ_STA_ERROR0) {
		NAND_PRINT1("Nand erase failed at block %d, status2 : 0x%x\n", block, status2);
		return ERR_ERASE;
	}

	return ERR_NONE;
}

static int comip_nfc_read_page(struct comip_nfc_context *context, uint32_t page)
{
	int status;
	uint8_t tries = 10;

read_again:
	status = comip_nfc_ll_read_page(context, page, context->data_buf,
					  context->data_buf + context->flash_info->page_size);
	if (status) {
		if (status != ERR_ECC_UNCORRECTABLE) {
			tries--;
			if (tries) {
				NAND_PRINT1("Nand read timeout at page %d, try it again, tries=0x%x\n", page, tries);
				ClearCMDBuf(context, NAND_OTHER_TIMEOUT);
				goto read_again;
			}

			return status;
		} else {
			/* When the data buf is blank, the NFC will report ecc error. */
			if (!is_buf_blank(context->data_buf, context->flash_info->page_size)) {
				tries--;
				if (tries) {
					NAND_PRINT1("Nand read ECC error at page %d, try it again, tries=0x%x\n", page, tries);
					ClearCMDBuf(context, NAND_OTHER_TIMEOUT);
					goto read_again;
				} else {
					NAND_PRINT1("Nand read ECC error at page %d\n", page);
					print_buf(context->data_buf, context->flash_info->page_size + context->flash_info->oob_size);
					return status;
				}
			}
		}
	}

	return ERR_NONE;
}

static int comip_nfc_read_oob(struct comip_nfc_context *context, uint32_t page)
{
	int status;
	uint8_t tries = 5;

read_oob_again:
	status = comip_nfc_ll_read_random(context, page,
				 context->flash_info->page_size,
				 context->flash_info->oob_size,
				 context->data_buf);
	if (status) {
		tries--;
		if (tries) {
			NAND_PRINT1("Nand read oob timeout at page %d, try it again, tries=0x%x\n", page, tries);
			ClearCMDBuf(context, NAND_OTHER_TIMEOUT);
			goto read_oob_again;
		}

		return status;
	}

	return ERR_NONE;
}

static int comip_nfc_write_page(struct comip_nfc_context *context, uint32_t page)
{
	int status;
	uint8_t status2;
	uint8_t tries = 3;

write_again:
	status = comip_nfc_ll_write_page(context, page, context->data_buf,
					context->data_buf + context->flash_info->page_size);
	if (!status)
		status = comip_nfc_wait_status(context, &status2, context->flash_info->timing.nPROG, 1);

	if (status) {
		tries--;
		if (tries) {
			NAND_PRINT1("Nand write timeout at page %d, try it again, tries=0x%x\n", page, tries);
			ClearCMDBuf(context, NAND_OTHER_TIMEOUT);
			goto write_again;
		}

		return status;
	}

	if (status2 & NFC_STA_SEQ_STA_ERROR0) {
		NAND_PRINT1("Nand write failed at page %d, status2 : 0x%x\n", page, status2);
		return ERR_PROG;
	}

	return ERR_NONE;
}

static int comip_nfc_write_oob(struct comip_nfc_context *context, uint32_t page)
{
	int status;
	uint8_t status2;
	uint8_t tries = 3;

write_oob_again:
	status = comip_nfc_ll_write_random(context, page,
				 context->flash_info->page_size,
				 context->flash_info->oob_size,
				 context->data_buf);
	if (!status)
		status = comip_nfc_wait_status(context, &status2, context->flash_info->timing.nPROG, 1);

	if (status) {
		tries--;
		if (tries) {
			NAND_PRINT1("Nand write oob timeout at page %d, try it again, tries=0x%x\n", page, tries);
			ClearCMDBuf(context, NAND_OTHER_TIMEOUT);
			goto write_oob_again;
		}

		return status;
	}

	if (status2 & NFC_STA_SEQ_STA_ERROR0) {
		NAND_PRINT1("Nand write oob failed at page %d, status2 : 0x%x\n", page, status2);
		return ERR_PROG;
	}

	return ERR_NONE;
}

static int comip_nfc_block_check(struct comip_nfc_context *context, uint32_t block)
{
	int ret = 0;
	int i;

	for (i = 0; i < 2; i++) {
		ret = comip_nfc_read_oob(context,
					block * context->flash_info->page_per_block + i);
		if (ret)
			return 1;

		if (context->data_buf[0] != 0xff)
			return 1;
	}

	return 0;
}

static int comip_nand_bbm_cmd(struct mtd_info *mtd, uint32_t cmd, uint32_t addr, uint8_t* buf)
{
	struct nand_chip *this = (struct nand_chip *)mtd->priv;
	struct comip_nand_info *info = (struct comip_nand_info *)(this->priv);
	struct comip_nfc_context *context = info->context;
	int ret = 0;

	this->select_chip(mtd, 0);

	switch (cmd) {
		case BBM_READ:
			comip_nfc_enable_ecc(context, 1);
			ret = comip_nfc_read_page(context, addr >> this->page_shift);
			memcpy(buf, info->data_buf, info->data_buf_len);
			break;
		case BBM_WRITE:
			comip_nfc_enable_ecc(context, 1);
			memcpy(info->data_buf, buf, info->data_buf_len);
			ret = comip_nfc_write_page(context, addr >> this->page_shift);
			break;
		case BBM_ERASE :
			comip_nfc_enable_ecc(context, 0);
			ret = comip_nfc_block_check(context, addr >> this->phys_erase_shift);
			comip_nfc_enable_ecc(context, 1);
			if (ret)
				return ret;

			ret = comip_nfc_erase(context, addr >> this->phys_erase_shift);
			break;
		default:
			break;
	}

	return ret;
}

/*call back funstions*/
static int comip_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);

	/* comip_nand_send_command has waited for command complete */
	if (this->state == FL_WRITING || this->state == FL_ERASING) {
		if (info->retcode == ERR_NONE)
			return 0;
		else {
			/*
			 * any error make it return 0x01 which will tell
			 * the caller the erase and write fail
			 */
			NAND_PRINT1("comip_nand_waitfunc: erase/write nand fail, state:%d, ret:%d\n", this->state, info->retcode);
			return 0x01;
		}
	}

	return 0;
}

static void comip_nand_select_chip(struct mtd_info *mtd, int chip)
{
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);
	struct comip_nfc_context *context = info->context;

	if (chip >= MAX_CHIPS) {
		NAND_PRINT1("Comip NAND device:" "not select the NAND chips!\n");
		return;
	}

	context->chip_select = 1;
}

/*
 * Now, we are not sure that the NFC_RB_STATUS_RDY mean the flash is ready.
 * Need more test.
 */
static int comip_nand_dev_ready(struct mtd_info *mtd)
{
	return (comip_nfc_read_reg32(NFC_RB_STATUS) & NFC_RB_STATUS_RDY);
}

static void comip_nand_command(struct mtd_info *mtd, unsigned command,
				   int column, int page_addr)
{
	struct nand_chip *this = (struct nand_chip *)(mtd->priv);
	struct comip_nand_info *info = (struct comip_nand_info *)(this->priv);
	struct comip_nfc_context *context = info->context;
	struct comip_nfc_flash_info *flash_info = context->flash_info;
	int ret, pages_shift;
	unsigned int to;

	NAND_PRINT2("command:0x%x at address:0x%x, column:0x%x\n",
		command, page_addr, column);

	/* unused commands. */
	if (command == NAND_CMD_ERASE2)
		return;

	if (info->state != STATE_READY) {
		NAND_PRINT1("CHIP is not ready. state: 0x%x\n",
			   info->state);
		dump_info(info);
		info->retcode = ERR_BUSY;
		return;
	}

	info->state = STATE_CMD_SEND;
	info->retcode = ERR_NONE;

command_again:
	pages_shift = this->phys_erase_shift - this->page_shift;
	if (context->bbm && context->bbm->table_init && context->bbm->search) {
		to = (int)(page_addr >> pages_shift);
		to = context->bbm->search(mtd, context->bbm, to);
		page_addr = (to << pages_shift) |
			(page_addr & ((1 << pages_shift) - 1));
	}

	switch (command) {
	case NAND_CMD_READOOB:
		comip_nfc_enable_ecc(context, 0);
		info->column = column;
		info->cmd = command;
		info->buf_count = mtd->oobsize;
		info->addr = page_addr << this->page_shift;
		info->retcode = comip_nfc_read_oob(context, page_addr);
		comip_nfc_enable_ecc(context, 1);
		break;

	case NAND_CMD_READ0:
		comip_nfc_enable_ecc(context, 1);
		info->column = column;
		info->cmd = command;
		info->buf_count = mtd->writesize + mtd->oobsize;
		info->addr = page_addr << this->page_shift;
		memset(info->data_buf, 0xff, info->buf_count);
		info->retcode = comip_nfc_read_page(context, page_addr);
		break;

	case NAND_CMD_SEQIN:
		info->cmd = command;
		if (column >= mtd->writesize) {	/* Write only OOB? */
			info->buf_count = mtd->oobsize;
			info->column = 0;
			comip_nfc_enable_ecc(context, 0);
		} else {
			info->buf_count = mtd->writesize + mtd->oobsize;
			info->column = column;
			comip_nfc_enable_ecc(context, 1);
		}
		memset(info->data_buf, 0xff, info->buf_count);
		info->addr = page_addr << this->page_shift;
		break;
	case NAND_CMD_PAGEPROG:
		/* prevois command is NAND_CMD_SEIN ? */
		if (info->cmd != NAND_CMD_SEQIN) {
			info->cmd = command;
			info->retcode = ERR_SENDCMD;
			NAND_PRINT1("Comip NAND device: " "No NAND_CMD_SEQIN executed before.\n");
			comip_nfc_enable_ecc(context, 1);
			break;
		}
		info->cmd = command;
		if (info->buf_count <= mtd->oobsize) {
			info->retcode = comip_nfc_write_oob(context,
					(info->addr) >> this->page_shift);
		} else {
			info->retcode = comip_nfc_write_page(context,
					(info->addr) >> this->page_shift);
		}
		if (info->retcode == ERR_PROG) {
			if (context->bbm && context->bbm->table_init && context->bbm->markbad) {
				ret = context->bbm->markbad(mtd, context->bbm, info->addr >> this->bbt_erase_shift);
				if (!ret)
					goto command_again;

				NAND_PRINT1("Comip NAND device: write error at addr:0x%x\n", info->addr);
			}
		}
		break;
	case NAND_CMD_ERASE1:
		info->cmd = command;
		info->addr = page_addr << this->page_shift;
		info->retcode = comip_nfc_erase(context, page_addr >> pages_shift);
		if (info->retcode == ERR_ERASE) {
			if (context->bbm && context->bbm->table_init && context->bbm->markbad) {
				ret = context->bbm->markbad(mtd, context->bbm, info->addr >> this->bbt_erase_shift);
				if (!ret)
					goto command_again;

				NAND_PRINT1("Comip NAND device: erase error at addr:0x%x\n", info->addr);
			}
		}
		break;
	case NAND_CMD_ERASE2:
		break;
	case NAND_CMD_READID:
		info->cmd = command;
		info->buf_count = flash_info->read_id_bytes;
		info->column = 0;
		info->addr = 0xffffffff;

		ret = comip_nfc_read_id(context, (uint32_t *) info->data_buf);
		if (ret)
			NAND_PRINT1("Comip NAND device:" "NAND_CMD_READID error\n");

		NAND_PRINT2("ReadID, [1]:0x%x, [2]:0x%x\n,[3]:0x%x, [4]:0x%x\n",
			info->data_buf[0], info->data_buf[1], info->data_buf[2],
			info->data_buf[3]);
		break;
	case NAND_CMD_STATUS:
		info->cmd = command;
		info->buf_count = 1;
		info->column = 0;
		info->addr = 0xffffffff;

		ret = comip_nfc_wait_status(context, info->data_buf, NAND_STATUS_TIMEOUT, 0);
		if (ret)
			NAND_PRINT1("Comip NAND device:" "NAND_CMD_STATUS error\n");
		break;

	case NAND_CMD_RESET:
		ret = comip_nfc_reset_flash(context);
		if (ret)
			NAND_PRINT1("Comip NAND device:" "NAND_CMD_RESET error\n");
		break;
	default:
		NAND_PRINT1("Comip NAND device:" "Non-support the command.\n");
		break;
	}

	if (info->retcode != ERR_NONE) {
		ClearCMDBuf(context, NAND_OTHER_TIMEOUT);
	}

	info->state = STATE_READY;
}

static uint16_t comip_nand_read_word(struct mtd_info *mtd)
{
	uint16_t retval = 0xffff;
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);

	if (!(info->column & 0x01) && info->column < info->buf_count) {
		retval = *((uint16_t *) (info->data_buf + info->column));
		info->column += 2;
	}
	return retval;
}

/* each read, we can only read 4bytes from NDDB, we must buffer it */
static uint8_t comip_nand_read_byte(struct mtd_info *mtd)
{
	char retval = 0xff;
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);

	if (info->column < info->buf_count) {
		/* Has just send a new command? */
		retval = info->data_buf[info->column];
		info->column += 1;
	}
	return retval;
}

static void comip_nand_read_buf(struct mtd_info *mtd, uint8_t * buf, int len)
{
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);
	int real_len = min((unsigned int)len, info->buf_count - info->column);

	memcpy(buf, info->data_buf + info->column, real_len);
	info->column += real_len;
}

static void comip_nand_write_buf(struct mtd_info *mtd,
				 const uint8_t * buf, int len)
{
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);
	int real_len = min((unsigned int)len, info->buf_count - info->column);

	memcpy(info->data_buf + info->column, buf, real_len);
	info->column += real_len;
}

static int comip_nand_verify_buf(struct mtd_info *mtd,
				 const uint8_t * buf, int len)
{
	return 0;
}

static void comip_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	return;
}

static int comip_nand_calculate_ecc(struct mtd_info *mtd,
					const uint8_t * dat, uint8_t * ecc_code)
{
	return 0;
}

static int comip_nand_correct_data(struct mtd_info *mtd,
				   uint8_t * dat, uint8_t * read_ecc,
				   uint8_t * calc_ecc)
{
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);

	/*
	 * Any error include ERR_SEND_CMD, ERR_DBERR, ERR_BUSERR, we
	 * consider it as a ecc error which will tell the caller the
	 * read fail We have distinguish all the errors, but the
	 * nand_read_ecc only check this function return value
	 */
	if (info->retcode != ERR_NONE)
		return -1;

	return 0;
}

static irqreturn_t comip_nand_irq(int this_irq, void *dev_id)
{
	struct comip_nand_info *info = (struct comip_nand_info *)dev_id;
	unsigned int status;

	status = comip_nfc_read_reg32(NFC_INTR_STAT);
	if (!status)
		return IRQ_NONE;

	comip_nfc_write_reg32(status, NFC_INTR_STAT);

	if (status & NFC_INT_NOP)
		complete(&(info->context->completion));
	else
		NAND_PRINT1("Unknown nfc irq 0x%08x!\n", status);

	return IRQ_HANDLED;
}

static void comip_nand_dma_tx_irq(int irq, int type, void *dev_id)
{
	struct comip_nfc_context *context = (struct comip_nfc_context *)dev_id;
	complete(&context->tx_dma_completion);
}

static void comip_nand_dma_rx_irq(int irq, int type, void *dev_id)
{
	struct comip_nfc_context *context = (struct comip_nfc_context *)dev_id;
	complete(&context->rx_dma_completion);
}

static int comip_nand_dma_init(struct comip_nfc_context* context)
{
	struct dmas_ch_cfg *cfg;
	int ret;

	cfg = &context->tx_dma_cfg;
	cfg->flags = DMAS_CFG_ALL;
	cfg->block_size = 0;
	cfg->src_addr = 0;
	cfg->dst_addr = (unsigned int)NFC_TX_BUF;
	cfg->priority = DMAS_CH_PRI_DEFAULT;
	cfg->bus_width = DMAS_DEV_WIDTH_16BIT;
	cfg->tx_trans_mode = DMAS_TRANS_NORMAL;
	cfg->tx_fix_value = 0;
	cfg->tx_block_mode = DMAS_SINGLE_BLOCK;
	cfg->irq_en = DMAS_INT_DONE;
	cfg->irq_handler = comip_nand_dma_tx_irq;
	cfg->irq_data = context;
	ret = comip_dmas_request("nfc tx dma", context->txdma);
	if (ret)
		return ret;

	ret = comip_dmas_config(context->txdma, cfg);
	if (ret)
		return ret;

	cfg = &context->rx_dma_cfg;
	cfg->flags = DMAS_CFG_ALL;
	cfg->block_size = 0;
	cfg->src_addr = (unsigned int)NFC_RX_BUF;
	cfg->dst_addr = 0;
	cfg->priority = DMAS_CH_PRI_DEFAULT;
	cfg->bus_width = DMAS_DEV_WIDTH_16BIT;
	cfg->rx_trans_type = DMAS_TRANS_BLOCK;
	cfg->rx_timeout = 0;
	cfg->irq_en = DMAS_INT_DONE;
	cfg->irq_handler = comip_nand_dma_rx_irq;
	cfg->irq_data = context;
	ret = comip_dmas_request("nfc rx dma", context->rxdma);
	if (ret)
		return ret;

	ret = comip_dmas_config(context->rxdma, cfg);
	if (ret)
		return ret;

	init_completion(&context->rx_dma_completion);
	init_completion(&context->tx_dma_completion);

	return ret;
}

static void comip_nand_dma_uninit(struct comip_nfc_context* context)
{
	comip_dmas_free(context->rxdma);
	comip_dmas_free(context->txdma);
}

static int comip_nand_bbm_init(struct comip_nfc_context* context,
			struct nand_chip *chip,
			struct mtd_info *mtd)
{
	int ret = 0;

	context->bbm = alloc_comip_bbm();
	if (!context->bbm)
		return -ENOMEM;

	context->bbm->nand_cmd = comip_nand_bbm_cmd;
	if (context->bbm && context->bbm->init && !context->bbm->table_init) {
		context->bbm->page_shift = chip->page_shift;
		context->bbm->erase_shift = chip->phys_erase_shift;
		ret = context->bbm->init(mtd, context->bbm);
	}

	return ret;
}

static int comip_nand_ecc_init(struct comip_nfc_context* context,
			struct nand_chip *chip)
{
	int i;

	context->sector_num = context->flash_info->page_size / SECTOR_SIZE;
	context->sector_spare = context->flash_info->oob_size / context->sector_num;

	if (context->flash_info->ecc_bit == 1) {
		context->ecc_start = 2;

		switch (context->flash_info->oob_size) {
		case 16:
			chip->ecc.layout = &comip_ecc1_16_layout;
			break;
		case 64:
			chip->ecc.layout = &comip_ecc1_64_layout;
			break;
		default:
			printk(KERN_WARNING "No oob scheme defined for "
			       "oobsize %d\n", context->flash_info->oob_size);
			return -EINVAL;
		}
		context->ecc_bytes = chip->ecc.layout->eccbytes;
	} else if ((context->flash_info->ecc_bit == 4)
				|| (context->flash_info->ecc_bit == 8)) {
		struct nand_ecclayout *ecclayout = &comip_ecc8_layout;
		uint32_t eccbytes;

		context->ecc_start = 2;
		if (context->flash_info->ecc_bit == 8)
			eccbytes = context->sector_num * 13;
		else
			eccbytes = context->sector_num * 52 / 8;

		ecclayout->eccbytes = eccbytes;
		ecclayout->oobfree[0].offset = context->ecc_start + eccbytes;
		ecclayout->oobfree[0].length = context->flash_info->oob_size - (2 + eccbytes);
		for (i = 0; i < eccbytes; i++)
			ecclayout->eccpos[i] = context->ecc_start + i;
		chip->ecc.layout = ecclayout;
		context->ecc_bytes = eccbytes;
	} else {
		printk(KERN_WARNING "No oob scheme defined for "
		       "eccbit %d\n", context->flash_info->ecc_bit);
		return -EINVAL;
	}

	return 0;
}

static int comip_nand_probe(struct platform_device *pdev)
{
	struct comip_nand_platform_data *pdata;
	struct nand_chip *this;
	struct comip_nand_info *info;
	struct mtd_part_parser_data ppdata = {};
	struct mtd_info *mtd;
	struct resource *res;
	struct comip_nfc_context *context;
	int tries = 0;
	int ret = 0;
	int irq;
	int i;

	pdata = pdev->dev.platform_data;
	if (pdata == NULL) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if ((res == NULL) || (irq < 0)) {
		dev_err(&pdev->dev, "no IO memory resource defined\n");
		return -ENODEV;
	}

	context = kzalloc(sizeof(struct comip_nfc_context), GFP_KERNEL);
	if (!context) {
		dev_err(&pdev->dev, "Unable to allocate NAND NFC context structure\n");
		return -ENOMEM;
	}

	context->seq_num = 1;
	context->nop_clk_cnt = 0;
	context->irq = irq;
	context->use_irq = pdata->use_irq;
	context->use_dma = pdata->use_dma;
	context->chip_select = 1;
	context->enable_ecc = 1;

	context->clk = clk_get(NULL, "nfc_clk");
	if (IS_ERR(context->clk)) {
		dev_err(&pdev->dev, "Failed to get nfc clock\n");
		ret = IS_ERR(context->clk);
		goto err_free_context;
	}

	context->clk_rate = clk_get_rate(context->clk);

retry:
	for (i = 0; i < ARRAY_SIZE(comip_flash_info); i++) {
		uint32_t id;

		ret = comip_nfc_init(context, i);
		if (ret)
			continue;
		ret = comip_nfc_read_id(context, &id);
		if (ret)
			continue;
		if ((id & 0xffff) == context->flash_info->chip_id)
			break;
	}

	if (i == ARRAY_SIZE(comip_flash_info)) {
		tries++;
		if (tries <= 20) {
			mdelay(1);
			goto retry;
		} else {
			dev_err(&pdev->dev, "Nand Flash initialize failure!\n");
			ret = -ENXIO;
			goto err_free_clk;
		}
	}

	mtd = kzalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip) +
			sizeof(struct comip_nand_info), GFP_KERNEL);
	if (!mtd) {
		dev_err(&pdev->dev, "Unable to allocate NAND MTD device structure\n");
		ret = -ENOMEM;
		goto err_free_clk;
	}

	/* Get pointer to private data */
	this =
		(struct nand_chip *)((void *)mtd + sizeof(struct mtd_info));
	info =
		(struct comip_nand_info *)((void *)this + sizeof(struct nand_chip));

	mtd->priv = this;
	mtd->owner = THIS_MODULE;
	this->priv = info;
	info->data_buf_len = context->flash_info->page_size +
		context->flash_info->oob_size;
	info->state = STATE_READY;
	info->context = context;

	context->data_buf_len = info->data_buf_len + context->flash_info->oob_size;
	context->data_buf =
		dma_alloc_coherent(&pdev->dev, context->data_buf_len,
				   &context->data_buf_addr, GFP_KERNEL);
	if (!context->data_buf) {
		ret = -ENOMEM;
		goto err_free_mtd;
	}
	context->oob_buf = context->data_buf + info->data_buf_len;
	info->data_buf = context->data_buf;

	if (context->use_irq) {
		init_completion(&(context->completion));
		ret = request_irq(irq, comip_nand_irq, IRQF_DISABLED,
				  "COMIP_NAND", info);
		if (ret)
			goto err_free_buf;

		comip_nfc_clear_irq(context, NFC_INT_NOP);
		comip_nfc_enable_irq(context, NFC_INT_NOP);
	}

	if (context->use_dma) {
		res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
		if (res == NULL) {
			return -ENODEV;
			goto err_free_irq;
		}
		context->rxdma = res->start;

		res = platform_get_resource(pdev, IORESOURCE_DMA, 1);
		if (res == NULL) {
			return -ENODEV;
			goto err_free_irq;
		}
		context->txdma = res->start;

		ret = comip_nand_dma_init(context);
		if (ret)
			goto err_free_irq;
	}

	/* set address of NAND IO lines */
	this->options = ((context->flash_info->flash_width == 16) ? \
		NAND_BUSWIDTH_16 : 0);

	this->waitfunc = comip_nand_waitfunc;
	this->select_chip = comip_nand_select_chip;
	this->dev_ready = comip_nand_dev_ready;
	this->cmdfunc = comip_nand_command;
	this->read_word = comip_nand_read_word;
	this->read_byte = comip_nand_read_byte;
	this->read_buf = comip_nand_read_buf;
	this->write_buf = comip_nand_write_buf;
	this->verify_buf = comip_nand_verify_buf;

	this->ecc.mode = NAND_ECC_HW;
	this->ecc.hwctl = comip_nand_enable_hwecc;
	this->ecc.calculate = comip_nand_calculate_ecc;
	this->ecc.correct = comip_nand_correct_data;
	this->ecc.size = context->flash_info->page_size;
	this->chip_delay = 25;

	/*
	 * Scan to find existence of the device
	 */
	if (nand_scan_ident(mtd, MAX_CHIPS, NULL)) {
		ret = -ENXIO;
		dev_err(&pdev->dev, "No NAND Device found!\n");
		goto err_free_dma;
	}

	mtd->oobsize = context->flash_info->oob_size;
	ret = comip_nand_ecc_init(context, this);
	if (ret) {
		dev_err(&pdev->dev, "Check ecc layout failed\n");
		goto err_free_dma;
	}

	ret = comip_nand_bbm_init(context, this, mtd);
	if (ret) {
		dev_err(&pdev->dev, "Scan bbm failed\n");
		goto err_free_dma;
	}

	/* Second stage of scan to fill MTD data-structures */
	if (nand_scan_tail(mtd)) {
		dev_err(&pdev->dev, "Nand scan failed\n");
		ret = -ENXIO;
		goto err_free_bbm;
	}

	mtd->name = "comip-nand";
	ppdata.of_node = pdev->dev.of_node;
	ret = mtd_device_parse_register(mtd, NULL, &ppdata, NULL, 0);
	if (ret) {
		dev_err(&pdev->dev, "Register mtd device failed\n");
		goto err_probe;
	}

	platform_set_drvdata(pdev, mtd);
	dev_info(&pdev->dev, "COMIP NAND driver registration successful\n");

	return 0;

err_probe:
err_free_bbm:
	free_comip_bbm(context->bbm);
err_free_dma:
	if (context->use_dma)
		comip_nand_dma_uninit(context);
err_free_irq:
	if (context->use_irq) {
		comip_nfc_disable_irq(context, NFC_INT_NOP);
		free_irq(context->irq, info);
	}
err_free_buf:
	dma_free_coherent(&pdev->dev, context->data_buf_len,
			  context->data_buf,
			  context->data_buf_addr);
err_free_mtd:
	kfree(mtd);
err_free_clk:
	clk_put(context->clk);
err_free_context:
	kfree(context);
	return ret;
}

static int comip_nand_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd = (struct mtd_info *)platform_get_drvdata(pdev);
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);
	struct comip_nfc_context *context = info->context;

	platform_set_drvdata(pdev, NULL);
	nand_release(mtd);

	if (context->bbm && context->bbm->uninit) {
		context->bbm->uninit(mtd, context->bbm);
		free_comip_bbm(context->bbm);
	}

	if (context->use_irq) {
		comip_nfc_disable_irq(context, NFC_INT_NOP);
		free_irq(context->irq, info);
	}

	if (context->use_dma) {
		comip_nand_dma_uninit(context);
	}

	dma_free_coherent(&pdev->dev, context->data_buf_len,
			  context->data_buf,
			  context->data_buf_addr);

	clk_put(context->clk);
	kfree(mtd);
	kfree(context);

	return 0;
}

#ifdef CONFIG_PM
static int comip_nand_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mtd_info *mtd = (struct mtd_info *)platform_get_drvdata(pdev);
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);

	if (info->state != STATE_READY) {
		NAND_PRINT1("current state is %d\n", info->state);
		return -EAGAIN;
	}
	info->state = STATE_SUSPENDED;
	return 0;
}

static int comip_nand_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mtd_info *mtd = (struct mtd_info *)platform_get_drvdata(pdev);
	struct comip_nand_info *info = (struct comip_nand_info *)
		(((struct nand_chip *)(mtd->priv))->priv);

	if (info->state != STATE_SUSPENDED)
		NAND_PRINT1("Error State after resume back\n");

	info->state = STATE_READY;

	return 0;
}

static const struct dev_pm_ops comip_nand_pm_ops = {
	.suspend	= comip_nand_suspend,
	.resume		= comip_nand_resume,
};

#endif

static struct platform_driver comip_nand_driver = {
	.driver = {
		.name = "comip-nand",
#ifdef CONFIG_PM
		.pm = &comip_nand_pm_ops,
#endif
	},
	.probe = comip_nand_probe,
	.remove = comip_nand_remove,
};

static void __exit comip_nand_exit(void)
{
	platform_driver_unregister(&comip_nand_driver);
}

static int __init comip_nand_init(void)
{
	return platform_driver_register(&comip_nand_driver);
}

device_initcall_sync(comip_nand_init);
module_exit(comip_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zouqiao (zouqiao@leadcoretech.com)");
MODULE_DESCRIPTION("Glue logic layer for NAND flash on comip NFC");

