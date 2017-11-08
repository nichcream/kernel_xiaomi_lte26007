#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
/* For firmware read */
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/mmc/sdio_func.h>
#include <linux/fs.h>   //added to check vfs read
#include <linux/vmalloc.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>

#include "UMI_Api.h"
#include "cw1200_common.h"
#include "sbus.h"

#define stlc9000_sdio_suspend NULL
#define stlc9000_sdio_resume  NULL
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

#define TESTCASE_4  4
#define TESTCASE_6  6

DECLARE_COMPLETION(all_done);

MODULE_LICENSE("GPL v2");
#define MILLI_SECS_TO_JIFFIES( x  )  ( ( 1000/HZ ) * x )

struct sk_buff * skb_g = NULL;
#define PKTSIZE   4096
static int __packet_size = 1024;
static uint32_t send = FALSE;

LL_HANDLE  UMI_CB_Create(UMI_HANDLE UMACHandle,UL_HANDLE ulHandle);
UMI_STATUS_CODE  UMI_CB_Start (LL_HANDLE  		LowerHandle,uint32_t FmLength,
			       void * FirmwareImage);

CW1200_STATUS_E SBUS_SDIO_RW_Reg( struct CW1200_priv * priv, uint32_t addr, uint8_t * rw_buf,
				  uint16_t length, int32_t read_write )  ;


void cw1200_sbus_interrupt(struct sdio_func *func);

wait_queue_head_t   startup_ind_waitq;

static struct CW1200_priv private;
static struct CW1200_priv * priv = &private;

static int last_block_size;
static int last_packet_size;

struct stlc_firmware
{
	void * firmware;
	uint32_t length;
};

static struct stlc_firmware frmwr;
struct stlc_firmware * WDEV_Readfirmware(void){
	mm_segment_t fs;
	unsigned int bcount, length;
	char *  buffer;
	struct file * fp = NULL;
	loff_t pos;

	frmwr.firmware=NULL;
	frmwr.length=0;

	/* for file opening temporarily tell the kernel I am not a user for
	 *      * memory management segment access */
	fs = get_fs();
	set_fs(KERNEL_DS);
	printk("Attempting download of FW file \"%s\"\n", FIRMWARE_FILE);
	fp = filp_open( FIRMWARE_FILE,O_RDONLY | O_LARGEFILE, 0 ) ;
	if(IS_ERR(fp)) {
		printk("Attempting download of FW file \"%s\"\n", FIRMWARE_FILE_ALT1);
		fp = filp_open( FIRMWARE_FILE_ALT1,O_RDONLY | O_LARGEFILE, 0 ) ;
	}
	if(IS_ERR(fp))
	{
		/* It is OK not to open the optional LMAC file */
		printk("Not able to open the firmware file \n");
		set_fs(fs);
		return NULL;
	}
	length = fp->f_path.dentry->d_inode->i_size;

	if(length <= 0)
	{
		printk("Invalid Firmware Size \n");
		return NULL;
	}
	//length=sys_lseek(ifp,0,SEEK_END);
	/* We assume kmalloc gives us an aligned buffer! */
	buffer = kmalloc(length,GFP_KERNEL);
	if(!buffer)
	{
		printk("Error in creating buffer for FW\n");
		return NULL;
	}
	pos = 0;
	bcount = vfs_read(fp, (char __user *)buffer, length,&pos);
	if(length != bcount)
	{
		printk("Length and Read Byte Count do not match [%d],[%d] \n",length,bcount);
		return NULL;
	}


	frmwr.firmware = buffer;
	frmwr.length = bcount;
	if(bcount!=length)
	{
		printk("ncorrect reading [%d],[%d] \n",bcount,length);
	}
	filp_close(fp,current->files);
	set_fs(fs);
	return (&frmwr);
}



struct stlc_firmware * fw_ptr = NULL;

uint32_t sent = 0;

static int boot_fw(void)
{
	void * p_ret = NULL;
	int retval;

	fw_ptr = WDEV_Readfirmware();

	if(fw_ptr == NULL)
	{
		printk("Unable to read firmware \n");
		return -ENOENT;
	}

	p_ret = UMI_CB_Create(NULL,priv);
	printk("TEST1:UMI_CB_Create Retval [%p] \n",p_ret);

	retval = UMI_CB_Start(priv,fw_ptr->length,fw_ptr->firmware);
	printk("TEST1:UMI_CB_Start Retval [%d] \n",retval);

	if(retval)
	{
		UMI_CB_Stop((void *)priv);
		UMI_CB_Destroy((void *)priv);
	}

	return 0;
}

static void test_block(void)
{
	int i, j, ret;
	unsigned int iteration = 1;

	/* Allocate a large, reusable sk buf */
	skb_g = dev_alloc_skb(PKTSIZE);
	skb_put(skb_g, PKTSIZE);

	/* Fill it with a pattern */
	memset(skb_g->data, 0xAA, PKTSIZE);

#define CONTINOUS 0

#if CONTINOUS
	while (1) {
#endif
		printk("============ TEST ALL execution starts (iteration %i) ============\n", iteration);
		/* Attempt all 4 byte aligned block sizes */
		for (j=8;j<=2048;j+=4) {
			/* Attempt to change block size */
			sdio_claim_host(priv->func);
			ret = sdio_set_block_size(priv->func, j);
			sdio_release_host(priv->func);
			if (ret) {
				printk("Unable to set SDIO block size to %i! Error: %i!\n", j, ret);
				j++;
				continue;
			}
			last_block_size = j;
			printk(" === Doing test round (SDIO packets from 4 to 2100 bytes) with block size of %i bytes (iteration %i) ===\n", j, iteration);

			/* Attempt all packet sizes */
			for (i=4;i<2100;i++) {
				send = TRUE;
				__packet_size = i;
				last_packet_size = i;
				UMI_CB_ScheduleTx((void *)priv);
				wait_for_completion(&all_done);
			}
		}

		iteration++;
		printk("============ TEST ALL execution done ============\n\n\n\n\n");
#if CONTINOUS
	}
#endif

	kfree_skb(skb_g);
}

static void test_performance(void)
{
	int ret,testcycle;
	unsigned long packet_count = 0;
	struct timeval start;
	struct timeval stop;

#define TEST_TIME 30
#define PAYLOAD_SIZE 2048
#define TEST_CYCLLE 120


	/* Allocate a large, reusable sk buf */
	skb_g = dev_alloc_skb(PKTSIZE);
	skb_put(skb_g, PKTSIZE);

	/* Fill it with a pattern */
	memset(skb_g->data, 0xAA, PKTSIZE);

	/* Try to set up WLAN-like conditions */
	/* Attempt to change block size */
	sdio_claim_host(priv->func);
	ret = sdio_set_block_size(priv->func, SDIO_BLOCK_SIZE);
	sdio_release_host(priv->func);
	if (ret) {
		printk("Unable to set SDIO block size to %i! Error: %i!\n", SDIO_BLOCK_SIZE, ret);
		return;
	}
	__packet_size = last_packet_size = PAYLOAD_SIZE;
	last_block_size = SDIO_BLOCK_SIZE;

for(testcycle=1; testcycle <= TEST_CYCLLE;testcycle++)
{
	printk("============ TEST PERF execution starts %d ============\n", testcycle);

	do_gettimeofday(&start);
	do_gettimeofday(&stop);
	packet_count=0;

	while((stop.tv_sec - start.tv_sec) < TEST_TIME) {
		send = TRUE;
		UMI_CB_ScheduleTx((void *)priv);
		do_gettimeofday(&stop);
		wait_for_completion(&all_done);
		packet_count++;
	}
	send = FALSE;

/*
	printk("--------------------------------------------------\n");
	printk("packet_count: %lu, bytes: %lu, bits: %lu, doubled: %lu\n",
	       packet_count,
	       packet_count * PAYLOAD_SIZE,
	       packet_count * PAYLOAD_SIZE * 8,
	       packet_count * PAYLOAD_SIZE * 8 * 2);
*/
	printk("Calculated performance over %d seconds: %lu MBit/sec\n",TEST_TIME, packet_count * PAYLOAD_SIZE * 8 * 2 / (TEST_TIME*1000000));
//	printk("============ TEST PERF execution done ============\n");

}
	kfree_skb(skb_g);
}

static int testall (char *buf,char **start,off_t offset,int len,int *eof,void *data)
{
	test_block();
	test_performance();

	*eof= 1;
	return 0;
}
/*
static int testblock (char *buf,char **start,off_t offset,int len,int *eof,void *data)
{
	test_block();

	*eof= 1;
	return 0;
}
*/
static int testperf (char *buf,char **start,off_t offset,int len,int *eof,void *data)
{
	test_performance();

	*eof= 1;
	return 0;
}


CW1200_STATUS_E EIL_Init(struct sdio_func *func,const struct sdio_device_id *id)
{
	printk("Loaded SBUS Test Module driver, \n");

	priv->func = func;
	func->num = 1;

	init_waitqueue_head(&startup_ind_waitq);
	proc_create_data("test_all",0444,NULL,&testall,(void *)1);
	proc_create_data("test_block",0444,NULL,&testall,(void *)2);
	proc_create_data("test_perf",0444,NULL,&testperf,(void *)3);

#define CW1200_AUTOBOOT
#ifdef CW1200_AUTOBOOT
	/* Download FW */
	if (boot_fw())
		return UMI_STATUS_FAILURE;
#endif

	return UMI_STATUS_SUCCESS;
}

UMI_STATUS_CODE UMI_GetTxFrame(IN  UMI_HANDLE umacHandle,
			       OUT UMI_GET_TX_DATA **pTxData_p
			      )
{
	hif_rw_msg_hdr_payload_t * payload = NULL;
	uint32 * packet = NULL;

	static UMI_GET_TX_DATA pTxData;

	/* Allocate a new packet based on global __packet_size */
	payload =(hif_rw_msg_hdr_payload_t * )skb_g->data;
	payload->hdr.MsgId = 0x0003;
	payload->hdr.MsgLen = __packet_size;
	packet = (uint32 *)(payload->payload);
	/*Uncomment to test loopback mode */
	packet[0] = 0x00000001;

//	printk("UMI_GetTxFrame called! Payload addr: 0x%.8X, Payload size: %i bytes\n", payload->payload, __packet_size);

	pTxData.pTxDesc = skb_g->data;
	pTxData.pDriverInfo = NULL;

	*pTxData_p = &pTxData;

	if(send == TRUE )
	{
		send = FALSE;
		//printk(KERN_DEBUG "TEST APP: PACKET SIZE SENT [%d]\n",__packet_size);

		return UMI_STATUS_SUCCESS;
	}

	*pTxData_p = NULL;
	return UMI_STATUS_NO_MORE_DATA;
}

UMI_STATUS_CODE  UMI_ReceiveFrame(IN UMI_HANDLE 	umacHandle,
				  IN	void*	        pFrameBuffer,
				  void * buf_handle
				 )
{

	/* Indicate that the packet has now arrived */
	complete(&all_done);

	return UMI_STATUS_SUCCESS;
}


void EIL_Shutdown(struct CW1200_priv * priv)
{
	printk("EIL_Shutdown Called! last_block_size = %i, last_packet_size = %i\n", last_block_size, last_packet_size);

	/* Make sure we return from our debugfs hook */
	complete(&all_done);

	printk("Doing some sleeping...\n");
	msleep(10);

	printk("Removing proc fs...\n");
	remove_proc_entry("test_sdio", NULL);

	printk("Calling UMI_CB_Stop...\n");
	UMI_CB_Stop((void *)priv);

	printk("Calling UMI_CB_Destroy...\n");
	UMI_CB_Destroy((void *)priv);

	printk("EIL_Shutdown complete!\n");
}
