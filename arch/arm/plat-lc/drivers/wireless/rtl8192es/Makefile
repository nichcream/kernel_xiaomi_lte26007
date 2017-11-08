#
# drivers/net/rtl8192cd
#
# Makefile for the Linux Wireless network device drivers.
#

MODULE_NAME := rtl8192cd

ifndef RTK_WIFI_8192CD_ROOT
	export RTK_WIFI_8192CD_ROOT=$(shell cd ../.. && pwd)
	export RTK_WIFI_8192CD_SRC=$(shell pwd)
endif

PWD := $(RTK_WIFI_8192CD_SRC)

include $(PWD)/arch/arm/plat-lc/drivers/wireless/rtl8192es/platform.mk
include $(PWD)/arch/arm/plat-lc/drivers/wireless/rtl8192es/config.mk

ifeq ($(CONFIG_RTL_92C_SUPPORT),y)
RTL_WLAN_DATA_DIR := data
endif
  
ifeq ($(CONFIG_RTL_92D_SUPPORT),y)
RTL_WLAN_DATA_DIR_D := data_92d
endif

ifeq ($(CONFIG_RTL_88E_SUPPORT),y)
  RTL_WLAN_DATA_DIR_E := data_88e
endif

ifeq ($(CONFIG_RTL_8812_SUPPORT),y)
  RTL_WLAN_DATA_DIR_8812 := data_8812
endif

ifeq ($(CONFIG_WLAN_HAL),y)
	ifeq ($(CONFIG_WLAN_HAL_8814AE),y)
		RTL_WLAN_DATA_DIR_8814 := WlanHAL/Data/8814A
	endif	
	ifeq ($(CONFIG_WLAN_HAL_8192EE),y)
		RTL_WLAN_DATA_DIR_92E := WlanHAL/Data/8192E
	endif
	ifeq ($(CONFIG_WLAN_HAL_8881A),y)
		ifeq ($(CONFIG_MAC_PHY_RF_Parameter_V700),y)
			RTL_WLAN_DATA_DIR := WlanHAL/Data/8881A/V700
		endif
		ifeq ($(CONFIG_MAC_PHY_RF_Parameter_V702B),y)
			RTL_WLAN_DATA_DIR := WlanHAL/Data/8881A/V702B
		endif
		ifeq ($(CONFIG_MAC_PHY_RF_Parameter_V702B_Skyworth),y)
			RTL_WLAN_DATA_DIR := WlanHAL/Data/8881A/V702B_Skyworth
		endif
#		ifeq ($(CONFIG_MAC_PHY_RF_Parameter_V702B_MP),y)
#			RTL_WLAN_DATA_DIR := WlanHAL/Data/8881A/V702B_MP
#		endif
		RTL_WLAN_DATA_DIR_8812 := WlanHAL/Data/8881A/V702B_MP
	endif
endif

ifeq ($(CONFIG_RTL_MESH_SUPPORT),y)
        EXTRA_CFLAGS += -DCONFIG_RTK_MESH #-DMESH_USE_METRICOP
#        ifeq ($(CONFIG_11S_TEST_MODE),y)
#        EXTRA_CFLAGS += -D_11s_TEST_MODE_
#        endif
        obj-mesh = ../mesh_ext/mesh_proc.o\
        ../mesh_ext/mesh_route.o\
        ../mesh_ext/mesh_rx.o\
        ../mesh_ext/mesh_sme.o\
        ../mesh_ext/mesh_security.o\
        ../mesh_ext/mesh_tx.o\
        ../mesh_ext/mesh_util.o\
        ../mesh_ext/mesh_11kv.o\
        ../mesh_ext/hash_table.o
endif

ifeq ($(CONFIG_USB_HCI),y)
   ifeq ($(CONFIG_RTL_92C_SUPPORT),y)
      MODULE_NAME := rtl8192cu
   endif
   ifeq ($(CONFIG_RTL_88E_SUPPORT),y)
      MODULE_NAME := rtl8188eu
   endif
   EXTRA_CFLAGS += -DCONFIG_USB_HCI
   $(MODULE_NAME)-objs += \
			usb/8192cu/8192cd_usb.o \
			usb/8192cu/8192cd_usb_hw.o \
			usb/8192cu/8192cd_usb_xmit.o \
			usb/8192cu/8192cd_usb_recv.o \
			usb/8192cu/8192cd_usb_cmd.o \
			intf_xmit.o
endif

ifeq ($(CONFIG_SDIO_HCI),y)
   ifeq ($(CONFIG_RTL_88E_SUPPORT),y)
      MODULE_NAME := rtl8189es
      $(MODULE_NAME)-objs += \
			sdio/8189es/8188e_sdio.o \
			sdio/8189es/8188e_sdio_hw.o \
			sdio/8189es/8188e_sdio_xmit.o \
			sdio/8189es/8188e_sdio_recv.o \
			sdio/8189es/8188e_sdio_cmd.o \
			intf_xmit.o
   endif
   ifeq ($(CONFIG_WLAN_HAL_8192EE),y)
      MODULE_NAME := 8192es
      $(MODULE_NAME)-objs += \
			sdio/8192es/8192e_sdio.o \
			sdio/8192es/8192e_sdio_hw.o \
			sdio/8192es/8192e_sdio_xmit.o \
			sdio/8192es/8192e_sdio_recv.o \
			sdio/8192es/8192e_sdio_cmd.o \
			hal_intf_xmit.o
   endif
endif

TARGET=$(MODULE_NAME).ko

SRCS_TXT = $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/*.txt))))
SRCS_BIN = $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/*.bin))))
SRCS_BIN_U = $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_E)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_E)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_E)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812)/*.BIN))))

SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_92E)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_92E)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_92E)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8814)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8814)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8814)/*.BIN))))

TXPWR_HEADER = 
ifeq ($(CONFIG_RTL_92C_SUPPORT),y)
TXPWR_LMT_TXT_92C = $(addprefix $(src)/data/,$(notdir $(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/TXPWR*.txt)))
TXPWR_LMT_FNAME_92C = $(basename $(notdir $(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/TXPWR*.txt)))
TXPWR_HEADER += $(obj)/TXPWR_92C.h
endif

ifeq ($(CONFIG_RTL_92D_SUPPORT),y)
TXPWR_LMT_TXT_92D = $(addprefix $(src)/data_92d/,$(notdir $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/TXPWR*.txt)))
TXPWR_LMT_FNAME_92D = $(basename $(notdir $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/TXPWR*.txt)))
TXPWR_HEADER += $(obj)/TXPWR_92D.h
endif

obj-$(CONFIG_RTL8192CD) := $(MODULE_NAME).o

ifeq ($(CONFIG_RTL_WAPI_SUPPORT),y)
	$(MODULE_NAME)-objs += wapi_wai.o wapiCrypto.o wapiRandom.o
endif

ifeq ($(CONFIG_RTL_88E_SUPPORT),y)
	$(MODULE_NAME)-objs += HalPwrSeqCmd.o\
	Hal8188EPwrSeq.o\
	8188e_hw.o 
	ifneq ($(origin CONFIG_AP_PS), undefined)
		ifeq ($(CONFIG_AP_PS),1)
			$(MODULE_NAME)-objs += OUTSRC/rtl8188e/Hal8188EFWImg_CE_PS.o
			EXTRA_CFLAGS  += -DSOFTAP_PS_DURATION
		endif
		ifeq ($(CONFIG_AP_PS),2)
			$(MODULE_NAME)-objs += OUTSRC/rtl8188e/Hal8188EFWImg_CE_OFFLOAD5.o
			EXTRA_CFLAGS  += -DSDIO_AP_OFFLOAD
		endif
		ifeq ($(CONFIG_AP_PS),0)	
			$(MODULE_NAME)-objs += OUTSRC/rtl8188e/HalHWImg8188E_FW.o
		endif
	endif
	ifeq ($(CONFIG_RTL_ODM_WLAN_DRIVER),y)
		$(MODULE_NAME)-objs += \
        OUTSRC/rtl8188e/HalHWImg8188E_BB.o\
        OUTSRC/rtl8188e/HalHWImg8188E_MAC.o\
        OUTSRC/rtl8188e/HalHWImg8188E_RF.o\
        OUTSRC/rtl8188e/odm_RegConfig8188E.o\
        OUTSRC/rtl8188e/Hal8188ERateAdaptive.o\
        OUTSRC/rtl8188e/odm_RTL8188E.o\
        OUTSRC/rtl8188e/HalPhyRf_8188e.o			
	else
		$(MODULE_NAME)-objs += Hal8192CDMOutSrc.o \
		$(MODULE_NAME)-objs += RateAdaptive.o 
	endif
endif
ifeq ($(CONFIG_RTL_8812_SUPPORT),y)
       $(MODULE_NAME)-objs += HalPwrSeqCmd.o\
        Hal8812PwrSeq.o\
        8812_hw.o
	ifeq ($(CONFIG_RTL_ODM_WLAN_DRIVER),y)
	     $(MODULE_NAME)-objs += ./OUTSRC/rtl8812a/HalPhyRf_8812A.o
	endif
endif
ifeq ($(CONFIG_RTL_92C_SUPPORT),y) 
	$(MODULE_NAME)-objs += Hal8192CDMOutSrc.o
endif
ifeq ($(CONFIG_RTL_92D_SUPPORT),y)
        $(MODULE_NAME)-objs += Hal8192CDMOutSrc.o
endif


ifeq ($(CONFIG_RTL_ODM_WLAN_DRIVER),y)
	$(MODULE_NAME)-objs += \
	OUTSRC/odm.o \
	OUTSRC/odm_DIG.o\
	OUTSRC/odm_EdcaTurboCheck.o\
	OUTSRC/odm_AntDiv.o\
	OUTSRC/odm_DynamicBBPowerSaving.o\
	OUTSRC/odm_PathDiv.o\
	OUTSRC/odm_RaInfo.o\
	OUTSRC/odm_DynamicTxPower.o\
	OUTSRC/odm_PowerTracking.o\
	OUTSRC/PhyDM_Adaptivity.o\
	OUTSRC/odm_debug.o\
	OUTSRC/odm_interface.o\
	OUTSRC/odm_HWConfig.o\
	OUTSRC/HalPhyRf.o\
	OUTSRC/odm_CfoTracking.o\
	OUTSRC/odm_ACS.o
	EXTRA_CFLAGS += -I$(src) -I$(src)/OUTSRC
endif

ifeq ($(CONFIG_WLAN_HAL_8881A),y)
	$(MODULE_NAME)-objs += OUTSRC/rtl8821a/PhyDM_IQK_8821A.o
endif

ifeq ($(CONFIG_WLAN_HAL_8192EE),y)
	$(MODULE_NAME)-objs += \
	OUTSRC/rtl8192e/HalPhyRf_8192e.o\
	OUTSRC/rtl8192e/odm_RTL8192E.o
endif
ifeq ($(CONFIG_WLAN_HAL_8814AE),y)
	$(MODULE_NAME)-objs += OUTSRC/rtl8814a/HalPhyRf_8814A.o
	$(MODULE_NAME)-objs += OUTSRC/rtl8814a/PhyDM_IQK_8814A.o
	ifeq ($(CONFIG_RTL_ODM_WLAN_DRIVER),y)
		$(MODULE_NAME)-objs += \
        OUTSRC/rtl8814a/HalHWImg8814A_BB.o\
        OUTSRC/rtl8814a/HalHWImg8814A_MAC.o\
        OUTSRC/rtl8814a/HalHWImg8814A_RF.o\
		OUTSRC/rtl8814a/HalHWImg8814A_TestChip_BB.o\
		OUTSRC/rtl8814a/HalHWImg8814A_TestChip_MAC.o\
		OUTSRC/rtl8814a/HalHWImg8814A_TestChip_RF.o\
        OUTSRC/rtl8814a/odm_RegConfig8814A.o		
	endif
endif
$(MODULE_NAME)-objs +=\
	8192cd_tx.o\
	8192cd_rx.o\
	8192cd_osdep.o\
	8192cd_sme.o\
	8192cd_util.o\
	8192d_hw.o\
	8192cd_hw.o\
	8192cd_security.o\
	8192cd_tkip.o\
	8192cd_aes.o\
	8192cd_proc.o\
	8192cd_br_ext.o\
	8192cd_eeprom.o\
	8192cd_mp.o\
	8192cd_psk.o\
	8192cd_ioctl.o\
	1x_kmsm_aes.o\
	1x_kmsm_hmac.o\
	1x_md5c.o\
	1x_rc4.o\
	8192cd_mib.o\
	8192cd_dmem.o\
	8192cd_host.o\
	8192cd_led.o\
	8192cd_dfs.o\
	8192cd_dfs_det.o\
	8812_vht_gen.o\
	romeperf.o\
	HalDMOutSrc.o\
	Beamforming.o\
	8192cd_11h.o\
	$(obj-mesh)
	
ifeq ($(CONFIG_RTL_SIMPLE_CONFIG),y)
$(MODULE_NAME)-objs += 8192cd_profile.o
endif	
ifeq ($(CONFIG_RTL_COMAPI_CFGFILE),y)
$(MODULE_NAME)-objs += 8192cd_comapi.o
endif
ifeq ($(CONFIG_RTL_P2P_SUPPORT),y)	
$(MODULE_NAME)-objs += 8192cd_p2p.o
endif
ifeq ($(CONFIG_RTL_COMAPI_WLTOOLS),y)
$(MODULE_NAME)-objs += 8192cd_comapi.o
endif
ifeq ($(CONFIG_PACP_SUPPORT),y)
$(MODULE_NAME)-objs += 8192cd_comapi.o
endif	
ifeq ($(CONFIG_WPA_CLI),y)
$(MODULE_NAME)-objs += 8192cd_comapi.o	
endif

#For Hostapd
ifeq ($(CONFIG_RTL_HOSTAPD_SUPPORT),y)
$(MODULE_NAME)-objs += 8192cd_comapi.o
endif
ifeq ($(CONFIG_RTL8672),y)
$(MODULE_NAME)-objs += rtl8672_port.o
endif

$(MODULE_NAME)-objs += 8192cd_net80211.o
$(MODULE_NAME)-objs += 8192cd_psk_hapd.o
$(MODULE_NAME)-objs += 8192cd_cfg80211.o

ifeq ($(CONFIG_WLAN_HAL),y)
	EXTRA_CFLAGS += -I$(src) -I$(src)/WlanHAL/ -I$(src)/WlanHAL/Include -I$(src)/WlanHAL/HalHeader
	$(MODULE_NAME)-objs += WlanHAL/HalCommon.o \
										WlanHAL/HalCfg.o \
										WlanHAL/HalDbgCmd.o									
endif

ifeq ($(CONFIG_WLAN_HAL_88XX),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX
	$(MODULE_NAME)-objs += WlanHAL/RTL88XX/Hal88XXFirmware.o \
										WlanHAL/RTL88XX/Hal88XXGen.o \
										WlanHAL/RTL88XX/Hal88XXIsr.o	\
										WlanHAL/RTL88XX/Hal88XXPwrSeqCmd.o \
										WlanHAL/RTL88XX/Hal88XXRxDesc.o \
										WlanHAL/RTL88XX/Hal88XXTxDesc.o \
										WlanHAL/RTL88XX/Hal88XXPhyCfg.o
endif

ifeq ($(CONFIG_WLAN_HAL_8881A),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8881A
	$(MODULE_NAME)-objs += WlanHAL/RTL88XX/RTL8881A/Hal8881AFirmware.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881AGen.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881AHWImg.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881AIsr.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881APwrSeqCmd.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881ARxDesc.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881ATxDesc.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881AVerify.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881APhyCfg.o \
										8812_hw.o									
endif

ifeq ($(CONFIG_WLAN_HAL_8192EE),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8192E/RTL8192EE -I$(src)/WlanHAL/RTL88XX/RTL8192E
	$(MODULE_NAME)-objs += WlanHAL/RTL88XX/RTL8192E/Hal8192EGen.o \
			  WlanHAL/RTL88XX/RTL8192E/Hal8192EPhyCfg.o \
			  WlanHAL/RTL88XX/RTL8192E/Hal8192EPwrSeqCmd.o
	ifeq ($(CONFIG_PCI_HCI), y)
		$(MODULE_NAME)-objs += WlanHAL/RTL88XX/RTL8192E/RTL8192EE/Hal8192EEGen.o
	endif
	ifeq ($(CONFIG_SDIO_HCI), y)
		$(MODULE_NAME)-objs += WlanHAL/RTL88XX/RTL8192E/RTL8192ES/Hal8192ESGen.o
	endif
endif

ifeq ($(CONFIG_WLAN_HAL_8814AE),y)
		EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8814A/RTL8814AE -I$(src)/WlanHAL/RTL88XX/RTL8814A
		$(MODULE_NAME)-objs += WlanHAL/RTL88XX/RTL8814A/RTL8814AE/Hal8814AEGen.o \
				  WlanHAL/RTL88XX/RTL8814A/Hal8814AGen.o \
				  WlanHAL/RTL88XX/RTL8814A/Hal8814APhyCfg.o \
				  WlanHAL/RTL88XX/RTL8814A/Hal8814APwrSeqCmd.o	
endif

ifeq ($(CONFIG_RTL8672),y)
EXTRA_CFLAGS += -DCONFIG_RTL8196B -DCONFIG_RTL8196C -DCONFIG_RTL8196B_GW -DCONFIG_RTL8196C_TESTCHIP_PATCH -D_MP_TELNET_SUPPORT_
EXTRA_CFLAGS += -DCONFIG_COMPAT_NET_DEV_OPS
endif

#CONFIG_SINUX_SUPPORT=1
ifeq ($(CONFIG_SINUX_SUPPORT),1)
EXTRA_CFLAGS  += -DOPENSSL_FIPS -D__linux__ -DRSDK_BUILT -DOPENSSL_NO_SPEED -DOPENSSL_THREADS -D_REENTRANT \
                        -DDSO_DLFCN -DHAVE_DLFCN_H -DOPENSSL_NO_KRB5 -DB_ENDIAN -DTERMIO \
			-fomit-frame-pointer 
# -save-temps                      
                        
$(MODULE_NAME)-objs  += wps/ssl/mem.o wps/ssl/mem_clr.o wps/ssl/bn_add.o wps/ssl/bn_lib.o \
			wps/ssl/bn_asm.o wps/ssl/bn_const.o wps/ssl/bn_ctx.o \
                wps/ssl/bn_div.o wps/ssl/bn_exp.o wps/ssl/bn_gcd.o \
				wps/ssl/bn_mod.o wps/ssl/bn_mont.o \
                wps/ssl/bn_mul.o wps/ssl/bn_prime.o wps/ssl/bn_rand.o \
				wps/ssl/bn_recp.o wps/ssl/bn_shift.o wps/ssl/bn_sqr.o \
                wps/ssl/bn_word.o wps/ssl/dh_check.o wps/ssl/dh_gen.o \
				wps/ssl/dh_key.o wps/ssl/dh_lib.o wps/ssl/digest.o \
                wps/ssl/m_sha1.o wps/ssl/hmac.o wps/ssl/md_rand.o \
				wps/ssl/rand_lib.o wps/ssl/sha1dgst.o wps/ssl/sha256.o \
                wps/ssl/aes_cbc.o wps/ssl/fips_aes_core.o
				
#EXTRA_CFLAGS += -I../../../lib
$(MODULE_NAME)-objs += wps/8192cd_wscd.o
$(MODULE_NAME)-objs += wps/sercomm_intf.o
endif

ifneq ($(origin DIR_BOARD), undefined)
EXTRA_CFLAGS += -I$(DIR_BOARD) 
#EXTRA_CFLAGS += -I$(DIR_BOARD) -Werror
#EXTRA_CFLAGS += -I$(DIR_LINUX)/drivers/net/rtl819x/
endif

###############################################################################

#EXTRA_CFLAGS += -D"KBUILD_MODNAME=KBUILD_STR(rtl8192cd)" -DMODULE -D__KERNEL__
.PHONY: all debug clean install install_dev install_target FORCE

all: $(TARGET)

$(TARGET): FORCE
	@echo '--- build the $(TARGET) module'
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC)  M=$(PWD) modules 
	$(CROSS_COMPILE)strip --strip-debug $(TARGET)
	#cp $(TARGET) $(RTK_WIFI_LIB)/modules
	#mv Module.symvers rtl8192cd.symvers

debug: EXTRA_CFLAGS +=-DDEBUG -g -O0
debug: all

clean: 
#	@if [ -e $(KSRC) ] ; then \
#                make -C $(KSRC) M=$(PWD) MODFLAGS="$(EXTRA_CFLAGS)" clean; \
#        fi
	find . -name ".*.cmd" | xargs rm -f
	rm -rf Module.symvers rtl8192cd.symvers
	find . -name "*.o" | xargs rm -f
	rm -f *.mod.c
	rm -f modules.order
	rm -f *.ko
	rm -rf .tmp_versions
	rm -f data_*.c

release_clean: clean
	rm -rf usb/
	rm -rf sdio/8189es
	rm -rf data/
	rm -rf data_88e/
	rm -rf data_92d/
	rm -rf data_8812/
	rm -rf OUTSRC/rtl8188e
	rm -rf OUTSRC/rtl8812a
	rm -rf OUTSRC/rtl8821a
	rm -rf WlanHAL/Data/8814A
	rm -rf WlanHAL/Data/8881A
	rm -rf WlanHAL/RTL88XX/RTL8192E/RTL8192EE
	rm -rf WlanHAL/RTL88XX/RTL8814A
	rm -rf WlanHAL/RTL88XX/RTL8881A
	rm -f 8188e_hw.c
	rm -f Hal8188E*
	rm -f Hal8812*
	rm -f Hal8821A*
	rm -f wapi*
	rm -f intf_xmit.c
	rm -f HalPwrSeqCmd.c
	rm -f svn_mapping.xlsx
	rm -f RateAdaptive.c
	find ./ -name ".svn" | xargs rm -Rf	
	
install: install_dev install_target

install_dev:

install_target:
	#mkdir -p $(FSROOT)/lib/modules
	#cp -pd $(TARGET) $(FSROOT)/lib/modules
	#mkdir -p $(BUILD_DEST)/kernel/module.symvers/
	#cp -pd rtl8192cd.symvers $(BUILD_DEST)/kernel/module.symvers/rtl8192cd

#
#Build C code for TXT or BIN file
#
$(obj)/8192cd_hw.o :  $(SRCS_TXT) $(SRCS_BIN) $(SRCS_BIN_U) $(TXPWR_HEADER)

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR)/%.txt FORCE
	rm -f $(obj)/TXPWR_92C.h
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR)/%.bin FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR)/%.BIN FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_D)/%.txt FORCE
	rm -f $(obj)/TXPWR_92D.h
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_D)/%.bin FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_D)/%.BIN FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@	
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_E)/%.txt FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_E)/%.bin FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_E)/%.BIN FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@	
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8812)/%.txt FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8812)/%.bin FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8812)/%.BIN FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@	

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_92E)/%.txt FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_92E)/%.bin FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_92E)/%.BIN FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
	
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8814)/%.txt FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8814)/%.bin FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8814)/%.BIN FORCE
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@

$(obj)/TXPWR_92C.h: $(TXPWR_LMT_TXT_92C) FORCE 		 
	@echo $(TXPWR_LMT_FNAME_92C) > $(obj)/tmp_TXPWR
	@perl -f $(obj)/tplmt2h.pl < $(obj)/tmp_TXPWR > $@
	rm -f $(obj)/tmp_TXPWR
$(obj)/TXPWR_92D.h: $(TXPWR_LMT_TXT_92D) FORCE 		 
	@echo $(TXPWR_LMT_FNAME_92D) > $(obj)/tmp_TXPWR
	@perl -f $(obj)/tplmt2h.pl < $(obj)/tmp_TXPWR > $@
	rm -f $(obj)/tmp_TXPWR	
