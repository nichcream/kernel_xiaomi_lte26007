#ifndef __ASM_ARCH_PLAT_NAND_H
#define __ASM_ARCH_PLAT_NAND_H

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <plat/hardware.h>
#include <plat/dmas.h>
#include <linux/delay.h>

static inline void comip_nfc_write_reg32(uint32_t value, int paddr)
{
	writel(value, io_p2v(paddr));
	dsb();
}

static inline unsigned int comip_nfc_read_reg32(int paddr)
{
	return readl(io_p2v(paddr));
}

static inline void comip_nfc_write_reg16(uint16_t value, int paddr)
{
	writew(value, io_p2v(paddr));
	dsb();
}

static inline uint16_t comip_nfc_read_reg16(int paddr)
{
	return readw(io_p2v(paddr));
}

static inline void comip_nfc_write_reg8(uint8_t value, int paddr)
{
	writeb(value, io_p2v(paddr));
	dsb();
}

static inline uint8_t comip_nfc_read_reg8(int paddr)
{
	return readb(io_p2v(paddr));
}

/*
 * The Data Flash Controller Flash timing structure
 * user should use value at end of each row of following member
 * bracketed.
 */
struct comip_nfc_flash_timing {
	uint8_t tWP;		// the nWE pulse width, ns
	uint8_t tWC;		// write cycle time, ns
	uint8_t tCLS;		// time of control signal to WE setup, ns
	uint8_t tCLH;		// time of control signal hold, ns

	uint8_t tRP;		// the nRE pulse width, ns
	uint8_t tRC;		// read cycle time, ns
	uint8_t tCS;		// CS setup, ns
	uint8_t tCH;		// CS hold, ns

	uint32_t nWB;		// nWE high to busy, us
	uint32_t nFEAT;		// the poll time of feature operation, us
	uint32_t nREAD;		// the poll time of read, us
	uint32_t nPROG;		// the poll time of program, us
	uint32_t nERASE;	// the poll time of erase, us
	uint32_t nRESET;	// the poll time of reset, us
};

/*
 * The nand chip command set.
 */
struct comip_nfc_flash_cmd {
	uint8_t read_status;
	uint8_t read1;
	uint8_t read2;
	uint8_t readx;		// for some special nand which divides page into 2 part
	uint8_t reads;		// for some special nand which reads spare area in a page
	uint8_t write1;
	uint8_t write2;
	uint8_t erase1;
	uint8_t erase2;
	uint8_t random_in;
	uint8_t random_out1;
	uint8_t random_out2;
};

/*
 * The Data Flash Controller Flash specification structure
 * user should use value at end of each row of following member
 * bracketed.
 */
struct comip_nfc_flash_info {
	struct comip_nfc_flash_timing timing;	/* NAND Flash timing */
	struct comip_nfc_flash_cmd cmd;	/* NAND Flash command */

	uint32_t page_per_block;	/* Pages per block*/
	uint32_t row;		/* how many byte be needed to input page num */
	uint32_t col;		/* how many byte be needed to input page offset */
	uint32_t read_id_bytes;	/* returned ID bytes */
	uint32_t page_size;	/* Page size in bytes*/
	uint32_t oob_size;	/* OOB size */
	uint32_t flash_width;	/* Width of Flash memory*/
	uint32_t ecc_bit;	/* ECC bits */
	uint32_t num_blocks;	/* Number of physical blocks in Flash */
	uint32_t chip_id;
};

struct comip_nand_info {
	unsigned int state;
	struct comip_nfc_context *context;
	uint8_t* data_buf;
	unsigned int data_buf_len;

	/* relate to the command */
	unsigned int cmd;
	unsigned int cur_cmd;
	unsigned int addr;
	unsigned int column;
	int retcode;
	unsigned int buf_count;
};

/* NFC command type */
enum {
	NFC_CMD_READ = 0x00000000,
	NFC_CMD_PROGRAM = 0x00200000,
	NFC_CMD_ERASE = 0x00400000,
	NFC_CMD_READ_ID = 0x00600000,
	NFC_CMD_STATUS_READ = 0x00800000,
	NFC_CMD_RESET = 0x00a00000
};

#define WRITE_CMD(code, flag, cmd)		\
	comip_nfc_write_reg16((code | (flag << 8) | (cmd)), NFC_TX_BUF)
#define WRITE_DATA(data)				\
	comip_nfc_write_reg16(data, NFC_TX_BUF)

#define NOP_CODE					0x8000
#define CMD_CODE					0x1000
#define ADD_CODE					0x2000
#define READ_CODE					0x5000
#define WRITE_CODE					0x6000
#define STAT_CODE					0x7000
#define RST_CODE					0xF000

#define NAND_OTHER_TIMEOUT				(10) /* us. */
#define NAND_STATUS_TIMEOUT				(1000) /* us. */
#define NAND_OPERATION_TIMEOUT				(2 * HZ / 10)

#define ERR_NONE					0x0
#define ERR_DMABUSERR					(-0x01)
#define ERR_SENDCMD 					(-0x02)
#define ERR_DBERR					(-0x03)
#define ERR_BBERR					(-0x04)
#define ERR_BUSY					(-0x05)
#define ERR_RESET					(-0x06)
#define ERR_ERASE					(-0x07)
#define ERR_PROG					(-0x08)
#define ERR_ECC_CORRECTABLE				(-0x09)
#define ERR_ECC_UNCORRECTABLE				(-0x0A)
#define ERR_PARAM					(-0x0B)

#define STATE_CMD_SEND					0x1
#define STATE_CMD_HANDLE				0x2
#define STATE_DMA_TRANSFER				0x3
#define STATE_DMA_DONE					0x4
#define STATE_READY 					0x5
#define STATE_SUSPENDED 				0x6
#define STATE_DATA_TRANSFER 				0x7

#define MAX_CHIPS					1

/* Some common command for all nand chip. */
#define CMD_RESET					0xFF
#define CMD_READ_ID 					0x90

#define SECTOR_SIZE 					512
#define ECC_POSITION_MASK				0x0700

struct comip_nand_platform_data {
	int use_irq;
	int use_dma;
};

extern void comip_set_nand_info(struct comip_nand_platform_data *info);
#endif /* __ASM_ARCH_PLAT_NAND_H */

