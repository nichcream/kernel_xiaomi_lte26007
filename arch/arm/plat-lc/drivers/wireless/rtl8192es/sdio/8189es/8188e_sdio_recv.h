#ifndef	_8188E_SDIO_RECV_H_
#define _8188E_SDIO_RECV_H_

#define MAX_RECVBUF_MEM_SZ (65536)	// 64K=4*16K > 4*10K
#define MAX_RECVBUF_SZ (10240)
#define NR_RECVBUFF (8)
#define NR_PREALLOC_RECV_SKB (8)
#define RECVBUFF_ALIGN_SZ (8)

#define NR_RECVFRAME 256
#define RXFRAME_ALIGN	8
#define RXFRAME_ALIGN_SZ	(1<<RXFRAME_ALIGN)

#define RXDESC_SIZE	24
#define RXDESC_OFFSET RXDESC_SIZE

#define RECVBUF_POISON_END	0x5a


#define RECVBUF_DEBUG

#define TX_RPT1_PKT_LEN 8

typedef enum _RX_PACKET_TYPE{
	NORMAL_RX,//Normal rx packet
	TX_REPORT1,//CCX
	TX_REPORT2,//TX RPT
	HIS_REPORT,// USB HISR RPT
}RX_PACKET_TYPE, *PRX_PACKET_TYPE;

struct recv_priv
{
	spinlock_t lock;

	u8 *pallocated_frame_buf;
	u8 *precv_frame_buf;
	_queue	free_recv_queue;
	_queue	recv_pending_queue;

	struct tasklet_struct recv_tasklet;

	u8 *recvbuf_mem_head;
	u8 *recvbuf_mem_data;
	u8 *recvbuf_mem_tail;
	u8 *recvbuf_mem_end;
	
	u8 *pallocated_recv_buf;
	u8 *precv_buf;    // 4 alignment
	_queue	free_recv_buf_queue;
	_queue	recv_buf_pending_queue;
	
	unsigned int nr_out_of_recvbuf;
	unsigned int nr_out_of_recvbuf_mem;
	unsigned int nr_out_of_recvframe;
};

struct recv_buf
{
	struct list_head list;
	u8 *phead;
	u8 *pdata;
	u8 *ptail;
	u8 *pend;
};

struct rx_pkt_attrib
{
	u16	pkt_len;
	u8	physt;
	u8	drvinfo_sz;
	u8	shift_sz;
	u8	hdrlen; //the WLAN Header Len
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

#ifdef CONFIG_RTL_88E_SUPPORT
	u8	pkt_rpt_type;
	u32	MacIDValidEntry[2];
#endif
};

struct recv_frame_hdr
{
	struct list_head list;

	struct sk_buff *pkt;

	struct rx_pkt_attrib attrib;
};

struct recv_stat
{
	unsigned int rxdw0;
	unsigned int rxdw1;
	unsigned int rxdw2;
	unsigned int rxdw3;
	unsigned int rxdw4;
	unsigned int rxdw5;
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
		//unsigned int mem[/*RECVFRAME_HDR_ALIGN*/128>>2];
	}u;

	//uint mem[MAX_RXSZ>>2];
};

__inline static s32 translate_percentage_to_dbm(u32 SignalStrengthIndex)
{
	s32	SignalPower; // in dBm.

	// Translate to dBm (x=0.5y-95).
	SignalPower = (s32)((SignalStrengthIndex + 1) >> 1);
	SignalPower -= 95;

	return SignalPower;
}


void rtl8188es_recv_tasklet(void *priv);
int _rtw_init_recv_priv(struct rtl8192cd_priv *priv);
void _rtw_free_recv_priv (struct rtl8192cd_priv *priv);
void rtw_flush_recvbuf_pending_queue(struct rtl8192cd_priv *priv);

struct recv_buf* sd_recv_rxfifo(struct rtl8192cd_priv *priv, u32 size);
void sd_rxhandler(struct rtl8192cd_priv *priv, struct recv_buf *precvbuf);

#endif
