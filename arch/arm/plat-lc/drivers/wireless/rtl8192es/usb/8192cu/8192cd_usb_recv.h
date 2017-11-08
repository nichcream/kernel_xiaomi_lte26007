#ifndef	_8192CD_USB_RECV_H_
#define _8192CD_USB_RECV_H_

#define MAX_RECVBUF_SZ (15360) // 15k < 16k
#define NR_RECVBUFF (4)
#define NR_PREALLOC_RECV_SKB (8)
#define RECVBUFF_ALIGN_SZ (8)

#define RXDESC_SIZE	24

#define NR_RECVFRAME 256
#define RXFRAME_ALIGN	8
#define RXFRAME_ALIGN_SZ	(1<<RXFRAME_ALIGN)

#define RXDESC_SIZE	24
#define RXDESC_OFFSET RXDESC_SIZE

#define RECV_BULK_IN_ADDR		0x80
#define RECV_INT_IN_ADDR		0x81

#ifdef CONFIG_RTL_88E_SUPPORT
#define	USB_INTR_CONTENT_C2H_OFFSET		0
#define	USB_INTR_CONTENT_CPWM1_OFFSET	16
#define	USB_INTR_CONTENT_CPWM2_OFFSET	20
#define	USB_INTR_CONTENT_HISR_OFFSET		48
#define	USB_INTR_CONTENT_HISRE_OFFSET	52

#define TX_RPT1_PKT_LEN 8

typedef enum _RX_PACKET_TYPE{
	NORMAL_RX,//Normal rx packet
	TX_REPORT1,//CCX
	TX_REPORT2,//TX RPT
	HIS_REPORT,// USB HISR RPT
}RX_PACKET_TYPE, *PRX_PACKET_TYPE;
#endif // CONFIG_RTL_88E_SUPPORT

struct recv_priv
{
	spinlock_t lock;

#ifdef CONFIG_RECV_THREAD_MODE
	_sema	recv_sema;
	_sema	terminate_recvthread_sema;
#endif

	_queue	free_recv_queue;
	_queue	recv_pending_queue;

	u8 *pallocated_frame_buf;
	u8 *precv_frame_buf;

#ifdef CONFIG_USB_HCI
	//u8 *pallocated_urb_buf;
	_sema allrxreturnevt;
	uint	ff_hwaddr;
	u8	rx_pending_cnt;

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	struct urb *int_in_urb;
	u8 *int_in_buf;
#endif
#endif
	
	struct tasklet_struct recv_tasklet;
	struct sk_buff_head free_recv_skb_queue;
	struct sk_buff_head rx_skb_queue;

#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
	_queue	recv_buf_pending_queue;
#endif	// CONFIG_USE_USB_BUFFER_ALLOC_RX

	u8 *pallocated_recv_buf;
	u8 *precv_buf;    // 4 alignment
	_queue	free_recv_buf_queue;
	u32	free_recv_buf_queue_cnt;
};

struct recv_buf
{
	struct list_head list;
	spinlock_t recvbuf_lock;
	u32	ref_cnt;
	struct rtl8192cd_priv *priv;
	u8 *pbuf;
	u8 *pallocated_buf;
	u32 len;
	u8 *phead;
	u8 *pdata;
	u8 *ptail;
	u8 *pend;
	struct urb *purb;
	dma_addr_t dma_transfer_addr;	/* (in) dma addr for transfer_buffer */
	u32 alloc_sz;
	u8 irp_pending;
	int transfer_len;
	struct sk_buff *pskb;
};

struct rx_pkt_attrib
{
	u16	pkt_len;
	u8	physt;
	u8	drvinfo_sz;
	u8	shift_sz;
	u8	hdrlen; //the WLAN Header Len
	u8 	to_fr_ds;
	u8 	amsdu;
	u8	qos;
	u8	priority;
	u8	pw_save;
	u8	mdata;
	u16	seq_num;
	u8	frag_num;
	u8	mfrag;
	u8	order;
	u8	privacy; //in frame_ctrl field
	u8	bdecrypted;
	u8	encrypt; //when 0 indicate no encrypt. when non-zero, indicate the encrypt algorith
	u8	iv_len;
	u8	icv_len;
	u8	crc_err;
	u8	icv_err;
/*
	u8 	dst[6];
	u8 	src[6];
	u8 	ta[6];
	u8 	ra[6];
	u8 	bssid[6];
*/	
//#ifdef CONFIG_TCP_CSUM_OFFLOAD_RX
	u8	tcpchk_valid; // 0: invalid, 1: valid
	u8	ip_chkrpt; //0: incorrect, 1: correct
	u8	tcp_chkrpt; //0: incorrect, 1: correct
//#endif
	u8 	key_index;

	u8	mcs_rate;
	u8	rxht;
	u8 	sgi;
#ifdef CONFIG_RTL_88E_SUPPORT
	u8	pkt_rpt_type;
	u32	MacIDValidEntry[2];
#endif
/*
	u8	signal_qual;
	s8	rx_mimo_signal_qual[2];
	u8	signal_strength;
	u8	rx_rssi[2];  //This value is percentage
	u8	rx_snr[2];
	u32	RxPWDBAll;
	s32	RecvSignalPower;
*/
};

struct recv_frame_hdr
{
	struct list_head list;

	struct sk_buff *pkt;
	struct sk_buff *pkt_newalloc;

	u8 fragcnt;

	int frame_tag;

	struct rx_pkt_attrib attrib;

	uint  len;
	u8 *rx_head;
	u8 *rx_data;
	u8 *rx_tail;
	u8 *rx_end;

	void *precvbuf;

	//
//temp	struct sta_info *psta;

	//for A-MPDU Rx reordering buffer control
//temp	struct recv_reorder_ctrl *preorder_ctrl;
};

struct recv_stat
{
	unsigned int rxdw0;
	unsigned int rxdw1;
	unsigned int rxdw2;
	unsigned int rxdw3;
	unsigned int rxdw4;
	unsigned int rxdw5;
#ifdef CONFIG_PCI_HCI
	unsigned int rxdw6;
	unsigned int rxdw7;
#endif
};

struct phy_stat
{
	unsigned int phydw0;
	unsigned int phydw1;
	unsigned int phydw2;
	unsigned int phydw3;
	unsigned int phydw4;
	unsigned int phydw5;
	unsigned int phydw6;
	unsigned int phydw7;
};

union recv_frame{
	union{
		struct list_head list;
		struct recv_frame_hdr hdr;
		unsigned int mem[/*RECVFRAME_HDR_ALIGN*/128>>2];
	}u;

	//uint mem[MAX_RXSZ>>2];
};

#ifdef CONFIG_RTL_92C_SUPPORT
typedef struct _INTERRUPT_MSG_FORMAT_EX {
	unsigned int C2H_MSG0;
	unsigned int C2H_MSG1;
	unsigned int C2H_MSG2;
	unsigned int C2H_MSG3;
	unsigned int HISR_VALUE;	// from HISR Reg0x124, read to clear
	unsigned int HISRE_VALUE;	// from HISRE Reg0x12c, read to clear
	unsigned int MSG_EX;
} INTERRUPT_MSG_FORMAT_EX,*PINTERRUPT_MSG_FORMAT_EX;
#endif // CONFIG_RTL_92C_SUPPORT


__inline static u8 *recvframe_push(union recv_frame *precvframe, int sz)
{
	// append data before rx_data

	/* add data to the start of recv_frame
 *
 *      This function extends the used data area of the recv_frame at the buffer
 *      start. rx_data must be still larger than rx_head, after pushing.
 */

	if(precvframe==NULL)
		return NULL;


	precvframe->u.hdr.rx_data -= sz ;
	if( precvframe->u.hdr.rx_data < precvframe->u.hdr.rx_head )
	{
		precvframe->u.hdr.rx_data += sz ;
		return NULL;
	}

	precvframe->u.hdr.len +=sz;

	return precvframe->u.hdr.rx_data;

}


__inline static u8 *recvframe_pull(union recv_frame *precvframe, int sz)
{
	// rx_data += sz; move rx_data sz bytes  hereafter

	//used for extract sz bytes from rx_data, update rx_data and return the updated rx_data to the caller


	if(precvframe==NULL)
		return NULL;


	precvframe->u.hdr.rx_data += sz;

	if(precvframe->u.hdr.rx_data > precvframe->u.hdr.rx_tail)
	{
		precvframe->u.hdr.rx_data -= sz;
		return NULL;
	}

	precvframe->u.hdr.len -=sz;

	return precvframe->u.hdr.rx_data;

}

__inline static u8 *recvframe_put(union recv_frame *precvframe, int sz)
{
	// rx_tai += sz; move rx_tail sz bytes  hereafter

	//used for append sz bytes from ptr to rx_tail, update rx_tail and return the updated rx_tail to the caller
	//after putting, rx_tail must be still larger than rx_end.
 	unsigned char * prev_rx_tail;

	if(precvframe==NULL)
		return NULL;

	prev_rx_tail = precvframe->u.hdr.rx_tail;

	precvframe->u.hdr.rx_tail += sz;

	if(precvframe->u.hdr.rx_tail > precvframe->u.hdr.rx_end)
	{
		precvframe->u.hdr.rx_tail -= sz;
		return NULL;
	}

	precvframe->u.hdr.len +=sz;

	return precvframe->u.hdr.rx_tail;

}

__inline static u8 *recvframe_pull_tail(union recv_frame *precvframe, int sz)
{
	// rmv data from rx_tail (by yitsen)

	//used for extract sz bytes from rx_end, update rx_end and return the updated rx_end to the caller
	//after pulling, rx_end must be still larger than rx_data.

	if(precvframe==NULL)
		return NULL;

	precvframe->u.hdr.rx_tail -= sz;

	if(precvframe->u.hdr.rx_tail < precvframe->u.hdr.rx_data)
	{
		precvframe->u.hdr.rx_tail += sz;
		return NULL;
	}

	precvframe->u.hdr.len -=sz;

	return precvframe->u.hdr.rx_tail;

}

__inline static s32 translate_percentage_to_dbm(u32 SignalStrengthIndex)
{
	s32	SignalPower; // in dBm.

	// Translate to dBm (x=0.5y-95).
	SignalPower = (s32)((SignalStrengthIndex + 1) >> 1);
	SignalPower -= 95;

	return SignalPower;
}


void rtl8192cu_recv_tasklet(void *priv);
int _rtw_init_recv_priv(struct rtl8192cd_priv *priv);
void _rtw_free_recv_priv (struct rtl8192cd_priv *priv);
unsigned int rtl8192cu_inirp_init(struct rtl8192cd_priv *priv);
unsigned int rtl8192cu_inirp_deinit(struct rtl8192cd_priv *priv);
void rtw_flush_recvbuf_pending_queue(struct rtl8192cd_priv *priv);

#endif
