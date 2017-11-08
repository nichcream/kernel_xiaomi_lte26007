/******************************************************************************
 *
 * Copyright(c) 2007 - 2012 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _RTW_DEBUG_C_
#include <rtw_version.h>
#include <osdep_service.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#define GET_INODE_DATA(__node)		PDE_DATA(__node)
#else
#define GET_INODE_DATA(__node)		PDE(__node)->data
#endif
#include <rtw_debug.h>
#ifdef CONFIG_RTL8723A
#include <rtl8723a_hal.h>
#endif

#ifdef CONFIG_DEBUG_RTL871X

	u32 GlobalDebugLevel = _drv_err_;

	u64 GlobalDebugComponents = \
			_module_rtl871x_xmit_c_ |
			_module_xmit_osdep_c_ |
			_module_rtl871x_recv_c_ |
			_module_recv_osdep_c_ |
			_module_rtl871x_mlme_c_ |
			_module_mlme_osdep_c_ |
			_module_rtl871x_sta_mgt_c_ |
			_module_rtl871x_cmd_c_ |
			_module_cmd_osdep_c_ |
			_module_rtl871x_io_c_ |
			_module_io_osdep_c_ |
			_module_os_intfs_c_|
			_module_rtl871x_security_c_|
			_module_rtl871x_eeprom_c_|
			_module_hal_init_c_|
			_module_hci_hal_init_c_|
			_module_rtl871x_ioctl_c_|
			_module_rtl871x_ioctl_set_c_|
			_module_rtl871x_ioctl_query_c_|
			_module_rtl871x_pwrctrl_c_|
			_module_hci_intfs_c_|
			_module_hci_ops_c_|
			_module_hci_ops_os_c_|
			_module_rtl871x_ioctl_os_c|
			_module_rtl8712_cmd_c_|
			_module_hal_xmit_c_|
			_module_rtl8712_recv_c_ |
			_module_mp_ |
			_module_efuse_;

#endif

#ifdef CONFIG_PROC_DEBUG
static u32 proc_get_read_addr=0xeeeeeeee;
static u32 proc_get_read_len=0x4;

//driver version
static int rtl_proc_get_drv_version(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	len += seq_printf(m,"\n%s",DRIVERVERSION);
	seq_puts(m,"\n");
	return 0;

}

static int dl_proc_get_drv_version(struct inode *inode, struct file *file)
{
	return single_open(file,rtl_proc_get_drv_version,GET_INODE_DATA(inode));
	return 0;

}

static const struct file_operations file_ops_proc_get_drv_version = {
	.owner = THIS_MODULE,
	.open = dl_proc_get_drv_version,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//write_reg
ssize_t rtl_proc_set_write_reg(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 addr, val, len;

	if (count < 3)
	{
		DBG_871X("argument size is less than 3\n");
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%x %x %x", &addr, &val, &len);

		if (num !=	3) {
			DBG_871X("invalid write_reg parameter!\n");
			return count;
		}

		switch(len)
		{
			case 1:
				rtw_write8(padapter, addr, (u8)val);
				break;
			case 2:
				rtw_write16(padapter, addr, (u16)val);
				break;
			case 4:
				rtw_write32(padapter, addr, val);
				break;
			default:
				DBG_871X("error write length=%d", len);
				break;
		}

	}

	return count;

}

static int rtl_proc_get_write_reg(struct seq_file *m,void *v)
{
	return 0;
}



static int dl_proc_get_write_reg(struct inode *inode, struct file *file)
{
	return single_open(file,rtl_proc_get_write_reg,GET_INODE_DATA(inode));
}


static ssize_t dl_proc_set_write_reg(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &rtl_proc_set_write_reg;

	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);
}

static const struct file_operations file_ops_proc_set_write_reg = {
	.open = dl_proc_get_write_reg,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_write_reg,
};


//read_reg
static int rtl_proc_get_read_reg(struct seq_file *m,void *v)
{
	//struct ieee80211_hw *hw = m->private;
	//struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;

	if(proc_get_read_addr == 0xeeeeeeee)
	{
		//*eof =1;
		return 0;
	}

	switch(proc_get_read_len)
	{
		case 1:
			len += seq_printf(m, "\nrtw_read8(0x%x)=0x%x", proc_get_read_addr, rtw_read8(padapter,proc_get_read_addr));
			break;
		case 2:
			len += seq_printf(m, "\nrtw_read16(0x%x)=0x%x", proc_get_read_addr, rtw_read16(padapter,proc_get_read_addr));
			break;
		case 4:
			len += seq_printf(m, "\nrtw_read32(0x%x)=0x%x", proc_get_read_addr, rtw_read32(padapter,proc_get_read_addr));
			break;
		default:
			len += seq_printf(m, "\nerror read length=%d", proc_get_read_len);
			break;
	}
	seq_puts(m,"\n");
	return 0;

}


ssize_t rtl_proc_set_read_reg(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	char tmp[16];
	u32 addr, len;

	if (count < 2)
	{
		DBG_871X("argument size is less than 2\n");
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%x %x", &addr, &len);

		if (num !=	2) {
			DBG_871X("invalid read_reg parameter!\n");
			return count;
		}

		proc_get_read_addr = addr;

		proc_get_read_len = len;
	}

	return count;

}

static int dl_proc_get_read_reg(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_read_reg,GET_INODE_DATA(inode));
}

static ssize_t dl_proc_set_read_reg(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &rtl_proc_set_read_reg;

	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);
}


static const struct file_operations file_ops_proc_get_read_reg = {
	.open = dl_proc_get_read_reg,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_read_reg,
};


//fwstate
static int rtl_proc_get_fwstate(struct seq_file *m,void *v) //wating modified
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	int len = 0;
	len += seq_printf(m, "\nfwstate=0x%x", get_fwstate(pmlmepriv));
	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_fwstate(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_fwstate,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_fwstate = {
	.open = dl_proc_get_fwstate,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//sec_info
static int rtl_proc_get_sec_info(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct security_priv *psecuritypriv = &padapter->securitypriv;
	int len = 0;
	len += seq_printf(m,"\nauth_alg=0x%x, enc_alg=0x%x, auth_type=0x%x, enc_type=0x%x",
						psecuritypriv->dot11AuthAlgrthm, psecuritypriv->dot11PrivacyAlgrthm,
						psecuritypriv->ndisauthtype, psecuritypriv->ndisencryptstatus);
	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_sec_info(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_sec_info,GET_INODE_DATA(inode));
}


static const struct file_operations file_ops_proc_get_sec_info = {
	.open = dl_proc_get_sec_info,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


//mlmext
static int rtl_proc_get_mlmext_state(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	int len = 0;
	len += seq_printf(m,"\npmlmeinfo->state=0x%x",pmlmeinfo->state);
	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_mlmext_state(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_mlmext_state,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_mlmext_state = {
	.open = dl_proc_get_mlmext_state,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//qos_option
static int rtl_proc_get_qos_option(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);

	int len = 0;
	len += seq_printf(m,"\nqos_option=%d",pmlmepriv->qospriv.qos_option);
	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_qos_option(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_qos_option,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_qos_option = {
	.open = dl_proc_get_qos_option,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


//ht_option
static int rtl_proc_get_ht_option(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);

	int len = 0;
	len += seq_printf(m,"\nht_option=%d",pmlmepriv->htpriv.ht_option);
	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_ht_option(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_ht_option,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_ht_option = {
	.open = dl_proc_get_ht_option,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


//rf_info
static int rtl_proc_get_rf_info(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;

	int len = 0;
	len += seq_printf(m,"\ncur_ch=%d, cur_bw=%d, cur_ch_offet=%d",pmlmeext->cur_channel, pmlmeext->cur_bwmode, pmlmeext->cur_ch_offset);
	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_rf_info(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_rf_info,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_rf_info = {
	.open = dl_proc_get_rf_info,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//ap_info
static int rtl_proc_get_ap_info(struct seq_file *m,void *v)
{
	struct sta_info *psta;
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;
	struct wlan_network *cur_network = &(pmlmepriv->cur_network);
	struct sta_priv *pstapriv = &padapter->stapriv;

	int len = 0;
	psta = rtw_get_stainfo(pstapriv, cur_network->network.MacAddress);
	if(psta)
	{
		int i;
		struct recv_reorder_ctrl *preorder_ctrl;

		len += seq_printf(m, "\nSSID=%s", cur_network->network.Ssid.Ssid);
		len += seq_printf(m, "\nsta's macaddr:" MAC_FMT , MAC_ARG(psta->hwaddr));
		len += seq_printf(m, "\ncur_channel=%d, cur_bwmode=%d, cur_ch_offset=%d", pmlmeext->cur_channel, pmlmeext->cur_bwmode, pmlmeext->cur_ch_offset);
		len += seq_printf(m, "\nrtsen=%d, cts2slef=%d", psta->rtsen, psta->cts2self);
		len += seq_printf(m, "\nqos_en=%d, ht_en=%d, init_rate=%d", psta->qos_option, psta->htpriv.ht_option, psta->init_rate);
		len += seq_printf(m, "\nstate=0x%x, aid=%d, macid=%d, raid=%d", psta->state, psta->aid, psta->mac_id, psta->raid);
		len += seq_printf(m, "\nbwmode=%d, ch_offset=%d, sgi=%d", psta->htpriv.bwmode, psta->htpriv.ch_offset, psta->htpriv.sgi);
		len += seq_printf(m, "\nampdu_enable = %d", psta->htpriv.ampdu_enable);
		len += seq_printf(m, "\nagg_enable_bitmap=%x, candidate_tid_bitmap=%x", psta->htpriv.agg_enable_bitmap, psta->htpriv.candidate_tid_bitmap);

		for(i=0;i<16;i++)
		{
			preorder_ctrl = &psta->recvreorder_ctrl[i];
			if(preorder_ctrl->enable)
			{
				len += seq_printf(m, "\ntid=%d, indicate_seq=%d", i, preorder_ctrl->indicate_seq);
			}
		}
	}
	else
	{
		len += seq_printf(m, "\ncan't get sta's macaddr, cur_network's macaddr:" MAC_FMT , MAC_ARG(cur_network->network.MacAddress));
	}
	seq_puts(m,"\n");
	return 0;

}

static int dl_proc_get_ap_info(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_ap_info,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_ap_info = {
	.open = dl_proc_get_ap_info,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//adapter_state
static int rtl_proc_get_adapter_state(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	int len = 0;
	len += seq_printf(m,"\nbSurpriseRemoved=%d, bDriverStopped=%d",padapter->bSurpriseRemoved, padapter->bDriverStopped);
	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_adapter_state(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_adapter_state,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_adapter_state = {
	.open = dl_proc_get_adapter_state,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//trx_info
static int rtl_proc_get_trx_info(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;
	struct recv_priv  *precvpriv = &padapter->recvpriv;

	int len = 0;
	len += seq_printf(m,"\nfree_xmitbuf_cnt=%d, free_xmitframe_cnt=%d, free_ext_xmitbuf_cnt=%d, free_recvframe_cnt=%d",pxmitpriv->free_xmitbuf_cnt, pxmitpriv->free_xmitframe_cnt,pxmitpriv->free_xmit_extbuf_cnt, precvpriv->free_recvframe_cnt);
#ifdef CONFIG_USB_HCI
	len += seq_printf(m,"\nrx_urb_pending_cn=%d", precvpriv->rx_pending_cnt);
#endif
	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_trx_info(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_trx_info,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_trx_info = {
	.open = dl_proc_get_trx_info,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//mac_reg_dump1
static int rtl_proc_get_mac_reg_dump1(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1;
	len += seq_printf(m,"\n======= MAC REG =======\n");
	for(i = 0x0;i<0x300;i+=4)
	{
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",rtw_read32(padapter,i));
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_mac_reg_dump1(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_mac_reg_dump1,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_mac_reg_dump1 = {
	.open = dl_proc_get_mac_reg_dump1,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//mac_reg_dump2
static int rtl_proc_get_mac_reg_dump2(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1;
	len += seq_printf(m,"\n======= MAC REG =======\n");
	for(i = 0x0;i<0x600;i+=4)
	{
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",rtw_read32(padapter,i));
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_mac_reg_dump2(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_mac_reg_dump2,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_mac_reg_dump2 = {
	.open = dl_proc_get_mac_reg_dump2,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//mac_reg_dump3
static int rtl_proc_get_mac_reg_dump3(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1;
	len += seq_printf(m,"\n======= MAC REG =======\n");
	for(i = 0x600;i<0x800;i+=4)
	{
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",rtw_read32(padapter,i));
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_mac_reg_dump3(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_mac_reg_dump3,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_mac_reg_dump3 = {
	.open = dl_proc_get_mac_reg_dump3,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//bb_reg_dump1
static int rtl_proc_get_bb_reg_dump1(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1;
	len += seq_printf(m,"\n======= BB REG =======\n");
	for(i = 0x800;i<0xB00;i+=4)
	{
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",rtw_read32(padapter,i));
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_bb_reg_dump1(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_bb_reg_dump1,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_bb_reg_dump1 = {
	.open = dl_proc_get_bb_reg_dump1,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//bb_reg_dump2
static int rtl_proc_get_bb_reg_dump2(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1;
	len += seq_printf(m,"\n======= BB REG =======\n");
	for(i = 0xB00;i<0xE00;i+=4)
	{
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",rtw_read32(padapter,i));
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_bb_reg_dump2(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_bb_reg_dump2,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_bb_reg_dump2 = {
	.open = dl_proc_get_bb_reg_dump2,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//bb_reg_dump3
static int rtl_proc_get_bb_reg_dump3(struct seq_file *m,void *v)	{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1;
	len += seq_printf(m,"\n======= BB REG =======\n");
	for(i = 0xE00;i<0x1000;i+=4)
	{
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",rtw_read32(padapter,i));
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_bb_reg_dump3(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_bb_reg_dump3,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_bb_reg_dump3 = {
	.open = dl_proc_get_bb_reg_dump3,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//rf_reg_dump1
static int rtl_proc_get_rf_reg_dump1(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1,path;
	u32 value;
	len += seq_printf(m,"\n======= RF REG =======\n");
	path = 1;
	len += seq_printf(m,"\nRF_Path(%x)\n",path);
	for(i = 0;i < 0xC0; i++)
	{
		value = rtw_hal_read_rfreg(padapter, path, i, 0xffffffff);
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",value);
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}
static int dl_proc_get_rf_reg_dump1(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_rf_reg_dump1,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_rf_reg_dump1 = {
	.open = dl_proc_get_rf_reg_dump1,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//rf_reg_dump2
static int rtl_proc_get_rf_reg_dump2(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1,path;
	u32 value;
	len += seq_printf(m,"\n======= RF REG =======\n");
	path = 1;
	len += seq_printf(m,"\nRF_Path(%x)\n",path);
	for(i = 0xC0;i < 0x100; i++)
	{
		value = rtw_hal_read_rfreg(padapter, path, i, 0xffffffff);
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",value);
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_rf_reg_dump2(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_rf_reg_dump2,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_rf_reg_dump2 = {
	.open = dl_proc_get_rf_reg_dump2,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//rf_reg_dump3
static int rtl_proc_get_rf_reg_dump3(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1,path;
	u32 value;
	len += seq_printf(m,"\n======= RF REG =======\n");
	path = 1;
	len += seq_printf(m,"\nRF_Path(%x)\n",path);
	for(i = 0;i < 0xC0; i++)
	{
		value = rtw_hal_read_rfreg(padapter, path, i, 0xffffffff);
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",value);
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}
static int dl_proc_get_rf_reg_dump3(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_rf_reg_dump3,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_rf_reg_dump3 = {
	.open = dl_proc_get_rf_reg_dump3,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//rf_reg_dump4
static int rtl_proc_get_rf_reg_dump4(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	int len = 0;
	int i,j = 1,path;
	u32 value;
	len += seq_printf(m,"\n======= RF REG =======\n");
	path = 1;
	len += seq_printf(m,"\nRF_Path(%x)\n",path);
	for(i = 0xC0;i < 0x100; i++)
	{
		value = rtw_hal_read_rfreg(padapter, path, i, 0xffffffff);
		if(j%4 == 1) len += seq_printf(m,"0x%02x",i);
		len+=seq_printf(m," 0x%08x ",value);
		if((j++)%4 == 0) len += seq_printf(m,"\n");
	}

	seq_puts(m,"\n");
	return 0;
}
static int dl_proc_get_rf_reg_dump4(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_rf_reg_dump4,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_rf_reg_dump4 = {
	.open = dl_proc_get_rf_reg_dump4,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//rx_signal
static int rtl_proc_get_rx_signal(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	int len = 0;

	len += seq_printf(m,
		"\nrssi:%d"
		"\nrxpwdb:%d"
		"\nsignal_strength:%u"
		"\nsignal_qual:%u"
		"\nnoise:%u",
		padapter->recvpriv.rssi,
		padapter->recvpriv.rxpwdb,
		padapter->recvpriv.signal_strength,
		padapter->recvpriv.signal_qual,
		padapter->recvpriv.noise);

	seq_puts(m,"\n");
	return 0;
}

ssize_t proc_set_rx_signal(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 is_signal_dbg, signal_strength;

	if (count < 1)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%u %u", &is_signal_dbg, &signal_strength);

		is_signal_dbg = is_signal_dbg==0?0:1;

		if(is_signal_dbg && num!=2)
			return count;

		signal_strength = signal_strength>100?100:signal_strength;
		signal_strength = signal_strength<0?0:signal_strength;

		padapter->recvpriv.is_signal_dbg = is_signal_dbg;
		padapter->recvpriv.signal_strength_dbg=signal_strength;

		if(is_signal_dbg)
			DBG_871X("set %s %u\n", "DBG_SIGNAL_STRENGTH", signal_strength);
		else
			DBG_871X("set %s\n", "HW_SIGNAL_STRENGTH");

	}

	return count;

}

static int dl_proc_get_rx_signal(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_rx_signal,GET_INODE_DATA(inode));
}


static ssize_t dl_proc_set_rx_signal(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &proc_set_rx_signal;

	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);

}

static const struct file_operations file_ops_proc_get_rx_signal = {
	.open = dl_proc_get_rx_signal,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_rx_signal,
};

//rssi_disp
ssize_t rtl_proc_set_rssi_disp(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 enable=0;

	return count;

}

static int rtl_proc_get_rssi_disp(struct seq_file *m,void *v)
{
	seq_puts(m,"\n");
	return 0;
}
static int dl_proc_get_rssi_disp(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_rssi_disp,GET_INODE_DATA(inode));
}

static ssize_t dl_proc_set_rssi_disp(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &rtl_proc_set_rssi_disp;

	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);

}

static const struct file_operations file_ops_proc_get_rssi_disp = {
	.open = dl_proc_get_rssi_disp,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_rssi_disp,
};

#ifdef CONFIG_AP_MODE
//all_sta_info
static int rtl_proc_get_all_sta_info(struct seq_file *m,void *v)		//waiting modified
{
	_irqL irqL;
	struct sta_info *psta;
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct sta_priv *pstapriv = &padapter->stapriv;
	int i, j;
	_list	*plist, *phead;
	struct recv_reorder_ctrl *preorder_ctrl;
	int len = 0;


	len += seq_printf(m, "\nsta_dz_bitmap=0x%x, tim_bitmap=0x%x", pstapriv->sta_dz_bitmap, pstapriv->tim_bitmap);

	_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);

	for(i=0; i< NUM_STA; i++)
	{
		phead = &(pstapriv->sta_hash[i]);
		plist = get_next(phead);

		while ((rtw_end_of_queue_search(phead, plist)) == _FALSE)
		{
			psta = LIST_CONTAINOR(plist, struct sta_info, hash_list);

			plist = get_next(plist);

			//if(extra_arg == psta->aid)
			{
				len += seq_printf(m, "\nsta's macaddr:" MAC_FMT "", MAC_ARG(psta->hwaddr));
				len += seq_printf(m, "\nrtsen=%d, cts2slef=%d", psta->rtsen, psta->cts2self);
				len += seq_printf(m, "\nqos_en=%d, ht_en=%d, init_rate=%d", psta->qos_option, psta->htpriv.ht_option, psta->init_rate);
				len += seq_printf(m, "\nstate=0x%x, aid=%d, macid=%d, raid=%d", psta->state, psta->aid, psta->mac_id, psta->raid);
				len += seq_printf(m, "\nbwmode=%d, ch_offset=%d, sgi=%d", psta->htpriv.bwmode, psta->htpriv.ch_offset, psta->htpriv.sgi);
				len += seq_printf(m, "\nampdu_enable = %d", psta->htpriv.ampdu_enable);
				len += seq_printf(m, "\nagg_enable_bitmap=%x, candidate_tid_bitmap=%x", psta->htpriv.agg_enable_bitmap, psta->htpriv.candidate_tid_bitmap);
				len += seq_printf(m, "\nsleepq_len=%d", psta->sleepq_len);
				len += seq_printf(m, "\ncapability=0x%x", psta->capability);
				len += seq_printf(m, "\nflags=0x%x", psta->flags);
				len += seq_printf(m, "\nwpa_psk=0x%x", psta->wpa_psk);
				len += seq_printf(m, "\nwpa2_group_cipher=0x%x", psta->wpa2_group_cipher);
				len += seq_printf(m, "\nwpa2_pairwise_cipher=0x%x", psta->wpa2_pairwise_cipher);
				len += seq_printf(m, "\nqos_info=0x%x", psta->qos_info);
				len += seq_printf(m, "\ndot118021XPrivacy=0x%x", psta->dot118021XPrivacy);

				for(j=0;j<16;j++)
				{
					preorder_ctrl = &psta->recvreorder_ctrl[j];
					if(preorder_ctrl->enable)
					{
						len += seq_printf(m, "\ntid=%d, indicate_seq=%d", j, preorder_ctrl->indicate_seq);
					}
				}

			}

		}

	}

	_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);

	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_all_sta_info(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_all_sta_info,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_all_sta_info = {
	.open = dl_proc_get_all_sta_info,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#ifdef CONFIG_80211N_HT
//ht_enable
static int rtl_proc_get_ht_enable(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	int len = 0;

	if(pregpriv)
		len += seq_printf(m,"\n%d",pregpriv->ht_enable);
	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_ht_enable(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_ht_enable,GET_INODE_DATA(inode));
}


ssize_t rtl_proc_set_ht_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%d ", &mode);

		if( pregpriv && mode >= 0 && mode < 2 )
		{
			pregpriv->ht_enable= mode;
			printk("ht_enable=%d\n", pregpriv->ht_enable);
		}
	}

	return count;

}


static ssize_t dl_proc_set_ht_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &rtl_proc_set_ht_enable;
	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);
}

static const struct file_operations file_ops_proc_get_ht_enable = {
	.open = dl_proc_get_ht_enable,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_ht_enable,
};

//cbw40_enable
static int rtl_proc_get_cbw40_enable(struct seq_file *m,void *v)		//waiting modified
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	int len = 0;
	if(pregpriv)
		len += seq_printf(m,"\n%d", pregpriv->cbw40_enable);
	seq_puts(m,"\n");
	return 0;
}


ssize_t rtl_proc_set_cbw40_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%d ", &mode);

		if( pregpriv && mode >= 0 && mode < 2 )
		{

			pregpriv->cbw40_enable = mode;
			printk("cbw40_enable=%d\n", mode);

		}
	}

	return count;

}

static int dl_proc_get_cbw40_enable(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_cbw40_enable,GET_INODE_DATA(inode));
}

static ssize_t dl_proc_set_cbw40_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &rtl_proc_set_cbw40_enable;
	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);
}

static const struct file_operations file_ops_proc_get_cbw40_enable = {
	.open = dl_proc_get_cbw40_enable,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_cbw40_enable,
};


//ampdu_enable
static int rtl_proc_get_ampdu_enable(struct seq_file *m,void *v)		//waiting modified
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	int len = 0;
	if(pregpriv)
		len += seq_printf(m,"\n%d",pregpriv->ampdu_enable);
	seq_puts(m,"\n");
	return 0;
}

ssize_t rtl_proc_set_ampdu_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%d ", &mode);

		if( pregpriv && mode >= 0 && mode < 3 )
		{
			pregpriv->ampdu_enable= mode;
			printk("ampdu_enable=%d\n", mode);
		}

	}

	return count;

}

static int dl_proc_get_ampdu_enable(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_ampdu_enable,GET_INODE_DATA(inode));
}

static ssize_t dl_proc_set_ampdu_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &rtl_proc_set_ampdu_enable;
	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);
}

static const struct file_operations file_ops_proc_get_ampdu_enable = {
	.open = dl_proc_get_ampdu_enable,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_ampdu_enable,
};


//rx_stbc
static int rtl_proc_get_rx_stbc(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	int len = 0;
	if(pregpriv)
		len += seq_printf(m,
			"\n%d",
			pregpriv->rx_stbc
			);
	seq_puts(m,"\n");
	return 0;
}

ssize_t rtl_proc_set_rx_stbc(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%d ", &mode);

		if( pregpriv && (mode == 0 || mode == 1|| mode == 2|| mode == 3))
		{
			pregpriv->rx_stbc= mode;
			printk("rx_stbc=%d\n", mode);
		}
	}

	return count;

}

static int dl_proc_get_rx_stbc(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_rx_stbc,GET_INODE_DATA(inode));
}

static ssize_t dl_proc_set_rx_stbc(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &rtl_proc_set_rx_stbc;
	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);
}


static const struct file_operations file_ops_proc_get_rx_stbc = {
	.open = dl_proc_get_rx_stbc,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_rx_stbc,
};


#endif //CONFIG_80211N_HT

#ifdef DBG_MEMORY_LEAK
#include <asm/atomic.h>
extern atomic_t _malloc_cnt;;
extern atomic_t _malloc_size;;

//mem_cnt
static int rtl_proc_get_malloc_cnt(struct seq_file *m,void *v)
{
	struct net_device *dev = v;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	int len = 0;

	seq_puts(m,"\n");
	return 0;
}

static int dl_proc_get_malloc_cnt(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_malloc_cnt,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_malloc_cnt = {
	.open = dl_proc_get_malloc_cnt,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#endif /* DBG_MEMORY_LEAK */

#ifdef CONFIG_FIND_BEST_CHANNEL
//best channel
static int rtl_proc_get_best_channel(struct seq_file *m,void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;
	int len = 0;
	u32 i, best_channel_24G = 1, best_channel_5G = 36, index_24G = 0, index_5G = 0;
	for (i=0; pmlmeext->channel_set[i].ChannelNum !=0; i++) {
		if ( pmlmeext->channel_set[i].ChannelNum == 1)
			index_24G = i;
		if ( pmlmeext->channel_set[i].ChannelNum == 36)
			index_5G = i;
	}

	for (i=0; pmlmeext->channel_set[i].ChannelNum !=0; i++) {
			// 2.4G
			if ( pmlmeext->channel_set[i].ChannelNum == 6 ) {
				if ( pmlmeext->channel_set[i].rx_count < pmlmeext->channel_set[index_24G].rx_count ) {
					index_24G = i;
					best_channel_24G = pmlmeext->channel_set[i].ChannelNum;
				}
			}

			// 5G
			if ( pmlmeext->channel_set[i].ChannelNum >= 36
				&& pmlmeext->channel_set[i].ChannelNum < 140 ) {
				 // Find primary channel
				if ( (( pmlmeext->channel_set[i].ChannelNum - 36) % 8 == 0)
					&& (pmlmeext->channel_set[i].rx_count < pmlmeext->channel_set[index_5G].rx_count) ) {
					index_5G = i;
					best_channel_5G = pmlmeext->channel_set[i].ChannelNum;
				}
			}

			if ( pmlmeext->channel_set[i].ChannelNum >= 149
				&& pmlmeext->channel_set[i].ChannelNum < 165) {
				 // find primary channel
				if ( (( pmlmeext->channel_set[i].ChannelNum - 149) % 8 == 0)
					&& (pmlmeext->channel_set[i].rx_count < pmlmeext->channel_set[index_5G].rx_count) ) {
					index_5G = i;
					best_channel_5G = pmlmeext->channel_set[i].ChannelNum;
				}
			}
#if 1 // debug
			len += seq_printf(m, "\nThe rx cnt of channel %3d = %d",
						pmlmeext->channel_set[i].ChannelNum, pmlmeext->channel_set[i].rx_count);
#endif
	}

	len += seq_printf(m, "\nbest_channel_5G = %d", best_channel_5G);
	len += seq_printf(m, "\nbest_channel_24G = %d", best_channel_24G);
	seq_puts(m,"\n");
	return 0;
}


ssize_t proc_set_best_channel(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;
	char tmp[32];

	if(count < 1)
		return -EFAULT;

	if(buffer && !copy_from_user(tmp, buffer, sizeof(tmp)))
	{
		int i;
		for(i = 0; pmlmeext->channel_set[i].ChannelNum != 0; i++)
		{
			pmlmeext->channel_set[i].rx_count = 0;
		}

		DBG_871X("set %s\n", "Clean Best Channel Count");
	}

	return count;
}



static int dl_proc_get_best_channel(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_best_channel,GET_INODE_DATA(inode));
}


static ssize_t dl_proc_set_best_channel(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &proc_set_best_channel;
	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);
}

static const struct file_operations file_ops_proc_get_best_channel = {
	.open = dl_proc_get_best_channel,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_best_channel,
};

#endif /* CONFIG_FIND_BEST_CHANNEL */

#ifdef CONFIG_BT_COEXIST
//btcoex_dbg
static int rtl_proc_get_btcoex_dbg(struct seq_file *m,void *v)	//waiting modified
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	int len = 0;
	seq_puts(m,"\n");
	return 0;
}

ssize_t proc_set_btcoex_dbg(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	PADAPTER padapter;
	u8 tmp[80] = {0};
	u32 module[2] = {0};
	u32 num;

	padapter = (PADAPTER)rtw_netdev_priv(dev);
	return count;
}


static int dl_proc_get_btcoex_dbg(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_btcoex_dbg,GET_INODE_DATA(inode));
}

static ssize_t dl_proc_set_btcoex_dbg(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = &proc_set_btcoex_dbg;
	return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);
}


static const struct file_operations file_ops_proc_get_btcoex_dbg = {
	.open = dl_proc_get_btcoex_dbg,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = dl_proc_set_btcoex_dbg,
};

//btcoex_info
static int rtl_proc_get_btcoex_info(struct seq_file *m,void *v) //waiting modified
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	const u32 bufsize = 30*100;
	static u8 *pbuf = NULL;
	static u32 remainlen = 0;
	s32 len = 0;


	seq_puts(m,"\n");
	return 0;
}
static int dl_proc_get_btcoex_info(struct inode *inode,struct file *file)
{
	return single_open(file,rtl_proc_get_btcoex_info,GET_INODE_DATA(inode));
}

static const struct file_operations file_ops_proc_get_btcoex_info = {
	.open = dl_proc_get_btcoex_info,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#endif /* CONFIG_BT_COEXIST */

#ifdef CONFIG_PROC_DEBUG
#define RTL8192C_PROC_NAME "rtl819xC"
#define RTL8192D_PROC_NAME "rtl819xD"
static char rtw_proc_name[IFNAMSIZ];
static struct proc_dir_entry *rtw_proc = NULL;
static int	rtw_proc_cnt = 0;

#define RTW_PROC_NAME DRV_NAME

void rtw_proc_init_one(struct net_device *dev)
{
	struct proc_dir_entry *dir_dev = NULL;
	struct proc_dir_entry *entry=NULL;
	_adapter	*padapter = rtw_netdev_priv(dev);
	u8 rf_type;
	ssize_t i;

	if(rtw_proc == NULL)
	{
		if(padapter->chip_type == RTL8188C_8192C)
		{
			_rtw_memcpy(rtw_proc_name, RTL8192C_PROC_NAME, sizeof(RTL8192C_PROC_NAME));
		}
		else if(padapter->chip_type == RTL8192D)
		{
			_rtw_memcpy(rtw_proc_name, RTL8192D_PROC_NAME, sizeof(RTL8192D_PROC_NAME));
		}
		else if(padapter->chip_type == RTL8723A)
		{
			_rtw_memcpy(rtw_proc_name, RTW_PROC_NAME, sizeof(RTW_PROC_NAME));
		}
		else if(padapter->chip_type == RTL8188E)
		{
			_rtw_memcpy(rtw_proc_name, RTW_PROC_NAME, sizeof(RTW_PROC_NAME));
		}
		else
		{
			_rtw_memcpy(rtw_proc_name, RTW_PROC_NAME, sizeof(RTW_PROC_NAME));
		}

#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
		rtw_proc=create_proc_entry(rtw_proc_name, S_IFDIR, proc_net);
#else
		rtw_proc=proc_mkdir(rtw_proc_name, init_net.proc_net);
#endif

		if (rtw_proc == NULL) {
			DBG_871X(KERN_ERR "Unable to create rtw_proc directory\n");
			return;
		}

		i = 0;
		entry = proc_create_data("ver_info", S_IFREG | S_IRUGO, rtw_proc, &file_ops_proc_get_drv_version, (void *)i);
		if (!entry) {
			DBG_871X("Unable to proc_create_data!\n");
			return;
		}

	}



	if(padapter->dir_dev == NULL)
	{
		padapter->dir_dev = proc_mkdir(dev->name,
					 rtw_proc);

		dir_dev = padapter->dir_dev;

		if(dir_dev==NULL)
		{
			if(rtw_proc_cnt == 0)
			{
				if(rtw_proc){
#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
					remove_proc_entry(rtw_proc_name, proc_net);
#else
					remove_proc_entry(rtw_proc_name, init_net.proc_net);
#endif
					rtw_proc = NULL;
				}
			}

			DBG_871X("Unable to create dir_dev directory\n");
			return;
		}
	}
	else
	{
		return;
	}

	rtw_proc_cnt++;

	entry = proc_create_data("write_reg", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_set_write_reg, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("read_reg", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_read_reg, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("fwstate", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_fwstate, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}


	entry = proc_create_data("sec_info", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_sec_info, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}


	entry = proc_create_data("mlmext_state", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_mlmext_state, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}


	entry = proc_create_data("qos_option", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_qos_option, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("ht_option", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_ht_option, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("rf_info", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_rf_info, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("ap_info", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_ap_info, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("adapter_state", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_adapter_state, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("trx_info", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_trx_info, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("mac_reg_dump1", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_mac_reg_dump1, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("mac_reg_dump2", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_mac_reg_dump2, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("mac_reg_dump3", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_mac_reg_dump3, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("bb_reg_dump1", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_bb_reg_dump1, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("bb_reg_dump2", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_bb_reg_dump2, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("bb_reg_dump3", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_bb_reg_dump3, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("rf_reg_dump1", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_rf_reg_dump1, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("rf_reg_dump2", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_rf_reg_dump2, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	rtw_hal_get_hwreg(padapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
	if((RF_1T2R == rf_type) ||(RF_1T1R ==rf_type )) {
		entry = proc_create_data("rf_reg_dump3", S_IFREG | S_IRUGO,
					   dir_dev, &file_ops_proc_get_rf_reg_dump3, dev);
		if (!entry) {
			DBG_871X("Unable to create_proc_read_entry!\n");
			return;
		}

		entry = proc_create_data("rf_reg_dump4", S_IFREG | S_IRUGO,
					   dir_dev, &file_ops_proc_get_rf_reg_dump4, dev);
		if (!entry) {
			DBG_871X("Unable to create_proc_read_entry!\n");
			return;
		}
	}

#ifdef CONFIG_AP_MODE

	entry = proc_create_data("all_sta_info", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_all_sta_info, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}
#endif

#ifdef CONFIG_80211N_HT
	entry = proc_create_data("ht_enable", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_ht_enable, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("cbw40_enable", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_cbw40_enable, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("ampdu_enable", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_ampdu_enable, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("rx_stbc", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_rx_stbc, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}
#endif //CONFIG_80211N_HT

#ifdef DBG_MEMORY_LEAK
	entry = proc_create_data("_malloc_cnt", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_malloc_cnt, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}
#endif

#ifdef CONFIG_FIND_BEST_CHANNEL
	entry = proc_create_data("best_channel", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_best_channel, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}
#endif

#ifdef CONFIG_BT_COEXIST
	entry = proc_create_data("btcoex_dbg", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_btcoex_dbg, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("btcoex", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_btcoex_info, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry fo BTCOEX!\n");
		return;
	}
#endif // CONFIG_BT_COEXIST

	entry = proc_create_data("rx_signal", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_rx_signal, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}

	entry = proc_create_data("rssi_disp", S_IFREG | S_IRUGO,
				   dir_dev, &file_ops_proc_get_rssi_disp, dev);
	if (!entry) {
		DBG_871X("Unable to create_proc_read_entry!\n");
		return;
	}
}

void rtw_proc_remove_one(struct net_device *dev)
{
	struct proc_dir_entry *dir_dev = NULL;
	_adapter	*padapter = rtw_netdev_priv(dev);
	u8 rf_type;

	dir_dev = padapter->dir_dev;
	padapter->dir_dev = NULL;

	if (dir_dev) {

		remove_proc_entry("write_reg", dir_dev);
		remove_proc_entry("read_reg", dir_dev);
		remove_proc_entry("fwstate", dir_dev);
		remove_proc_entry("sec_info", dir_dev);
		remove_proc_entry("mlmext_state", dir_dev);
		remove_proc_entry("qos_option", dir_dev);
		remove_proc_entry("ht_option", dir_dev);
		remove_proc_entry("rf_info", dir_dev);
		remove_proc_entry("ap_info", dir_dev);
		remove_proc_entry("adapter_state", dir_dev);
		remove_proc_entry("trx_info", dir_dev);

		remove_proc_entry("mac_reg_dump1", dir_dev);
		remove_proc_entry("mac_reg_dump2", dir_dev);
		remove_proc_entry("mac_reg_dump3", dir_dev);
		remove_proc_entry("bb_reg_dump1", dir_dev);
		remove_proc_entry("bb_reg_dump2", dir_dev);
		remove_proc_entry("bb_reg_dump3", dir_dev);
		remove_proc_entry("rf_reg_dump1", dir_dev);
		remove_proc_entry("rf_reg_dump2", dir_dev);
		rtw_hal_get_hwreg(padapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
		if((RF_1T2R == rf_type) ||(RF_1T1R ==rf_type )) {
			remove_proc_entry("rf_reg_dump3", dir_dev);
			remove_proc_entry("rf_reg_dump4", dir_dev);
		}
#ifdef CONFIG_AP_MODE
		remove_proc_entry("all_sta_info", dir_dev);
#endif

#ifdef DBG_MEMORY_LEAK
		remove_proc_entry("_malloc_cnt", dir_dev);
#endif

#ifdef CONFIG_80211N_HT
		remove_proc_entry("cbw40_enable", dir_dev);

		remove_proc_entry("ht_enable", dir_dev);

		remove_proc_entry("ampdu_enable", dir_dev);

		remove_proc_entry("rx_stbc", dir_dev);
#endif //CONFIG_80211N_HT

#ifdef CONFIG_FIND_BEST_CHANNEL
		remove_proc_entry("best_channel", dir_dev);
#endif
		remove_proc_entry("rx_signal", dir_dev);

		remove_proc_entry("rssi_disp", dir_dev);

#ifdef CONFIG_BT_COEXIST
		remove_proc_entry("btcoex_dbg", dir_dev);
		remove_proc_entry("btcoex", dir_dev);
#endif // CONFIG_BT_COEXIST

		remove_proc_entry(dev->name, rtw_proc);
		dir_dev = NULL;

	}
	else
	{
		return;
	}

	rtw_proc_cnt--;

	if(rtw_proc_cnt == 0)
	{
		if(rtw_proc){
			remove_proc_entry("ver_info", rtw_proc);

#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
			remove_proc_entry(rtw_proc_name, proc_net);
#else
			remove_proc_entry(rtw_proc_name, init_net.proc_net);
#endif
			rtw_proc = NULL;
		}
	}
}
#endif


#endif

