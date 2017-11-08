#ifndef LC_BIH_H
#define LC_BIH_H


#include <linux/module.h>               /* Needed by all modules */
#include <linux/kernel.h>               /* Needed for KERN_ALERT */
#include <linux/init.h>                 /* Needed for the macros */
#include <linux/list.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>
#include <linux/proc_fs.h>      /* Necessary because we use proc fs */
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/interrupt.h>    /* mark_bh */
#include <linux/random.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>  /* eth_type_trans */
#include <linux/nsproxy.h>

#include <net/route.h>
#include <net/ip6_route.h>
#include <net/ip_fib.h>
#include <net/ip6_fib.h>
#include <net/flow.h>
#include <net/addrconf.h>
#include <net/ip.h>             /* struct iphdr */
#include <net/icmp.h>           /* struct icmphdr */
#include <net/ipv6.h>
#include <net/udp.h>

#include <asm/uaccess.h>                /* for get_user and put_user */
#include <asm/checksum.h>

//variable type
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long long U64;
typedef signed char S8;
typedef signed short S16;
typedef signed int S32;
typedef signed long long S64;
typedef float F32;
typedef double F64;

typedef U8 BOOLE;
typedef U8 CHARA;
typedef void* HANDLE;
typedef S32 RESULT;

//macro defination
#ifndef NULL
#define NULL ((void *)0)
#endif

#define BOOLE_TRUE 1
#define BOOLE_FALSE 0

#define BIH_RES_SUCCESS 0
#define BIH_RES_FAILED -1

#define BIH_RES_ERROR -10

#define BIH_PROC_DIR_NAME "lcbih"
#define BIH_PROC_MOD_CFG_NAME "mod_cfg"
#define BIH_PROC_MOD_LOG_NAME "mod_log"
#define BIH_PROC_MAP_HOST_NAME "map_host"
#define BIH_PROC_MAP_DNS_NAME "map_dns"
#define BIH_PROC_MAP_REMOTE_NAME "map_remote"
#define BIH_PROC_MAP_LIST "map_list"
#define BIH_PROC_DNS_LIST "dns_list"
#define BIH_PROC_MATCH_LIST "match_list"

#define BIH_MAP_HOST_NUM 5
#define BIH_MAP_DNS_NUM (BIH_MAP_HOST_NUM*2)

#define BIH_DNS_REC_LIST_NUM 30
#define BIH_DNS_REC_BUF_NUM 300

#define BIH_MAP_ADDR_LIST_NUM 50
#define BIH_MAP_ADDR_BUF_NUM 500

#define BIH_MATCH_ITEM_LIST_NUM 100
#define BIH_MATCH_ITEM_BUF_NUM 1000

#define BIH_MOD_TIME_CB (2*HZ) //second
#define BIH_DNS_TIMEOUT_V6 (2*HZ)
#define BIH_DNS_TIMEOUT_RSP (180*HZ)
#define BIH_MAP_TIMEOUT_READY (1*60*HZ)
#define BIH_MAP_TIMEOUT_INACT (10*60*HZ)
#define BIH_MATCH_TIMEOUT_TEMP (1*60*HZ)
#define BIH_MATCH_TIMEOUT_INACT (10*60*HZ)

#define BIH_IP4_HDR_SIZE 20
#define BIH_IP6_HDR_SIZE 40
#define BIH_IP6_HDR_FRAGMENT_SIZE 8
#define BIH_IP6_IP4_HDR_DIFF 20
#define BIH_IP_FRAG_OFFSET_MASK 0xfff8

#define BIH_TCP_HDR_SIZE 20
#define BIH_UDP_HDR_SIZE 8
#define BIH_ICMP4_HDR_SIZE 8
#define BIH_ICMP6_HDR_SIZE 8

#define BIH_DNS_UDP_PORT 53
#define BIH_DNS_TYPE_A 0x0001
#define BIH_DNS_TYPE_AAAA 0x001c
#define BIH_DNS_CLASS_IN 0x0001

#define BIH_MATCH_ITEM_TYPE_ICMP 0
#define BIH_MATCH_ITEM_TYPE_TCP 1
#define BIH_MATCH_ITEM_TYPE_UDP 2
#define BIH_MATCH_ITEM_TYPE_FRAG 3

//highest 0, lowest 5
#define BIH_LOG(level,format,...) { if(bih_obj.log_level>=(level)) {printk("[BIH][%d]"format"\n",level,##__VA_ARGS__ );} }

#define BIH_IS_DECIMAL(x) ((x)>='0' && (x)<='9')
#define BIH_GET_DECIMAL(x) ((x) - '0')
#define BIH_IS_HEX(x) ( ((x)>='A' && (x)<='F') || ((x)>='a' && (x)<='f') )
#define BIH_GET_HEX(x) ( ((x)>='A' && (x)<='F') ? (x)-'A'+10 : (x)-'a'+10 )

#define BIH_LINK_NEXT(unit) ( ((void **)(unit))[0] )
#define BIH_LINK_LAST(unit) ( ((void **)(unit))[1] )

#define BIH_READ_BUF16BE(buf) ( ((U8 *)(buf))[0]<<8 | ((U8 *)(buf))[1] )
#define BIH_READ_BUF24BE(buf) ( ((U8 *)(buf))[0]<<16 | ((U8 *)(buf))[1]<<8 | ((U8 *)(buf))[2] )
#define BIH_READ_BUF32BE(buf) ( ((U8 *)(buf))[0]<<24 | ((U8 *)(buf))[1]<<16 | ((U8 *)(buf))[2]<<8 | ((U8 *)(buf))[3] )
#define BIH_WRITE_BUF16BE(buf,data) { ((U8 *)(buf))[0]=(data)>>8; ((U8 *)(buf))[1]=(data); }
#define BIH_WRITE_BUF24BE(buf,data) { ((U8 *)(buf))[0]=(data)>>16; ((U8 *)(buf))[1]=(data)>>8; ((U8 *)(buf))[2]=(data); }
#define BIH_WRITE_BUF32BE(buf,data) { ((U8 *)(buf))[0]=(data)>>24; ((U8 *)(buf))[1]=(data)>>16; ((U8 *)(buf))[2]=(data)>>8; ((U8 *)(buf))[3]=(data); }

#define BIH_SEGMENT_ADDR4(addr4_ptr) ((U8 *)(addr4_ptr))[0],((U8 *)(addr4_ptr))[1],((U8 *)(addr4_ptr))[2],((U8 *)(addr4_ptr))[3]

#define BIH_MAP_GET_PAIR_PTR4(ptr4) ( (struct bih_map_pair *)((U8 *)(ptr4)-sizeof(struct bih_map_pair *)*2) )
#define BIH_MAP_GET_PAIR_PTR6(ptr6) ( BIH_MAP_GET_PAIR_PTR4( (U8 *)(ptr6)-sizeof(BIH_MAP_PTR) ) )
#define BIH_MAP_GET_PAIR_ADDR(map) ( BIH_MAP_GET_PAIR_PTR6( (U8 *)(map)-sizeof(BIH_MAP_PTR) ) )

#define BIH_PACK_SET_SRC(pack,buf,buf_len,data_used) {(pack)->src_buf=(buf); (pack)->src_buf_len=(buf_len); (pack)->src_data_used=(data_used);}
#define BIH_PACK_SET_DST(pack,buf,buf_len,data_filled) {(pack)->dst_buf=(buf); (pack)->dst_buf_len=(buf_len); (pack)->dst_data_filled=(data_filled);}

typedef struct bih_link_head
{
	void			*next;
	void			*last;
}BIH_LINK_HEAD;

#define BIH_TRANS_INFO_FLAG_SRC_FRAG (1<<0)
#define BIH_TRANS_INFO_FLAG_SRC_FRAG_LAST (1<<1)
#define BIH_TRANS_INFO_FLAG_DST_FRAG (1<<8)
#define BIH_TRANS_INFO_FLAG_ICMP (1<<16)
#define BIH_TRANS_INFO_FLAG_TCP (1<<17)
#define BIH_TRANS_INFO_FLAG_UDP (1<<18)
#define BIH_TRANS_INFO_FLAG_DNS (1<<19)

typedef struct bih_trans_info
{
	U32			flag;

//src
	U32			src_protocol;
	U32			src_frag_offset;//BIH_TRANS_INFO_FLAG_SRC_FRAG
	U32			src_frag_id;//BIH_TRANS_INFO_FLAG_SRC_FRAG
	U32			src_ip_len;//ip head len
	U32			src_tp_len;//transport head len
	U32			src_pl_len;//payload len

//dst
	U32			dst_protocol;
	U32			dst_frag_offset;//BIH_TRANS_INFO_FLAG_DST_FRAG
	U32			dst_frag_id;//BIH_TRANS_INFO_FLAG_DST_FRAG
	U32			dst_ip_len;//ip head len
	U32			dst_tp_len;//transport head len
	U32			dst_pl_len;//payload len

	struct in_addr saddr4;
	struct in_addr daddr4;
	struct in6_addr saddr6;
	struct in6_addr daddr6;
}BIH_TRANS_INFO;


typedef struct bih_trans_pack //payload
{
	U8			*src_buf;
	U32			src_buf_len;
	U32			src_data_used;

	U8			*dst_buf;
	U32			dst_buf_len;
	U32			dst_data_filled;

}BIH_TRANS_PACK;

typedef struct bih_map_host
{
	U32			mask4_len;
	struct in_addr addr4;

	U32			mask6_len;
	struct in6_addr addr6;

	U32			prefix6_len;
	struct in6_addr prefix6;

}BIH_MAP_HOST;

#define BIH_MAP_ADDR_FLAG_READY (1<<0) //addr will be clear if inact in BIH_MAP_TIMEOUT_READY
#define BIH_MAP_ADDR_FLAG_FIXED (1<<1) //max valid time is limited

typedef struct bih_map_addr
{
	U32			flag;
	U32			start_time;
	U32			last_time;
	U32			timeout;//BIH_MAP_ADDR_FLAG_FIXED

	struct in_addr addr4;
	struct in6_addr addr6;
}BIH_MAP_ADDR;

typedef struct bih_map_ptr
{
	struct bih_map_ptr *next;
	struct bih_map_ptr *last;
}BIH_MAP_PTR;

typedef struct bih_map_pair
{
	struct bih_map_pair *next;
	struct bih_map_pair *last;

	BIH_MAP_PTR ptr4;
	BIH_MAP_PTR ptr6;
	BIH_MAP_ADDR map;
}BIH_MAP_PAIR;

typedef struct bih_dns_rec
{
	struct bih_dns_rec *next;
	struct bih_dns_rec *last;

	U32			start_time;

	U32			port;
	U32			id;

	struct sk_buff *rsp;//AAAA rsp

	struct in_addr addr4;

}BIH_DNS_REC;

typedef struct bih_dns_msg
{
	U8			*src_msg;
	U32			src_msg_len;

	U8			*dst_msg;
	U32			dst_msg_len;
	U32			dst_buf_len;
}BIH_DNS_MSG;

typedef struct bih_dns_sect
{
	U32			tol_len;//total len of this sector

	U8			*name;
	U32			name_len;

	U8			*name_real;
	U32			name_real_len;

	U32			type;
	U32			class0;
	U32			ttl;
	U32			data_len;
	U8			*data;
}BIH_DNS_SECT;

#define BIH_MATCH_ITEM_FLAG_TEMP (1<<0)

typedef struct bih_match_item
{
	struct bih_match_item *next;
	struct bih_match_item *last;

	U32			start_time;
	U32			last_time;

	U32			flag;
	U32			type;
	U32			id;

	struct in_addr addr4;
}BIH_MATCH_ITEM;

typedef struct bih_obj
{
//dns
	BIH_LINK_HEAD dns_free;
	BIH_LINK_HEAD dns_list[BIH_DNS_REC_LIST_NUM];

	BIH_DNS_REC dns_buf[BIH_DNS_REC_BUF_NUM];

//addr map
	BIH_LINK_HEAD map_free;
	BIH_LINK_HEAD map4_list[BIH_MAP_ADDR_LIST_NUM];
	BIH_LINK_HEAD map6_list[BIH_MAP_ADDR_LIST_NUM];

	BIH_MAP_PAIR map_buf[BIH_MAP_ADDR_BUF_NUM];

//match
	BIH_LINK_HEAD match_free;
	BIH_LINK_HEAD match_list[BIH_MATCH_ITEM_LIST_NUM];

	BIH_MATCH_ITEM match_buf[BIH_MATCH_ITEM_BUF_NUM];

//config
	BOOLE		run;
	BOOLE		host_valid;//update after set host info
	U32			log_level;

	BIH_MAP_HOST host[BIH_MAP_HOST_NUM];
	BIH_MAP_ADDR dns[BIH_MAP_DNS_NUM];

//become effective after restart
	struct in_addr dst_addr0;//range of remote addr pool
	struct in_addr dst_addr1;//if 10.170.160.0/20, max range from 10.170.160.1 to 10.170.175.254,

}BIH_OBJ;

//bih_mod.c
RESULT Bih_InitHook(void);
void Bih_CleanupHook(void);
RESULT Bih_InitProc(void);
void Bih_CleanupProc(void);
RESULT Bih_ModReadCfg(BIH_OBJ *bih,U8 *buf,U32 *buf_len);
RESULT Bih_ModWriteCfg(BIH_OBJ *bih,U8 *buf,U32 buf_len);
RESULT Bih_ModReadLog(BIH_OBJ *bih,U8 *buf,U32 *buf_len);
RESULT Bih_ModWriteLog(BIH_OBJ *bih,U8 *buf,U32 buf_len);
void Bih_ModStart(BIH_OBJ *bih);
void Bih_ModStop(BIH_OBJ *bih);

//bih_trans.c
RESULT Bih_Pack46(BIH_OBJ *bih,struct sk_buff *skb4,struct net_device *out);
RESULT Bih_PackDns46(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb4,struct dst_entry *dst);
RESULT Bih_PackIcmp46(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb4,struct dst_entry *dst);
RESULT Bih_PackNormal46(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb4,struct dst_entry *dst);
RESULT Bih_PackCheck46(BIH_TRANS_INFO *info,U8 *buf,U32 buf_len);
RESULT Bih_PackTrans46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *ip_tp,BIH_TRANS_PACK *pl,BOOLE copy_pl);
RESULT Bih_PackCopy46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U32 hdr_len);
RESULT Bih_PackFillSkb46(struct sk_buff *skb6,U32 pack_len,struct net_device *out);
RESULT Bih_Pack64(BIH_OBJ *bih,struct sk_buff *skb6,struct net_device *in);
RESULT Bih_PackDns64(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb6,struct net_device *in);
RESULT Bih_PackIcmp64(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb6,struct net_device *in);
RESULT Bih_PackNormal64(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb6,struct net_device *in);
RESULT Bih_PackCheck64(BIH_TRANS_INFO *info,U8 *buf,U32 buf_len);
RESULT Bih_PackTrans64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *ip_tp,BIH_TRANS_PACK *pl,BOOLE copy_pl);
RESULT Bih_PackCopy64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack);
RESULT Bih_PackFillSkb64(struct sk_buff *skb4,U32 pack_len,struct net_device *in);

//bih_proto.c
RESULT Bih_ProtoIp46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U32 tol_len,BOOLE last);
RESULT Bih_ProtoIcmp46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len);
RESULT Bih_ProtoTcp46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len);
RESULT Bih_ProtoUdp46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len);
RESULT Bih_ProtoIp64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U32 tol_len,BOOLE last);
RESULT Bih_ProtoIcmp64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len);
RESULT Bih_ProtoTcp64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len);
RESULT Bih_ProtoUdp64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len);

//bih_dns.c
RESULT Bih_Dns46(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb4,struct dst_entry *dst);
RESULT Bih_Dns64(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb6,struct net_device *in);
RESULT Bih_DnsReq46(BIH_OBJ *bih,BIH_DNS_MSG *msg,BOOLE *has_addr,BOOLE *map_addr);
RESULT Bih_DnsRsp64(BIH_OBJ *bih,BIH_DNS_MSG *msg,BOOLE *has_addr,BOOLE *map_addr);
void Bih_DnsTimerCb(BIH_OBJ *bih);
RESULT Bih_DnsGetSect(BIH_DNS_MSG *msg,BIH_DNS_SECT *sect,U32 src_offset,BOOLE resource);
RESULT Bih_DnsCopySect(BIH_DNS_MSG *msg,BIH_DNS_SECT *sect,U32 *dst_offset,BOOLE resource);
RESULT Bih_DnsCheckDomain(BIH_DNS_MSG *msg,BIH_DNS_SECT *sect,U32 src_offset);
RESULT Bih_DnsCopyDomain(BIH_DNS_MSG *msg,BIH_DNS_SECT *sect,U32 *dst_offset);
BIH_DNS_REC *Bih_DnsAddRec(BIH_OBJ *bih,struct in_addr *addr4,U32 port,U32 id);
BIH_DNS_REC *Bih_DnsSearchRec(BIH_OBJ *bih,U32 port,U32 id);
BIH_DNS_REC *Bih_DnsAllocRec(BIH_OBJ *bih);
void Bih_DnsFreeRec(BIH_OBJ *bih,BIH_DNS_REC *rec);
void Bih_DnsSendRec(BIH_OBJ *bih,BIH_DNS_REC *rec);
void Bih_DnsSetRecRsp(BIH_DNS_REC *rec,struct sk_buff *rsp);
BIH_LINK_HEAD *Bih_DnsGetHead(BIH_OBJ *bih,U32 port);
RESULT Bih_DnsReadList(BIH_OBJ *bih, struct seq_file *m);
RESULT Bih_DnsWriteList(BIH_OBJ *bih,U8 *buf,U32 buf_len);

//bih_map.c
RESULT Bih_MapAddr46(BIH_OBJ *bih,struct in_addr *local4,struct in_addr *remote4,struct in6_addr *local6,struct in6_addr *remote6);
RESULT Bih_MapConvert46(BIH_MAP_HOST *host,struct in_addr *addr4,struct in6_addr *addr6);
RESULT Bih_MapAddr64(BIH_OBJ *bih,struct in6_addr *remote6,struct in6_addr *local6,struct in_addr *remote4,struct in_addr *local4);
RESULT Bih_MapConvert64(BIH_MAP_HOST *host,struct in6_addr *addr6,struct in_addr *addr4);
void Bih_MapTimerCb(BIH_OBJ *bih);
RESULT Bih_MapBuild64(BIH_OBJ *bih,struct in6_addr *addr6,struct in_addr *addr4,U32 flag,U32 timeout);
BIH_MAP_ADDR *Bih_MapSearch4(BIH_OBJ *bih,struct in_addr *addr4);
BIH_MAP_ADDR *Bih_MapSearch6(BIH_OBJ *bih,struct in6_addr *addr6);
BIH_MAP_PAIR *Bih_MapAllocPair(BIH_OBJ *bih);
void Bih_MapFreePair(BIH_OBJ *bih,BIH_MAP_PAIR *pair);
BIH_LINK_HEAD *Bih_MapGetHead4(BIH_OBJ *bih,struct in_addr *addr4);
BIH_LINK_HEAD *Bih_MapGetHead6(BIH_OBJ *bih,struct in6_addr *addr6);
RESULT Bih_MapReadHost(BIH_OBJ *bih, struct seq_file *m);
RESULT Bih_MapWriteHost(BIH_OBJ *bih,U8 *buf,U32 buf_len);
RESULT Bih_MapReadDns(BIH_OBJ *bih, struct seq_file *m);
RESULT Bih_MapWriteDns(BIH_OBJ *bih,U8 *buf,U32 buf_len);
RESULT Bih_MapReadRemote(BIH_OBJ *bih, struct seq_file *m);
RESULT Bih_MapWriteRemote(BIH_OBJ *bih,U8 *buf,U32 buf_len);
RESULT Bih_MapReadList(BIH_OBJ *bih, struct seq_file *m);
RESULT Bih_MapWriteList(BIH_OBJ *bih,U8 *buf,U32 buf_len);

//bih_match.c
void Bih_MatchTimerCb(BIH_OBJ *bih);
BIH_MATCH_ITEM *Bih_MatchAddItem(BIH_OBJ *bih,struct in_addr *addr4,U32 type,U32 id,U32 flag);
BIH_MATCH_ITEM *Bih_MatchSearchItem(BIH_OBJ *bih,struct in_addr *addr4,U32 type,U32 id);
BIH_MATCH_ITEM *Bih_MatchAllocItem(BIH_OBJ *bih);
void Bih_MatchFreeItem(BIH_OBJ *bih,BIH_MATCH_ITEM *item);
BIH_LINK_HEAD *Bih_MatchGetHead(BIH_OBJ *bih,struct in_addr *addr4,U32 type,U32 id);
RESULT Bih_MatchReadList(BIH_OBJ *bih, struct seq_file *m);
RESULT Bih_MatchWriteList(BIH_OBJ *bih,U8 *buf,U32 buf_len);

//bih_opera.c
RESULT Bih_BufGetAddr4(U8 *buf,U32 *buf_len,struct in_addr *addr4,U32 *mask4_len);
RESULT Bih_BufGetAddr6(U8 *buf,U32 *buf_len,struct in6_addr *addr6,U32 *mask6_len);
RESULT Bih_BufGetDecimal(U8 *buf,U32 *buf_len,U32 *value);
RESULT Bih_BufGetHex(U8 *buf,U32 *buf_len,U32 *value);
RESULT Bih_BufSearchStr(U8 *buf,U32 buf_len,U8 *str,U32 str_len,U32 *offset);
void Bih_PrintIp4(struct iphdr *ih4);
void Bih_PrintTcp(struct tcphdr *tcph);
void Bih_LinkInit(void *head);
void Bih_LinkPutFront(void *unit,void *new_unit);
void Bih_LinkPutBehind(void *unit,void *new_unit);
void *Bih_LinkGetFront(void *unit);
void *Bih_LinkGetBehind(void *unit);
void Bih_LinkRemove(void *unit);


#endif //LC_BIH_H

/*
log level
0 mod info
1 sys error
2 transfer error and important info
3 all bih detail
*/


//echo 0 172.21.45.16/23 2002:0da8:00bf:1010:0000:0000:AC15:2D10/64 2002:0da8:00bf:1010:0000:0000:0000:0000/96 > /proc/lcbih/map_host
//echo 0 172.21.1.20/23 2002:0da8:00bf:1010:0000:0000:AC15:2D40/64 > /proc/lcbih/map_dns
//echo 1 172.21.1.21/23 2002:0da8:00bf:1010:0000:0000:AC15:2D41/64 > /proc/lcbih/map_dns
//echo 10.170.160.1/20 10.170.175.254/20 > /proc/lcbih/map_remote
//echo 1 > /proc/lcbih/mod_cfg
//echo 2 > /proc/lcbih/mod_log

//linux
//ip -6 addr add 2002:0da8:00bf:1010:0000:0000:AC15:2D10/64 dev wlan0

//winxp
//ipv6 adu 4/2002:0da8:00bf:1010:0000:0000:AC15:2D43
//ipv6 adu 4/2002:0da8:00bf:1010:0000:0000:AC15:2D42
//ipv6 adu 4/2002:0da8:00bf:1010:0000:0000:AC15:2D41
//ipv6 adu 4/2002:0da8:00bf:1010:0000:0000:AC15:2D40

