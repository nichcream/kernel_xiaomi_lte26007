/*
 *   Handling routines for DFS DET functions
 *
 *  $Id: 8192cd_dfs_det.c,v 1.1 2012/05/04 12:49:07 jimmylin Exp $
 *
 *  Copyright (c) 2012 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_DFS_DET_C_

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"


#if defined(DFS) && !defined(CONFIG_RTL_92D_SUPPORT)

extern u2Byte dB_Invert_Table[8][12];

static u4Byte 
ConvertTo_dB(
	u4Byte 	Value)
{
	u1Byte i;
	u1Byte j;
	u4Byte dB;

	Value = Value & 0xFFFF;
	
	for (i=0;i<8;i++)
	{
		if (Value <= dB_Invert_Table[i][11])
		{
			break;
		}
	}

	if (i >= 8)
	{
		return (96);	// maximum 96 dB
	}

	for (j=0;j<12;j++)
	{
		if (Value <= dB_Invert_Table[i][j])
		{
			break;
		}
	}

	dB = i*12 + j + 1;

	return (dB);
}


static int
GetPSDData_8812(
	IN PDM_ODM_T	pDM_Odm,
	unsigned int 	point,
	u1Byte initial_gain_psd)
{
	int	psd_report;
	
	//Set DCO frequency index, offset=(40MHz/SamplePts)*point
	ODM_SetBBReg(pDM_Odm, 0x910, 0x3FF, point);

	//Start PSD calculation, Reg808[22]=0->1
	ODM_SetBBReg(pDM_Odm, 0x910, BIT22, 1);

	//Need to wait for HW PSD report
	delay_us(110);

	ODM_SetBBReg(pDM_Odm, 0x910, BIT22, 0);

	//Read PSD report, Reg8B4[15:0]
	psd_report = (int)ODM_GetBBReg(pDM_Odm,0xf44, bMaskDWord) & 0x0000FFFF;
	psd_report = (int)(ConvertTo_dB((u4Byte)psd_report));		

	return psd_report;
	
}


void Scan_BB_PSD(
	IN PDM_ODM_T	pDM_Odm,
	int *PSD_report_right,
	int *PSD_report_left,
	int len,
	int initial_gain)
{
	struct rtl8192cd_priv *priv=pDM_Odm->priv;
	pDIG_T  pDM_DigTable = &pDM_Odm->DM_DigTable;
	u1Byte  ST_TH_origin;
	u1Byte	idx[20]={//96,99,102,106,109,112,115,118,122,125,
	                     224,227,230,234,237,240,243,246,250,253,
					0,3,6,10,13,16,19,22,26,29};
	int tone_idx, channel_org, channel, i;

	// set DFS ST_TH to max value
	ST_TH_origin = RTL_R8(0x91c);
	RTL_W8(0x91c, 0x4e);
	
	// Turn off CCK
	ODM_SetBBReg(pDM_Odm, 0x808, BIT28, 0);   //808[28]

	// Turn off TX
	// Pause TX Queue
	if (!priv->pmib->dot11DFSEntry.disable_tx)
		ODM_Write1Byte(pDM_Odm, 0x522, 0xFF); //REG_TXPAUSE 改為0x522

	// Turn off CCA
	ODM_SetBBReg(pDM_Odm, 0x838, BIT3, 0x1); //838[3] 設為1

	// PHYTXON while loop
	i = 0;
	while (ODM_GetBBReg(pDM_Odm, 0xfa0, BIT18)) {
		i++;
		if (i > 1000000) {
			panic_printk("Wait in %s() more than %d times!\n", __FUNCTION__, i);
			break;
		}
	}

	// backup IGI_origin , set IGI = 0x3e;
	pDM_DigTable->bPSDInProgress = TRUE;
	odm_PauseDIG(pDM_Odm, ODM_PAUSE_DIG, initial_gain);

	// Turn off 3-wire
	ODM_SetBBReg(pDM_Odm, 0xC00, BIT1|BIT0, 0x0); //c00[1:0] 寫0

	// pts value = 128, 256, 512, 1024
	ODM_SetBBReg(pDM_Odm, 0x910, BIT14|BIT15, 0x1); //910[15:14]設為1, 用256點

	// scan in-band PSD
	channel_org = ODM_GetRFReg(pDM_Odm, RF_PATH_A, RF_CHNLBW, 0x3FF);
	if(priv, priv->pshare->CurrentChannelBW != HT_CHANNEL_WIDTH_20){
		priv->pshare->No_RF_Write = 0;
		SwBWMode(priv, HT_CHANNEL_WIDTH_20, 0);
		priv->pshare->No_RF_Write = 1;
	}
	
	if (priv->pshare->rf_ft_var.dfs_scan_inband) {
		int PSD_report_inband[20];
		for (tone_idx=0;tone_idx<len;tone_idx++)
			PSD_report_inband[tone_idx] = GetPSDData_8812(pDM_Odm, idx[tone_idx], initial_gain);
		panic_printk("PSD inband: ");
		for (i=0; i<len; i++)
			panic_printk("%d ", PSD_report_inband[i]);
		panic_printk("\n");
	}

	// scan right(higher) neighbor channel
	if (priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20)
		channel = channel_org + 4;
	else if (priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20_40)
		channel = channel_org + 6;
	else
		channel = channel_org + 10;
	delay_us(300);  // for idle 20M, it will  emit signal in right 20M channel
	priv->pshare->No_RF_Write = 0;
	ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_CHNLBW, 0x3FF, channel);
	priv->pshare->No_RF_Write = 1;

	for (tone_idx=0;tone_idx<len;tone_idx++)
		PSD_report_right[tone_idx] = GetPSDData_8812(pDM_Odm, idx[tone_idx], initial_gain);
	
	// scan left(lower) neighbor channel
	if (priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20)
		channel = channel_org - 4;
	else if (priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20_40)
		channel = channel_org - 6;
	else
		channel = channel_org - 10;
	priv->pshare->No_RF_Write = 0;
	ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_CHNLBW, 0x3FF, channel);
	priv->pshare->No_RF_Write = 1;

	for (tone_idx=0;tone_idx<len;tone_idx++)
		PSD_report_left[tone_idx] = GetPSDData_8812(pDM_Odm, idx[tone_idx], initial_gain);
	

	// restore originl center frequency
	if(priv, priv->pshare->CurrentChannelBW != HT_CHANNEL_WIDTH_20){
		priv->pshare->No_RF_Write = 0;
		SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
		priv->pshare->No_RF_Write = 1;
	}
	priv->pshare->No_RF_Write = 0;
	ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_CHNLBW, 0x3FF, channel_org);
	priv->pshare->No_RF_Write = 1;

	// Turn on 3-wire
	ODM_SetBBReg(pDM_Odm, 0xc00, BIT1|BIT0, 0x3); //c00[1:0] 寫3

	// Restore Current Settings
	// Resume DIG
	pDM_DigTable->bPSDInProgress = FALSE;
	odm_PauseDIG(pDM_Odm, ODM_RESUME_DIG, NONE);

	//Turn on CCA
	ODM_SetBBReg(pDM_Odm, 0x838, BIT3, 0); //838[3] 設為0

	// Turn on TX
	// Resume TX Queue
	if (!priv->pmib->dot11DFSEntry.disable_tx)
		ODM_Write1Byte(pDM_Odm, 0x522, 0x00); //REG_TXPAUSE 改為0x522

	// CCK on
	ODM_SetBBReg(pDM_Odm, 0x808, BIT28, 1); //808[28]	
	
	// Resume DFS ST_TH
	RTL_W8(0x91c, ST_TH_origin);
}


void rtl8192cd_dfs_det_chk(struct rtl8192cd_priv *priv)
{
	unsigned int regf98_value;
	unsigned int reg918_value;
	unsigned int reg91c_value;
	unsigned int reg920_value;
	unsigned int reg924_value;
	unsigned int FA_count_cur=0, FA_count_inc=0;
	unsigned int VHT_CRC_ok_cnt_cur=0, VHT_CRC_ok_cnt_inc=0;
	unsigned int HT_CRC_ok_cnt_cur=0, HT_CRC_ok_cnt_inc=0;
	unsigned int LEG_CRC_ok_cnt_cur=0, LEG_CRC_ok_cnt_inc=0;
	unsigned int Total_CRC_OK_cnt_inc=0, FA_CRCOK_ratio=0;
	unsigned char DFS_tri_short_pulse=0, DFS_tri_long_pulse=0, fa_mask_mid_th=0, fa_mask_lower_th=0;
	unsigned int short_pulse_cnt_cur=0, short_pulse_cnt_inc=0;
	unsigned int long_pulse_cnt_cur=0, long_pulse_cnt_inc=0;
	unsigned int total_pulse_count_inc=0, max_sht_pusle_cnt_th=0;
	unsigned int sum, k, fa_flag=0;
	unsigned int st_L2H_new=0, st_L2H_tmp, index=0, fault_flag_det, fault_flag_psd;
	int flags=0;
	unsigned long throughput = 0;
	int j;

	int i, PSD_report_right[20], PSD_report_left[20];
	int max_right, max_left;

	if (priv->det_asoc_clear > 0) {
		priv->det_asoc_clear--;
		priv->pmib->dot11DFSEntry.DFS_detected = 0;
		priv->FA_count_pre = 0;
		priv->VHT_CRC_ok_cnt_pre = 0;
		priv->HT_CRC_ok_cnt_pre = 0;
		priv->LEG_CRC_ok_cnt_pre = 0;
		priv->mask_idx = 0;
		priv->mask_hist_checked = 0;
		memset(priv->radar_det_mask_hist, 0, sizeof(priv->radar_det_mask_hist));
		memset(priv->pulse_flag_hist, 0, sizeof(priv->pulse_flag_hist));
		mod_timer(&priv->dfs_det_chk_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(priv->pshare->rf_ft_var.dfs_det_period*10));
		return;
	}
	
	throughput = priv->ext_stats.tx_avarage+priv->ext_stats.rx_avarage;

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		for (j=0; j<RTL8192CD_NUM_VWLAN; j++) {
			if (IS_DRV_OPEN(priv->pvap_priv[j])) {
				throughput += priv->pvap_priv[j]->ext_stats.tx_avarage+priv->pvap_priv[j]->ext_stats.rx_avarage;
			}
		}
	}
#endif

	// Get FA count during past 100ms
	FA_count_cur = PHY_QueryBBReg(priv, 0xf48, 0x0000ffff);

	if (priv->FA_count_pre == 0)
		FA_count_inc = 0;
	else if (FA_count_cur >= priv->FA_count_pre)
		FA_count_inc = FA_count_cur - priv->FA_count_pre;
	else
		FA_count_inc = FA_count_cur;
	priv->FA_count_pre = FA_count_cur;

	// Get VHT CRC32 ok count during past 100ms
	VHT_CRC_ok_cnt_cur = PHY_QueryBBReg(priv, 0xf0c, 0x00003fff);
	if (VHT_CRC_ok_cnt_cur >= priv->VHT_CRC_ok_cnt_pre)
		VHT_CRC_ok_cnt_inc = VHT_CRC_ok_cnt_cur - priv->VHT_CRC_ok_cnt_pre;
	else
		VHT_CRC_ok_cnt_inc = VHT_CRC_ok_cnt_cur;
	priv->VHT_CRC_ok_cnt_pre = VHT_CRC_ok_cnt_cur;

	// Get HT CRC32 ok count during past 100ms
	HT_CRC_ok_cnt_cur = PHY_QueryBBReg(priv, 0xf10, 0x00003fff);
	if (HT_CRC_ok_cnt_cur >= priv->HT_CRC_ok_cnt_pre)
		HT_CRC_ok_cnt_inc = HT_CRC_ok_cnt_cur - priv->HT_CRC_ok_cnt_pre;
	else
		HT_CRC_ok_cnt_inc = HT_CRC_ok_cnt_cur;
	priv->HT_CRC_ok_cnt_pre = HT_CRC_ok_cnt_cur;

	// Get Legacy CRC32 ok count during past 100ms
	LEG_CRC_ok_cnt_cur = PHY_QueryBBReg(priv, 0xf14, 0x00003fff);
	if (LEG_CRC_ok_cnt_cur >= priv->LEG_CRC_ok_cnt_pre)
		LEG_CRC_ok_cnt_inc = LEG_CRC_ok_cnt_cur - priv->LEG_CRC_ok_cnt_pre;
	else
		LEG_CRC_ok_cnt_inc = LEG_CRC_ok_cnt_cur;
	priv->LEG_CRC_ok_cnt_pre = LEG_CRC_ok_cnt_cur;

	if ((VHT_CRC_ok_cnt_cur == 0x3fff) ||
		(HT_CRC_ok_cnt_cur == 0x3fff) ||
		(LEG_CRC_ok_cnt_cur == 0x3fff)) {
		PHY_SetBBReg(priv, 0xb58, BIT(0), 1);
		PHY_SetBBReg(priv, 0xb58, BIT(0), 0);
	}

	Total_CRC_OK_cnt_inc = VHT_CRC_ok_cnt_inc + HT_CRC_ok_cnt_inc + LEG_CRC_ok_cnt_inc;

	// check if the FA occrus frequencly during 100ms
	// FA_count_inc is divided by Total_CRC_OK_cnt_inc, which helps to distinguish normal trasmission from interference
	if (Total_CRC_OK_cnt_inc > 0)
		FA_CRCOK_ratio  = FA_count_inc / Total_CRC_OK_cnt_inc;

	//=====dynamic power threshold (DPT) ========
	// Get short pulse count, need carefully handle the counter overflow
	regf98_value = PHY_QueryBBReg(priv, 0xf98, 0xffffffff);
	short_pulse_cnt_cur = regf98_value & 0x000000ff;
	if (short_pulse_cnt_cur >= priv->short_pulse_cnt_pre)
		short_pulse_cnt_inc = short_pulse_cnt_cur - priv->short_pulse_cnt_pre;
	else
		short_pulse_cnt_inc = short_pulse_cnt_cur;
	priv->short_pulse_cnt_pre = short_pulse_cnt_cur;

	// Get long pulse count, need carefully handle the counter overflow
	long_pulse_cnt_cur = (regf98_value & 0x0000ff00) >> 8;
	if (long_pulse_cnt_cur >= priv->long_pulse_cnt_pre)
		long_pulse_cnt_inc = long_pulse_cnt_cur - priv->long_pulse_cnt_pre;
	else
		long_pulse_cnt_inc = long_pulse_cnt_cur;
	priv->long_pulse_cnt_pre = long_pulse_cnt_cur;

	total_pulse_count_inc = short_pulse_cnt_inc + long_pulse_cnt_inc;

	if (priv->pshare->rf_ft_var.dfs_det_print) {
		panic_printk("=====================================================================\n");
		panic_printk("Total_CRC_OK_cnt_inc[%d] VHT_CRC_ok_cnt_inc[%d] HT_CRC_ok_cnt_inc[%d] LEG_CRC_ok_cnt_inc[%d] FA_count_inc[%d] FA_CRCOK_ratio[%d]\n",
			Total_CRC_OK_cnt_inc, VHT_CRC_ok_cnt_inc, HT_CRC_ok_cnt_inc, LEG_CRC_ok_cnt_inc, FA_count_inc, FA_CRCOK_ratio);
		panic_printk("Init_Gain[%x] 0x91c[%x] 0xf98[%08x] short_pulse_cnt_inc[%d] long_pulse_cnt_inc[%d]\n",
			priv->ini_gain_cur, priv->st_L2H_cur, regf98_value, short_pulse_cnt_inc, long_pulse_cnt_inc);
		panic_printk("Throughput: %luMbps\n", (throughput>>17));
		reg918_value = PHY_QueryBBReg(priv, 0x918, 0xffffffff);
		reg91c_value = PHY_QueryBBReg(priv, 0x91c, 0xffffffff);
		reg920_value = PHY_QueryBBReg(priv, 0x920, 0xffffffff);
		reg924_value = PHY_QueryBBReg(priv, 0x924, 0xffffffff);
		printk("0x918[%08x] 0x91c[%08x] 0x920[%08x] 0x924[%08x]\n", reg918_value, reg91c_value, reg920_value, reg924_value);
	}

	DFS_tri_short_pulse = (regf98_value & BIT(17))? 1 : 0;
	DFS_tri_long_pulse = (regf98_value & BIT(19))? 1 : 0;
	if (DFS_tri_short_pulse) {
		//RTL_W32(0x920, RTL_R32(0x920) | BIT(24));
		//RTL_W32(0x920, RTL_R32(0x920) & ~BIT(24));
		RTL_W32(0x924, RTL_R32(0x924) & ~BIT(15));
		RTL_W32(0x924, RTL_R32(0x924) | BIT(15));
	}
	if (DFS_tri_long_pulse) {
		//RTL_W32(0x920, RTL_R32(0x920) | BIT(25));
		//RTL_W32(0x920, RTL_R32(0x920) & ~BIT(25));
		RTL_W32(0x924, RTL_R32(0x924) & ~BIT(15));
		RTL_W32(0x924, RTL_R32(0x924) | BIT(15));
		if (priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_MKK) {	
			if ((priv->pmib->dot11RFEntry.dot11channel >= 52) &&
				(priv->pmib->dot11RFEntry.dot11channel <= 64)) {
				DFS_tri_long_pulse = 0;
			}
		}
		if (priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_ETSI) {
			DFS_tri_long_pulse = 0;
		}
	}

	st_L2H_new = priv->st_L2H_cur;
	priv->pulse_flag_hist[priv->mask_idx] = DFS_tri_short_pulse | DFS_tri_long_pulse;

	//max_sht_pusle_cnt_th =  PHY_QueryBBReg(priv, 0x920, 0x000f0000)-1; //read 920[19:16]
	max_sht_pusle_cnt_th = priv->pshare->rf_ft_var.dfs_max_sht_pusle_cnt_th;
	if (priv->pshare->rf_ft_var.dfs_det_print3)
		panic_printk("max_sht_pusle_cnt_th = %d\n", max_sht_pusle_cnt_th);

	if(priv->idle_flag == 1){		
		if(priv->pshare->rf_ft_var.dfs_psd_idle_on == 1){
			SAVE_INT_AND_CLI(flags);
			Scan_BB_PSD(ODMPTR, PSD_report_right, PSD_report_left, 20, 0x3e);	
			RESTORE_INT(flags);
		}
		else{
			for (i=0; i<20; i++) {
				PSD_report_right[i] = 0;
				PSD_report_left[i] = 0;
			}
		}		
	}
	else{
		if(priv->pshare->rf_ft_var.dfs_psd_TP_on == 1){
			SAVE_INT_AND_CLI(flags);
			Scan_BB_PSD(ODMPTR, PSD_report_right, PSD_report_left, 20, 0x3e);	
			RESTORE_INT(flags);
		}
		else{
			for (i=0; i<20; i++) {
				PSD_report_right[i] = 0;
				PSD_report_left[i] = 0;
			}
		}
	}
	
	for (i=0; i<20; i++) {
		PSD_report_right[i] = (-110 + 0x3e) - 39 + PSD_report_right[i];
		PSD_report_left[i] = (-110 + 0x3e) - 39 + PSD_report_left[i];
	}
	if (priv->pshare->rf_ft_var.dfs_det_print_psd) {
		panic_printk("PSD right: ");
		for (i=0; i<20; i++)
			panic_printk("%d ", PSD_report_right[i]);
		//panic_printk("\n");
		panic_printk("   PSD left: ");
		for (i=0; i<20; i++)
			panic_printk("%d ", PSD_report_left[i]);
		panic_printk("\n");
	}

	fault_flag_det = 0;
	fault_flag_psd = 0;
	fa_flag = 0;
	if(priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_ETSI){
		fa_mask_mid_th = priv->pshare->rf_ft_var.dfs_fa_cnt_mid + 20;
		fa_mask_lower_th = priv->pshare->rf_ft_var.dfs_fa_cnt_lower + 40;
	}
	else{
		fa_mask_mid_th = priv->pshare->rf_ft_var.dfs_fa_cnt_mid;
		fa_mask_lower_th = priv->pshare->rf_ft_var.dfs_fa_cnt_lower;
	}
	if (((FA_count_inc >= priv->pshare->rf_ft_var.dfs_dpt_fa_th_upper) && (short_pulse_cnt_inc > max_sht_pusle_cnt_th)) ||
		(priv->ini_gain_cur >= priv->pshare->rf_ft_var.dpt_ini_gain_th)) {
		st_L2H_new = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_max;
		if (priv->pshare->rf_ft_var.dfs_det_print3)
			panic_printk("[1] st_L2H_new %x\n", st_L2H_new);
		priv->radar_det_mask_hist[priv->mask_idx] = 1;
		if (priv->pulse_flag_hist[priv->mask_idx] == 1){
			priv->pulse_flag_hist[priv->mask_idx] = 0;
			if (priv->pshare->rf_ft_var.dfs_det_print4){
				panic_printk("Radar is masked : FA mask case1\n");			
			}
		}
		fa_flag = 1;
	}
	else if (((FA_count_inc >= fa_mask_mid_th) ||
				((FA_count_inc >= fa_mask_lower_th) &&
					(FA_count_inc >= priv->pshare->rf_ft_var.dfs_fa_cnt_inc_ratio * priv->FA_count_inc_pre))) 
		&& (short_pulse_cnt_inc > max_sht_pusle_cnt_th)) {
		if (priv->pshare->rf_ft_var.dfs_dpt_st_l2h_add)
			st_L2H_new += 2;
		if (priv->pshare->rf_ft_var.dfs_det_print3)
			panic_printk("[2] st_L2H_new %x\n", st_L2H_new);
		priv->radar_det_mask_hist[priv->mask_idx] = 1;
		if (priv->pulse_flag_hist[priv->mask_idx] == 1){
			priv->pulse_flag_hist[priv->mask_idx] = 0;
			if (priv->pshare->rf_ft_var.dfs_det_print4){
				panic_printk("Radar is masked : FA mask case2\n");			
			}
		}
		fa_flag = 1;
	}
	else 
	{
		if (((FA_CRCOK_ratio > priv->pshare->rf_ft_var.dfs_fa_ratio_th) &&
				(FA_count_inc >= priv->pshare->rf_ft_var.dfs_fa_cnt_lower) &&
				(Total_CRC_OK_cnt_inc > priv->pshare->rf_ft_var.dfs_crc32_cnt_lower)) ||
			((short_pulse_cnt_inc <= max_sht_pusle_cnt_th) && (short_pulse_cnt_inc != 0) &&
				(priv->idle_flag == 1) && (priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_MKK)) ||
			(FA_count_inc >= priv->pshare->rf_ft_var.dfs_fa_cnt_upper))
			priv->radar_det_mask_hist[priv->mask_idx] = 1;
		else
			priv->radar_det_mask_hist[priv->mask_idx] = 0;
	}

	if (priv->pshare->rf_ft_var.dfs_det_print2) {
		panic_printk("mask_idx: %d\n", priv->mask_idx);
		panic_printk("radar_det_mask_hist: ");
		for (i=0; i<priv->pshare->rf_ft_var.dfs_det_hist_len; i++)
			panic_printk("%d ", priv->radar_det_mask_hist[i]);
		panic_printk("pulse_flag_hist: ");
		for (i=0; i<priv->pshare->rf_ft_var.dfs_det_hist_len; i++)
			panic_printk("%d ", priv->pulse_flag_hist[i]);
	}

	memcpy(&priv->PSD_report_right[priv->mask_idx][0], PSD_report_right, 20*sizeof(int));
	memcpy(&priv->PSD_report_left[priv->mask_idx][0], PSD_report_left, 20*sizeof(int));

	for (i=0; i<20; i++) {
		priv->max_hold_right[i] = -1000;
		priv->max_hold_left[i] = -1000;
	}

	for (k=0; k<5; k++) {
		for (i=0; i<20; i++) {
			if (priv->PSD_report_right[k][i] > priv->max_hold_right[i])
				priv->max_hold_right[i] = priv->PSD_report_right[k][i];
			if (priv->PSD_report_left[k][i] > priv->max_hold_left[i])
				priv->max_hold_left[i] = priv->PSD_report_left[k][i];
		}
	}

	if ((priv->pshare->rf_ft_var.dfs_psd_op == 1) && (priv->idle_flag == 0))
	{
		int right_index_start, left_index_end;
		int avg_1 = (priv->max_hold_right[0]+priv->max_hold_right[1]+priv->max_hold_right[2])/3;
		int avg_2 = (priv->max_hold_right[17]+priv->max_hold_right[18]+priv->max_hold_right[19])/3;
		max_right = -1000;
		if(RTL_ABS(avg_1, avg_2) <= 3)
			right_index_start=2;
		else
			right_index_start=11;								
		for (i=right_index_start; i<20; i++) {
				if ((i != 10) && (max_right < priv->max_hold_right[i]))
					max_right = priv->max_hold_right[i];
		}

		avg_1 = (priv->max_hold_left[0]+priv->max_hold_left[1]+priv->max_hold_left[2])/3;
		avg_2 = (priv->max_hold_left[17]+priv->max_hold_left[18]+priv->max_hold_left[19])/3;
		max_left = -1000;
		if (RTL_ABS(avg_1, avg_2) <= 3)
			left_index_end=20;								
		else
			left_index_end=8;		
		for (i=0; i<left_index_end; i++) {
			if ((i != 10) && (max_left < priv->max_hold_left[i]))
				max_left = priv->max_hold_left[i];
		}
		if(max_right == 0){
			max_right = -99;
		}
		if(max_left == 0){
			max_left = -99;
		}
	}
	else
	{
		max_right = -1000;
		for (i=2; i<20; i++) {
			if ((i != 10) && (max_right < priv->max_hold_right[i]))
					max_right = priv->max_hold_right[i];
		}
		max_left = -1000;
		for (i=0; i<20; i++) {
			if ((i != 10) && (max_left < priv->max_hold_left[i]))
				max_left = priv->max_hold_left[i];
		}
		if(max_right == 0){
			max_right = -99;
		}
		if(max_left == 0){
			max_left = -99;
		}
	}

	if (priv->pshare->rf_ft_var.dfs_det_print)
		panic_printk("max_right %d, max_left %d\n", max_right, max_left);

	if (priv->mask_idx >= priv->pshare->rf_ft_var.dfs_det_flag_offset)
		index = priv->mask_idx - priv->pshare->rf_ft_var.dfs_det_flag_offset;
	else
		index = priv->pshare->rf_ft_var.dfs_det_hist_len + priv->mask_idx - priv->pshare->rf_ft_var.dfs_det_flag_offset;
		
	// use PSD detection result
	if ((max_right > (0-(int)priv->pshare->rf_ft_var.dfs_psd_pw_th)) || (max_left > (0-(int)priv->pshare->rf_ft_var.dfs_psd_pw_th))) {
		if (priv->pshare->rf_ft_var.dfs_dpt_st_l2h_add) {
			st_L2H_tmp = 110 + ((max_right > max_left)? max_right:max_left) - priv->pshare->rf_ft_var.dfs_psd_fir_decay;
			if (st_L2H_tmp > priv->st_L2H_cur)
				st_L2H_new = st_L2H_tmp;
		}
		if (priv->pshare->rf_ft_var.dfs_det_print3)
			panic_printk("[3] st_L2H_new %x\n", st_L2H_new);

		if (priv->pshare->rf_ft_var.dfs_det_print)
			panic_printk("st_L2H_cur %x pwdb_th %x\n", st_L2H_new, priv->pwdb_th);

		fault_flag_psd = 1;
		if (priv->pulse_flag_hist[index] == 1){
			priv->pulse_flag_hist[index] = 0;
			if (priv->pshare->rf_ft_var.dfs_det_print4){
				panic_printk("Radar is masked : PSD mask\n");			
			}
		}
		priv->radar_det_mask_hist[index] = 1;
	}

	sum = 0;
	for (k=0; k<priv->pshare->rf_ft_var.dfs_det_hist_len; k++) {
		if (priv->radar_det_mask_hist[k] == 1)
			sum++;
	}

	if (priv->mask_hist_checked <= priv->pshare->rf_ft_var.dfs_det_hist_len)
		priv->mask_hist_checked++;

	//only the decision result of short pulse needs to refer the FA decision results
	if ((priv->mask_hist_checked >= priv->pshare->rf_ft_var.dfs_det_hist_len) &&
		//(DFS_tri_long_pulse || DFS_tri_short_pulse) &&
		priv->pulse_flag_hist[index])
	{
		//if ((sum <= priv->pshare->rf_ft_var.dfs_det_sum_th) &&
		//	(fa_flag == 0))
		if (sum <= priv->pshare->rf_ft_var.dfs_det_sum_th) 
		{
			priv->pmib->dot11DFSEntry.DFS_detected = 1 ; // DFS detect
			panic_printk("%s %d DFS detected\n", __FUNCTION__, __LINE__);
		}
		else {
			fault_flag_det = 1;
			if (priv->pshare->rf_ft_var.dfs_det_print4){
				panic_printk("Radar is masked : mask_hist large than thd\n");			
			}
		}
	}

	priv->mask_idx++;
	if (priv->mask_idx == priv->pshare->rf_ft_var.dfs_det_hist_len)
		priv->mask_idx = 0;

	if ((fault_flag_det == 0) && (fault_flag_psd == 0) && (fa_flag ==0)) {
		//if (total_pulse_count_inc < priv->pshare->rf_ft_var.dfs_dpt_pulse_th_lower) {
		//	// limit the ST value to absoulte lower bound 0x1c
		//	st_L2H_new -= 4;
		//	if (priv->pshare->rf_ft_var.dfs_det_print3)
		//		panic_printk("[4] st_L2H_new %x\n", st_L2H_new);
		//}
		///*else if (total_pulse_count_inc < priv->pshare->rf_ft_var.dfs_dpt_pulse_th_mid) {
		//	// limit the ST value to absoulte lower bound 0x1c
		//	st_L2H_new -= 2;
		//	if (priv->pshare->rf_ft_var.dfs_det_print3)
		//		panic_printk("[5] st_L2H_new %x\n", st_L2H_new);
		//}*/
		//else if (ini_gain_inc < 0){
		//	st_L2H_new += ini_gain_inc;
		//	if (priv->pshare->rf_ft_var.dfs_det_print3)
		//		panic_printk("[6] st_L2H_new %x\n", st_L2H_new);
		//}
		///*else if ((total_pulse_count_inc < max_sht_pusle_cnt_th) &&
		//		 (((priv->ext_stats.tx_avarage+priv->ext_stats.rx_avarage)>>17) <= priv->pshare->rf_ft_var.dfs_psd_tp_th)) {
		//	st_L2H_new += priv->pshare->rf_ft_var.dfs_dpt_st_l2h_idle_th;
		//}*/
		if (priv->ini_gain_cur < priv->pshare->rf_ft_var.dpt_ini_gain_th) {
			if ((priv->idle_flag == 1) && (priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_MKK))
				st_L2H_new = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min + priv->pshare->rf_ft_var.dfs_dpt_st_l2h_idle_offset;
			else
				st_L2H_new = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min;
		}
	 }	
	else{
		
		
		if (priv->pshare->rf_ft_var.dfs_dpt_st_l2h_add)
			st_L2H_new += 2;
		if (priv->pshare->rf_ft_var.dfs_det_print3)
			panic_printk("[7] st_L2H_new %x\n", st_L2H_new);
		
		
		if (DFS_tri_short_pulse) {
			//RTL_W32(0x920, RTL_R32(0x920) | (BIT(24) | BIT(28)));
			//RTL_W32(0x920, RTL_R32(0x920) & ~(BIT(24) | BIT(28)));
			RTL_W32(0x924, RTL_R32(0x924) & ~BIT(15));
			RTL_W32(0x924, RTL_R32(0x924) | BIT(15));
		}
		if (DFS_tri_long_pulse) {
			//RTL_W32(0x920, RTL_R32(0x920) | (BIT(25) | BIT(28)));
			//RTL_W32(0x920, RTL_R32(0x920) & ~(BIT(25) | BIT(28)));
			RTL_W32(0x924, RTL_R32(0x924) & ~BIT(15));
			RTL_W32(0x924, RTL_R32(0x924) | BIT(15));
		}
	}

	if ((st_L2H_new != priv->st_L2H_cur)) {
		// limit the ST value to absoulte lower bound 0x22
		// limit the ST value to absoulte upper bound 0x4e
		if (st_L2H_new < priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min) {
			if ((priv->idle_flag == 1) && (priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_MKK))
				priv->st_L2H_cur = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min + priv->pshare->rf_ft_var.dfs_dpt_st_l2h_idle_offset;
			else
				priv->st_L2H_cur = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min;
			//priv->st_L2H_cur = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min;
		}
		else if (st_L2H_new > priv->pshare->rf_ft_var.dfs_dpt_st_l2h_max)
			priv->st_L2H_cur = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_max;
		else
			priv->st_L2H_cur = st_L2H_new;
		RTL_W8(0x91c, priv->st_L2H_cur);

		priv->pwdb_th = ((int)priv->st_L2H_cur - (int)priv->ini_gain_cur)/2 + priv->pshare->rf_ft_var.dfs_pwdb_scalar_factor;
		priv->pwdb_th = MAX_NUM(priv->pwdb_th, (int)priv->pshare->rf_ft_var.dfs_pwdb_th); // limit the pwdb value to absoulte lower bound 0xa
		priv->pwdb_th = MIN_NUM(priv->pwdb_th, 0x1f);    // limit the pwdb value to absoulte upper bound 0x1f
		PHY_SetBBReg(priv, 0x918, 0x00001f00, priv->pwdb_th);
	}

	if (priv->pshare->rf_ft_var.dfs_det_print2) {
		panic_printk("fault_flag_det[%d], fault_flag_psd[%d], DFS_detected [%d]\n",fault_flag_det, fault_flag_psd, priv->pmib->dot11DFSEntry.DFS_detected );
	}
	
	priv->FA_count_inc_pre = FA_count_inc;
}


void rtl8192cd_dfs_dynamic_setting(struct rtl8192cd_priv *priv)
{
	unsigned char peak_th_cur=0, short_pulse_cnt_th_cur=0, three_peak_opt_cur=0, three_peak_th2_cur=0;
	unsigned char peak_window_cur=0, nb2wb_th_cur=0;
	
	if ((priv->idle_flag == 1)) { // idle (no traffic)
		peak_th_cur = 3;
		short_pulse_cnt_th_cur = 6;
		peak_window_cur = 2;
		nb2wb_th_cur = 6;
		three_peak_opt_cur = 1;
		three_peak_th2_cur = 2;
		if(priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_MKK){
			if ((priv->pmib->dot11RFEntry.dot11channel >= 52) &&
			(priv->pmib->dot11RFEntry.dot11channel <= 64)) {
				short_pulse_cnt_th_cur = priv->pshare->rf_ft_var.dfs_pc0_th_idle_w53;
				peak_window_cur = 2;
				nb2wb_th_cur = 3;
			}
			else {
				short_pulse_cnt_th_cur = priv->pshare->rf_ft_var.dfs_pc0_th_idle_w56;
				peak_window_cur = 2;
				nb2wb_th_cur = 3;
				three_peak_th2_cur = 0;
			}
		}
		else if(priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_FCC){
		}
		else if(priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_ETSI){
			if((priv->ch_120_132_CAC_end == 0) && (priv->pmib->dot11RFEntry.dot11channel >= 120) &&
				(priv->pmib->dot11RFEntry.dot11channel <= 132)){
				peak_th_cur = 2;
				short_pulse_cnt_th_cur = 7;
				peak_window_cur = 2;
				nb2wb_th_cur = 3;
				three_peak_opt_cur = 1;
				if(priv->pmib->dot11nConfigEntry.dot11nUse40M == HT_CHANNEL_WIDTH_80 || priv->pmib->dot11nConfigEntry.dot11nUse40M == HT_CHANNEL_WIDTH_20){
					short_pulse_cnt_th_cur = 10;
					three_peak_opt_cur = 0;
				}
				else{ // 40M
				}
			}
			else{
				short_pulse_cnt_th_cur = 7;
				nb2wb_th_cur = 4;
			}
		}
		else{  // default: FCC
		}
	}
	else{ // in service (with TP)
		peak_th_cur = 2;
		short_pulse_cnt_th_cur = 6;
		peak_window_cur = 2;
		nb2wb_th_cur = 3;
		three_peak_opt_cur = 1;
		three_peak_th2_cur = 2;
		if(priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_MKK){
			if ((priv->pmib->dot11RFEntry.dot11channel >= 52) &&
			(priv->pmib->dot11RFEntry.dot11channel <= 64)) {
			}
			else {
				three_peak_opt_cur = 0;
			}
		}
		else if(priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_FCC){
		}
		else if(priv->pmib->dot11StationConfigEntry.dot11RegDomain == DOMAIN_ETSI){
			short_pulse_cnt_th_cur = 5;
			if(priv->pmib->dot11nConfigEntry.dot11nUse40M == HT_CHANNEL_WIDTH_20){
				three_peak_opt_cur = 0;
			}
			else{
			}
		}
		else{
		}
	}
	if((priv->peak_th != peak_th_cur)){		
		PHY_SetBBReg(priv, 0x918, 0x00030000, peak_th_cur);
	}
	if((priv->short_pulse_cnt_th != short_pulse_cnt_th_cur)){		
		PHY_SetBBReg(priv, 0x920, 0x000f0000, short_pulse_cnt_th_cur);
	}
	if((priv->peak_window != peak_window_cur)){		
		PHY_SetBBReg(priv, 0x920, 0x00000300, peak_window_cur);
	}
	if((priv->nb2wb_th != nb2wb_th_cur)){		
		PHY_SetBBReg(priv, 0x920, 0x0000e000, nb2wb_th_cur);
	}
	if((priv->three_peak_opt != three_peak_opt_cur)){		
		PHY_SetBBReg(priv, 0x924, 0x00000180, three_peak_opt_cur);
	}
	if((priv->three_peak_th2 != three_peak_th2_cur)){		
		PHY_SetBBReg(priv, 0x924, 0x00007000, three_peak_th2_cur);
	}
	
    priv->peak_th = peak_th_cur;
	priv->short_pulse_cnt_th = short_pulse_cnt_th_cur;
	priv->peak_window = peak_window_cur;
	priv->nb2wb_th = nb2wb_th_cur;
	priv->three_peak_opt = three_peak_opt_cur;
	priv->three_peak_th2 = three_peak_th2_cur;
}

#endif

