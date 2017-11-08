/* 
 **
 ** Use of source code is subject to the terms of the LeadCore license agreement under
 ** which you licensed source code. If you did not accept the terms of the license agreement,
 ** you are not authorized to use source code. For the terms of the license, please see the
 ** license agreement between you and LeadCore.
 **
 ** Copyright (c) 2013-2020  LeadCoreTech Corp.
 **
 **  PURPOSE: nand bad block management
 **
 **  CHANGE HISTORY:
 **
 **  Version		Date		Author		Description
 **  1.0.0		2013-06-05	zouqiao		created
 **
 */
	 
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <plat/comip-bbm.h>

#define NAND_RELOC_HEADER				(0x524e)
#define NAND_MAX_BBT_SLOTS				(24)
#define NAND_MAX_RELOC_ENTRY				(127)
#define NAND_RELOC_ENTRY(maxblocks)			(((maxblocks) * 2) / 100)

static uint8_t scan_ff_pattern[] = {0xff, 0xff};
static uint8_t scan_main_bbt_pattern[] = {'B', 'b', 't', '0'};
static uint8_t scan_mirror_bbt_pattern[] = {'1', 't', 'b', 'B'};

static struct nand_bbt_descr comip_bbt_default = {
	.options = 0,
	.offs = 0,
	.len = 2,
	.pattern = scan_ff_pattern,
};

static struct nand_bbt_descr comip_bbt_main = {
	.options = NAND_BBT_ABSPAGE | NAND_BBT_CREATE | NAND_BBT_WRITE
			| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.veroffs = 6,
	.maxblocks = 2,
	.offs = 2,
	.len = 4,
	.pattern = scan_main_bbt_pattern,
};

static struct nand_bbt_descr comip_bbt_mirror = {
	.options = NAND_BBT_ABSPAGE | NAND_BBT_CREATE | NAND_BBT_WRITE
			| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.veroffs = 6,
	.maxblocks = 2,
	.offs = 2,
	.len = 4,
	.pattern = scan_mirror_bbt_pattern,
};

static int comip_nand_scan_bbt(struct mtd_info *mtd)
{
	nand_scan_bbt(mtd, &comip_bbt_default);

	return 0;
}

static int comip_nand_init_bbt(struct mtd_info *mtd, struct comip_bbm *bbm)
{
	struct nand_chip *this = mtd->priv;
	uint16_t blocks;

	this->bbt_td = &comip_bbt_main;
	this->bbt_md = &comip_bbt_mirror;

	blocks = (this->chipsize >> this->phys_erase_shift) - bbm->max_reloc_entry;

	this->scan_bbt = comip_nand_scan_bbt;
	this->bbt_td->pages[0] =
		(blocks - 1) << (this->phys_erase_shift - this->page_shift);
	this->bbt_md->pages[0] =
		(blocks - this->bbt_td->maxblocks - 1) << (this->phys_erase_shift - this->page_shift);

	return 0;
}

static void comip_dump_reloc_table(struct comip_bbm *bbm)
{
	int i;
	int maxslot = 1 << (bbm->erase_shift - bbm->page_shift);

	printk("relocation table at page:%d, length:%d\n",
		maxslot - bbm->current_slot - 1, bbm->table->total);
	for (i = 0; i < bbm->table->total; i++) {
		printk("block: %d is relocated to block: %d\n",
				bbm->reloc[i].from, bbm->reloc[i].to);
	}
}

static int comip_nand_cmd(struct mtd_info *mtd, uint32_t cmd,
				uint32_t addr, uint8_t* buf)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	struct erase_info instr;
	int retlen;
	int ret = 0;

	switch (cmd) {
		case BBM_READ:
			ret = mtd->_read(mtd, addr, mtd->writesize, &retlen, buf);
			break;
		case BBM_WRITE:
			ret = mtd->_write(mtd, addr, mtd->writesize, &retlen, buf);
			break;
		case BBM_ERASE:
			ret = chip->block_bad(mtd, addr, 0);
			if (!ret) {
				instr.mtd = mtd;
				instr.addr = addr;
				instr.len = mtd->erasesize;
				ret = mtd->_erase(mtd, &instr);
			}
			break;
		default:
			break;
	}

	return ret;
}

/* add the relocation entry into the relocation table
 * If the relocated block is bad, an new entry will be added into the
 * bottom of the relocation table.
 */
static int comip_update_reloc_tb(struct mtd_info *mtd,
					struct comip_bbm *bbm, int block)
{
	struct reloc_table *table = bbm->table;
	struct reloc_item *item = bbm->reloc;
	int max_blocks;
	int reloc_num;
	int reloc_block;
	int ret = 0;

	if (bbm->table_init == 0) {
		printk(KERN_ERR "Error: the initial relocation \
				table can't be read\n");
		memset(table, 0xFF, sizeof(struct reloc_table));
		table->header = NAND_RELOC_HEADER;
		table->total = 0;
		bbm->table_init = 1;
	}

	max_blocks = (mtd->size >> bbm->erase_shift);
	reloc_num = table->total;
	if (reloc_num == 0) {
		reloc_block = max_blocks;
	} else if (reloc_num < bbm->max_reloc_entry) {
		do {
			/* Last Entry left off. */
			reloc_block = item[reloc_num - 1].to;
			reloc_num--;
		} while((reloc_block > max_blocks) && (reloc_num > 0));

		if ((reloc_block > max_blocks) && (reloc_num == 0))
			reloc_block = max_blocks;
	} else {
		   printk(KERN_ERR "Relocation table exceed max number, \
				   cannot mark block 0x%x as bad block\n",
				   block);
		   return -ENOSPC;
	}

	do {
		reloc_block--;
		if (reloc_block < (max_blocks - bbm->max_reloc_entry))
			return -ENOSPC;

		ret = bbm->nand_cmd(mtd, BBM_ERASE,
			(reloc_block << bbm->erase_shift), NULL);
		if (ret)
			continue;
	} while (ret);

	/* Create the relocated block information in the table */
	printk(KERN_DEBUG "block: %d is relocated to block: %d\n", block, reloc_block);
	item[table->total].from = block;
	item[table->total].to = reloc_block;
	table->total++;
	return 0;
}

/* Write the relocation table back to device, if there's room. */
static int comip_sync_reloc_tb(struct mtd_info *mtd,
				struct comip_bbm *bbm, int *idx)
{
	int start_page;

	if (*idx >= NAND_MAX_BBT_SLOTS) {
		printk(KERN_ERR "Can't write relocation table to device \
				any more.\n");
		return -1;
	}

	if (*idx < 0) {
		printk(KERN_ERR "Wrong Slot is specified.\n");
		return -1;
	}

	/* write to device */
	start_page = (1 << (bbm->erase_shift - bbm->page_shift));
	start_page = start_page - 2 - *idx;

	printk(KERN_DEBUG "DUMP relocation table before write. \
			page:0x%x\n", start_page);

	bbm->nand_cmd(mtd, BBM_WRITE, (start_page << bbm->page_shift),
			bbm->data_buf);

	/* write to idx */
	(*idx)++;
	return 0;
}

static int comip_scan_reloc_tb(struct mtd_info *mtd, struct comip_bbm *bbm)
{
	struct reloc_table *table = bbm->table;
	int page, maxslot, valid = 0;
	int ret;

	/* There're several relocation tables in the first block.
	 * When new bad blocks are found, a new relocation table will
	 * be generated and written back to the first block. But the
	 * original relocation table won't be erased. Even if the new
	 * relocation table is written wrong, system can still find an
	 * old one.
	 * One page contains one slot.
	 */
	maxslot = 1 << (bbm->erase_shift - bbm->page_shift);
	page = maxslot - NAND_MAX_BBT_SLOTS;
	for (; page < maxslot; page++) {
		memset(bbm->data_buf, 0,
				mtd->writesize + mtd->oobsize);
		ret = bbm->nand_cmd(mtd, BBM_READ, (page << bbm->page_shift),
					bbm->data_buf);
		if (ret == 0) {
			if (table->header != NAND_RELOC_HEADER) {
				continue;
			} else {
				bbm->current_slot = maxslot - page - 1;
				valid = 1;
				break;
			}
		}
	}

	if (valid) {
		bbm->table_init = 1;
		comip_dump_reloc_table(bbm);
	} else {
		/* There should be a valid relocation table slot at least. */
		printk(KERN_ERR "NO VALID relocation table can be \
				recognized\n");
		printk(KERN_ERR "CAUTION: It may cause unpredicated error\n");
		printk(KERN_ERR "Please re-initialize the NAND flash.\n");
		memset((unsigned char *)bbm->table, 0,
				sizeof(struct reloc_table));
		bbm->table_init = 0;
		return -EINVAL;
	}
	return 0;
}

static int comip_init_reloc_tb(struct mtd_info *mtd, struct comip_bbm *bbm)
{
	int size = mtd->writesize + mtd->oobsize;
	int slots = mtd->size >> bbm->erase_shift;

	bbm->max_reloc_entry = NAND_RELOC_ENTRY(slots);
	if (bbm->max_reloc_entry > NAND_MAX_RELOC_ENTRY)
		bbm->max_reloc_entry = NAND_MAX_RELOC_ENTRY;

	bbm->table_init = 0;

	bbm->data_buf = kzalloc(size, GFP_KERNEL);
	if (!bbm->data_buf)
		return -ENOMEM;

	bbm->table = (struct reloc_table *)bbm->data_buf;
	memset(bbm->table, 0x0, sizeof(struct reloc_table));

	bbm->reloc = (struct reloc_item *)((uint8_t *)bbm->data_buf +
			sizeof(struct reloc_table));
	memset(bbm->reloc, 0x0,
			sizeof(struct reloc_item) * bbm->max_reloc_entry);

	comip_nand_init_bbt(mtd, bbm);

	return comip_scan_reloc_tb(mtd, bbm);
}

static int comip_uninit_reloc_tb(struct mtd_info *mtd, struct comip_bbm *bbm)
{
	kfree(bbm->data_buf);
	return 0;
}

/* Find the relocated block of the bad one.
 * If it's a good block, return 0. Otherwise, return a relocated one.
 * idx points to the next relocation entry
 * If the relocated block is bad, an new entry will be added into the
 * bottom of the relocation table.
 */
static int comip_search_reloc_tb(struct mtd_info *mtd, struct comip_bbm *bbm,
					unsigned int block)
{
	struct reloc_table *table = bbm->table;
	struct reloc_item *item = bbm->reloc;
	int i, reloc_block;

	if ((block <= 0) ||
	(block >= ((mtd->size >> bbm->erase_shift) - bbm->max_reloc_entry)) ||
	(bbm->table_init == 0) || (table->total == 0))
		return block;

	if (table->total > bbm->max_reloc_entry)
		table->total = bbm->max_reloc_entry;

	/* If can't find reloc tb entry for block, return block */
	reloc_block = block;
	for (i = 0; i < table->total; i++) {
		if (item[i].from == reloc_block) {
			reloc_block = item[i].to;
			/* Start over. */
			i = 0;
			continue;
		}
	}

	return reloc_block;
}

static int comip_mark_reloc_tb(struct mtd_info *mtd, struct comip_bbm *bbm,
				unsigned int block)
{
	int ret = 0;

	ret = comip_update_reloc_tb(mtd, bbm, block);
	if (ret)
		return ret;

	ret = comip_sync_reloc_tb(mtd, bbm, &(bbm->current_slot));
	return ret;
}

struct comip_bbm* alloc_comip_bbm(void)
{
	/* FIXME: We don't want to add module_init entry
	 * here to avoid dependency issue.
	 */
	struct comip_bbm *bbm;

	bbm = kzalloc(sizeof(struct comip_bbm), GFP_KERNEL);
	if (!bbm)
		return NULL;

	bbm->init = comip_init_reloc_tb;
	bbm->uninit = comip_uninit_reloc_tb;
	bbm->search = comip_search_reloc_tb;
	bbm->markbad = comip_mark_reloc_tb;
	bbm->nand_cmd = comip_nand_cmd;

	return bbm;
}
EXPORT_SYMBOL(alloc_comip_bbm);

void free_comip_bbm(struct comip_bbm *bbm)
{
	if (bbm) {
		kfree(bbm);
		bbm = NULL;
	}
}
EXPORT_SYMBOL(free_comip_bbm);
