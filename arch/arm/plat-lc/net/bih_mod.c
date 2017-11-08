#include "bih.h"


/*bih_mod.c**********************************************************/

static int __init Bih_Init(void);
static void __exit Bih_Cleanup(void);
static unsigned int Bih_HookIp4PostRouting(unsigned int hookunm,struct sk_buff *skb,const struct net_device *in,const struct net_device *out,int (*okfn)(struct sk_buff *));
static unsigned int Bih_HookIp6PreRouting(unsigned int hookunm,struct sk_buff *skb,const struct net_device *in,const struct net_device *out,int (*okfn)(struct sk_buff *));
static int proc_file_mod_cfg_open(struct inode *inode, struct file *file);
static int proc_file_mod_cfg_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp);
static int proc_file_mod_log_open(struct inode *inode, struct file *file);
static int proc_file_mod_log_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp);
static int proc_file_map_host_open(struct inode *inode, struct file *file);
static int proc_file_map_host_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp);
static int proc_file_map_dns_open(struct inode *inode, struct file *file);
static int proc_file_map_dns_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp);
static int proc_file_map_remote_open(struct inode *inode, struct file *file);
static int proc_file_map_remote_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp);
static int proc_file_map_list_open(struct inode *inode, struct file *file);
static int proc_file_map_list_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp);
static int proc_file_dns_list_open(struct inode *inode, struct file *file);
static int proc_file_dns_list_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp);
static int proc_file_match_list_open(struct inode *inode, struct file *file);
static int proc_file_match_list_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp);
static void Bih_TimerCb(unsigned long data);


struct nf_hook_ops nfho_bih_ip4 =
{
        .hook = Bih_HookIp4PostRouting,
        .owner = THIS_MODULE,
        .pf = PF_INET,
        .hooknum = NF_INET_POST_ROUTING,
        .priority = NF_IP_PRI_FIRST,
};


struct nf_hook_ops nfho_bih_ip6 =
{
        .hook = Bih_HookIp6PreRouting,
        .owner = THIS_MODULE,
        .pf = PF_INET6,
        .hooknum = NF_INET_PRE_ROUTING,
        .priority = NF_IP6_PRI_FIRST,
};

static const struct file_operations proc_file_mod_cfg_fops = {
       .open           = proc_file_mod_cfg_open,
       .read           = seq_read,
       .write          = proc_file_mod_cfg_write,
       .llseek         = seq_lseek,
       .release        = seq_release,
};

static const struct file_operations proc_file_mod_log_fops = {
       .open           = proc_file_mod_log_open,
       .read           = seq_read,
       .write          = proc_file_mod_log_write,
       .llseek         = seq_lseek,
       .release        = seq_release,
};

static const struct file_operations proc_file_map_host_fops = {
       .open           = proc_file_map_host_open,
       .read           = seq_read,
       .write          = proc_file_map_host_write,
       .llseek         = seq_lseek,
       .release        = seq_release,
};

static const struct file_operations proc_file_map_dns_fops = {
       .open           = proc_file_map_dns_open,
       .read           = seq_read,
       .write          = proc_file_map_dns_write,
       .llseek         = seq_lseek,
       .release        = seq_release,
};

static const struct file_operations proc_file_map_remote_fops = {
       .open           = proc_file_map_remote_open,
       .read           = seq_read,
       .write          = proc_file_map_remote_write,
       .llseek         = seq_lseek,
       .release        = seq_release,
};

static const struct file_operations proc_file_map_list_fops = {
       .open           = proc_file_map_list_open,
       .read           = seq_read,
       .write          = proc_file_map_list_write,
       .llseek         = seq_lseek,
       .release        = seq_release,
};

static const struct file_operations proc_file_dns_list_fops = {
       .open           = proc_file_dns_list_open,
       .read           = seq_read,
       .write          = proc_file_dns_list_write,
       .llseek         = seq_lseek,
       .release        = seq_release,
};

static const struct file_operations proc_file_match_list_fops = {
       .open           = proc_file_match_list_open,
       .read           = seq_read,
       .write          = proc_file_match_list_write,
       .llseek         = seq_lseek,
       .release        = seq_release,
};

static BIH_OBJ bih_obj;
static struct timer_list bih_timer;
static struct proc_dir_entry *proc_dir,*proc_file_mod_cfg,*proc_file_mod_log;
static struct proc_dir_entry *proc_file_map_host,*proc_file_map_dns,*proc_file_map_remote;
static struct proc_dir_entry *proc_file_map_list,*proc_file_dns_list,*proc_file_match_list;
//static struct net_device *fanyidev;

DEFINE_SPINLOCK(bih_dns_lock);
DEFINE_SPINLOCK(bih_map_lock);
DEFINE_SPINLOCK(bih_match_lock);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LEADCORETECH");
MODULE_DESCRIPTION("BIH");
MODULE_INFO(vermagic,VERMAGIC_STRING);
module_init(Bih_Init);
module_exit(Bih_Cleanup);

static int __init Bih_Init(void)
{
	memset(&bih_obj,0,sizeof(BIH_OBJ));

	if(Bih_InitProc()!= BIH_RES_SUCCESS)
	{
		return -1;
	}

	if(Bih_InitHook()!= BIH_RES_SUCCESS)
	{
		Bih_CleanupProc();
		return -1;
	}

//	fanyidev=dev_get_by_name(&init_net,"lo");

	init_timer(&bih_timer);
	bih_timer.data = 0;
	bih_timer.function = Bih_TimerCb;

	BIH_LOG(0,"Bih_Init ok");

	return 0;
}

static void __exit Bih_Cleanup(void)
{
	del_timer(&bih_timer);

	Bih_CleanupHook();
	Bih_CleanupProc();

	BIH_LOG(0,"Bih_Cleanup ok");

	return;
}

static unsigned int Bih_HookIp4PostRouting(unsigned int hooknum,struct sk_buff *skb,const struct net_device *in,const struct net_device *out,int (*okfn)(struct sk_buff *))
{
	if(!bih_obj.run || !bih_obj.host_valid)
	{
		return NF_ACCEPT;
	}

	if(skb==NULL || out==NULL || ntohs(skb->protocol)!=ETH_P_IP)
	{
		return NF_ACCEPT;
	}

	if(Bih_Pack46(&bih_obj,skb,(struct net_device *)out)!= BIH_RES_SUCCESS)
	{
		return NF_ACCEPT;
	}

	return NF_STOLEN;
}

static unsigned int Bih_HookIp6PreRouting(unsigned int hooknum,struct sk_buff *skb,const struct net_device *in,const struct net_device *out,int (*okfn)(struct sk_buff *))
{
	if(!bih_obj.run || !bih_obj.host_valid)
	{
		return NF_ACCEPT;
	}

	if(skb==NULL || in==NULL || ntohs(skb->protocol)!=ETH_P_IPV6)
	{
		return NF_ACCEPT;
	}

	if(Bih_Pack64(&bih_obj,skb,(struct net_device *)in)!= BIH_RES_SUCCESS)
	{
		return NF_ACCEPT;
	}

	return NF_STOLEN;
}

static int show_mod_cfg(struct seq_file *m, void *v)
{
	return seq_printf(m, "%d\n", (bih_obj.run ? 1 : 0));
}

static int proc_file_mod_cfg_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_mod_cfg, NULL);
}

static int proc_file_mod_cfg_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *offp)
{
	BOOLE old_run;

	old_run = bih_obj.run;

	Bih_ModWriteCfg(&bih_obj,(U8 *)buffer,count);

	if(!old_run && bih_obj.run)
	{
		Bih_ModStart(&bih_obj);
	}
	else if(old_run && !bih_obj.run)
	{
		Bih_ModStop(&bih_obj);
	}
	return count;
}

static int show_mod_log(struct seq_file *m, void *v)
{
	return seq_printf(m, "%d\n", bih_obj.log_level);
}

static int proc_file_mod_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_mod_log, NULL);
}

static int proc_file_mod_log_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *offp)
{
	Bih_ModWriteLog(&bih_obj,(U8 *)buffer,count);

	return count;
}

static int show_map_host(struct seq_file *m, void *v)
{
	return Bih_MapReadHost(&bih_obj, m);
}

static int proc_file_map_host_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_map_host, NULL);
}

static int proc_file_map_host_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *offp)
{
	U32 i;

	Bih_MapWriteHost(&bih_obj,(U8 *)buffer,count);

	//check host
	for(i=0;i<BIH_MAP_HOST_NUM;i++)
	{
		if(bih_obj.host[i].addr4.s_addr != 0)//host valid
		{
			break;
		}
	}

	if(i < BIH_MAP_HOST_NUM)
	{
		bih_obj.host_valid = BOOLE_TRUE;
	}
	else
	{
		bih_obj.host_valid = BOOLE_FALSE;
	}

	return count;
}

static int show_map_dns(struct seq_file *m, void *v)
{
	return Bih_MapReadDns(&bih_obj, m);
}

static int proc_file_map_dns_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_map_dns, NULL);
}

static int proc_file_map_dns_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *offp)
{
	Bih_MapWriteDns(&bih_obj,(U8 *)buffer,count);

	return count;
}

static int show_map_remote(struct seq_file *m, void *v)
{
	return Bih_MapReadRemote(&bih_obj, m);
}

static int proc_file_map_remote_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_map_remote, NULL);
}

static int proc_file_map_remote_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *offp)
{
	Bih_MapWriteRemote(&bih_obj,(U8 *)buffer,count);

	return count;
}

static int show_map_list(struct seq_file *m, void *v)
{
	return Bih_MapReadList(&bih_obj, m);
}

static int proc_file_map_list_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_map_list, NULL);
}

static int proc_file_map_list_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *offp)
{
	Bih_MapWriteList(&bih_obj,(U8 *)buffer,count);

	return count;
}

static int show_dns_list(struct seq_file *m, void *v)
{
	return Bih_DnsReadList(&bih_obj, m);
}

static int proc_file_dns_list_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_dns_list, NULL);
}

static int proc_file_dns_list_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *offp)
{
	Bih_DnsWriteList(&bih_obj,(U8 *)buffer,count);

	return count;
}

static int show_match_list(struct seq_file *m, void *v)
{
	return Bih_MatchReadList(&bih_obj, m);
}

static int proc_file_match_list_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_match_list, NULL);
}

static int proc_file_match_list_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *offp)
{
	Bih_MatchWriteList(&bih_obj,(U8 *)buffer,count);

	return count;
}

static void Bih_TimerCb(unsigned long data)
{
	if(bih_obj.run)
	{
		Bih_DnsTimerCb(&bih_obj);
		Bih_MapTimerCb(&bih_obj);
		Bih_MatchTimerCb(&bih_obj);

		bih_timer.expires = jiffies + BIH_MOD_TIME_CB;
		add_timer(&bih_timer);
	}

	return;
}


RESULT Bih_InitHook(void)
{
	if(nf_register_hook(&nfho_bih_ip4)!= 0)
	{
		BIH_LOG(0,"Bih_InitHook: init hook ip4 error");
		return BIH_RES_ERROR;
	}

	if(nf_register_hook(&nfho_bih_ip6)!= 0)
	{
		BIH_LOG(0,"Bih_InitHook: init hook ip6 error");
		nf_unregister_hook(&nfho_bih_ip4);
		return BIH_RES_ERROR;
	}

	return BIH_RES_SUCCESS;
}


void Bih_CleanupHook(void)
{
	nf_unregister_hook(&nfho_bih_ip4);
	nf_unregister_hook(&nfho_bih_ip6);

	return;
}


RESULT Bih_InitProc(void)
{
	proc_dir = proc_mkdir(BIH_PROC_DIR_NAME,NULL);
	if(proc_dir == NULL)
	{
		BIH_LOG(0,"Bih_InitProc: proc_dir error");
		goto error_dir;
	}

	proc_file_mod_cfg = proc_create_data(BIH_PROC_MOD_CFG_NAME, 00666, proc_dir,
                                                       &proc_file_mod_cfg_fops, NULL);
	if(proc_file_mod_cfg == NULL)
	{
		BIH_LOG(0,"Bih_InitProc: proc_file_mod_cfg error");
		goto error_mod_cfg;
	}

	proc_file_mod_log = proc_create_data(BIH_PROC_MOD_LOG_NAME, 00666, proc_dir,
                                                       &proc_file_mod_log_fops, NULL);
	if(proc_file_mod_log == NULL)
	{
		BIH_LOG(0,"Bih_InitProc: proc_file_mod_log error");
		goto error_mod_log;
	}

	proc_file_map_host = proc_create_data(BIH_PROC_MAP_HOST_NAME, 00666, proc_dir,
                                                       &proc_file_map_host_fops, NULL);
	if(proc_file_map_host == NULL)
	{
		BIH_LOG(0,"Bih_InitProc: proc_file_map_host error");
		goto error_map_host;
	}

	proc_file_map_dns = proc_create_data(BIH_PROC_MAP_DNS_NAME, 00666, proc_dir,
                                                       &proc_file_map_dns_fops, NULL);
	if(proc_file_map_dns == NULL)
	{
		BIH_LOG(0,"Bih_InitProc: proc_file_map_dns error");
		goto error_map_dns;
	}

	proc_file_map_remote = proc_create_data(BIH_PROC_MAP_REMOTE_NAME, 00666, proc_dir,
                                                       &proc_file_map_remote_fops, NULL);
	if(proc_file_map_remote == NULL)
	{
		BIH_LOG(0,"Bih_InitProc: proc_file_map_remote error");
		goto error_map_remote;
	}

	proc_file_map_list = proc_create_data(BIH_PROC_MAP_LIST, 00666, proc_dir,
                                                       &proc_file_map_list_fops, NULL);
	if(proc_file_map_list == NULL)
	{
		BIH_LOG(0,"Bih_InitProc: proc_file_map_list error");
		goto error_map_list;
	}

	proc_file_dns_list = proc_create_data(BIH_PROC_DNS_LIST, 00666, proc_dir,
                                                       &proc_file_dns_list_fops, NULL);
	if(proc_file_dns_list == NULL)
	{
		BIH_LOG(0,"Bih_InitProc: proc_file_dns_list error");
		goto error_dns_list;
	}

	proc_file_match_list = proc_create_data(BIH_PROC_MATCH_LIST, 00666, proc_dir,
                                                       &proc_file_match_list_fops, NULL);
	if(proc_file_match_list == NULL)
	{
		BIH_LOG(0,"Bih_InitProc: proc_file_match_list error");
		goto error_match_list;
	}

	return BIH_RES_SUCCESS;

error_match_list:
	remove_proc_entry(BIH_PROC_DNS_LIST,proc_dir);
error_dns_list:
	remove_proc_entry(BIH_PROC_MAP_LIST,proc_dir);
error_map_list:
	remove_proc_entry(BIH_PROC_MAP_REMOTE_NAME,proc_dir);
error_map_remote:
	remove_proc_entry(BIH_PROC_MAP_DNS_NAME,proc_dir);
error_map_dns:
	remove_proc_entry(BIH_PROC_MAP_HOST_NAME,proc_dir);
error_map_host:
	remove_proc_entry(BIH_PROC_MOD_LOG_NAME,proc_dir);
error_mod_log:
	remove_proc_entry(BIH_PROC_MOD_CFG_NAME,proc_dir);
error_mod_cfg:
	remove_proc_entry(BIH_PROC_DIR_NAME,NULL);
error_dir:
	return BIH_RES_ERROR;
}


void Bih_CleanupProc(void)
{
	remove_proc_entry(BIH_PROC_MATCH_LIST,proc_dir);
	remove_proc_entry(BIH_PROC_DNS_LIST,proc_dir);
	remove_proc_entry(BIH_PROC_MAP_LIST,proc_dir);
	remove_proc_entry(BIH_PROC_MAP_REMOTE_NAME,proc_dir);
	remove_proc_entry(BIH_PROC_MAP_DNS_NAME,proc_dir);
	remove_proc_entry(BIH_PROC_MAP_HOST_NAME,proc_dir);
	remove_proc_entry(BIH_PROC_MOD_LOG_NAME,proc_dir);
	remove_proc_entry(BIH_PROC_MOD_CFG_NAME,proc_dir);
	remove_proc_entry(BIH_PROC_DIR_NAME,NULL);

	return;
}


RESULT Bih_ModReadCfg(BIH_OBJ *bih,U8 *buf,U32 *buf_len)
{
	S32 res;
	U32 len,tol_len;

	len = 0;
	tol_len = *buf_len - 1;

	res = snprintf(buf+len,tol_len-len,"%d\n",(bih->run ? 1 : 0));
	if(res < 0)
	{
		return BIH_RES_FAILED;
	}
	len += res;

	buf[tol_len] = '\0';
	*buf_len = len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_ModWriteCfg(BIH_OBJ *bih,U8 *buf,U32 buf_len)
{
	U32 u0,offset,len;

	offset = 0;

	len = buf_len - offset;
	if(Bih_BufGetDecimal(buf+offset,&len,&u0)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	bih->run = (u0!=0)? BOOLE_TRUE : BOOLE_FALSE;

	return BIH_RES_SUCCESS;
}

RESULT Bih_ModWriteLog(BIH_OBJ *bih,U8 *buf,U32 buf_len)
{
	U32 u0,offset,len;

	offset = 0;

	//log_level
	len = buf_len - offset;
	if(Bih_BufGetDecimal(buf+offset,&len,&u0)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	bih->log_level = u0;

	return BIH_RES_SUCCESS;
}


void Bih_ModStart(BIH_OBJ *bih)
{
	U32 i,j,u0,u1;

	//dns
	Bih_LinkInit(&bih->dns_free);

	for(i=0;i<BIH_DNS_REC_LIST_NUM;i++)
	{
		Bih_LinkInit(&bih->dns_list[i]);
	}

	for(i=0;i<BIH_DNS_REC_BUF_NUM;i++)
	{
		bih->dns_buf[i].rsp = NULL;
		Bih_LinkPutFront(&bih->dns_free,&bih->dns_buf[i]);
	}

	//map
	Bih_LinkInit(&bih->map_free);

	for(i=0;i<BIH_MAP_ADDR_LIST_NUM;i++)
	{
		Bih_LinkInit(&bih->map4_list[i]);
	}

	for(i=0;i<BIH_MAP_ADDR_LIST_NUM;i++)
	{
		Bih_LinkInit(&bih->map6_list[i]);
	}

	u0 = ntohl(bih->dst_addr0.s_addr);
	u1 = ntohl(bih->dst_addr1.s_addr);
	j = (u1>u0)? u1-u0 : 0;

	for(i=0;i<j && i<BIH_MAP_ADDR_BUF_NUM;i++)
	{
		bih->map_buf[i].map.addr4.s_addr = htonl(u0+i);
		Bih_LinkInit(&bih->map_buf[i].ptr4);
		Bih_LinkInit(&bih->map_buf[i].ptr6);
		Bih_LinkPutFront(&bih->map_free,&bih->map_buf[i]);
	}

	//match
	Bih_LinkInit(&bih->match_free);

	for(i=0;i<BIH_MATCH_ITEM_LIST_NUM;i++)
	{
		Bih_LinkInit(&bih->match_list[i]);
	}
	for(i=0;i<BIH_MATCH_ITEM_BUF_NUM;i++)
	{
		Bih_LinkPutFront(&bih->match_free,&bih->match_buf[i]);
	}

	//timer
	bih_timer.expires = jiffies + BIH_MOD_TIME_CB;
	add_timer(&bih_timer);

	BIH_LOG(0,"Bih_ModStart: ok");

	return;
}


void Bih_ModStop(BIH_OBJ *bih)
{
	del_timer(&bih_timer);

	BIH_LOG(0,"Bih_ModStop: ok");

	return;
}



/*bih_pack.c**********************************************************/


RESULT Bih_Pack46(BIH_OBJ *bih,struct sk_buff *skb4,struct net_device *out)
{
	BOOLE add_item,free_item;
	U32 u0,u1,u2;
	struct sk_buff *skb,*skb_linear;
	struct dst_entry *dst;
	struct icmphdr *icmph4;
	struct flowi6 fl6;
	BIH_MATCH_ITEM *item;
	BIH_TRANS_INFO info;

	skb = NULL;
	skb_linear = NULL;
	dst = NULL;
	item = NULL;
	add_item = BOOLE_FALSE;
	free_item = BOOLE_FALSE;

	BIH_LOG(3,"Bih_Pack46: start");

	if(skb_is_nonlinear(skb4))
	{
		BIH_LOG(2,"Bih_Pack46: skb nonlinear");

		skb_linear = skb_copy(skb4,GFP_ATOMIC);
		if(skb_linear == NULL)
		{
			BIH_LOG(1,"Bih_Pack46: skb_copy error");
			goto error;
		}
		skb = skb_linear;
	}
	else
	{
		skb = skb4;
	}

	//check pack
	if(Bih_PackCheck46(&info,skb->data,skb->len)!= BIH_RES_SUCCESS)
	{
		goto error;
	}

	if(skb->len != info.src_ip_len+info.src_tp_len+info.src_pl_len)
	{
		BIH_LOG(2,"Bih_Pack46: skb len error");
		goto error;
	}
	//map addr
	BIH_LOG(2,"Bih_Pack46: saddr4=%d.%d.%d.%d,daddr4=%d.%d.%d.%d",BIH_SEGMENT_ADDR4(&info.saddr4.s_addr),BIH_SEGMENT_ADDR4(&info.daddr4.s_addr));

	if(Bih_MapAddr46(bih,&info.saddr4,&info.daddr4,&info.saddr6,&info.daddr6)!= BIH_RES_SUCCESS)
	{
		goto error;
	}

	BIH_LOG(2,"Bih_Pack46: saddr6=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x,daddr6=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
				ntohs(info.saddr6.s6_addr16[0]),
				ntohs(info.saddr6.s6_addr16[1]),
				ntohs(info.saddr6.s6_addr16[2]),
				ntohs(info.saddr6.s6_addr16[3]),
				ntohs(info.saddr6.s6_addr16[4]),
				ntohs(info.saddr6.s6_addr16[5]),
				ntohs(info.saddr6.s6_addr16[6]),
				ntohs(info.saddr6.s6_addr16[7]),
				ntohs(info.daddr6.s6_addr16[0]),
				ntohs(info.daddr6.s6_addr16[1]),
				ntohs(info.daddr6.s6_addr16[2]),
				ntohs(info.daddr6.s6_addr16[3]),
				ntohs(info.daddr6.s6_addr16[4]),
				ntohs(info.daddr6.s6_addr16[5]),
				ntohs(info.daddr6.s6_addr16[6]),
				ntohs(info.daddr6.s6_addr16[7]));

	//search route
	memset(&fl6,0,sizeof(fl6));
	fl6.saddr = info.saddr6;
	fl6.daddr = info.daddr6;

	dst = ip6_route_output(dev_net(out),NULL,&fl6);
	if(dst==NULL || dst->error!=0 || dst->dev==NULL)
	{
		BIH_LOG(2,"Bih_Pack46: no route found");
		goto error;
	}

	//add match item
	if(!(info.flag & BIH_TRANS_INFO_FLAG_SRC_FRAG))
	{
		if(info.flag & BIH_TRANS_INFO_FLAG_ICMP)
		{
			icmph4 = (struct icmphdr *)(skb4->data+info.src_ip_len);

			if(icmph4->type == ICMP_ECHO)
			{
				u0 = BIH_MATCH_ITEM_TYPE_ICMP;
				u1 = icmph4->un.echo.id<<16 | icmph4->un.echo.sequence;
				u2 = BIH_MATCH_ITEM_FLAG_TEMP;
				add_item = BOOLE_TRUE;
				free_item = BOOLE_TRUE;
			}
		}
		else if(info.flag & BIH_TRANS_INFO_FLAG_TCP)
		{
			u0 = BIH_MATCH_ITEM_TYPE_TCP;
			u1 = BIH_READ_BUF16BE(skb4->data+info.src_ip_len);//port
			u2 = 0;
			add_item = BOOLE_TRUE;
		}
		else if(info.flag & BIH_TRANS_INFO_FLAG_UDP)
		{
			u0 = BIH_MATCH_ITEM_TYPE_UDP;
			u1 = BIH_READ_BUF16BE(skb4->data+info.src_ip_len);//port
			u2 = 0;
			add_item = BOOLE_TRUE;
		}

		if(add_item)
		{
			item = Bih_MatchAddItem(bih,&info.saddr4,u0,u1,u2);
			if(item == NULL)
			{
				BIH_LOG(1,"Bih_Pack46: add match item error");
				goto error;
			}
		}
	}
	//transfer
	if(info.flag & BIH_TRANS_INFO_FLAG_DNS)
	{
		if(Bih_PackDns46(bih,&info,skb,dst)!= BIH_RES_SUCCESS)
		{
			goto error;
		}
	}
	else if(info.flag & BIH_TRANS_INFO_FLAG_ICMP)
	{
		if(Bih_PackIcmp46(bih,&info,skb,dst)!= BIH_RES_SUCCESS)
		{
			goto error;
		}
	}
	else
	{
		if(Bih_PackNormal46(bih,&info,skb,dst)!= BIH_RES_SUCCESS)
		{
			goto error;
		}
	}

	//ok
	dst_release(dst);

	if(skb_linear != NULL)
	{
		dev_kfree_skb(skb_linear);
	}

	kfree_skb(skb4);

	BIH_LOG(3,"Bih_Pack46: ok");

	return BIH_RES_SUCCESS;

error:
	if(dst != NULL)
	{
		dst_release(dst);
	}
	if(skb_linear != NULL)
	{
		dev_kfree_skb(skb_linear);
	}
	if(item!=NULL && free_item)
	{
		Bih_MatchFreeItem(bih,item);
	}
	return BIH_RES_FAILED;
}


RESULT Bih_PackDns46(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb4,struct dst_entry *dst)
{
	BOOLE addr_ok,map_ok;
	U32 port,id;
	struct sk_buff *skb6;
	struct net_device *dev;
	BIH_DNS_REC *rec;
	BIH_DNS_MSG msg;
	BIH_TRANS_PACK ip_tp,pl;

	skb6 = NULL;
	rec = NULL;
	dev = dst->dev;
	port = BIH_READ_BUF16BE(skb4->data+info->src_ip_len);
	id = BIH_READ_BUF16BE(skb4->data+info->src_ip_len+info->src_tp_len);

	BIH_LOG(3,"Bih_PackDns46: port=%d,id=%d",port,id);

	if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
	{
		BIH_LOG(2,"Bih_PackDns46: unsupport ip fragment");
		goto error;
	}

	if(dev->mtu < info->dst_ip_len+info->dst_tp_len)
	{
		BIH_LOG(2,"Bih_PackDns46: mtu too small");
		goto error;
	}

	//add transfer rec
	rec = Bih_DnsAddRec(bih,&info->saddr4,port,id);
	if(rec == NULL)
	{
		BIH_LOG(1,"Bih_PackDns46: alloc dns rec error");
		goto error;
	}

	//dns IN AAAA
	skb6 = dev_alloc_skb(dev->hard_header_len+dev->mtu);//actual size will be set later
	if(skb6 == NULL)
	{
		BIH_LOG(1,"Bih_PackDns46: alloc skb error");
		goto error;
	}

	msg.src_msg = skb4->data + info->src_ip_len + info->src_tp_len;
	msg.src_msg_len = info->src_pl_len;
	msg.dst_msg = skb6->data + dev->hard_header_len + info->dst_ip_len + info->dst_tp_len;
	msg.dst_msg_len = 0;
	msg.dst_buf_len = dev->mtu - info->dst_ip_len - info->dst_tp_len;

	if(Bih_DnsReq46(bih,&msg,&addr_ok,&map_ok)!= BIH_RES_SUCCESS)
	{
		goto error;
	}

	if(!addr_ok)
	{
		BIH_LOG(2,"Bih_PackDns46: no dns IN req found");
		goto error;
	}

	//trans to AAAA req
	BIH_WRITE_BUF16BE(msg.dst_msg,(id+1)&0xffff);//inc id in AAAA req

	BIH_PACK_SET_SRC(&ip_tp,skb4->data,info->src_ip_len+info->src_tp_len,0);
	BIH_PACK_SET_DST(&ip_tp,skb6->data+dev->hard_header_len,info->dst_ip_len+info->dst_tp_len,0);
	BIH_PACK_SET_SRC(&pl,msg.dst_msg,msg.dst_msg_len,0);

	if(Bih_PackTrans46(info,&ip_tp,&pl,BOOLE_FALSE)!= BIH_RES_SUCCESS)
	{
		goto error;
	}

	info->dst_pl_len = msg.dst_msg_len;

	if(Bih_PackFillSkb46(skb6,info->dst_ip_len+info->dst_tp_len+info->dst_pl_len,dev)!= BIH_RES_SUCCESS)
	{
		goto error;
	}

	dst_hold(dst);
	skb_dst_set(skb6,dst);
	skb_dst(skb6)->output(skb6);
	skb6 = NULL;

	//dns IN A
	if(map_ok)//there is a IN A req
	{
		skb6 = dev_alloc_skb(dev->hard_header_len+dev->mtu);//actual size will be set later
		if(skb6 == NULL)
		{
			BIH_LOG(1,"Bih_PackDns46: alloc skb error");
			goto error;
		}

		BIH_PACK_SET_SRC(&ip_tp,skb4->data,info->src_ip_len+info->src_tp_len,0);
		BIH_PACK_SET_DST(&ip_tp,skb6->data+dev->hard_header_len,info->dst_ip_len+info->dst_tp_len,0);
		BIH_PACK_SET_SRC(&pl,skb4->data+info->src_ip_len+info->src_tp_len,info->src_pl_len,0);
		BIH_PACK_SET_DST(&pl,skb6->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len,dev->mtu-info->dst_ip_len-info->dst_tp_len,0);

		if(Bih_PackTrans46(info,&ip_tp,&pl,BOOLE_TRUE)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		info->dst_pl_len = pl.dst_data_filled;

		if(Bih_PackFillSkb46(skb6,info->dst_ip_len+info->dst_tp_len+info->dst_pl_len,dev)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		dst_hold(dst);
		skb_dst_set(skb6,dst);
		skb_dst(skb6)->output(skb6);
		skb6 = NULL;
	}
	else//no dns IN A req found
	{
		BIH_LOG(2,"Bih_PackDns46: no dns IN A req found");
	}

	return BIH_RES_SUCCESS;

error:
	if(skb6 != NULL)
	{
		dev_kfree_skb(skb6);
	}
	if(rec != NULL)
	{
		Bih_DnsFreeRec(bih,rec);
	}
	return BIH_RES_FAILED;
}


RESULT Bih_PackIcmp46(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb4,struct dst_entry *dst)
{
	struct sk_buff *skb6;
	struct net_device *dev;
	struct icmphdr *icmph4;
	BIH_TRANS_PACK ip_tp,pl;
	BIH_TRANS_INFO info2;

	skb6 = NULL;
	dev = dst->dev;
	icmph4 = (struct icmphdr *)(skb4->data+info->src_ip_len);

	if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
	{
		BIH_LOG(2,"Bih_PackIcmp46: unsupport ip fragment");
		goto error;
	}

	if(icmph4->type==ICMP_ECHO || icmph4->type==ICMP_ECHOREPLY)
	{
		if(info->dst_ip_len+info->dst_tp_len+info->src_pl_len > dev->mtu)
		{
			BIH_LOG(3,"Bih_PackIcmp46: ip frag");
			info->flag |= BIH_TRANS_INFO_FLAG_DST_FRAG;
			info->dst_ip_len = BIH_IP6_HDR_SIZE + BIH_IP6_HDR_FRAGMENT_SIZE;
		}
	}

	if(dev->mtu < info->dst_ip_len+info->dst_tp_len)
	{
		BIH_LOG(2,"Bih_PackIcmp46: mtu too small");
		goto error;
	}

	//transfer
	if(icmph4->type==ICMP_ECHO || icmph4->type==ICMP_ECHOREPLY)
	{
		BIH_PACK_SET_SRC(&pl,skb4->data+info->src_ip_len+info->src_tp_len,info->src_pl_len,0);

		while(1)
		{
			skb6 = dev_alloc_skb(dev->hard_header_len+dev->mtu);//actual size will be set later
			if(skb6 == NULL)
			{
				BIH_LOG(1,"Bih_PackIcmp46: alloc skb error");
				goto error;
			}

			BIH_PACK_SET_SRC(&ip_tp,skb4->data,info->src_ip_len+info->src_tp_len,0);
			BIH_PACK_SET_DST(&ip_tp,skb6->data+dev->hard_header_len,info->dst_ip_len+info->dst_tp_len,0);
			BIH_PACK_SET_DST(&pl,skb6->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len,dev->mtu-info->dst_ip_len-info->dst_tp_len,0);

			if(Bih_PackTrans46(info,&ip_tp,&pl,BOOLE_TRUE)!= BIH_RES_SUCCESS)
			{
				goto error;
			}

			info->dst_pl_len = pl.dst_data_filled;

			if(Bih_PackFillSkb46(skb6,info->dst_ip_len+info->dst_tp_len+info->dst_pl_len,dev)!= BIH_RES_SUCCESS)
			{
				goto error;
			}
			//send
			dst_hold(dst);
			skb_dst_set(skb6,dst);
			skb_dst(skb6)->output(skb6);
			skb6 = NULL;

			if(pl.src_data_used >= pl.src_buf_len)
			{
				break;
			}
			else//more ip fragment
			{
				info->src_tp_len = 0;
				info->dst_tp_len = 0;
			}
		}
	}
	else
	{
		BIH_LOG(3,"Bih_PackIcmp46: err msg");

		//check payload
		if(Bih_PackCheck46(&info2,skb4->data+info->src_ip_len+info->src_tp_len,info->src_pl_len)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		if(info2.flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
		{
			info2.flag |= BIH_TRANS_INFO_FLAG_DST_FRAG;
			info2.dst_ip_len = BIH_IP6_HDR_SIZE + BIH_IP6_HDR_FRAGMENT_SIZE;
		}

		if(info->src_pl_len < info2.src_ip_len+info2.src_tp_len+info2.src_pl_len)
		{
			BIH_LOG(2,"Bih_PackIcmp46: payload turncation");
			info2.src_pl_len = info->src_pl_len - info2.src_ip_len - info2.src_tp_len;
		}

		if(Bih_MapAddr46(bih,&info2.daddr4,&info2.saddr4,&info2.daddr6,&info2.saddr6)!= BIH_RES_SUCCESS)
		{
			BIH_LOG(3,"Bih_Pack46: map addr error");
			goto error;
		}

		//transfer payload
		if(dev->mtu < info->dst_ip_len+info->dst_tp_len+info2.dst_ip_len+info2.dst_tp_len)
		{
			BIH_LOG(2,"Bih_PackIcmp46: mtu too small");
			goto error;
		}
		skb6 = dev_alloc_skb(dev->hard_header_len+dev->mtu);//actual size will be set later
		if(skb6 == NULL)
		{
			BIH_LOG(1,"Bih_PackIcmp46: alloc skb error");
			goto error;
		}

		BIH_PACK_SET_SRC(&ip_tp,skb4->data+info->src_ip_len+info->src_tp_len,info2.src_ip_len+info2.src_tp_len,0);
		BIH_PACK_SET_DST(&ip_tp,skb6->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len,info2.dst_ip_len+info2.dst_tp_len,0);
		BIH_PACK_SET_SRC(&pl,skb4->data+info->src_ip_len+info->src_tp_len+info2.src_ip_len+info2.src_tp_len,info2.src_pl_len,0);
		BIH_PACK_SET_DST(&pl,skb6->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len+info2.dst_ip_len+info2.dst_tp_len,dev->mtu-info->dst_ip_len-info->dst_tp_len-info2.dst_ip_len-info2.dst_tp_len,0);

		if(Bih_PackTrans46(&info2,&ip_tp,&pl,BOOLE_TRUE)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		info2.dst_pl_len = pl.dst_data_filled;
		info->dst_pl_len = info2.dst_ip_len + info2.dst_tp_len + info2.dst_pl_len;

		//transfer pack
		BIH_PACK_SET_SRC(&ip_tp,skb4->data,info->src_ip_len+info->src_tp_len,0);
		BIH_PACK_SET_DST(&ip_tp,skb6->data+dev->hard_header_len,info->dst_ip_len+info->dst_tp_len,0);
		BIH_PACK_SET_SRC(&pl,skb6->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len,info->dst_pl_len,0);

		if(Bih_PackTrans46(&info2,&ip_tp,&pl,BOOLE_FALSE)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		if(Bih_PackFillSkb46(skb6,info->dst_ip_len+info->dst_tp_len+info->dst_pl_len,dev)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		//send
		dst_hold(dst);
		skb_dst_set(skb6,dst);
		skb_dst(skb6)->output(skb6);
		skb6 = NULL;
	}

	return BIH_RES_SUCCESS;

error:
	if(skb6 != NULL)
	{
		dev_kfree_skb(skb6);
	}
	return BIH_RES_FAILED;
}


RESULT Bih_PackNormal46(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb4,struct dst_entry *dst)
{
	struct sk_buff *skb6;
	struct net_device *dev;
	BIH_TRANS_PACK ip_tp,pl;

	skb6 = NULL;
	dev = dst->dev;

	if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
	{
		BIH_LOG(2,"Bih_PackIcmp46: unsupport ip fragment");
		goto error;
	}

	if(info->dst_ip_len+info->dst_tp_len+info->src_pl_len > dev->mtu)
	{
		BIH_LOG(3,"Bih_PackNormal46: ip frag");
		info->flag |= BIH_TRANS_INFO_FLAG_DST_FRAG;
		info->dst_ip_len = BIH_IP6_HDR_SIZE + BIH_IP6_HDR_FRAGMENT_SIZE;
	}

	if(dev->mtu < info->dst_ip_len+info->dst_tp_len)
	{
		BIH_LOG(2,"Bih_PackNormal46: mtu too small");
		goto error;
	}

	//transfer
	BIH_PACK_SET_SRC(&pl,skb4->data+info->src_ip_len+info->src_tp_len,info->src_pl_len,0);

	while(1)
	{
		skb6 = dev_alloc_skb(dev->hard_header_len+dev->mtu);//actual size will be set later
		if(skb6 == NULL)
		{
			BIH_LOG(1,"Bih_PackNormal46: alloc skb error");
			goto error;
		}

		BIH_PACK_SET_SRC(&ip_tp,skb4->data,info->src_ip_len+info->src_tp_len,0);
		BIH_PACK_SET_DST(&ip_tp,skb6->data+dev->hard_header_len,info->dst_ip_len+info->dst_tp_len,0);
		BIH_PACK_SET_DST(&pl,skb6->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len,dev->mtu-info->dst_ip_len-info->dst_tp_len,0);

		if(Bih_PackTrans46(info,&ip_tp,&pl,BOOLE_TRUE)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		info->dst_pl_len = pl.dst_data_filled;

		if(Bih_PackFillSkb46(skb6,info->dst_ip_len+info->dst_tp_len+info->dst_pl_len,dev)!= BIH_RES_SUCCESS)
		{
			goto error;
		}
		//send
		dst_hold(dst);
		skb_dst_set(skb6,dst);
		skb_dst(skb6)->output(skb6);
		skb6 = NULL;

		if(pl.src_data_used >= pl.src_buf_len)
		{
			break;
		}
		else//more ip fragment
		{
			info->src_tp_len = 0;
			info->dst_tp_len = 0;
		}
	}

	return BIH_RES_SUCCESS;

error:
	if(skb6 != NULL)
	{
		dev_kfree_skb(skb6);
	}
	return BIH_RES_ERROR;
}


RESULT Bih_PackCheck46(BIH_TRANS_INFO *info,U8 *buf,U32 buf_len)
{
	U32 u0,ih4_len,tcph_len;
	struct iphdr *ih4;
	struct icmphdr *icmph4;
	struct tcphdr *tcph;
	struct udphdr *udph;

	memset(info,0,sizeof(BIH_TRANS_INFO));

	//ip head
	ih4 = (struct iphdr *)buf;
	ih4_len = ih4->ihl * 4;
	if(ih4_len<BIH_IP4_HDR_SIZE || buf_len<ih4_len)
	{
		BIH_LOG(2,"Bih_PackCheck46: ip len error");
		return BIH_RES_FAILED;
	}
	if(ntohs(ih4->frag_off)& IP_OFFSET)
	{
		BIH_LOG(2,"Bih_PackCheck46: ip fragment");

		u0 = ntohs(ih4->frag_off);
		info->src_frag_offset = u0<<3 & 0xffff;
		info->flag |= (u0 & IP_MF)? BIH_TRANS_INFO_FLAG_SRC_FRAG : BIH_TRANS_INFO_FLAG_SRC_FRAG|BIH_TRANS_INFO_FLAG_SRC_FRAG_LAST;
	}

	info->src_protocol = ih4->protocol;
	info->src_frag_id = ntohs(ih4->id);
	info->src_ip_len = ih4_len;
	info->saddr4.s_addr = ih4->saddr;
	info->daddr4.s_addr = ih4->daddr;

	//tp head
	if((info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)&& info->src_frag_offset>0)
	{
		switch(ih4->protocol)
		{
		case IPPROTO_ICMP:
			info->flag |= BIH_TRANS_INFO_FLAG_ICMP;
			info->dst_protocol = NEXTHDR_ICMP;
			break;

		case IPPROTO_TCP:
			info->flag |= BIH_TRANS_INFO_FLAG_TCP;
			info->dst_protocol = NEXTHDR_TCP;
			break;

		case IPPROTO_UDP:
			info->flag |= BIH_TRANS_INFO_FLAG_UDP;
			info->dst_protocol = NEXTHDR_UDP;
			break;

		default:
			BIH_LOG(2,"Bih_PackCheck46: unsupport protocol %d",ih4->protocol);
			return BIH_RES_FAILED;
		}
	}
	else
	{
		switch(ih4->protocol)
		{
		case IPPROTO_ICMP:
			info->flag |= BIH_TRANS_INFO_FLAG_ICMP;
			info->dst_protocol = NEXTHDR_ICMP;
			icmph4 = (struct icmphdr *)(buf + ih4_len);

			if(buf_len < ih4_len+BIH_ICMP4_HDR_SIZE)
			{
				BIH_LOG(2,"Bih_PackCheck46: icmp len error");
				return BIH_RES_FAILED;
			}

			if(icmph4->type==ICMP_DEST_UNREACH || icmph4->type==ICMP_TIME_EXCEEDED || icmph4->type==ICMP_PARAMETERPROB)
			{
			}
			else if(icmph4->type==ICMP_ECHO || icmph4->type==ICMP_ECHOREPLY)
			{
			}
			else
			{
				BIH_LOG(3,"Bih_PackCheck46: unsupport icmp type %d",icmph4->type);
				return BIH_RES_FAILED;
			}

			info->src_tp_len = BIH_ICMP4_HDR_SIZE;
			info->dst_tp_len = BIH_ICMP6_HDR_SIZE;
			break;

		case IPPROTO_TCP:
			info->flag |= BIH_TRANS_INFO_FLAG_TCP;
			info->dst_protocol = NEXTHDR_TCP;
			tcph = (struct tcphdr *)(buf + ih4_len);

			tcph_len = tcph->doff * 4;
			if(tcph_len<BIH_TCP_HDR_SIZE || buf_len<ih4_len+tcph_len)
			{
				BIH_LOG(2,"Bih_PackCheck46: tcp len error");
				return BIH_RES_FAILED;
			}
			info->src_tp_len = tcph_len;
			info->dst_tp_len = tcph_len;
			break;

		case IPPROTO_UDP:
			info->flag |= BIH_TRANS_INFO_FLAG_UDP;
			info->dst_protocol = NEXTHDR_UDP;
			udph = (struct udphdr *)(buf + ih4_len);

			if(buf_len < ih4_len+BIH_UDP_HDR_SIZE)
			{
				BIH_LOG(2,"Bih_PackCheck46: udp len error");
				return BIH_RES_FAILED;
			}

			if(ntohs(udph->source)==BIH_DNS_UDP_PORT || ntohs(udph->dest)==BIH_DNS_UDP_PORT)
			{
				info->flag |= BIH_TRANS_INFO_FLAG_DNS;
			}

			info->src_tp_len = BIH_UDP_HDR_SIZE;
			info->dst_tp_len = BIH_UDP_HDR_SIZE;
			break;

		default:
			BIH_LOG(2,"Bih_PackCheck46: unsupport protocol %d",ih4->protocol);
			return BIH_RES_FAILED;
		}
	}

	info->src_pl_len = ntohs(ih4->tot_len)- ih4_len - info->src_tp_len;
	info->dst_frag_id = info->src_frag_id;
	info->dst_frag_offset = info->src_frag_offset;
	info->dst_ip_len = BIH_IP6_HDR_SIZE;

	if(info->flag & BIH_TRANS_INFO_FLAG_DNS)
	{
		BIH_LOG(3,"Bih_PackCheck46: dns");
	}
	else if(info->flag & BIH_TRANS_INFO_FLAG_ICMP)
	{
		BIH_LOG(3,"Bih_PackCheck46: icmp");
	}
	else if(info->flag & BIH_TRANS_INFO_FLAG_TCP)
	{
		BIH_LOG(3,"Bih_PackCheck46: tcp");
	}
	else if(info->flag & BIH_TRANS_INFO_FLAG_UDP)
	{
		BIH_LOG(3,"Bih_PackCheck46: udp");
	}
	BIH_LOG(3,"Bih_PackCheck46: frag=%d,frag_last=%d,frag_off=%d,ip_len=%d,tp_len=%d,pl_len=%d",
		(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)? 1 : 0,
		(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG_LAST)? 1 : 0,
		info->src_frag_offset,
		info->src_ip_len,
		info->src_tp_len,
		info->src_pl_len);

	return BIH_RES_SUCCESS;
}


RESULT Bih_PackTrans46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *ip_tp,BIH_TRANS_PACK *pl,BOOLE copy_pl)
{
	BOOLE ip_last;
	U32 pl_len;
	BIH_TRANS_PACK pack2;

	ip_last = BOOLE_FALSE;

	if(ip_tp->dst_buf_len < info->dst_ip_len)
	{
		BIH_LOG(2,"Bih_PackTrans46: dst_buf_len too small");
		return BIH_RES_FAILED;
	}

	//transfer pl
	if(copy_pl)
	{
		BIH_PACK_SET_SRC(&pack2,pl->src_buf+pl->src_data_used,pl->src_buf_len-pl->src_data_used,0);
		BIH_PACK_SET_DST(&pack2,pl->dst_buf,pl->dst_buf_len,0);

		if(Bih_PackCopy46(info,&pack2,info->dst_tp_len)!= BIH_RES_SUCCESS)
		{
			return BIH_RES_FAILED;
		}

		pl_len = pack2.dst_data_filled;
		pl->src_data_used += pl_len;
		pl->dst_data_filled = pl_len;
	}
	else
	{
		pl_len = pl->src_buf_len - pl->src_data_used;
		pl->src_data_used = pl->src_buf_len;
	}

	//transfer tp
	if(info->src_tp_len > 0)
	{
		BIH_PACK_SET_SRC(&pack2,ip_tp->src_buf+info->src_ip_len,info->src_tp_len,0);
		BIH_PACK_SET_DST(&pack2,ip_tp->dst_buf+info->dst_ip_len,ip_tp->dst_buf_len-info->dst_ip_len,0);

		if(info->flag & BIH_TRANS_INFO_FLAG_ICMP)
		{
			if(Bih_ProtoIcmp46(info,&pack2,pl->src_buf,pl->src_buf_len)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}
		}
		else if(info->flag & BIH_TRANS_INFO_FLAG_TCP)
		{
			if(Bih_ProtoTcp46(info,&pack2,pl->src_buf,pl->src_buf_len)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}
		}
		else if(info->flag & BIH_TRANS_INFO_FLAG_UDP)
		{
			if(Bih_ProtoUdp46(info,&pack2,pl->src_buf,pl->src_buf_len)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}
		}
		else
		{
			return BIH_RES_FAILED;
		}
	}

	//transfer ip
	BIH_PACK_SET_SRC(&pack2,ip_tp->src_buf,info->src_ip_len,0);
	BIH_PACK_SET_DST(&pack2,ip_tp->dst_buf,info->dst_ip_len,0);
	if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
	{
		if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG_LAST)
		{
			ip_last = BOOLE_TRUE;
		}
	}
	else
	{
		if(pl->src_data_used >= pl->src_buf_len)
		{
			ip_last = BOOLE_TRUE;
		}
	}

	if(Bih_ProtoIp46(info,&pack2,info->dst_ip_len+info->dst_tp_len+pl_len,ip_last)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}

	if(info->flag & BIH_TRANS_INFO_FLAG_DST_FRAG)
	{
		if(!(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG))
		{
			info->dst_frag_offset += info->dst_tp_len + pl_len;
		}
	}

	ip_tp->dst_data_filled = info->dst_ip_len + info->dst_tp_len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_PackCopy46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U32 hdr_len)
{
	U32 len,pl_len;

	//cacu data len
	len = pack->dst_buf_len;
	if(len > pack->src_buf_len)
	{
		len = pack->src_buf_len;
	}

	pl_len = hdr_len + len;

	if(info->flag & BIH_TRANS_INFO_FLAG_DST_FRAG)//adjust payload len
	{
		pl_len &= BIH_IP_FRAG_OFFSET_MASK;
		if(pl_len==0 || pl_len<hdr_len)
		{
			BIH_LOG(2,"Bih_PackCopy46: copy len error");
			return BIH_RES_FAILED;
		}

		len = pl_len - hdr_len;
	}

	//copy data
	if(len > 0)
	{
		memcpy(pack->dst_buf,pack->src_buf,len);
		pack->src_data_used = len;
		pack->dst_data_filled = len;
	}

	return BIH_RES_SUCCESS;
}


RESULT Bih_PackFillSkb46(struct sk_buff *skb6,U32 pack_len,struct net_device *out)
{
	struct ethhdr *eth_h;

	skb_put(skb6,out->hard_header_len+pack_len);

	//eth hdr
	skb_reset_mac_header(skb6);
	if(out->hard_header_len >= sizeof(struct ethhdr))
	{
		eth_h = (struct ethhdr *)skb6->data;
		eth_h->h_proto = htons(ETH_P_IPV6);
	}
	skb_pull(skb6,out->hard_header_len);
	skb_reset_network_header(skb6);
	skb6->mac_len = out->hard_header_len;

	//ip hdr
	skb6->protocol = htons(ETH_P_IPV6);
	skb6->pkt_type = PACKET_OTHERHOST;//PACKET_OUTGOING;
	skb6->ip_summed = CHECKSUM_UNNECESSARY;
	skb6->dev = out;
	skb6->sk = NULL;

	return BIH_RES_SUCCESS;
}


RESULT Bih_Pack64(BIH_OBJ *bih,struct sk_buff *skb6,struct net_device *in)
{
	BOOLE search_item,free_item;
	U32 u0,u1;
	struct sk_buff *skb,*skb_linear;
	struct icmp6hdr *icmph6;
	struct in_addr addr4;
	BIH_MATCH_ITEM *item;
	BIH_TRANS_INFO info,info2;

	skb = NULL;
	skb_linear = NULL;
	item = NULL;
	search_item = BOOLE_FALSE;
	free_item = BOOLE_FALSE;

	BIH_LOG(3,"Bih_Pack64: start");

	if(skb_is_nonlinear(skb6))
	{
		BIH_LOG(2,"Bih_Pack64: skb nonlinear");

		skb_linear = skb_copy(skb6,GFP_ATOMIC);
		if(skb_linear == NULL)
		{
			BIH_LOG(1,"Bih_Pack64: skb_copy error");
			goto error;
		}
		skb = skb_linear;
	}
	else
	{
		skb = skb6;
	}

	//check pack
	if(Bih_PackCheck64(&info,skb->data,skb->len)!= BIH_RES_SUCCESS)
	{
		goto error;
	}

	if(skb->len != info.src_ip_len+info.src_tp_len+info.src_pl_len)
	{
		BIH_LOG(2,"Bih_Pack64: skb len error");
		goto error;
	}

	//map addr
	BIH_LOG(2,"Bih_Pack64: saddr6=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x,daddr6=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
				ntohs(info.saddr6.s6_addr16[0]),
				ntohs(info.saddr6.s6_addr16[1]),
				ntohs(info.saddr6.s6_addr16[2]),
				ntohs(info.saddr6.s6_addr16[3]),
				ntohs(info.saddr6.s6_addr16[4]),
				ntohs(info.saddr6.s6_addr16[5]),
				ntohs(info.saddr6.s6_addr16[6]),
				ntohs(info.saddr6.s6_addr16[7]),
				ntohs(info.daddr6.s6_addr16[0]),
				ntohs(info.daddr6.s6_addr16[1]),
				ntohs(info.daddr6.s6_addr16[2]),
				ntohs(info.daddr6.s6_addr16[3]),
				ntohs(info.daddr6.s6_addr16[4]),
				ntohs(info.daddr6.s6_addr16[5]),
				ntohs(info.daddr6.s6_addr16[6]),
				ntohs(info.daddr6.s6_addr16[7]));

	if(Bih_MapAddr64(bih,&info.saddr6,&info.daddr6,&info.saddr4,&info.daddr4)!= BIH_RES_SUCCESS)
	{
		goto error;
	}

	BIH_LOG(2,"Bih_Pack64: saddr4=%d.%d.%d.%d,daddr4=%d.%d.%d.%d",BIH_SEGMENT_ADDR4(&info.saddr4.s_addr),BIH_SEGMENT_ADDR4(&info.daddr4.s_addr));

	//check match item
	if((info.flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)&& info.src_frag_offset>0)
	{
		item = Bih_MatchSearchItem(bih,&info.daddr4,BIH_MATCH_ITEM_TYPE_FRAG,info.src_frag_id);
		if(item == NULL)
		{
			BIH_LOG(3,"Bih_Pack64: no match item found");
			goto error;
		}

		if(info.flag & BIH_TRANS_INFO_FLAG_SRC_FRAG_LAST)
		{
			free_item = BOOLE_TRUE;
		}
	}
	else
	{
		if(info.flag & BIH_TRANS_INFO_FLAG_ICMP)
		{
			icmph6 = (struct icmp6hdr *)(skb6->data+info.src_ip_len);

			if(icmph6->icmp6_type == ICMPV6_ECHO_REPLY)
			{
				u0 = BIH_MATCH_ITEM_TYPE_ICMP;
				u1 = icmph6->icmp6_identifier<<16 | icmph6->icmp6_sequence;
				addr4.s_addr = info.daddr4.s_addr;
				search_item = BOOLE_TRUE;
				free_item = BOOLE_TRUE;
			}
			else if(icmph6->icmp6_type != ICMPV6_ECHO_REQUEST)
			{
				if(Bih_PackCheck64(&info2,skb6->data+info.src_ip_len+info.src_tp_len,info.src_pl_len)!= BIH_RES_SUCCESS)
				{
					goto error;
				}

				if(Bih_MapAddr64(bih,&info2.daddr6,&info2.saddr6,&info2.daddr4,&info2.saddr4)!= BIH_RES_SUCCESS)
				{
					BIH_LOG(3,"Bih_Pack64: map addr error");
					goto error;
				}

				if(info2.flag & BIH_TRANS_INFO_FLAG_TCP)
				{
					u0 = BIH_MATCH_ITEM_TYPE_TCP;
					u1 = BIH_READ_BUF16BE(skb6->data+info.src_ip_len+info.src_tp_len+info2.src_ip_len);
					addr4.s_addr = info2.saddr4.s_addr;
					search_item = BOOLE_TRUE;
				}
				else if(info2.flag & BIH_TRANS_INFO_FLAG_UDP)
				{
					u0 = BIH_MATCH_ITEM_TYPE_UDP;
					u1 = BIH_READ_BUF16BE(skb6->data+info.src_ip_len+info.src_tp_len+info2.src_ip_len);
					addr4.s_addr = info2.saddr4.s_addr;
					search_item = BOOLE_TRUE;
				}
			}
		}
		else if(info.flag & BIH_TRANS_INFO_FLAG_TCP)
		{
			u0 = BIH_MATCH_ITEM_TYPE_TCP;
			u1 = BIH_READ_BUF16BE(skb6->data+info.src_ip_len+2);//port
			addr4.s_addr = info.daddr4.s_addr;
			search_item = BOOLE_TRUE;
		}
		else if(info.flag & BIH_TRANS_INFO_FLAG_UDP)
		{
			u0 = BIH_MATCH_ITEM_TYPE_UDP;
			u1 = BIH_READ_BUF16BE(skb6->data+info.src_ip_len+2);//port
			addr4.s_addr = info.daddr4.s_addr;
			search_item = BOOLE_TRUE;
		}

		if(search_item)
		{
			item = Bih_MatchSearchItem(bih,&addr4,u0,u1);
			if(item == NULL)
			{
				BIH_LOG(3,"Bih_Pack64: no match item found");
				goto error;
			}
		}
		else
		{
			BIH_LOG(3,"Bih_Pack64: no match item searched");
			goto error;
		}

		if(info.flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)//register frag id
		{
			if(Bih_MatchAddItem(bih,&info.daddr4,BIH_MATCH_ITEM_TYPE_FRAG,info.src_frag_id,BIH_MATCH_ITEM_FLAG_TEMP)== NULL)
			{
				BIH_LOG(1,"Bih_Pack64: add frag match item error");
			}
		}
	}

	//transfer
	if(info.flag & BIH_TRANS_INFO_FLAG_DNS)
	{
		if(Bih_PackDns64(bih,&info,skb,in)!= BIH_RES_SUCCESS)
		{
			goto error;
		}
	}
	else if(info.flag & BIH_TRANS_INFO_FLAG_ICMP)
	{
		if(Bih_PackIcmp64(bih,&info,skb,in)!= BIH_RES_SUCCESS)
		{
			goto error;
		}
	}
	else
	{
		if(Bih_PackNormal64(bih,&info,skb,in)!= BIH_RES_SUCCESS)
		{
			goto error;
		}
	}

	//ok
	if(item!=NULL && free_item)
	{
		Bih_MatchFreeItem(bih,item);
	}
	if(skb_linear != NULL)
	{
		dev_kfree_skb(skb_linear);
	}
	kfree_skb(skb6);

	BIH_LOG(3,"Bih_Pack64: ok");

	return BIH_RES_SUCCESS;

error:
	if(skb_linear != NULL)
	{
		dev_kfree_skb(skb_linear);
	}
	if(item!=NULL && free_item)
	{
		Bih_MatchFreeItem(bih,item);
	}
	return BIH_RES_FAILED;
}


RESULT Bih_PackDns64(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb6,struct net_device *in)
{
	BOOLE addr_ok,map_ok;
	U32 port,id;
	struct sk_buff *skb4;
	struct net_device *dev;
	BIH_DNS_REC *rec;
	BIH_DNS_MSG msg;
	BIH_TRANS_PACK ip_tp,pl;

	skb4 = NULL;
	rec = NULL;
	dev = in;
	port = BIH_READ_BUF16BE(skb6->data+info->src_ip_len+2);
	id = BIH_READ_BUF16BE(skb6->data+info->src_ip_len+info->src_tp_len);

	BIH_LOG(3,"Bih_PackDns64: port=%d,id=%d",port,id);

	if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
	{
		BIH_LOG(2,"Bih_PackDns64: unsupport ip fragment");
		goto error;
	}
	if(dev->mtu < info->dst_ip_len+info->dst_tp_len)
	{
		BIH_LOG(2,"Bih_PackDns64: buf too small");
		goto error;
	}

	//get dns record
	rec = Bih_DnsSearchRec(bih,port,id);
	if(rec == NULL)
	{
		BIH_LOG(2,"Bih_PackDns64: no rec found");
		goto error;
	}
	//transfer dns
	skb4 = dev_alloc_skb(dev->hard_header_len+dev->mtu);//actual size will be set later
	if(skb4 == NULL)
	{
		BIH_LOG(1,"Bih_PackDns64: alloc skb error");
		goto error;
	}

	msg.src_msg = skb6->data + info->src_ip_len + info->src_tp_len;
	msg.src_msg_len = info->src_pl_len;
	msg.dst_msg = skb4->data + dev->hard_header_len + info->dst_ip_len + info->dst_tp_len;
	msg.dst_msg_len = 0;
	msg.dst_buf_len = dev->mtu - info->dst_ip_len - info->dst_tp_len;

	if(Bih_DnsRsp64(bih,&msg,&addr_ok,&map_ok)!= BIH_RES_SUCCESS)
	{
		goto error;
	}

	//transfer pack
	if(addr_ok)
	{
		if(map_ok)//AAAA rsp
		{
			//port = (port-1)& 0xffff;
			id  = (id-1)& 0xffff;
			BIH_WRITE_BUF16BE(msg.dst_msg,id);//dec id in AAAA rsp
		}
		BIH_PACK_SET_SRC(&ip_tp,skb6->data,info->src_ip_len+info->src_tp_len,0);
		BIH_PACK_SET_DST(&ip_tp,skb4->data+dev->hard_header_len,info->dst_ip_len+info->dst_tp_len,0);
		BIH_PACK_SET_SRC(&pl,msg.dst_msg,msg.dst_msg_len,0);

		if(Bih_PackTrans64(info,&ip_tp,&pl,BOOLE_FALSE)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		info->dst_pl_len = msg.dst_msg_len;

		if(Bih_PackFillSkb64(skb4,info->dst_ip_len+info->dst_tp_len+info->dst_pl_len,dev)!= BIH_RES_SUCCESS)
		{
			goto error;
		}
		if(map_ok)//IN AAAA,store skb
		{
			BIH_LOG(2,"Bih_PackDns64: recv IN AAAA rsp,store dns rec");

			Bih_DnsSetRecRsp(rec,skb4);
			skb4 = NULL;
		}
		else//IN A
		{
			BIH_LOG(2,"Bih_PackDns64: recv IN A rsp,free dns rec");

			netif_rx(skb4);
			skb4 = NULL;

			Bih_DnsFreeRec(bih,rec);
			rec = NULL;
		}
	}
	else
	{
		if(rec->rsp != NULL)
		{
			BIH_LOG(2,"Bih_PackDns64: no v4 addr found,recv v6 rsp");

			dev_kfree_skb(skb4);
			skb4 = NULL;

			Bih_DnsSendRec(bih,rec);
			Bih_DnsFreeRec(bih,rec);
			rec = NULL;
		}
		else
		{
			BIH_LOG(2,"Bih_PackDns64: no addr found");
			goto error;
		}
	}

	return BIH_RES_SUCCESS;

error:
	if(skb4 != NULL)
	{
		dev_kfree_skb(skb4);
	}

	//discard this packet
	return BIH_RES_SUCCESS;
}


RESULT Bih_PackIcmp64(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb6,struct net_device *in)
{
	BOOLE echo_msg;
	struct sk_buff *skb4;
	struct net_device *dev;
	struct icmp6hdr *icmph6;
	BIH_TRANS_PACK ip_tp,pl;
	BIH_TRANS_INFO info2;

	echo_msg = BOOLE_FALSE;
	skb4 = NULL;
	dev = in;

	if(info->src_tp_len > 0)
	{
		icmph6 = (struct icmp6hdr *)(skb6->data+info->src_ip_len);

		if(icmph6->icmp6_type==ICMPV6_ECHO_REQUEST || icmph6->icmp6_type==ICMPV6_ECHO_REPLY)
		{
			echo_msg = BOOLE_TRUE;
			if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
			{
				info->flag |= BIH_TRANS_INFO_FLAG_DST_FRAG;
			}
		}
		else
		{
			if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
			{
				BIH_LOG(2,"Bih_PackIcmp64: unsupport ip fragment");
				goto error;
			}
		}
	}
	else
	{
		echo_msg = BOOLE_TRUE;
		if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
		{
			info->flag |= BIH_TRANS_INFO_FLAG_DST_FRAG;
		}
	}
	if(dev->mtu < info->dst_ip_len+info->dst_tp_len)
	{
		BIH_LOG(2,"Bih_PackIcmp64: mtu too small");
		goto error;
	}
	//transfer
	if(echo_msg)
	{
		skb4 = dev_alloc_skb(dev->hard_header_len+dev->mtu);//actual size will be set later
		if(skb4 == NULL)
		{
			BIH_LOG(1,"Bih_PackIcmp64: alloc skb error");
			goto error;
		}

		BIH_PACK_SET_SRC(&ip_tp,skb6->data,info->src_ip_len+info->src_tp_len,0);
		BIH_PACK_SET_DST(&ip_tp,skb4->data+dev->hard_header_len,info->dst_ip_len+info->dst_tp_len,0);
		BIH_PACK_SET_SRC(&pl,skb6->data+info->src_ip_len+info->src_tp_len,info->src_pl_len,0);
		BIH_PACK_SET_DST(&pl,skb4->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len,dev->mtu-info->dst_ip_len-info->dst_tp_len,0);

		if(Bih_PackTrans64(info,&ip_tp,&pl,BOOLE_TRUE)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		info->dst_pl_len = pl.dst_data_filled;

		if(Bih_PackFillSkb64(skb4,info->dst_ip_len+info->dst_tp_len+info->dst_pl_len,dev)!= BIH_RES_SUCCESS)
		{
			goto error;
		}
		//recv
		netif_rx(skb4);
		skb4 = NULL;
	}
	else
	{
		BIH_LOG(3,"Bih_PackIcmp64: err msg");

		//check payload
		if(Bih_PackCheck64(&info2,skb6->data+info->src_ip_len+info->src_tp_len,info->src_pl_len)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		if(info2.flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
		{
			info2.flag |= BIH_TRANS_INFO_FLAG_DST_FRAG;
		}

		if(info->src_pl_len < info2.src_ip_len+info2.src_tp_len+info2.src_pl_len)
		{
			BIH_LOG(2,"Bih_PackIcmp64: payload turncation");
			info2.src_pl_len = info->src_pl_len - info2.src_ip_len - info2.src_tp_len;
		}

		if(Bih_MapAddr64(bih,&info2.daddr6,&info2.saddr6,&info2.daddr4,&info2.saddr4)!= BIH_RES_SUCCESS)
		{
			BIH_LOG(3,"Bih_PackIcmp64: map addr error");
			goto error;
		}

		//transfer payload
		if(dev->mtu < info->dst_ip_len+info->dst_tp_len+info2.dst_ip_len+info2.dst_tp_len)
		{
			BIH_LOG(2,"Bih_PackIcmp64: mtu too small");
			goto error;
		}

		skb4 = dev_alloc_skb(dev->hard_header_len+dev->mtu);//actual size will be set later
		if(skb4 == NULL)
		{
			BIH_LOG(1,"Bih_PackIcmp64: alloc skb error");
			goto error;
		}

		BIH_PACK_SET_SRC(&ip_tp,skb6->data+info->src_ip_len+info->src_tp_len,info2.src_ip_len+info2.src_tp_len,0);
		BIH_PACK_SET_DST(&ip_tp,skb4->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len,info2.dst_ip_len+info2.dst_tp_len,0);
		BIH_PACK_SET_SRC(&pl,skb6->data+info->src_ip_len+info->src_tp_len+info2.src_ip_len+info2.src_tp_len,info2.src_pl_len,0);
		BIH_PACK_SET_DST(&pl,skb4->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len+info2.dst_ip_len+info2.dst_tp_len,dev->mtu-info->dst_ip_len-info->dst_tp_len-info2.dst_ip_len-info2.dst_tp_len,0);

		if(Bih_PackTrans64(&info2,&ip_tp,&pl,BOOLE_TRUE)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		info2.dst_pl_len = pl.dst_data_filled;
		info->dst_pl_len = info2.dst_ip_len + info2.dst_tp_len + info2.dst_pl_len;

		//transfer pack
		BIH_PACK_SET_SRC(&ip_tp,skb6->data,info->src_ip_len+info->src_tp_len,0);
		BIH_PACK_SET_DST(&ip_tp,skb4->data+dev->hard_header_len,info->dst_ip_len+info->dst_tp_len,0);
		BIH_PACK_SET_SRC(&pl,skb4->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len,info->dst_pl_len,0);

		if(Bih_PackTrans64(info,&ip_tp,&pl,BOOLE_FALSE)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		if(Bih_PackFillSkb64(skb4,info->dst_ip_len+info->dst_tp_len+info->dst_pl_len,dev)!= BIH_RES_SUCCESS)
		{
			goto error;
		}

		//recv
		netif_rx(skb4);
		skb4 = NULL;
	}

	return BIH_RES_SUCCESS;

error:
	if(skb4 != NULL)
	{
		dev_kfree_skb(skb4);
	}
	return BIH_RES_FAILED;
}


RESULT Bih_PackNormal64(BIH_OBJ *bih,BIH_TRANS_INFO *info,struct sk_buff *skb6,struct net_device *in)
{
	struct sk_buff *skb4;
	struct net_device *dev;
	BIH_TRANS_PACK ip_tp,pl;

	skb4 = NULL;
	dev = in;

	if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
	{
		BIH_LOG(2,"Bih_PackNormal64: unsupport ip fragment");
		goto error;
	}

	if(dev->mtu < info->dst_ip_len+info->dst_tp_len)
	{
		BIH_LOG(2,"Bih_PackNormal64: mtu too small");
		goto error;
	}
	//transfer
	skb4 = dev_alloc_skb(dev->hard_header_len+dev->mtu);//actual size will be set later
	if(skb4 == NULL)
	{
		BIH_LOG(1,"Bih_PackNormal64: alloc skb error");
		goto error;
	}

	BIH_PACK_SET_SRC(&ip_tp,skb6->data,info->src_ip_len+info->src_tp_len,0);
	BIH_PACK_SET_DST(&ip_tp,skb4->data+dev->hard_header_len,info->dst_ip_len+info->dst_tp_len,0);
	BIH_PACK_SET_SRC(&pl,skb6->data+info->src_ip_len+info->src_tp_len,info->src_pl_len,0);
	BIH_PACK_SET_DST(&pl,skb4->data+dev->hard_header_len+info->dst_ip_len+info->dst_tp_len,dev->mtu-info->dst_ip_len-info->dst_tp_len,0);

	if(Bih_PackTrans64(info,&ip_tp,&pl,BOOLE_TRUE)!= BIH_RES_SUCCESS)
	{
		goto error;
	}

	info->dst_pl_len = pl.dst_data_filled;

	if(Bih_PackFillSkb64(skb4,info->dst_ip_len+info->dst_tp_len+info->dst_pl_len,dev)!= BIH_RES_SUCCESS)
	{
		goto error;
	}
	//recv
	netif_rx(skb4);
	skb4 = NULL;

	return BIH_RES_SUCCESS;

error:
	if(skb4 != NULL)
	{
		dev_kfree_skb(skb4);
	}
	return BIH_RES_FAILED;
}


RESULT Bih_PackCheck64(BIH_TRANS_INFO *info,U8 *buf,U32 buf_len)
{
	U32 u0,ih6_len,ipl6_len,tcph_len,next_hdr;
	struct ipv6hdr *ih6;
	struct ipv6_opt_hdr *ioh6;
	struct frag_hdr *ih6_frag;
	struct icmp6hdr *icmph6;
	struct tcphdr *tcph;
	struct udphdr *udph;

	memset(info,0,sizeof(BIH_TRANS_INFO));

	//ip head
	ih6 = (struct ipv6hdr *)buf;
	ipl6_len = ntohs(ih6->payload_len);

	if(buf_len < BIH_IP6_HDR_SIZE)
	{
		BIH_LOG(2,"Bih_PackCheck64: ip len error");
		return BIH_RES_FAILED;
	}

	next_hdr = ih6->nexthdr;
	ih6_len = BIH_IP6_HDR_SIZE;
	while(1)
	{
		if(next_hdr==NEXTHDR_ICMP || next_hdr==NEXTHDR_TCP || next_hdr==NEXTHDR_UDP)
		{
			break;
		}

		//check next head
		if(buf_len < ih6_len+2)
		{
			BIH_LOG(2,"Bih_PackCheck64: no payload found");
			return BIH_RES_FAILED;
		}
		ioh6 = (struct ipv6_opt_hdr *)((U8 *)ih6 + ih6_len);

		if(next_hdr == NEXTHDR_FRAGMENT)
		{
			ih6_frag = (struct frag_hdr *)ioh6;

			if(buf_len < ih6_len+8)
			{
				BIH_LOG(2,"Bih_PackCheck64: fragment len error");
				return BIH_RES_FAILED;
			}

			u0 = ntohs(ih6_frag->frag_off);
			info->flag |= (u0 & IP6_MF)? BIH_TRANS_INFO_FLAG_SRC_FRAG : BIH_TRANS_INFO_FLAG_SRC_FRAG|BIH_TRANS_INFO_FLAG_SRC_FRAG_LAST;
			info->src_frag_offset = u0 & BIH_IP_FRAG_OFFSET_MASK;
			info->src_frag_id = ntohl(ih6_frag->identification);
		}
		else if(next_hdr==NEXTHDR_ESP || next_hdr==NEXTHDR_AUTH || next_hdr==NEXTHDR_IPV6)
		{
			BIH_LOG(2,"Bih_PackCheck64: cannot translate hdr=%d",next_hdr);
			return BIH_RES_FAILED;
		}

		//next head
		next_hdr = ioh6->nexthdr;
		u0 = 8 + ioh6->hdrlen*8;
		if(buf_len < ih6_len+u0)
		{
			BIH_LOG(2,"Bih_PackCheck64: opt hdr error");
			return BIH_RES_FAILED;
		}
		ih6_len += u0;
	}

	info->src_protocol = next_hdr;
	info->src_ip_len = ih6_len;
	memcpy(info->saddr6.s6_addr,ih6->saddr.s6_addr,16);
	memcpy(info->daddr6.s6_addr,ih6->daddr.s6_addr,16);

	if((info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)&& info->src_frag_offset>0)
	{
		switch(next_hdr)
		{
		case NEXTHDR_ICMP:
			info->flag |= BIH_TRANS_INFO_FLAG_ICMP;
			info->dst_protocol = IPPROTO_ICMP;
			break;

		case NEXTHDR_TCP:
			info->flag |= BIH_TRANS_INFO_FLAG_TCP;
			info->dst_protocol = IPPROTO_TCP;
			break;

		case NEXTHDR_UDP:
			info->flag |= BIH_TRANS_INFO_FLAG_UDP;
			info->dst_protocol = IPPROTO_UDP;
			break;

		default:
			BIH_LOG(2,"Bih_PackCheck64: unsupport protocol %d",next_hdr);
			return BIH_RES_FAILED;
		}
	}
	else
	{
		switch(next_hdr)
		{
		case NEXTHDR_ICMP:
			info->flag |= BIH_TRANS_INFO_FLAG_ICMP;
			info->dst_protocol = IPPROTO_ICMP;
			icmph6 = (struct icmp6hdr *)(buf + ih6_len);

			if(buf_len < ih6_len+BIH_ICMP6_HDR_SIZE)
			{
				BIH_LOG(2,"Bih_PackCheck64: icmp len error");
				return BIH_RES_FAILED;
			}
			if(icmph6->icmp6_type==ICMPV6_DEST_UNREACH || icmph6->icmp6_type==ICMPV6_PKT_TOOBIG || icmph6->icmp6_type==ICMPV6_TIME_EXCEED || icmph6->icmp6_type==ICMPV6_PARAMPROB)
			{
			}
			else if(icmph6->icmp6_type==ICMPV6_ECHO_REQUEST || icmph6->icmp6_type==ICMPV6_ECHO_REPLY)
			{
			}
			else
			{
				BIH_LOG(3,"Bih_PackCheck64: unsupport icmp type %d",icmph6->icmp6_type);
				return BIH_RES_FAILED;
			}

			info->src_tp_len = BIH_ICMP6_HDR_SIZE;
			info->dst_tp_len = BIH_ICMP4_HDR_SIZE;
			break;

		case NEXTHDR_TCP:
			info->flag |= BIH_TRANS_INFO_FLAG_TCP;
			info->dst_protocol = IPPROTO_TCP;
			tcph = (struct tcphdr *)(buf + ih6_len);

			tcph_len = tcph->doff * 4;
			if(tcph_len<BIH_TCP_HDR_SIZE || buf_len<ih6_len+tcph_len)
			{
				BIH_LOG(2,"Bih_PackCheck64: tcp len error");
				return BIH_RES_FAILED;
			}

			info->src_tp_len = tcph_len;
			info->dst_tp_len = tcph_len;
			break;

		case NEXTHDR_UDP:
			info->flag |= BIH_TRANS_INFO_FLAG_UDP;
			info->dst_protocol = IPPROTO_UDP;
			udph = (struct udphdr *)(buf + ih6_len);

			if(buf_len < ih6_len+BIH_UDP_HDR_SIZE)
			{
				BIH_LOG(2,"Bih_PackCheck64: udp len error");
				return BIH_RES_FAILED;
			}

			if(ntohs(udph->source)==BIH_DNS_UDP_PORT || ntohs(udph->dest)==BIH_DNS_UDP_PORT)
			{
				info->flag |= BIH_TRANS_INFO_FLAG_DNS;
			}

			info->src_tp_len = BIH_UDP_HDR_SIZE;
			info->dst_tp_len = BIH_UDP_HDR_SIZE;
			break;

		default:
			BIH_LOG(2,"Bih_PackCheck64: unsupport protocol %d",next_hdr);
			return BIH_RES_FAILED;
		}
	}

	info->src_pl_len = BIH_IP6_HDR_SIZE + ipl6_len - ih6_len - info->src_tp_len;
	info->dst_frag_id = info->src_frag_id;
	info->dst_frag_offset = info->src_frag_offset;
	info->dst_ip_len = BIH_IP4_HDR_SIZE;

	if(info->flag & BIH_TRANS_INFO_FLAG_DNS)
	{
		BIH_LOG(3,"Bih_PackCheck64: dns");
	}
	else if(info->flag & BIH_TRANS_INFO_FLAG_ICMP)
	{
		BIH_LOG(3,"Bih_PackCheck64: icmp");
	}
	else if(info->flag & BIH_TRANS_INFO_FLAG_TCP)
	{
		BIH_LOG(3,"Bih_PackCheck64: tcp");
	}
	else if(info->flag & BIH_TRANS_INFO_FLAG_UDP)
	{
		BIH_LOG(3,"Bih_PackCheck64: udp");
	}
	BIH_LOG(3,"Bih_PackCheck64: frag=%d,frag_last=%d,frag_off=%d,ip_len=%d,tp_len=%d,pl_len=%d",
		(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)? 1 : 0,
		(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG_LAST)? 1 : 0,
		info->src_frag_offset,
		info->src_ip_len,
		info->src_tp_len,
		info->src_pl_len);

	return BIH_RES_SUCCESS;
}


RESULT Bih_PackTrans64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *ip_tp,BIH_TRANS_PACK *pl,BOOLE copy_pl)
{
	BOOLE ip_last;
	U32 pl_len;
	BIH_TRANS_PACK pack2;

	ip_last = BOOLE_FALSE;

	if(ip_tp->dst_buf_len < info->dst_ip_len)
	{
		BIH_LOG(2,"Bih_PackTrans64: dst_buf_len too small");
		return BIH_RES_FAILED;
	}
	//transfer pl
	if(copy_pl)
	{
		BIH_PACK_SET_SRC(&pack2,pl->src_buf,pl->src_buf_len,0);
		BIH_PACK_SET_DST(&pack2,pl->dst_buf,pl->dst_buf_len,0);

		if(Bih_PackCopy64(info,&pack2)!= BIH_RES_SUCCESS)
		{
			return BIH_RES_FAILED;
		}

		pl_len = pack2.dst_data_filled;
		pl->src_data_used = pl_len;
		pl->dst_data_filled = pl_len;
	}
	else
	{
		pl_len = pl->src_buf_len;
		pl->src_data_used = pl->src_buf_len;
	}

	//transfer tp
	if(info->src_tp_len > 0)
	{
		BIH_PACK_SET_SRC(&pack2,ip_tp->src_buf+info->src_ip_len,info->src_tp_len,0);
		BIH_PACK_SET_DST(&pack2,ip_tp->dst_buf+info->dst_ip_len,ip_tp->dst_buf_len-info->dst_ip_len,0);

		if(info->flag & BIH_TRANS_INFO_FLAG_ICMP)
		{
			if(Bih_ProtoIcmp64(info,&pack2,pl->src_buf,pl->src_buf_len)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}
		}
		else if(info->flag & BIH_TRANS_INFO_FLAG_TCP)
		{
			if(Bih_ProtoTcp64(info,&pack2,pl->src_buf,pl->src_buf_len)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}
		}
		else if(info->flag & BIH_TRANS_INFO_FLAG_UDP)
		{
			if(Bih_ProtoUdp64(info,&pack2,pl->src_buf,pl->src_buf_len)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}
		}
		else
		{
			return BIH_RES_FAILED;
		}
	}

	//transfer ip
	BIH_PACK_SET_SRC(&pack2,ip_tp->src_buf,info->src_ip_len,0);
	BIH_PACK_SET_DST(&pack2,ip_tp->dst_buf,info->dst_ip_len,0);
	if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG)
	{
		if(info->flag & BIH_TRANS_INFO_FLAG_SRC_FRAG_LAST)
		{
			ip_last = BOOLE_TRUE;
		}
	}
	else
	{
		ip_last = BOOLE_TRUE;
	}
	if(Bih_ProtoIp64(info,&pack2,info->dst_ip_len+info->dst_tp_len+pl_len,ip_last)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}

	ip_tp->dst_data_filled = info->dst_ip_len + info->dst_tp_len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_PackCopy64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack)
{
	U32 len;

	len = pack->dst_buf_len;

	if(len > pack->src_buf_len)
	{
		len = pack->src_buf_len;
	}

	//copy data
	if(len > 0)
	{
		memcpy(pack->dst_buf,pack->src_buf,len);
		pack->src_data_used = len;
		pack->dst_data_filled = len;
	}
	return BIH_RES_SUCCESS;
}


RESULT Bih_PackFillSkb64(struct sk_buff *skb4,U32 pack_len,struct net_device *in)
{
	struct ethhdr *eth_h;

	skb_put(skb4,in->hard_header_len+pack_len);

	//eth hdr
	skb_reset_mac_header(skb4);
	if(in->hard_header_len >= sizeof(struct ethhdr))
	{
		eth_h = (struct ethhdr *)skb4->data;
		eth_h->h_proto = htons(ETH_P_IP);
	}
	skb_pull(skb4,in->hard_header_len);
	skb_reset_network_header(skb4);
	skb4->mac_len = in->hard_header_len;

	//ip hdr
	skb4->protocol = htons(ETH_P_IP);
	skb4->pkt_type = PACKET_HOST;
	skb4->ip_summed = CHECKSUM_UNNECESSARY;
	skb4->dev = in;
	skb4->sk = NULL;

	return BIH_RES_SUCCESS;
}



/*bih_proto.c**********************************************************/


RESULT Bih_ProtoIp46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U32 tol_len,BOOLE last)
{
	struct iphdr *ih4;
	struct ipv6hdr *ih6;
	struct frag_hdr *ih6_frag;

	ih4 = (struct iphdr *)pack->src_buf;
	ih6 = (struct ipv6hdr *)pack->dst_buf;

	if(pack->src_buf_len<info->src_ip_len || pack->dst_buf_len<info->dst_ip_len)
	{
		BIH_LOG(2,"Bih_PackIp46: buf len error");
		return BIH_RES_FAILED;
	}
	//transfer ip
	ih6->version = 6;
	ih6->priority = 0;
	ih6->flow_lbl[0] = 0;
	ih6->flow_lbl[1] = 0;
	ih6->flow_lbl[2] = 0;
	ih6->payload_len = htons(tol_len-BIH_IP6_HDR_SIZE);
	ih6->nexthdr = info->dst_protocol;
	ih6->hop_limit = ih4->ttl;
	memcpy(ih6->saddr.s6_addr,info->saddr6.s6_addr,16);
	memcpy(ih6->daddr.s6_addr,info->daddr6.s6_addr,16);

	//fragment
	if(info->flag & BIH_TRANS_INFO_FLAG_DST_FRAG)
	{
		ih6_frag = (struct frag_hdr *)((U8 *)ih6 + BIH_IP6_HDR_SIZE);

		ih6_frag->nexthdr = ih6->nexthdr;
		ih6_frag->reserved = 0;
		ih6_frag->frag_off = htons((info->dst_frag_offset&BIH_IP_FRAG_OFFSET_MASK)| (last ? 0:IP6_MF));
		ih6_frag->identification = htonl(info->dst_frag_id);

		ih6->nexthdr = NEXTHDR_FRAGMENT;
	}

	//ok
	pack->src_data_used = info->src_ip_len;
	pack->dst_data_filled = info->dst_ip_len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_ProtoIcmp46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len)
{
	U32 u0,icmph4_len,icmph6_len;
	struct icmphdr *icmph4;
	struct icmp6hdr *icmph6;

	icmph4 = (struct icmphdr *)pack->src_buf;
	icmph6 = (struct icmp6hdr *)pack->dst_buf;
	icmph4_len = BIH_ICMP4_HDR_SIZE;
	icmph6_len = BIH_ICMP6_HDR_SIZE;

	if(pack->src_buf_len<icmph4_len || pack->dst_buf_len<icmph6_len)
	{
		BIH_LOG(2,"Bih_ProtoIcmp46: len error");
		return BIH_RES_FAILED;
	}

	switch(icmph4->type)
	{
	case ICMP_DEST_UNREACH:/* Destination Unreachable (Type 3)*/
		icmph6->icmp6_type = ICMPV6_DEST_UNREACH;
		icmph6->icmp6_unused = 0;
		switch(icmph4->code)
		{
		case ICMP_NET_UNREACH: /* Code 0 */
		case ICMP_HOST_UNREACH: /* Code 1 */
		case ICMP_SR_FAILED: /* Code 5 */
		case ICMP_NET_UNKNOWN: /* Code 6 */
		case ICMP_HOST_UNKNOWN: /* Code 7 */
		case ICMP_HOST_ISOLATED: /* Code 8 */
		case ICMP_NET_UNR_TOS: /* Code 11 */
		case ICMP_HOST_UNR_TOS: /* Code 12 */
			icmph6->icmp6_code = ICMPV6_NOROUTE;/* to Code 0 */
			break;

		case ICMP_PROT_UNREACH: /* Code 2 */
			icmph6->icmp6_type = ICMPV6_PARAMPROB;/* to Type 4 */
			icmph6->icmp6_code = ICMPV6_UNK_NEXTHDR;/* to Code 1 */
			/* Set pointer filed to 6,it's octet offset IPv6 Next Header field */
			icmph6->icmp6_pointer = htonl(6);
			break;

		case ICMP_PORT_UNREACH: /* Code 3 */
			icmph6->icmp6_code = ICMPV6_PORT_UNREACH;/* to Code 4 */
			break;

		case ICMP_FRAG_NEEDED: /* Code 4 */
			icmph6->icmp6_type = ICMPV6_PKT_TOOBIG;/* to Type 2 */
			icmph6->icmp6_code = 0;
			/* Correct MTU  */
			if(icmph4->un.frag.mtu == 0)
			{
				icmph6->icmp6_mtu = htonl(576);
			}
			else
			{
				icmph6->icmp6_mtu = htonl(ntohs(icmph4->un.frag.mtu)+BIH_IP6_IP4_HDR_DIFF);
			}
			break;

		case ICMP_NET_ANO: /* Code 9 */
		case ICMP_HOST_ANO: /* Code 10 */
			icmph6->icmp6_code = ICMPV6_ADM_PROHIBITED;/* to Code 1 */
			break;

		default: /* discard any other Code */
			BIH_LOG(3,"Bih_ProtoIcmp46: unsupport code");
			return BIH_RES_FAILED;
		}
		break;

	case ICMP_TIME_EXCEEDED: /* Time Exceeded (Type 11)*/
		icmph6->icmp6_type = ICMPV6_TIME_EXCEED;
		icmph6->icmp6_code = icmph4->code;
		break;

	case ICMP_PARAMETERPROB: /* Parameter Problem (Type 12)*/
		icmph6->icmp6_type = ICMPV6_PARAMPROB;
		icmph6->icmp6_code = icmph4->code;
		switch(ntohs(icmph4->un.echo.id)>> 8)
		{
		case 0:
			icmph6->icmp6_pointer = 0;
			break;
		case 2:
			icmph6->icmp6_pointer = __constant_htonl(4);
			break;
		case 8:
			icmph6->icmp6_pointer = __constant_htonl(7);
			break;
		case 9:
			icmph6->icmp6_pointer = __constant_htonl(6);
			break;
		case 12:
			icmph6->icmp6_pointer = __constant_htonl(8);
			break;
		case 16:
			icmph6->icmp6_pointer = __constant_htonl(24);
			break;
		default:
			icmph6->icmp6_pointer = 0xffffffff;
			break;
		}
		break;

	case ICMP_ECHO:
		icmph6->icmp6_type = ICMPV6_ECHO_REQUEST;
		icmph6->icmp6_code = 0;
		icmph6->icmp6_identifier = icmph4->un.echo.id;
		icmph6->icmp6_sequence = icmph4->un.echo.sequence;
		break;

	case ICMP_ECHOREPLY:
		icmph6->icmp6_type = ICMPV6_ECHO_REPLY;
		icmph6->icmp6_code = 0;
		icmph6->icmp6_identifier = icmph4->un.echo.id;
		icmph6->icmp6_sequence = icmph4->un.echo.sequence;
		break;

	default:
		BIH_LOG(3,"Bih_ProtoIcmp46: unsupport type %d",icmph4->type);
		return BIH_RES_FAILED;
	}

	//update checksum
	u0 = csum_partial(pl,pl_len,0);
	icmph6->icmp6_cksum = 0;
	u0 = csum_partial((U8 *)icmph6,icmph6_len,u0);
	icmph6->icmp6_cksum = csum_ipv6_magic(&info->saddr6,&info->daddr6,icmph6_len+pl_len,IPPROTO_ICMPV6,u0);

	//ok
	pack->src_data_used = icmph4_len;
	pack->dst_data_filled = icmph6_len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_ProtoTcp46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len)
{
	U32 u0,tcph_len,op_len;
	U8 *op;
	struct tcphdr *tcph4,*tcph6;

	tcph4 = (struct tcphdr *)pack->src_buf;
	tcph6 = (struct tcphdr *)pack->dst_buf;
	tcph_len = tcph4->doff * 4;

	if(tcph_len<BIH_TCP_HDR_SIZE || pack->src_buf_len<tcph_len || pack->dst_buf_len<tcph_len)
	{
		BIH_LOG(2,"Bih_ProtoTcp46: len error");
		return BIH_RES_FAILED;
	}

	//transfer tcp
	memcpy(tcph6,tcph4,tcph_len);

	//update mss
	if(tcph6->syn)
	{
		op_len = tcph_len - BIH_TCP_HDR_SIZE;
		op = (U8 *)tcph6 + BIH_TCP_HDR_SIZE;
		while(op_len >= 4)//mss op len is 4
		{
			if(op[0]==2 && op[1]==4)
			{
				u0 = BIH_READ_BUF16BE(op+2);
				if(u0 > BIH_IP6_IP4_HDR_DIFF)
				{
					u0 -= BIH_IP6_IP4_HDR_DIFF;
					BIH_WRITE_BUF16BE(op+2,u0);
				}
				break;
			}

			u0 = op[0]<2 ? 1 : op[1];
			if(op_len < u0)
			{
				break;
			}

			op += u0;
			op_len -= u0;
		}
	}

	//update checksum
	u0 = csum_partial(pl,pl_len,0);
	tcph6->check = 0;
	u0 = csum_partial((U8 *)tcph6,tcph_len,u0);
	tcph6->check = csum_ipv6_magic(&info->saddr6,&info->daddr6,tcph_len+pl_len,IPPROTO_TCP,u0);

	//ok
	pack->src_data_used = tcph_len;
	pack->dst_data_filled = tcph_len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_ProtoUdp46(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len)
{
	U32 u0,udph_len;
	struct udphdr *udph4,*udph6;

	udph4 = (struct udphdr *)pack->src_buf;
	udph6 = (struct udphdr *)pack->dst_buf;
	udph_len = BIH_UDP_HDR_SIZE;

	if(pack->src_buf_len<udph_len || pack->dst_buf_len<udph_len)
	{
		BIH_LOG(2,"Bih_ProtoUdp46: len error");
		return BIH_RES_FAILED;
	}

	//transfer udp
	memcpy(udph6,udph4,udph_len);
	udph6->len = htons(udph_len+pl_len);

	//update checksum
	u0 = csum_partial(pl,pl_len,0);
	udph6->check = 0;
	u0 = csum_partial((U8 *)udph6,udph_len,u0);
	udph6->check = csum_ipv6_magic(&info->saddr6,&info->daddr6,udph_len+pl_len,IPPROTO_UDP,u0);

	//ok
	pack->src_data_used = udph_len;
	pack->dst_data_filled = udph_len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_ProtoIp64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U32 tol_len,BOOLE last)
{
	struct iphdr *ih4;
	struct ipv6hdr *ih6;

	ih6 = (struct ipv6hdr *)pack->src_buf;
	ih4 = (struct iphdr *)pack->dst_buf;

	if(pack->src_buf_len<info->src_ip_len || pack->dst_buf_len<info->dst_ip_len)
	{
		BIH_LOG(2,"Bih_ProtoIp64: buf len error");
		return BIH_RES_FAILED;
	}
	//transfer ip
	ih4->version = 4;
	ih4->ihl = info->dst_ip_len >> 2;
	ih4->tos = 0;
	ih4->tot_len = htons(tol_len);
	ih4->id = 0;
	ih4->frag_off = 0;
	ih4->ttl = ih6->hop_limit;
	ih4->protocol = info->dst_protocol;
	ih4->check = 0;
	ih4->saddr = info->saddr4.s_addr;
	ih4->daddr = info->daddr4.s_addr;

	if(info->flag & BIH_TRANS_INFO_FLAG_DST_FRAG)
	{
		ih4->id = htons(info->dst_frag_id & 0xffff);
		ih4->frag_off = htons((info->dst_frag_offset & BIH_IP_FRAG_OFFSET_MASK)>>3 | (last ? 0 : IP_MF));
	}

	ih4->check = ip_fast_csum((U8 *)ih4,ih4->ihl);

	//ok
	pack->src_data_used = info->src_ip_len;
	pack->dst_data_filled = info->dst_ip_len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_ProtoIcmp64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len)
{
	U32 u0,icmph4_len,icmph6_len;
	struct icmphdr *icmph4;
	struct icmp6hdr *icmph6;

	icmph6 = (struct icmp6hdr *)pack->src_buf;
	icmph4 = (struct icmphdr *)pack->dst_buf;
	icmph6_len = BIH_ICMP6_HDR_SIZE;
	icmph4_len = BIH_ICMP4_HDR_SIZE;

	if(pack->src_buf_len<icmph6_len || pack->dst_buf_len<icmph4_len)
	{
		BIH_LOG(2,"Bih_ProtoIcmp64: len error");
		return BIH_RES_FAILED;
	}

	switch(icmph6->icmp6_type)
	{
	case ICMPV6_DEST_UNREACH: /* Type 1 */
		icmph4->type = ICMP_DEST_UNREACH;/* to Type 3 */
		icmph4->un.echo.id = 0;
		icmph4->un.echo.sequence = 0;
		switch(icmph6->icmp6_code)
		{
		case ICMPV6_NOROUTE: /* Code 0 */
		case ICMPV6_NOT_NEIGHBOUR: /* Code 2 */
		case ICMPV6_ADDR_UNREACH: /* Code 3  */
			icmph4->code = ICMP_HOST_UNREACH;/* To Code 1 */
			break;

		case ICMPV6_ADM_PROHIBITED: /* Code 1 */
			icmph4->code = ICMP_HOST_ANO;/* To Code 10 */
			break;

		case ICMPV6_PORT_UNREACH: /* Code 4 */
			icmph4->code = ICMP_PORT_UNREACH;/* To Code 3 */
			break;

		default: /* discard any other codes */
			BIH_LOG(3,"Bih_ProtoIcmp64: unsupport code");
			return BIH_RES_FAILED;
		}
		break;

	case ICMPV6_PKT_TOOBIG: /* Type 2 */
		icmph4->type = ICMP_DEST_UNREACH;/* to Type 3  */
		icmph4->code = ICMP_FRAG_NEEDED;/*  to Code 4 */
		icmph4->un.frag.mtu = (__u16)icmph6->icmp6_mtu;
		break;

	case ICMPV6_TIME_EXCEED:
		icmph4->type = ICMP_TIME_EXCEEDED;/* to Type 11 */
		icmph4->code = icmph6->icmp6_code;/* Code unchanged */
		break;

	case ICMPV6_PARAMPROB:
		switch (icmph6->icmp6_code)
		{
		case ICMPV6_UNK_NEXTHDR: /* Code 1 */
			icmph4->type = ICMP_DEST_UNREACH;/* to Type 3 */
			icmph4->code = ICMP_PROT_UNREACH;/* to Code 2 */
			break;

		default: /* if Code != 1 */
			icmph4->type = ICMP_PARAMETERPROB;/* to Type 12 */
			icmph4->code = 0;/* to Code 0 */
			switch (ntohl(icmph6->icmp6_pointer))
			{
			case 0: /* IPv6 Version -> IPv4 Version */
				icmph4->un.echo.id = 0;
				break;
			case 4: /* IPv6 PayloadLength -> IPv4 Total Length */
				icmph4->un.echo.id = 0x0002;/* 2 */
				break;
			case 6: /* IPv6 Next Header-> IPv4 Protocol */
				icmph4->un.echo.id = 0x0009;/* 9 */
				break;
			case 7: /* IPv6 Hop Limit -> IPv4 TTL */
				icmph4->un.echo.id = 0x0008;/* 8 */
				break;
			case 8: /* IPv6 Src addr -> IPv4 Src addr */
				icmph4->un.echo.id = 0x000c;/* 12 */
				break;
			case 24: /* IPv6 Dst addr -> IPv4 Dst addr*/
				icmph4->un.echo.id = 0x0010;/* 16 */
				break;
			default: /* set all ones in other cases */
				icmph4->un.echo.id = 0xff;
				break;
			}
			break;
		}
		break;

	case ICMPV6_ECHO_REQUEST:
		icmph4->type = ICMP_ECHO;
		icmph4->code = 0;
		icmph4->un.echo.id = icmph6->icmp6_identifier;
		icmph4->un.echo.sequence = icmph6->icmp6_sequence;
		break;

	case ICMPV6_ECHO_REPLY:
		icmph4->type = ICMP_ECHOREPLY;
		icmph4->code = 0;
		icmph4->un.echo.id = icmph6->icmp6_identifier;
		icmph4->un.echo.sequence = icmph6->icmp6_sequence;
		break;

	default:
		BIH_LOG(3,"Bih_ProtoIcmp64: unsupport type %d",icmph6->icmp6_type);
		return BIH_RES_FAILED;
	}

	//update checksum
	u0 = csum_partial(pl,pl_len,0);
	icmph4->checksum = 0;
	u0 = csum_partial((U8 *)icmph4,icmph4_len,u0);
	icmph4->checksum = csum_fold(u0);

	//ok
	pack->src_data_used = icmph6_len;
	pack->dst_data_filled = icmph4_len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_ProtoTcp64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len)
{
	U32 u0,tcph_len;
	struct tcphdr *tcph4,*tcph6;

	tcph6 = (struct tcphdr *)pack->src_buf;
	tcph4 = (struct tcphdr *)pack->dst_buf;
	tcph_len = tcph6->doff * 4;

	if(tcph_len<BIH_TCP_HDR_SIZE || pack->src_buf_len<tcph_len || pack->dst_buf_len<tcph_len)
	{
		BIH_LOG(2,"Bih_ProtoTcp64: len error");
		return BIH_RES_FAILED;
	}

	//transfer tcp
	memcpy(tcph4,tcph6,tcph_len);

	//update checksum
	u0 = csum_partial(pl,pl_len,0);
	tcph4->check = 0;
	u0 = csum_partial((U8 *)tcph4,tcph_len,u0);
	tcph4->check = csum_tcpudp_magic(info->saddr4.s_addr,info->daddr4.s_addr,tcph_len+pl_len,IPPROTO_TCP,u0);

	//ok
	pack->src_data_used = tcph_len;
	pack->dst_data_filled = tcph_len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_ProtoUdp64(BIH_TRANS_INFO *info,BIH_TRANS_PACK *pack,U8 *pl,U32 pl_len)
{
	U32 u0,udph_len;
	struct udphdr *udph4,*udph6;

	udph6 = (struct udphdr *)pack->src_buf;
	udph4 = (struct udphdr *)pack->dst_buf;
	udph_len = BIH_UDP_HDR_SIZE;

	if(pack->src_buf_len<udph_len || pack->dst_buf_len<udph_len)
	{
		BIH_LOG(2,"Bih_ProtoUdp64: len error");
		return BIH_RES_FAILED;
	}

	//transfer udp
	memcpy(udph4,udph6,udph_len);
	udph4->len = htons(udph_len+pl_len);

	//update checksum
	u0 = csum_partial(pl,pl_len,0);
	udph4->check = 0;
	u0 = csum_partial((U8 *)udph4,udph_len,u0);
	udph4->check = csum_tcpudp_magic(info->saddr4.s_addr,info->daddr4.s_addr,udph_len+pl_len,IPPROTO_UDP,u0);

	//ok
	pack->src_data_used = udph_len;
	pack->dst_data_filled = udph_len;

	return BIH_RES_SUCCESS;

}



/*bih_dns.c**********************************************************/


RESULT Bih_DnsReq46(BIH_OBJ *bih,BIH_DNS_MSG *msg,BOOLE *has_addr,BOOLE *map_addr)
{
	BOOLE addr_ok,map_ok,resource;
	U32 i,j,offset0,offset1;
	U8 *buf0,*buf1;
	U32 num[4];
	BIH_DNS_SECT sect;

	addr_ok = BOOLE_FALSE;
	map_ok = BOOLE_FALSE;
	buf0 = msg->src_msg;
	buf1 = msg->dst_msg;
	offset0 = 0;
	offset1 = 0;

	//copy head
	if(offset0+12>msg->src_msg_len || offset1+12>msg->dst_buf_len)
	{
		BIH_LOG(2,"Bih_DnsReq46: msg len error");
		return BIH_RES_FAILED;
	}

	num[0] = BIH_READ_BUF16BE(buf0+4);//question
	num[1] = BIH_READ_BUF16BE(buf0+6);//answer
	num[2] = BIH_READ_BUF16BE(buf0+8);//authority
	num[3] = BIH_READ_BUF16BE(buf0+10);	//additional

	memcpy(buf1,buf0,12);
	offset0 += 12;
	offset1 += 12;

	//copy sector
	resource = BOOLE_FALSE;
	for(i=0;i<4;i++)
	{
		for(j=0;j<num[i];j++)
		{
			if(Bih_DnsGetSect(msg,&sect,offset0,resource)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}

			if(i==0 && sect.type==BIH_DNS_TYPE_A && sect.class0==BIH_DNS_CLASS_IN)
			{
				sect.type = BIH_DNS_TYPE_AAAA;
				addr_ok = BOOLE_TRUE;
				map_ok = BOOLE_TRUE;
			}
			else if(i==0 && sect.type==BIH_DNS_TYPE_AAAA && sect.class0==BIH_DNS_CLASS_IN)
			{
				addr_ok = BOOLE_TRUE;
			}
			if(Bih_DnsCopySect(msg,&sect,&offset1,resource)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}

			offset0 += sect.tol_len;
		}

		resource = BOOLE_TRUE;
	}

	msg->dst_msg_len = offset1;
	*has_addr = addr_ok;
	*map_addr = map_ok;

	return BIH_RES_SUCCESS;
}


RESULT Bih_DnsRsp64(BIH_OBJ *bih,BIH_DNS_MSG *msg,BOOLE *has_addr,BOOLE *map_addr)
{
	BOOLE addr_ok,map_ok,resource;
	U32 i,j,offset0,offset1;
	U8 *buf0,*buf1;
	BIH_MAP_ADDR *map;
	BIH_DNS_SECT sect;
	struct in_addr addr4;
	struct in6_addr addr6;
	U32 num[4];

	addr_ok = BOOLE_FALSE;
	map_ok = BOOLE_FALSE;
	buf0 = msg->src_msg;
	buf1 = msg->dst_msg;
	offset0 = 0;
	offset1 = 0;

	//copy head
	if(offset0+12>msg->src_msg_len || offset1+12>msg->dst_buf_len)
	{
		BIH_LOG(2,"Bih_DnsRsp64: msg len error");
		return BIH_RES_FAILED;
	}

	num[0] = BIH_READ_BUF16BE(buf0+4);//question
	num[1] = BIH_READ_BUF16BE(buf0+6);//answer
	num[2] = BIH_READ_BUF16BE(buf0+8);//authority
	num[3] = BIH_READ_BUF16BE(buf0+10);	//additional

	memcpy(buf1,buf0,12);
	offset0 += 12;
	offset1 += 12;

	//copy sector
	resource = BOOLE_FALSE;
	for(i=0;i<4;i++)
	{
		for(j=0;j<num[i];j++)
		{
			if(Bih_DnsGetSect(msg,&sect,offset0,resource)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}

			if(i==0 && sect.type==BIH_DNS_TYPE_AAAA && sect.class0==BIH_DNS_CLASS_IN)//question
			{
				sect.type = BIH_DNS_TYPE_A;
				//map_ok = BOOLE_TRUE;//this is a AAAA rsp
			}
			else if(i==1 && sect.type==BIH_DNS_TYPE_A && sect.class0==BIH_DNS_CLASS_IN)//answer
			{
				BIH_LOG(3,"Bih_DnsRsp64: A rsp");
				addr_ok = BOOLE_TRUE;
			}
			else if(i==1 && sect.type==BIH_DNS_TYPE_AAAA && sect.class0==BIH_DNS_CLASS_IN && sect.data_len==16)//answer
			{
				BIH_LOG(3,"Bih_DnsRsp64: AAAA rsp");

				memcpy(addr6.s6_addr,sect.data,16);

				map = Bih_MapSearch6(bih,&addr6);
				if(map != NULL)
				{
					addr4.s_addr = map->addr4.s_addr;
				}
				else
				{
					if(Bih_MapBuild64(bih,&addr6,&addr4,BIH_MAP_ADDR_FLAG_READY|BIH_MAP_ADDR_FLAG_FIXED,sect.ttl)!= BIH_RES_SUCCESS)
					{
						BIH_LOG(1,"Bih_DnsRsp64: build map error");
						return BIH_RES_FAILED;
					}
				}

				sect.type = BIH_DNS_TYPE_A;
				sect.data_len = 4;
				sect.data = (U8 *)&addr4.s_addr;

				map_ok = BOOLE_TRUE;//this is a AAAA rsp
				addr_ok = BOOLE_TRUE;
			}
			if(Bih_DnsCopySect(msg,&sect,&offset1,resource)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}

			offset0 += sect.tol_len;
		}

		resource = BOOLE_TRUE;
	}

	msg->dst_msg_len = offset1;
	*has_addr = addr_ok;
	*map_addr = map_ok;

	return BIH_RES_SUCCESS;
}


void Bih_DnsTimerCb(BIH_OBJ *bih)
{
	U32 i,curr_time;
	BIH_LINK_HEAD *head;
	BIH_MATCH_ITEM *item;
	BIH_DNS_REC *rec,*rec2;

	curr_time = jiffies;

	BIH_LOG(3,"Bih_DnsTimerCb: time cb");

	for(i=0;i<BIH_DNS_REC_LIST_NUM;i++)
	{
		head = &bih->dns_list[i];

		rec = head->next;
		while(rec != (void *)head)
		{
			rec2 = rec;
			rec = rec->next;

			if(time_after((unsigned long)curr_time,(unsigned long)rec2->start_time+BIH_DNS_TIMEOUT_RSP))
			{
				BIH_LOG(2,"Bih_DnsTimerCb: dns time out");

				item = Bih_MatchSearchItem(bih,&rec2->addr4,BIH_MATCH_ITEM_TYPE_UDP,rec2->port);

				Bih_DnsFreeRec(bih,rec2);

				if(item != NULL)
				{
					Bih_MatchFreeItem(bih,item);
				}
			}
			else if(time_after((unsigned long)curr_time,(unsigned long)rec2->start_time+BIH_DNS_TIMEOUT_V6))
			{
				if(rec2->rsp != NULL)
				{
					BIH_LOG(2,"Bih_DnsTimerCb: dns v4 time out,recv v6 rsp");

					Bih_DnsSendRec(bih,rec2);

					item = Bih_MatchSearchItem(bih,&rec2->addr4,BIH_MATCH_ITEM_TYPE_UDP,rec2->port);

					Bih_DnsFreeRec(bih,rec2);

					if(item != NULL)
					{
						Bih_MatchFreeItem(bih,item);
					}
				}
			}
		}
	}

	return;
}

RESULT Bih_DnsGetSect(BIH_DNS_MSG *msg,BIH_DNS_SECT *sect,U32 src_offset,BOOLE resource)
{
	U32 offset;
	U8 *buf;

	offset = src_offset;
	buf = msg->src_msg;

	//name
	if(Bih_DnsCheckDomain(msg,sect,offset)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += sect->name_len;

	//type,class
	if(offset+4 > msg->src_msg_len)
	{
		BIH_LOG(2,"Bih_DnsGetSect: msg len error");
		return BIH_RES_FAILED;
	}

	sect->type = BIH_READ_BUF16BE(buf+offset);
	sect->class0 = BIH_READ_BUF16BE(buf+offset+2);
	offset += 4;

	if(resource)
	{
		//ttl,data len
		if(offset+6 > msg->src_msg_len)
		{
			BIH_LOG(2,"Bih_DnsGetSect: msg len error");
			return BIH_RES_FAILED;
		}

		sect->ttl = BIH_READ_BUF32BE(buf+offset);
		sect->data_len = BIH_READ_BUF16BE(buf+offset+4);
		offset += 6;

		//data
		if(offset+sect->data_len > msg->src_msg_len)
		{
			BIH_LOG(2,"Bih_DnsGetSect: msg len error");
			return BIH_RES_FAILED;
		}

		sect->data = buf + offset;
		offset += sect->data_len;
	}
	else
	{
		sect->data_len = 0;
		sect->data = NULL;
	}

	sect->tol_len = offset - src_offset;

	return BIH_RES_SUCCESS;
}


RESULT Bih_DnsCopySect(BIH_DNS_MSG *msg,BIH_DNS_SECT *sect,U32 *dst_offset,BOOLE resource)
{
	U32 offset;
	U8 *buf;

	offset = *dst_offset;
	buf = msg->dst_msg;

	//name
	if(Bih_DnsCopyDomain(msg,sect,&offset)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}

	//type,class
	if(offset+4 > msg->dst_buf_len)
	{
		BIH_LOG(2,"Bih_DnsCopySect: msg len error");
		return BIH_RES_FAILED;
	}

	BIH_WRITE_BUF16BE(buf+offset,sect->type);
	BIH_WRITE_BUF16BE(buf+offset+2,sect->class0);
	offset += 4;

	if(resource)
	{
		//ttl,data len
		if(offset+6 > msg->dst_buf_len)
		{
			BIH_LOG(2,"Bih_DnsCopySect: msg len error");
			return BIH_RES_FAILED;
		}

		BIH_WRITE_BUF32BE(buf+offset,sect->ttl);
		BIH_WRITE_BUF16BE(buf+offset+4,sect->data_len);
		offset += 6;

		//data
		if(offset+sect->data_len > msg->dst_buf_len)
		{
			BIH_LOG(2,"Bih_DnsCopySect: msg len error");
			return BIH_RES_FAILED;
		}

		memcpy(buf+offset,sect->data,sect->data_len);
		offset += sect->data_len;
	}

	*dst_offset = offset;

	return BIH_RES_SUCCESS;
}


RESULT Bih_DnsCheckDomain(BIH_DNS_MSG *msg,BIH_DNS_SECT *sect,U32 src_offset)
{
	BOOLE is_ptr;
	U32 u0,offset,len,rem;
	U8 *buf;

	is_ptr = BOOLE_FALSE;
	offset = src_offset;
	len = 0;
	buf = msg->src_msg + src_offset;
	rem = msg->src_msg_len - src_offset;

	//check pointer
	if(rem>0 && (buf[0]&0xc0)==0xc0)
	{
		//msg offset
		if(rem < 2)
		{
			BIH_LOG(2,"Bih_DnsCheckDomain: msg len error");
			return BIH_RES_FAILED;
		}
		u0 = buf[0]<<8 | buf[1];
		u0 &= 0x3f;

		if(u0 >= msg->src_msg_len)
		{
			BIH_LOG(2,"Bih_DnsCheckDomain: msg len error");
			return BIH_RES_FAILED;
		}
		buf = msg->src_msg + u0;
		rem = msg->src_msg_len - u0;
		offset = u0;

		is_ptr = BOOLE_TRUE;
	}

	//domain
	while(1)
	{
		//label len
		if(rem < 1)
		{
			BIH_LOG(2,"Bih_DnsCheckDomain: label error");
			return BIH_RES_FAILED;
		}
		u0 = buf[0];
		buf++;
		rem--;
		len++;

		if(u0 == 0)//ok
		{
			break;
		}
		else if((u0&0xc0)!= 0)
		{
			BIH_LOG(2,"Bih_DnsCheckDomain: label error");
			return BIH_RES_FAILED;
		}

		//skip label
		if(rem < u0)
		{
			BIH_LOG(2,"Bih_DnsCheckDomain: label error");
			return BIH_RES_FAILED;
		}
		buf += u0;
		rem -= u0;
		len += u0;
	}

	sect->name = msg->src_msg + src_offset;
	sect->name_len = (is_ptr)? 2 : len;
	sect->name_real = msg->src_msg + offset;
	sect->name_real_len = len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_DnsCopyDomain(BIH_DNS_MSG *msg,BIH_DNS_SECT *sect,U32 *dst_offset)
{
	U32 u0,offset;
	U8 *buf;

	offset = *dst_offset;
	buf = msg->dst_msg;

	//check pointer
	if(Bih_BufSearchStr(msg->dst_msg,offset,sect->name_real,sect->name_real_len,&u0)== BIH_RES_SUCCESS)
	{
		if(offset+2 > msg->dst_buf_len)
		{
			BIH_LOG(2,"Bih_DnsCopyDomain: msg len error");
			return BIH_RES_FAILED;
		}

		u0 |= 0xc000;
		BIH_WRITE_BUF16BE(buf+offset,u0);
		offset += 2;
	}
	else//copy domain
	{
		if(offset+sect->name_real_len > msg->dst_buf_len)
		{
			BIH_LOG(2,"Bih_DnsCopyDomain: msg len error");
			return BIH_RES_FAILED;
		}

		memcpy(buf+offset,sect->name_real,sect->name_real_len);
		offset += sect->name_real_len;
	}

	*dst_offset = offset;

	return BIH_RES_SUCCESS;
}


BIH_DNS_REC *Bih_DnsAddRec(BIH_OBJ *bih,struct in_addr *addr4,U32 port,U32 id)
{
	BIH_DNS_REC *rec;

	rec = Bih_DnsSearchRec(bih,port,id);
	if(rec == NULL)
	{
		rec = Bih_DnsAllocRec(bih);
		if(rec == NULL)
		{
			return NULL;
		}

		rec->port = port;
		rec->id = id;
		rec->start_time = jiffies;
		rec->rsp = NULL;
		rec->addr4.s_addr = addr4->s_addr;

		spin_lock_bh(&bih_dns_lock);//lock
		Bih_LinkPutFront(Bih_DnsGetHead(bih,port),rec);
		spin_unlock_bh(&bih_dns_lock);//unlock
	}

	return rec;
}


BIH_DNS_REC *Bih_DnsSearchRec(BIH_OBJ *bih,U32 port,U32 id)
{
	BIH_LINK_HEAD *head;
	BIH_DNS_REC *rec;

	head = Bih_DnsGetHead(bih,port);

	rec = head->next;
	while(rec != (void *)head)
	{
		if(rec->port==port && (rec->id==id || rec->id==((id-1)&0xffff)))
		{
			break;
		}

		rec = rec->next;
	}
	if(rec == (void *)head)
	{
		return NULL;
	}

	return rec;
}


BIH_DNS_REC *Bih_DnsAllocRec(BIH_OBJ *bih)
{
	BIH_DNS_REC *rec;

	spin_lock_bh(&bih_dns_lock);//lock

	rec = Bih_LinkGetBehind(&bih->dns_free);
	if(rec == NULL)
	{
		spin_unlock_bh(&bih_dns_lock);//unlock
		return NULL;
	}

	spin_unlock_bh(&bih_dns_lock);//unlock

	return rec;
}


void Bih_DnsFreeRec(BIH_OBJ *bih,BIH_DNS_REC *rec)
{
	struct sk_buff *skb;

	skb = NULL;

	spin_lock_bh(&bih_dns_lock);//lock

	Bih_LinkRemove(rec);

	skb = rec->rsp;
	rec->rsp = NULL;
	Bih_LinkPutFront(&bih->dns_free,rec);

	spin_unlock_bh(&bih_dns_lock);//unlock

	if(skb != NULL)
	{
		dev_kfree_skb(skb);
	}

	return;
}


void Bih_DnsSendRec(BIH_OBJ *bih,BIH_DNS_REC *rec)
{
	struct sk_buff *skb;

	spin_lock_bh(&bih_dns_lock);//lock
	skb = rec->rsp;
	rec->rsp = NULL;
	spin_unlock_bh(&bih_dns_lock);//unlock

	if(skb != NULL)
	{
		netif_rx(skb);
	}
	return;
}


void Bih_DnsSetRecRsp(BIH_DNS_REC *rec,struct sk_buff *rsp)
{
	struct sk_buff *skb;

	spin_lock_bh(&bih_dns_lock);//lock
	skb = rec->rsp;
	rec->rsp = rsp;
	spin_unlock_bh(&bih_dns_lock);//unlock

	if(skb != NULL)
	{
		dev_kfree_skb(skb);
	}

	return;
}


BIH_LINK_HEAD *Bih_DnsGetHead(BIH_OBJ *bih,U32 port)
{
	U32 u0;

	u0 = port % BIH_DNS_REC_LIST_NUM;

	return &bih->dns_list[u0];
}


RESULT Bih_DnsReadList(BIH_OBJ *bih, struct seq_file *m)
{
	S32 res;
	U32 i,j;
	BIH_DNS_REC *rec;

	if(!bih->run)
	{
		return BIH_RES_FAILED;
	}

	j = 0;
	for(i=0;i<BIH_DNS_REC_LIST_NUM && j<50;i++)
	{
		rec = bih->dns_list[i].next;

		while(rec!=(void *)&bih->dns_list[i] && j<50)
		{
			res = seq_printf(m,"%d, %04x, %u, %p\n",rec->port,rec->id,rec->start_time,rec->rsp);
			if(res < 0)
			{
				//return BIH_RES_FAILED;
				j = 50;
				break;
			}

			rec = rec->next;
			j++;
		}
	}

	return BIH_RES_SUCCESS;
}


RESULT Bih_DnsWriteList(BIH_OBJ *bih,U8 *buf,U32 buf_len)
{
	return BIH_RES_SUCCESS;
}



/*bih_map.c**********************************************************/


RESULT Bih_MapAddr46(BIH_OBJ *bih,struct in_addr *local4,struct in_addr *remote4,struct in6_addr *local6,struct in6_addr *remote6)
{
	U32 i;
	BIH_MAP_HOST *host;
	BIH_MAP_ADDR *map;

	if(ipv4_is_zeronet(local4->s_addr)||
		ipv4_is_loopback(remote4->s_addr)||
		ipv4_is_multicast(remote4->s_addr)||
		ipv4_is_lbcast(remote4->s_addr)||
		ipv4_is_zeronet(remote4->s_addr)||
		remote4->s_addr==local4->s_addr)
	{
		return BIH_RES_FAILED;
	}

	//saddr4
	for(i=0;i<BIH_MAP_HOST_NUM;i++)
	{
		host = &bih->host[i];
		if(host->addr4.s_addr!=0 && host->addr4.s_addr==local4->s_addr)
		{
			memcpy(local6->s6_addr,host->addr6.s6_addr,16);
			break;
		}
	}
	if(i >= BIH_MAP_HOST_NUM)
	{
		return BIH_RES_FAILED;
	}

	//daddr4
		map = Bih_MapSearch4(bih,remote4);
		if(map != NULL)
		{
			memcpy(remote6->s6_addr,map->addr6.s6_addr,16);
		}
		else
		{
			BIH_LOG(3,"Bih_MapAddr46: convert addr");
			if(Bih_MapConvert46(host,remote4,remote6)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}
		}

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapConvert46(BIH_MAP_HOST *host,struct in_addr *addr4,struct in6_addr *addr6)
{
	memcpy(addr6->s6_addr,host->prefix6.s6_addr,16);

	switch(host->prefix6_len)
	{
	case 32:
		addr6->s6_addr[4] = ((U8 *)&addr4->s_addr)[0];
		addr6->s6_addr[5] = ((U8 *)&addr4->s_addr)[1];
		addr6->s6_addr[6] = ((U8 *)&addr4->s_addr)[2];
		addr6->s6_addr[7] = ((U8 *)&addr4->s_addr)[3];
		break;

	case 40:
		addr6->s6_addr[5] = ((U8 *)&addr4->s_addr)[0];
		addr6->s6_addr[6] = ((U8 *)&addr4->s_addr)[1];
		addr6->s6_addr[7] = ((U8 *)&addr4->s_addr)[2];
		addr6->s6_addr[9] = ((U8 *)&addr4->s_addr)[3];
		break;

	case 48:
		addr6->s6_addr[6] = ((U8 *)&addr4->s_addr)[0];
		addr6->s6_addr[7] = ((U8 *)&addr4->s_addr)[1];
		addr6->s6_addr[9] = ((U8 *)&addr4->s_addr)[2];
		addr6->s6_addr[10] = ((U8 *)&addr4->s_addr)[3];
		break;

	case 56:
		addr6->s6_addr[7] = ((U8 *)&addr4->s_addr)[0];
		addr6->s6_addr[9] = ((U8 *)&addr4->s_addr)[1];
		addr6->s6_addr[10] = ((U8 *)&addr4->s_addr)[2];
		addr6->s6_addr[11] = ((U8 *)&addr4->s_addr)[3];
		break;

	case 64:
		addr6->s6_addr[9] = ((U8 *)&addr4->s_addr)[0];
		addr6->s6_addr[10] = ((U8 *)&addr4->s_addr)[1];
		addr6->s6_addr[11] = ((U8 *)&addr4->s_addr)[2];
		addr6->s6_addr[12] = ((U8 *)&addr4->s_addr)[3];
		break;

	case 96:
		addr6->s6_addr[12] = ((U8 *)&addr4->s_addr)[0];
		addr6->s6_addr[13] = ((U8 *)&addr4->s_addr)[1];
		addr6->s6_addr[14] = ((U8 *)&addr4->s_addr)[2];
		addr6->s6_addr[15] = ((U8 *)&addr4->s_addr)[3];
		break;

	default:
		return BIH_RES_FAILED;
	}

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapAddr64(BIH_OBJ *bih,struct in6_addr *remote6,struct in6_addr *local6,struct in_addr *remote4,struct in_addr *local4)
{
	int addr_type;
	U32 i;
	BIH_MAP_HOST *host;
	BIH_MAP_ADDR *map;

	addr_type = ipv6_addr_type(local6);
	if(addr_type&IPV6_ADDR_MULTICAST || addr_type&IPV6_ADDR_LOOPBACK || addr_type&IPV6_ADDR_LINKLOCAL || addr_type&IPV6_ADDR_SITELOCAL)
	{
		return BIH_RES_FAILED;
	}

	addr_type = ipv6_addr_type(remote6);
	if(addr_type&IPV6_ADDR_LINKLOCAL || addr_type&IPV6_ADDR_SITELOCAL)
	{
		return BIH_RES_FAILED;
	}

	//daddr4
	for(i=0;i<BIH_MAP_HOST_NUM;i++)
	{
		host = &bih->host[i];
		if(host->addr4.s_addr!=0 && memcmp(host->addr6.s6_addr,local6->s6_addr,16)==0)
		{
			local4->s_addr = host->addr4.s_addr;
			break;
		}
	}
	if(i >= BIH_MAP_HOST_NUM)
	{
		return BIH_RES_FAILED;
	}

	//saddr4
		map = Bih_MapSearch6(bih,remote6);
		if(map != NULL)
		{
			remote4->s_addr = map->addr4.s_addr;
		}
		else
		{
			BIH_LOG(3,"Bih_MapAddr64: convert addr");
			if(Bih_MapConvert64(host,remote6,remote4)!= BIH_RES_SUCCESS)
			{
				return BIH_RES_FAILED;
			}
		}

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapConvert64(BIH_MAP_HOST *host,struct in6_addr *addr6,struct in_addr *addr4)
{
	U8 ip4[4],ip6[16];

	memcpy(ip6,addr6->s6_addr,16);

	switch(host->prefix6_len)
	{
	case 32:
		ip4[0] = ip6[4];
		ip4[1] = ip6[5];
		ip4[2] = ip6[6];
		ip4[3] = ip6[7];
		ip6[4] = 0;
		ip6[5] = 0;
		ip6[6] = 0;
		ip6[7] = 0;
		break;

	case 40:
		ip4[0] = ip6[5];
		ip4[1] = ip6[6];
		ip4[2] = ip6[7];
		ip4[3] = ip6[9];
		ip6[5] = 0;
		ip6[6] = 0;
		ip6[7] = 0;
		ip6[9] = 0;
		break;

	case 48:
		ip4[0] = ip6[6];
		ip4[1] = ip6[7];
		ip4[2] = ip6[9];
		ip4[3] = ip6[10];
		ip6[6] = 0;
		ip6[7] = 0;
		ip6[9] = 0;
		ip6[10] = 0;
		break;

	case 56:
		ip4[0] = ip6[7];
		ip4[1] = ip6[9];
		ip4[2] = ip6[10];
		ip4[3] = ip6[11];
		ip6[7] = 0;
		ip6[9] = 0;
		ip6[10] = 0;
		ip6[11] = 0;
		break;

	case 64:
		ip4[0] = ip6[9];
		ip4[1] = ip6[10];
		ip4[2] = ip6[11];
		ip4[3] = ip6[12];
		ip6[9] = 0;
		ip6[10] = 0;
		ip6[11] = 0;
		ip6[12] = 0;
		break;

	case 96:
		ip4[0] = ip6[12];
		ip4[1] = ip6[13];
		ip4[2] = ip6[14];
		ip4[3] = ip6[15];
		ip6[12] = 0;
		ip6[13] = 0;
		ip6[14] = 0;
		ip6[15] = 0;
		break;

	default:
		return BIH_RES_FAILED;
	}

	if(memcmp(ip6,host->prefix6.s6_addr,16)!= 0)
	{
		return BIH_RES_FAILED;
	}

	addr4->s_addr = *((U32 *)ip4);

	return BIH_RES_SUCCESS;
}


void Bih_MapTimerCb(BIH_OBJ *bih)
{
	U32 i,curr_time;
	BIH_LINK_HEAD *head;
	BIH_MAP_PTR *ptr;
	BIH_MAP_PAIR *pair;
	BIH_MAP_ADDR *map;

	curr_time = jiffies;

	BIH_LOG(3,"Bih_MapTimerCb: time cb");

	for(i=0;i<BIH_MAP_ADDR_LIST_NUM;i++)
	{
		head = &bih->map4_list[i];

		ptr = head->next;
		while(ptr != (void *)head)
		{
			pair = BIH_MAP_GET_PAIR_PTR4(ptr);
			map = &pair->map;
			ptr = ptr->next;

			if(map->flag & BIH_MAP_ADDR_FLAG_READY)
			{
				if(time_after((unsigned long)curr_time,(unsigned long)map->last_time+BIH_MAP_TIMEOUT_READY))
				{
					BIH_LOG(2,"Bih_MapTimerCb: map ready timeout");
					Bih_MapFreePair(bih,pair);
					continue;
				}
			}

			if(map->flag & BIH_MAP_ADDR_FLAG_FIXED)
			{
				if(time_after((unsigned long)curr_time,(unsigned long)map->start_time+(unsigned long)map->timeout))
				{
					BIH_LOG(2,"Bih_MapTimerCb: map fixed timeout");
					Bih_MapFreePair(bih,pair);
					continue;
				}
			}

			if(time_after((unsigned long)curr_time,(unsigned long)map->last_time+BIH_MAP_TIMEOUT_INACT))
			{
				BIH_LOG(2,"Bih_MapTimerCb: map timeout");
				Bih_MapFreePair(bih,pair);
			}
		}
	}

	return;
}


RESULT Bih_MapBuild64(BIH_OBJ *bih,struct in6_addr *addr6,struct in_addr *addr4,U32 flag,U32 timeout)
{
	BIH_MAP_PAIR *pair;
	BIH_MAP_ADDR *map;

	//alloc map addr pair
	pair = Bih_MapAllocPair(bih);
	if(pair == NULL)
	{
		return BIH_RES_FAILED;
	}
	map = &pair->map;

	//fill param
	map->flag = flag;
	map->start_time = jiffies;
	map->last_time = map->start_time;
	map->timeout = timeout * HZ;
	memcpy(map->addr6.s6_addr,addr6->s6_addr,16);

	spin_lock_bh(&bih_map_lock);//lock
	Bih_LinkPutFront(Bih_MapGetHead4(bih,&pair->map.addr4),&pair->ptr4);
	Bih_LinkPutFront(Bih_MapGetHead6(bih,&pair->map.addr6),&pair->ptr6);
	spin_unlock_bh(&bih_map_lock);//unlock

	addr4->s_addr = map->addr4.s_addr;

	return BIH_RES_SUCCESS;
}


BIH_MAP_ADDR *Bih_MapSearch4(BIH_OBJ *bih,struct in_addr *addr4)
{
	U32 i;
	BIH_LINK_HEAD *head;
	BIH_MAP_PTR *ptr;
	BIH_MAP_PAIR *pair;
	BIH_MAP_ADDR *map;

	//check dns
	for(i=0;i<BIH_MAP_DNS_NUM;i++)
	{
		map = &bih->dns[i];
		if(map->addr4.s_addr!=0 && map->addr4.s_addr==addr4->s_addr)
		{
			break;
		}
	}
	if(i < BIH_MAP_DNS_NUM)
	{
		return map;
	}

	//not dns addr
	head = Bih_MapGetHead4(bih,addr4);

	ptr = head->next;
	while(ptr != (void *)head)
	{
		pair = BIH_MAP_GET_PAIR_PTR4(ptr);

		if(pair->map.addr4.s_addr == addr4->s_addr)
		{
			break;
		}

		ptr = ptr->next;
	}
	if(ptr == (void *)head)
	{
		return NULL;
	}

	pair->map.flag &= ~BIH_MAP_ADDR_FLAG_READY;
	pair->map.last_time = jiffies;

	return &pair->map;
}


BIH_MAP_ADDR *Bih_MapSearch6(BIH_OBJ *bih,struct in6_addr *addr6)
{
	U32 i;
	BIH_LINK_HEAD *head;
	BIH_MAP_PTR *ptr;
	BIH_MAP_PAIR *pair;
	BIH_MAP_ADDR *map;

	//check dns
	for(i=0;i<BIH_MAP_DNS_NUM;i++)
	{
		map = &bih->dns[i];
		if(map->addr4.s_addr!=0 && memcmp(map->addr6.s6_addr,addr6->s6_addr,16)==0)
		{
			break;
		}
	}
	if(i < BIH_MAP_DNS_NUM)
	{
		return map;
	}

	//not dns addr
	head = Bih_MapGetHead6(bih,addr6);

	ptr = head->next;
	while(ptr != (void *)head)
	{
		pair = BIH_MAP_GET_PAIR_PTR6(ptr);

		if(memcmp(pair->map.addr6.s6_addr,addr6->s6_addr,16)== 0)
		{
			break;
		}

		ptr = ptr->next;
	}
	if(ptr == (void *)head)
	{
		return NULL;
	}

	pair->map.flag &= ~BIH_MAP_ADDR_FLAG_READY;
	pair->map.last_time = jiffies;

	return &pair->map;
}


BIH_MAP_PAIR *Bih_MapAllocPair(BIH_OBJ *bih)
{
	BIH_MAP_PAIR *pair;

	spin_lock_bh(&bih_map_lock);//lock

	pair = Bih_LinkGetBehind(&bih->map_free);
	if(pair == NULL)
	{
		spin_unlock_bh(&bih_map_lock);//unlock
		return NULL;
	}

	Bih_LinkInit(&pair->ptr4);
	Bih_LinkInit(&pair->ptr6);

	spin_unlock_bh(&bih_map_lock);//unlock

	return pair;
}


void Bih_MapFreePair(BIH_OBJ *bih,BIH_MAP_PAIR *pair)
{
	spin_lock_bh(&bih_map_lock);//lock

	Bih_LinkRemove(&pair->ptr4);
	Bih_LinkRemove(&pair->ptr6);
	Bih_LinkPutFront(&bih->map_free,pair);

	spin_unlock_bh(&bih_map_lock);//unlock

	return;
}


BIH_LINK_HEAD *Bih_MapGetHead4(BIH_OBJ *bih,struct in_addr *addr4)
{
	U32 u0;

	u0 = addr4->s_addr % BIH_MAP_ADDR_LIST_NUM;

	return &bih->map4_list[u0];
}


BIH_LINK_HEAD *Bih_MapGetHead6(BIH_OBJ *bih,struct in6_addr *addr6)
{
	U32 u0;

	u0 = addr6->s6_addr32[0];
	u0 += addr6->s6_addr32[1];
	u0 += addr6->s6_addr32[2];
	u0 += addr6->s6_addr32[3];
	u0 %= BIH_MAP_ADDR_LIST_NUM;

	return &bih->map6_list[u0];
}


RESULT Bih_MapReadHost(BIH_OBJ *bih, struct seq_file *m)
{
	S32 res;
	U32 i;
	BIH_MAP_HOST *host;

	for(i=0;i<BIH_MAP_HOST_NUM;i++)
	{
		host = &bih->host[i];
		if(host->addr4.s_addr == 0)
		{
			continue;
		}

		//id
		res = seq_printf(m,"%d  ",i);
		if(res < 0)
		{
			return BIH_RES_FAILED;
		}

		//addr4
		res = seq_printf(m,"%d.%d.%d.%d/%d  ",BIH_SEGMENT_ADDR4(&host->addr4.s_addr),host->mask4_len);
		if(res < 0)
		{
			return BIH_RES_FAILED;
		}

		//addr6
		res = seq_printf(m,"%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x/%d  ",
					ntohs(host->addr6.s6_addr16[0]),
					ntohs(host->addr6.s6_addr16[1]),
					ntohs(host->addr6.s6_addr16[2]),
					ntohs(host->addr6.s6_addr16[3]),
					ntohs(host->addr6.s6_addr16[4]),
					ntohs(host->addr6.s6_addr16[5]),
					ntohs(host->addr6.s6_addr16[6]),
					ntohs(host->addr6.s6_addr16[7]),
					host->mask6_len);
		if(res < 0)
		{
			return BIH_RES_FAILED;
		}

		//prefix6
		res = seq_printf(m,"%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x/%d\n",
					ntohs(host->prefix6.s6_addr16[0]),
					ntohs(host->prefix6.s6_addr16[1]),
					ntohs(host->prefix6.s6_addr16[2]),
					ntohs(host->prefix6.s6_addr16[3]),
					ntohs(host->prefix6.s6_addr16[4]),
					ntohs(host->prefix6.s6_addr16[5]),
					ntohs(host->prefix6.s6_addr16[6]),
					ntohs(host->prefix6.s6_addr16[7]),
					host->prefix6_len);
		if(res < 0)
		{
			return BIH_RES_FAILED;
		}
	}

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapWriteHost(BIH_OBJ *bih,U8 *buf,U32 buf_len)
{
	U32 u0,id,offset,len;
	BIH_MAP_HOST host;

	offset = 0;

	//id addr4/mask4 addr6/mask6 prefix6/prefix_len6
	//0 172.21.45.67/20 2002:0da8:00bf:1010:0000:0000:c0a8:0002/64 2002:0da8:00bf:1010:0000:0000:0000:0000/64

	//id
	len = buf_len - offset;
	if(Bih_BufGetDecimal(buf+offset,&len,&u0)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	if(u0 >= BIH_MAP_HOST_NUM)
	{
		return BIH_RES_FAILED;
	}
	id = u0;

	//addr4
	len = buf_len - offset;
	if(Bih_BufGetAddr4(buf+offset,&len,&host.addr4,&host.mask4_len)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	//addr6
	len = buf_len - offset;
	if(Bih_BufGetAddr6(buf+offset,&len,&host.addr6,&host.mask6_len)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	//addr6 prefix
	len = buf_len - offset;
	if(Bih_BufGetAddr6(buf+offset,&len,&host.prefix6,&host.prefix6_len)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	if(host.prefix6_len!=32 && host.prefix6_len!=40 && host.prefix6_len!=48 && host.prefix6_len!=56 && host.prefix6_len!=64 && host.prefix6_len!=96)
	{
		return BIH_RES_FAILED;
	}

	spin_lock_bh(&bih_map_lock);//lock
	memcpy(&bih->host[id],&host,sizeof(BIH_MAP_HOST));
	spin_unlock_bh(&bih_map_lock);//unlock

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapReadDns(BIH_OBJ *bih, struct seq_file *m)
{
	S32 res;
	U32 i;
	BIH_MAP_ADDR *map;


	for(i=0;i<BIH_MAP_DNS_NUM;i++)
	{
		map = &bih->dns[i];
		if(map->addr4.s_addr == 0)
		{
			continue;
		}

		//id
		res = seq_printf(m,"%d  ",i);
		if(res < 0)
		{
			return BIH_RES_FAILED;
		}

		//dns_addr4
		res = seq_printf(m,"%d.%d.%d.%d  ",BIH_SEGMENT_ADDR4(&map->addr4.s_addr));
		if(res < 0)
		{
			return BIH_RES_FAILED;
		}

		//dns_addr6
		res = seq_printf(m,"%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
					ntohs(map->addr6.s6_addr16[0]),
					ntohs(map->addr6.s6_addr16[1]),
					ntohs(map->addr6.s6_addr16[2]),
					ntohs(map->addr6.s6_addr16[3]),
					ntohs(map->addr6.s6_addr16[4]),
					ntohs(map->addr6.s6_addr16[5]),
					ntohs(map->addr6.s6_addr16[6]),
					ntohs(map->addr6.s6_addr16[7]));
		if(res < 0)
		{
			return BIH_RES_FAILED;
		}
	}

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapWriteDns(BIH_OBJ *bih,U8 *buf,U32 buf_len)
{
	U32 u0,id,offset,len;
	BIH_MAP_ADDR map;

	offset = 0;

	//id dns_addr4 dns_addr6
	//0 172.21.1.20/20 2002:0da8:00bf:1010:0000:0000:AC15:2D43/64

	//id
	len = buf_len - offset;
	if(Bih_BufGetDecimal(buf+offset,&len,&u0)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	if(u0 >= BIH_MAP_DNS_NUM)
	{
		return BIH_RES_FAILED;
	}
	id = u0;

	//dns_addr4
	len = buf_len - offset;
	if(Bih_BufGetAddr4(buf+offset,&len,&map.addr4,&u0)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	//dns_addr6
	len = buf_len - offset;
	if(Bih_BufGetAddr6(buf+offset,&len,&map.addr6,&u0)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	spin_lock_bh(&bih_map_lock);//lock
	memcpy(&bih->dns[id],&map,sizeof(BIH_MAP_ADDR));
	spin_unlock_bh(&bih_map_lock);//unlock

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapReadRemote(BIH_OBJ *bih, struct seq_file *m)
{
	S32 res;

	//addr4_begin
	res = seq_printf(m,"%d.%d.%d.%d to ",BIH_SEGMENT_ADDR4(&bih->dst_addr0.s_addr));
	if(res < 0)
	{
		return BIH_RES_FAILED;
	}

	//addr4_end
	res = seq_printf(m,"%d.%d.%d.%d\n",BIH_SEGMENT_ADDR4(&bih->dst_addr1.s_addr));
	if(res < 0)
	{
		return BIH_RES_FAILED;
	}

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapWriteRemote(BIH_OBJ *bih,U8 *buf,U32 buf_len)
{
	U32 offset,len,u0;

	offset = 0;

	//addr4_begin addr4_end
	//10.170.160.1/20 10.170.175.254/20

	//addr4_begin
	len = buf_len - offset;
	if(Bih_BufGetAddr4(buf+offset,&len,&bih->dst_addr0,&u0)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	//addr4_end
	len = buf_len - offset;
	if(Bih_BufGetAddr4(buf+offset,&len,&bih->dst_addr1,&u0)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapReadList(BIH_OBJ *bih, struct seq_file *m)
{
	S32 res;
	U32 i,j;
	BIH_MAP_PAIR *pair;
	BIH_MAP_PTR *ptr;
	BIH_MAP_ADDR *map;

	if(!bih->run)
	{
		return BIH_RES_FAILED;
	}

	j = 0;
	for(i=0;i<BIH_MAP_ADDR_LIST_NUM && j<50;i++)
	{
		ptr = bih->map4_list[i].next;

		while(ptr!=(void *)&bih->map4_list[i] && j<50)
		{
			pair = BIH_MAP_GET_PAIR_PTR4(ptr);
			map = &pair->map;

			res = seq_printf(m,"%08x, %u,%u,%u, %d.%d.%d.%d, %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
				map->flag,map->start_time,map->last_time,map->timeout,BIH_SEGMENT_ADDR4(&map->addr4.s_addr),
				ntohs(map->addr6.s6_addr16[0]),
				ntohs(map->addr6.s6_addr16[1]),
				ntohs(map->addr6.s6_addr16[2]),
				ntohs(map->addr6.s6_addr16[3]),
				ntohs(map->addr6.s6_addr16[4]),
				ntohs(map->addr6.s6_addr16[5]),
				ntohs(map->addr6.s6_addr16[6]),
				ntohs(map->addr6.s6_addr16[7]));
			if(res < 0)
			{
				//return BIH_RES_FAILED;
				j = 50;
				break;
			}

			ptr = ptr->next;
			j++;
		}
	}

	return BIH_RES_SUCCESS;
}


RESULT Bih_MapWriteList(BIH_OBJ *bih,U8 *buf,U32 buf_len)
{
	return BIH_RES_SUCCESS;
}



/*bih_match.c**********************************************************/


void Bih_MatchTimerCb(BIH_OBJ *bih)
{
	U32 i,curr_time;
	BIH_LINK_HEAD *head;
	BIH_MATCH_ITEM *item,*item2;

	curr_time = jiffies;

	BIH_LOG(3,"Bih_MatchTimerCb: time cb");

	for(i=0;i<BIH_MATCH_ITEM_LIST_NUM;i++)
	{
		head = &bih->match_list[i];

		item = head->next;
		while(item != (void *)head)
		{
			item2 = item;
			item = item->next;

			if(item2->flag & BIH_MATCH_ITEM_FLAG_TEMP)
			{
				if(time_after((unsigned long)curr_time,(unsigned long)item2->last_time+BIH_MATCH_TIMEOUT_TEMP))
				{
					BIH_LOG(2,"Bih_MatchTimerCb: temp timeout");
					Bih_MatchFreeItem(bih,item2);
					continue;
				}
			}
			if(time_after((unsigned long)curr_time,(unsigned long)item2->last_time+BIH_MATCH_TIMEOUT_INACT))
			{
				BIH_LOG(2,"Bih_MatchTimerCb: match timeout");
				Bih_MatchFreeItem(bih,item2);
			}
		}
	}

	return;
}


BIH_MATCH_ITEM *Bih_MatchAddItem(BIH_OBJ *bih,struct in_addr *addr4,U32 type,U32 id,U32 flag)
{
	BIH_MATCH_ITEM *item;

	item = Bih_MatchSearchItem(bih,addr4,type,id);
	if(item == NULL)
	{
		item = Bih_MatchAllocItem(bih);
		if(item == NULL)
		{
			return NULL;
		}

		item->start_time = jiffies;
		item->last_time = item->start_time;
		item->flag = flag;
		item->type = type;
		item->id = id;
		item->addr4.s_addr = addr4->s_addr;

		spin_lock_bh(&bih_match_lock);//lock
		Bih_LinkPutFront(Bih_MatchGetHead(bih,addr4,type,id),item);
		spin_unlock_bh(&bih_match_lock);//unlock
	}

	return item;
}


BIH_MATCH_ITEM *Bih_MatchSearchItem(BIH_OBJ *bih,struct in_addr *addr4,U32 type,U32 id)
{
	BIH_LINK_HEAD *head;
	BIH_MATCH_ITEM *item;

	head = Bih_MatchGetHead(bih,addr4,type,id);

	item = head->next;
	while(item != (void *)head)
	{
		if(item->addr4.s_addr==addr4->s_addr && item->type==type && item->id==id)
		{
			break;
		}

		item = item->next;
	}
	if(item == (void *)head)
	{
		return NULL;
	}

	item->last_time = jiffies;

	return item;
}


BIH_MATCH_ITEM *Bih_MatchAllocItem(BIH_OBJ *bih)
{
	BIH_MATCH_ITEM *item;

	spin_lock_bh(&bih_match_lock);//lock

	item = Bih_LinkGetBehind(&bih->match_free);
	if(item == NULL)
	{
		spin_unlock_bh(&bih_match_lock);//unlock
		return NULL;
	}

	spin_unlock_bh(&bih_match_lock);//unlock

	return item;
}


void Bih_MatchFreeItem(BIH_OBJ *bih,BIH_MATCH_ITEM *item)
{
	spin_lock_bh(&bih_match_lock);//lock

	Bih_LinkRemove(item);
	Bih_LinkPutFront(&bih->match_free,item);

	spin_unlock_bh(&bih_match_lock);//unlock

	return;
}


BIH_LINK_HEAD *Bih_MatchGetHead(BIH_OBJ *bih,struct in_addr *addr4,U32 type,U32 id)
{
	U32 u0;

	u0 = addr4->s_addr + type + id;
	u0 %= BIH_MATCH_ITEM_LIST_NUM;

	return &bih->match_list[u0];
}


RESULT Bih_MatchReadList(BIH_OBJ *bih, struct seq_file *m)
{
	S32 res;
	U32 i,j;
	BIH_MATCH_ITEM *item;

	if(!bih->run)
	{
		return BIH_RES_FAILED;
	}

	j = 0;
	for(i=0;i<BIH_MATCH_ITEM_LIST_NUM && j<50;i++)
	{
		item = bih->match_list[i].next;

		while(item!=(void *)&bih->match_list[i] && j<50)
		{
			res = seq_printf(m,"%u,%u, %08x, %d, %d, %d.%d.%d.%d\n",
				item->start_time,item->last_time,item->flag,item->type,item->id,
				BIH_SEGMENT_ADDR4(&item->addr4.s_addr));
			if(res < 0)
			{
				//return BIH_RES_FAILED;
				j = 50;
				break;
			}

			item = item->next;
			j++;
		}
	}

	return BIH_RES_SUCCESS;
}


RESULT Bih_MatchWriteList(BIH_OBJ *bih,U8 *buf,U32 buf_len)
{
	return BIH_RES_SUCCESS;
}


/*bih_opera.c**********************************************************/


RESULT Bih_BufGetAddr4(U8 *buf,U32 *buf_len,struct in_addr *addr4,U32 *mask4_len)
{
	U32 i,u0,offset,len,tol_len,addr,mask;

	offset = 0;
	tol_len = *buf_len;

	//addr
	addr = 0;
	for(i=0;i<4;i++)
	{
		len = tol_len - offset;
		if(Bih_BufGetDecimal(buf+offset,&len,&u0)!= BIH_RES_SUCCESS)
		{
			return BIH_RES_FAILED;
		}
		offset += len;

		addr |= u0 << (i*8);

		u0 = (i<3)? '.' : '/';
		if(buf[offset] != u0)
		{
			return BIH_RES_FAILED;
		}
		offset++;
	}

	//mask
	len = tol_len - offset;
	if(Bih_BufGetDecimal(buf+offset,&len,&mask)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	*buf_len = offset;
	addr4->s_addr = addr;
	*mask4_len = mask;

	return BIH_RES_SUCCESS;
}


RESULT Bih_BufGetAddr6(U8 *buf,U32 *buf_len,struct in6_addr *addr6,U32 *mask6_len)
{
	U32 i,u0,offset,len,tol_len,mask;
	U16 addr[8];

	offset = 0;
	tol_len = *buf_len;

	//addr
	for(i=0;i<8;i++)
	{
		len = tol_len - offset;
		if(Bih_BufGetHex(buf+offset,&len,&u0)!= BIH_RES_SUCCESS)
		{
			return BIH_RES_FAILED;
		}
		offset += len;

		addr[i] = htons(u0);

		u0 = (i<7)? ':' : '/';
		if(buf[offset] != u0)
		{
			return BIH_RES_FAILED;
		}
		offset++;
	}

	//mask
	len = tol_len - offset;
	if(Bih_BufGetDecimal(buf+offset,&len,&mask)!= BIH_RES_SUCCESS)
	{
		return BIH_RES_FAILED;
	}
	offset += len;

	*buf_len = offset;
	memcpy(addr6->s6_addr,addr,16);
	*mask6_len = mask;

	return BIH_RES_SUCCESS;
}

RESULT Bih_BufGetDecimal(U8 *buf,U32 *buf_len,U32 *value)
{
	U32 u0,offset,len,data;

	offset = 0;
	len = *buf_len;
	data = 0;

	//search data
	while(offset < len)
	{
		u0 = buf[offset];
		if(BIH_IS_DECIMAL(u0))
		{
			break;
		}

		offset++;
	}
	if(offset >= len)
	{
		return BIH_RES_FAILED;
	}

	//read data
	while(offset < len)
	{
		u0 = buf[offset];
		if(!BIH_IS_DECIMAL(u0))
		{
			break;
		}

		data = data*10 + BIH_GET_DECIMAL(u0);
		offset++;
	}

	*buf_len = offset;
	*value = data;

	return BIH_RES_SUCCESS;
}


RESULT Bih_BufGetHex(U8 *buf,U32 *buf_len,U32 *value)
{
	U32 u0,offset,len,data;

	offset = 0;
	len = *buf_len;
	data = 0;

	//search data
	while(offset < len)
	{
		u0 = buf[offset];
		if(BIH_IS_DECIMAL(u0)|| BIH_IS_HEX(u0))
		{
			break;
		}

		offset++;
	}
	if(offset >= len)
	{
		return BIH_RES_FAILED;
	}

	//read data
	while(offset < len)
	{
		u0 = buf[offset];
		if(BIH_IS_DECIMAL(u0))
		{
			data = data*16 + BIH_GET_DECIMAL(u0);
		}
		else if(BIH_IS_HEX(u0))
		{
			data = data*16 + BIH_GET_HEX(u0);
		}
		else
		{
			break;
		}

		offset++;
	}

	*buf_len = offset;
	*value = data;

	return BIH_RES_SUCCESS;
}


RESULT Bih_BufSearchStr(U8 *buf,U32 buf_len,U8 *str,U32 str_len,U32 *offset)
{
	U32 i,j;
	U8 *buf2;

	//search str in buf and return offset

	if(buf_len < str_len)
	{
		return BIH_RES_FAILED;
	}

	for(i=0;i<buf_len-str_len;i++)
	{
		buf2 = buf + i;

		for(j=0;j<str_len;j++)
		{
			if(str[j] != buf2[j])
			{
				break;
			}
		}
		if(j >= str_len)//ok
		{
			break;
		}
	}

	if(i >= buf_len-str_len)
	{
		return BIH_RES_FAILED;
	}

	*offset = i;

	return BIH_RES_SUCCESS;
}


void Bih_LinkInit(void *head)
{
	BIH_LINK_LAST(head)= head;
	BIH_LINK_NEXT(head)= head;

	return;
}


void Bih_LinkPutFront(void *unit,void *new_unit)
{
	BIH_LINK_LAST(new_unit)= BIH_LINK_LAST(unit);
	BIH_LINK_NEXT(new_unit)= unit;
	BIH_LINK_NEXT(BIH_LINK_LAST(unit))= new_unit;
	BIH_LINK_LAST(unit)= new_unit;

	return;
}


void Bih_LinkPutBehind(void *unit,void *new_unit)
{
	BIH_LINK_NEXT(new_unit)= BIH_LINK_NEXT(unit);
	BIH_LINK_LAST(new_unit)= unit;
	BIH_LINK_LAST(BIH_LINK_NEXT(unit))= new_unit;
	BIH_LINK_NEXT(unit)= new_unit;

	return;
}


void *Bih_LinkGetFront(void *unit)
{
	void *pfront;

	if(BIH_LINK_LAST(unit)== unit)
	{
		return NULL;
	}
	pfront = (void *)BIH_LINK_LAST(unit);

	BIH_LINK_NEXT(BIH_LINK_LAST(pfront))= unit;
	BIH_LINK_LAST(unit)= BIH_LINK_LAST(pfront);
	BIH_LINK_NEXT(pfront)= pfront;
	BIH_LINK_LAST(pfront)= pfront;

	return pfront;
}


void *Bih_LinkGetBehind(void *unit)
{
	void *pbehind;

	if(BIH_LINK_NEXT(unit)== unit)
	{
		return NULL;
	}
	pbehind = BIH_LINK_NEXT(unit);

	BIH_LINK_LAST(BIH_LINK_NEXT(pbehind))= unit;
	BIH_LINK_NEXT(unit)= BIH_LINK_NEXT(pbehind);
	BIH_LINK_NEXT(pbehind)= pbehind;
	BIH_LINK_LAST(pbehind)= pbehind;

	return pbehind;
}


void Bih_LinkRemove(void *unit)
{
	if(BIH_LINK_NEXT(unit)== unit)
	{
		return;
	}
	BIH_LINK_LAST(BIH_LINK_NEXT(unit))= BIH_LINK_LAST(unit);
	BIH_LINK_NEXT(BIH_LINK_LAST(unit))= BIH_LINK_NEXT(unit);
	BIH_LINK_NEXT(unit)= unit;
	BIH_LINK_LAST(unit)= unit;

	return;
}



