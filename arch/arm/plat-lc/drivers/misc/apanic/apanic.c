/* drivers/misc/apanic.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/mtd/mtd.h>
#include <linux/notifier.h>
#include <linux/mtd/mtd.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/preempt.h>
#include <linux/fb.h>
#include <linux/kmsg_dump.h>

#include <plat/comip-pmic.h>
#include <plat/hardware.h>

#include "mmc.h"
#include "comip_mmc.h"

#define CONFIG_APANIC_PLABEL "panic"
#define LC181X_PANIC_PARTITION_START	(0x600000)
#define LC186X_PANIC_PARTITION_START	(0xA00000)

struct panic_header {
	u32 magic;
#define PANIC_MAGIC 0xdeadf00d

	u32 version;
#define PHDR_VERSION   0x01

	u32 console_offset;
	u32 console_length;

	u32 threads_offset;
	u32 threads_length;
};

struct apanic_data {
	void			*flash;
	u32			flash_block_len;
	struct panic_header	curr;
	void			*bounce;
	void			*kmsg_buffer;
	struct proc_dir_entry	*apanic_console;
	struct proc_dir_entry	*apanic_threads;
#if !defined(CONFIG_NAND_COMIP)
	struct proc_dir_entry 	*apanic_mmc;
#endif
	unsigned long start;
	unsigned long end;
};

static struct apanic_data drv_ctx;
static struct work_struct proc_removal_work;
static DEFINE_MUTEX(drv_mutex);

#define APANIC_INVALID_OFFSET 0xFFFFFFFF

static void apanic_proc_init(void);

#if !defined(CONFIG_NAND_COMIP)

#define APANIC_PART_NAME "/dev/block/platform/comip-mmc.1/by-name/panic"
extern int cmdline_get_partition_byname(char *p_name, unsigned long *from,  unsigned long *to);

int board_mmc_init(void)
{
	printk("board_mmc_init called\n");

	comip_mmc_init(1, 4, 0, 0);

	return 0;
}

static int apanic_mmc_init(void)
{
	struct apanic_data *ctx = &drv_ctx;
	struct mmc *mmc;

	printk("apanic mmc_init ...\n");

	mmc_initialize();

	mmc = find_mmc_device(0);
	if (!mmc) {
		printk("no mmc device at slot 0\n");
		return -1;
	}

	mmc_init(mmc);

	if(0 == mmc->read_bl_len){
		printk("mmc->read_bl_len == 0\n");
		return -1;
	}

	if(0 == mmc->block_dev.lba){
		printk("mmc->block_dev.lba == 0\n");
		mmc->block_dev.lba = 0xEE000;
		return -1;
	}

	ctx->flash = mmc;
	ctx->flash_block_len = mmc->write_bl_len;

	if (cmdline_get_partition_byname(CONFIG_APANIC_PLABEL, &(ctx->start), &(ctx->end))) {
		printk(KERN_EMERG"apanic get mmc address error!\n");
		ctx->start = 0;
		ctx->end = 0;
	}
	printk(KERN_INFO"apanic start mmc address is %lu to %lu\n", ctx->start, ctx->end);

	return 0;
}

static int apanic_mmc_proc_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct apanic_data *ctx = &drv_ctx;
	struct file *apanic_file = NULL;
	mm_segment_t old_fs;
	size_t len = 0;

	apanic_file = filp_open(APANIC_PART_NAME, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(apanic_file)) {
		printk("%s open panic file failed error =%ld .\n", __func__, PTR_ERR(apanic_file));
		return PTR_ERR(apanic_file);
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		len = apanic_file->f_op->read(apanic_file, ctx->bounce, 512, &apanic_file->f_pos);
		set_fs(old_fs);
		filp_close(apanic_file, NULL);
	}

	if (len != 512) {
		printk(KERN_WARNING
		       "apanic: %s read hdr error\n", __func__);
	}

	apanic_proc_init();

	return count;
}

static const struct file_operations apanic_mmc_operations = {
        .owner = THIS_MODULE,
        .read  = NULL,
        .write = apanic_mmc_proc_write,
};

static void apanic_mmc_create_proc(void)
{
	struct apanic_data *ctx = &drv_ctx;

	ctx->apanic_mmc = proc_create_data("apanic_mmc", S_IFREG | S_IRUGO, NULL,
		&apanic_mmc_operations, (void *)1);
	
	if (!ctx->apanic_mmc)
			printk(KERN_ERR "%s: failed creating procfile\n",
			       __func__);
	else
		proc_set_size(ctx->apanic_mmc, 1);
}

static void apanic_eraseflash(void)
{
	struct apanic_data *ctx = &drv_ctx;
	struct file *file = NULL;
	unsigned int len;
	mm_segment_t old_fs;

	file = filp_open(APANIC_PART_NAME, O_RDWR, 0);
	if (IS_ERR_OR_NULL(file)) {

		printk("%s open panic file failed error =%ld .\n", __func__, PTR_ERR(file));

	} else {

		memset(ctx->bounce, 0, 512);
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		len = file->f_op->write(file, ctx->bounce, 512, &file->f_pos);
		set_fs(old_fs);
		filp_close(file, NULL);
	}	
}

static int apanic_writeflashpage(loff_t to,
				 const u_char *buf)
{
	struct apanic_data *ctx = &drv_ctx;
	struct mmc *mmc = (struct mmc *)ctx->flash;
	int rc;
	unsigned long wlen;

	if (!ctx->start) {
		printk(KERN_EMERG"apanic could not get mmc start address\n");
		return 0;
	}

	wlen = to + ctx->start;
	if (wlen > ctx->end)
		return 0;

	wlen = wlen >> 9;

	if (to == APANIC_INVALID_OFFSET) {
		printk(KERN_EMERG "apanic: write to invalid address\n");
		return 0;
	}

	rc = mmc->block_dev.block_write(0, wlen, 1, buf);
	if (rc != 1) {
		printk(KERN_EMERG
		       "%s: Error writing data to mmc (%d)\n",
		       __func__, rc);
		return 0;
	}

	return mmc->write_bl_len;
}

static int apanic_readflashpage(off_t offset, char *buffer,
				 int count)
{
	struct file *file = NULL;
	mm_segment_t old_fs;
	size_t len;

	file = filp_open(APANIC_PART_NAME, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(file)) {
		pr_err("%s open panic file failed error = %ld .\n", __func__, PTR_ERR(file));
		return -EINVAL;
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		file->f_op->llseek(file, offset, 0);
		len = file->f_op->read(file, buffer, count, &file->f_pos);
		set_fs(old_fs);
		filp_close(file, NULL);
	}

	return len;
}

#elif defined(CONFIG_NAND_COMIP)

static unsigned int *apanic_bbt;
static unsigned int apanic_erase_blocks;
static unsigned int apanic_good_blocks;

static void set_bb(unsigned int block, unsigned int *bbt)
{
	unsigned int flag = 1;

	BUG_ON(block >= apanic_erase_blocks);

	flag = flag << (block%32);
	apanic_bbt[block/32] |= flag;
	apanic_good_blocks--;
}

static unsigned int get_bb(unsigned int block, unsigned int *bbt)
{
	unsigned int flag;

	BUG_ON(block >= apanic_erase_blocks);

	flag = 1 << (block%32);
	return apanic_bbt[block/32] & flag;
}

static void alloc_bbt(struct mtd_info *mtd, unsigned int *bbt)
{
	int bbt_size;
	apanic_erase_blocks = (mtd->size)>>(mtd->erasesize_shift);
	bbt_size = (apanic_erase_blocks+32)/32;

	apanic_bbt = kmalloc(bbt_size*4, GFP_KERNEL);
	memset(apanic_bbt, 0, bbt_size*4);
	apanic_good_blocks = apanic_erase_blocks;
}
static void scan_bbt(struct mtd_info *mtd, unsigned int *bbt)
{
	int i;

	for (i = 0; i < apanic_erase_blocks; i++) {
		if (mtd_block_isbad(mtd, i*mtd->erasesize))
			set_bb(i, apanic_bbt);
	}
}

static unsigned int phy_offset(struct mtd_info *mtd, unsigned int offset)
{
	unsigned int logic_block = offset>>(mtd->erasesize_shift);
	unsigned int phy_block;
	unsigned good_block = 0;

	for (phy_block = 0; phy_block < apanic_erase_blocks; phy_block++) {
		if (!get_bb(phy_block, apanic_bbt))
			good_block++;
		if (good_block == (logic_block + 1))
			break;
	}

	if (good_block != (logic_block + 1))
		return APANIC_INVALID_OFFSET;

	return offset + ((phy_block-logic_block)<<mtd->erasesize_shift);
}

static void mtd_panic_notify_add(struct mtd_info *mtd)
{
	struct apanic_data *ctx = &drv_ctx;
	size_t len;
	int rc;

	if (strcmp(mtd->name, CONFIG_APANIC_PLABEL))
		return;

	ctx->flash = mtd;
	ctx->flash_block_len = mtd->writesize;

	alloc_bbt(mtd, apanic_bbt);
	scan_bbt(mtd, apanic_bbt);

	if (apanic_good_blocks == 0) {
		printk(KERN_ERR "apanic: no any good blocks?!\n");
		goto out_err;
	}

	rc = mtd_read(mtd, phy_offset(mtd, 0), mtd->writesize,
			&len, ctx->bounce);
	if (rc && rc == -EBADMSG) {
		printk(KERN_WARNING
		       "apanic: Bad ECC on block 0 (ignored)\n");
	} else if (rc && rc != -EUCLEAN) {
		printk(KERN_ERR "apanic: Error reading block 0 (%d)\n", rc);
		goto out_err;
	}

	if (len != mtd->writesize) {
		printk(KERN_ERR "apanic: Bad read size (%d)\n", rc);
		goto out_err;
	}

	printk(KERN_INFO "apanic: Bound to mtd partition '%s'\n", mtd->name);

	apanic_proc_init();

	return;
out_err:
	ctx->flash = NULL;
	ctx->flash_block_len = 0;
}

static void mtd_panic_notify_remove(struct mtd_info *mtd)
{
	struct apanic_data *ctx = &drv_ctx;
	if ((void *)mtd == ctx->flash) {
		ctx->flash = NULL;
		printk(KERN_INFO "apanic: Unbound from %s\n", mtd->name);
	}
}

static struct mtd_notifier mtd_panic_notifier = {
	.add	= mtd_panic_notify_add,
	.remove	= mtd_panic_notify_remove,
};

static void apanic_erase_callback(struct erase_info *done)
{
	wait_queue_head_t *wait_q = (wait_queue_head_t *) done->priv;
	wake_up(wait_q);
}

static void apanic_eraseflash(void)
{
	struct apanic_data *ctx = &drv_ctx;
	struct mtd_info *mtd = (struct mtd_info *)ctx->flash;
	struct erase_info erase;
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wait_q;
	int rc, i;

	init_waitqueue_head(&wait_q);
	erase.mtd = mtd;
	erase.callback = apanic_erase_callback;
	erase.len = mtd->erasesize;
	erase.priv = (u_long)&wait_q;
	for (i = 0; i < mtd->size; i += mtd->erasesize) {
		erase.addr = i;
		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&wait_q, &wait);

		if (get_bb(erase.addr >> mtd->erasesize_shift, apanic_bbt)) {
			printk(KERN_WARNING
			       "apanic: Skipping erase of bad "
			       "block @%llx\n", erase.addr);
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&wait_q, &wait);
			continue;
		}

		rc = mtd_erase(mtd, &erase);
		if (rc) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&wait_q, &wait);
			printk(KERN_ERR
			       "apanic: Erase of 0x%llx, 0x%llx failed\n",
			       (unsigned long long) erase.addr,
			       (unsigned long long) erase.len);
			if (rc == -EIO) {
				if (mtd_block_markbad(mtd, erase.addr)) {
					printk(KERN_ERR
					       "apanic: Err marking blk bad\n");
					goto out;
				}
				printk(KERN_INFO
				       "apanic: Marked a bad block"
				       " @%llx\n", erase.addr);
				set_bb(erase.addr >> mtd->erasesize_shift,
					apanic_bbt);
				continue;
			}
			goto out;
		}
		schedule();
		remove_wait_queue(&wait_q, &wait);
	}
	printk(KERN_DEBUG "apanic: %s partition erased\n",
	       CONFIG_APANIC_PLABEL);
out:
	return;
}

static int apanic_writeflashpage(loff_t to, const u_char *buf)
{
	struct apanic_data *ctx = &drv_ctx;
	struct mtd_info *mtd = (struct mtd_info *)ctx->flash;
	int rc;
	size_t wlen;
	int panic = in_interrupt() | in_atomic();

	if (panic && !mtd->_panic_write) {
		printk(KERN_EMERG "%s: No panic_write available\n", __func__);
		return 0;
	} else if (!panic && !mtd->_write) {
		printk(KERN_EMERG "%s: No write available\n", __func__);
		return 0;
	}

	to = phy_offset(mtd, to);
	if (to == APANIC_INVALID_OFFSET) {
		printk(KERN_EMERG "apanic: write to invalid address\n");
		return 0;
	}

	if (panic)
		rc = mtd_panic_write(mtd, to, mtd->writesize, &wlen, buf);
	else
		rc = mtd_write(mtd, to, mtd->writesize, &wlen, buf);

	if (rc) {
		printk(KERN_EMERG
		       "%s: Error writing data to flash (%d)\n",
		       __func__, rc);
		return rc;
	}

	return wlen;
}

static int apanic_readflashpage(off_t offset, char *buffer,
				 int count)
{
	struct apanic_data *ctx = &drv_ctx;
	struct mtd_info *mtd = (struct mtd_info *)ctx->flash;
	unsigned int page_no;
	off_t page_offset;
	size_t len;
	int rc;

	/* We only support reading a maximum of a flash page */
	if (count > mtd->writesize)
		count = mtd->writesize;

	page_no = (offset) / mtd->writesize;
	page_offset = (offset) % mtd->writesize;

	if (phy_offset(mtd, (page_no * mtd->writesize))
		== APANIC_INVALID_OFFSET) {
		pr_err("apanic: reading an invalid address\n");
		mutex_unlock(&drv_mutex);
		return -EINVAL;
	}
	rc = mtd_read(mtd,
		phy_offset(mtd, (page_no * mtd->writesize)),
		mtd->writesize,
		&len, ctx->bounce);

	if (page_offset)
		count -= page_offset;
	memcpy(buffer, ctx->bounce + page_offset, count);

	return len;
}

#endif

static int apanic_proc_read(struct file *file, char __user *buffer,
                        size_t count, loff_t *ppos)
{
	struct apanic_data *ctx = &drv_ctx;
	void *dat = PDE_DATA(file_inode(file));
	size_t file_length;
	off_t file_offset;
	char * buff_temp = NULL;
	size_t ret = -EFAULT;
	int len = -EINVAL;
	int cnt;

	buff_temp = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if(!buff_temp)
		goto out_free;

	if (!count)
		goto out_free;

	mutex_lock(&drv_mutex);

	switch ((int) dat) {
	case 1:	/* apanic_console */
		file_length = ctx->curr.console_length;
		file_offset = ctx->curr.console_offset;
		break;
	case 2:	/* apanic_threads */
		file_length = ctx->curr.threads_length;
		file_offset = ctx->curr.threads_offset;
		break;
	default:
		pr_err("Bad dat (%d)\n", (int) dat);
		len = -EINVAL;
		goto out;
	}

	if((int)*ppos >= file_length) {
		len = 0;
		goto out;
	}

	cnt = min(file_length - (size_t)*ppos, min((size_t)count, (size_t)PAGE_SIZE));
	len = apanic_readflashpage(file_offset + (int)*ppos, buff_temp,cnt);
	if (len < 0) {
		len = -EIO;
		goto out;
	}

	ret = copy_to_user(buffer, buff_temp, len);
	len = len - ret;
	*ppos += len;

out:
	mutex_unlock(&drv_mutex);
out_free:
	kfree(buff_temp);

	return len;
}

static void apanic_remove_proc_work(struct work_struct *work)
{
	struct apanic_data *ctx = &drv_ctx;

	mutex_lock(&drv_mutex);
	apanic_eraseflash();
	memset(&ctx->curr, 0, sizeof(struct panic_header));
	if (ctx->apanic_console) {
		remove_proc_entry("apanic_console", NULL);
		ctx->apanic_console = NULL;
	}
	if (ctx->apanic_threads) {
		remove_proc_entry("apanic_threads", NULL);
		ctx->apanic_threads = NULL;
	}
	mutex_unlock(&drv_mutex);
}

static int apanic_proc_write(struct file *file, const char __user *buffer,
                        size_t count, loff_t *ppos)
{
	schedule_work(&proc_removal_work);
	return count;
}


static const struct file_operations apanic_console_operations = {
        .owner = THIS_MODULE,
        .read  = apanic_proc_read,
        .write = apanic_proc_write,
};

static void apanic_proc_init(void)
{
	struct apanic_data *ctx = &drv_ctx;
	struct panic_header *hdr = ctx->bounce;
	int proc_entry_created = 0;

	if (hdr->magic == 0) {
		printk(KERN_INFO "apanic: magic is zero \n");
		return;
	}

	if (hdr->magic != PANIC_MAGIC) {
		printk(KERN_INFO "apanic: No panic data available\n");
		apanic_eraseflash();
		return;
	}

	if (hdr->version != PHDR_VERSION) {
		printk(KERN_INFO "apanic: Version mismatch (%d != %d)\n",
		       hdr->version, PHDR_VERSION);
		apanic_eraseflash();
		return;
	}

	memcpy(&ctx->curr, hdr, sizeof(struct panic_header));

	printk(KERN_INFO "apanic: c(%u, %u) t(%u, %u)\n",
	       hdr->console_offset, hdr->console_length,
	       hdr->threads_offset, hdr->threads_length);

	if (hdr->console_length) {
		ctx->apanic_console = proc_create_data("apanic_console", S_IFREG | S_IRUGO, NULL,
			&apanic_console_operations, (void *)1);
		if (!ctx->apanic_console)
			printk(KERN_ERR "%s: failed creating procfile\n",
			       __func__);
		else {
			proc_set_size(ctx->apanic_console, hdr->console_length);
			proc_entry_created = 1;
		}
	}

	if (hdr->threads_length) {
		ctx->apanic_threads = proc_create_data("apanic_threads", S_IFREG | S_IRUGO, NULL,
			&apanic_console_operations, (void *)2);

		if (!ctx->apanic_threads)
			printk(KERN_ERR "%s: failed creating procfile\n",
			       __func__);
		else {
			proc_set_size(ctx->apanic_threads, hdr->threads_length);
			proc_entry_created = 1;
		}
	}

	if (!proc_entry_created)
		apanic_eraseflash();
}

static int in_panic = 0;

int syslog_print_all(char __user *buf, int size, bool clear);

/*
 * Writes the contents of the console to the specified offset in flash.
 * Returns number of bytes written
 */
static int apanic_write_console(unsigned int off)
{
	struct apanic_data *ctx = &drv_ctx;
	int saved_oip;
	int idx = 0;
	int rc, rc2, end;
	unsigned int last_chunk = 0;
	unsigned int remain = 0, dump_len;
	size_t len = 0;
	struct kmsg_dumper apanic_dumper;

	kmsg_dump_rewind_nolock(&apanic_dumper);
	apanic_dumper.active = true;

	while (!last_chunk) {
		saved_oip = oops_in_progress;
		oops_in_progress = 1;

		dump_len = 0;
		end = 0;
		
		if(remain < ctx->flash_block_len){
			while (kmsg_dump_get_line(&apanic_dumper, false,
				ctx->kmsg_buffer + dump_len + remain,
				PAGE_SIZE - dump_len - remain, &len)) {
				end = 1;
				dump_len += len;
				if((dump_len + remain) >= ctx->flash_block_len){
					break;
				}
			}
		} else {
			end = 1;
		}

		if (!end)
			break;

		if(dump_len + remain >= ctx->flash_block_len){
			memcpy(ctx->bounce, ctx->kmsg_buffer, ctx->flash_block_len);
			remain = dump_len + remain - ctx->flash_block_len;
			memcpy(ctx->kmsg_buffer, ctx->kmsg_buffer + ctx->flash_block_len, remain);
			rc = ctx->flash_block_len;
		} else {
			memcpy(ctx->bounce, ctx->kmsg_buffer, dump_len + remain);
			rc = dump_len + remain;
			remain = 0;
		}

		if (rc != ctx->flash_block_len)
			last_chunk = rc;

		oops_in_progress = saved_oip;
		if (rc <= 0)
			break;
		if (rc != ctx->flash_block_len)
			memset(ctx->bounce + rc, 0, ctx->flash_block_len - rc);

		rc2 = apanic_writeflashpage(off, ctx->bounce);
		if (rc2 <= 0) {
			printk(KERN_EMERG
			       "apanic: mmc write failed (%d)\n", rc2);
			return idx;
		}
		if (!last_chunk)
			idx += rc2;
		else
			idx += last_chunk;
		off += rc2;
	}
	return idx;
}

static int apanic(struct notifier_block *this, unsigned long event,
			void *ptr)
{
	struct apanic_data *ctx = &drv_ctx;
	struct panic_header *hdr = (struct panic_header *) ctx->bounce;
	int console_offset = 0;
	int console_len = 0;
	int threads_offset = 0;
	int threads_len = 0;
	int rc;

	if (in_panic)
		return NOTIFY_DONE;
	in_panic = 1;
#ifdef CONFIG_PREEMPT
	/* Ensure that cond_resched() won't try to preempt anybody */
	add_preempt_count(PREEMPT_ACTIVE);
#endif

	touch_softlockup_watchdog();

#if !defined(CONFIG_NAND_COMIP)
	apanic_mmc_init();
#endif

	if (!ctx->flash)
		goto out;

	if (ctx->curr.magic) {
		printk(KERN_EMERG "Crash partition in use!\n");
		goto out;
	}

	console_offset = ctx->flash_block_len;

	/*
	 * Write out the console
	 */
	console_len = apanic_write_console(console_offset);
	if (console_len < 0) {
		printk(KERN_EMERG "Error writing console to panic log! (%d)\n",
		       console_len);
		console_len = 0;
	}

	/*
	 * Write out all threads
	 */
	threads_offset = ALIGN(console_offset + console_len, ctx->flash_block_len);
	if (!threads_offset)
		threads_offset = ctx->flash_block_len;

	syslog_print_all(NULL, 0, true);
	show_state_filter(0);
	threads_len = apanic_write_console(threads_offset);
	if (threads_len < 0) {
		printk(KERN_EMERG "Error writing threads to panic log! (%d)\n",
		       threads_len);
		threads_len = 0;
	}

	/*
	 * Finally write the panic header
	 */
	memset(ctx->bounce, 0, PAGE_SIZE);
	hdr->magic = PANIC_MAGIC;
	hdr->version = PHDR_VERSION;

	hdr->console_offset = console_offset;
	hdr->console_length = console_len;

	hdr->threads_offset = threads_offset;
	hdr->threads_length = threads_len;

	rc = apanic_writeflashpage(0, ctx->bounce);
	if (rc <= 0) {
		printk(KERN_EMERG "apanic: Header write failed (%d)\n",
		       rc);
		goto out;
	}

	printk(KERN_EMERG "apanic: Panic dump sucessfully written to flash\n");

out:

#ifdef CONFIG_PREEMPT
	sub_preempt_count(PREEMPT_ACTIVE);
#endif

	in_panic = 0;
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= apanic,
};

static int panic_dbg_get(void *data, u64 *val)
{
	apanic(NULL, 0, NULL);
	return 0;
}

static int panic_dbg_set(void *data, u64 val)
{
	BUG();
	return -1;
}

DEFINE_SIMPLE_ATTRIBUTE(panic_dbg_fops, panic_dbg_get, panic_dbg_set, "%llu\n");

int __init apanic_init(void)
{
#if !defined(CONFIG_NAND_COMIP)
	apanic_mmc_create_proc();
#elif defined(CONFIG_NAND_COMIP)
	register_mtd_user(&mtd_panic_notifier);
#endif
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	debugfs_create_file("apanic", 0644, NULL, NULL, &panic_dbg_fops);
	memset(&drv_ctx, 0, sizeof(drv_ctx));
	drv_ctx.bounce = (void *) __get_free_page(GFP_KERNEL);
	drv_ctx.kmsg_buffer = (void *) __get_free_page(GFP_KERNEL);
	INIT_WORK(&proc_removal_work, apanic_remove_proc_work);
	printk(KERN_INFO "Android kernel panic handler initialized (bind=%s)\n",
	       CONFIG_APANIC_PLABEL);

	return 0;
}

postcore_initcall(apanic_init);
