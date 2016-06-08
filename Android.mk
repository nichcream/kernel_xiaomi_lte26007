# Copyright (c) 2010 Wind River Systems, Inc.
#
# The right to copy, distribute, modify, or otherwise make use
# of this software may be licensed only pursuant to the terms
# of an applicable Wind River license agreement.


ifeq ($(TARGET_KERNEL_DIR),kernel/linux-3.10)
LOCAL_PATH := $(call my-dir)
KERNEL_MODULES_OUT := $(TARGET_OUT)/lib/modules
KERNEL_CFLAGS :=
ifeq ($(BUILD_MASS_PRODUCTION),true)
	KERNEL_CFLAGS += KCFLAGS=-DBUILD_MASS_PRODUCTION
endif
kernel_MAKE_CMD := make -C $(LOCAL_PATH) ARCH=$(TARGET_ARCH) CROSS_COMPILE=$(shell pwd)/$(CROSS_COMPILE) $(KERNEL_CFLAGS)
kernel_RM_CMD := rm -f $(LOCAL_PATH)/arch/arm/boot/zImage

define mv-modules
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`;\
ko=`find $$mpath/kernel -type f -name *.ko`;\
for i in $$ko; do mv $$i $(KERNEL_MODULES_OUT)/; done;\
fi
endef

define clean-module-folder
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`; rm -rf $$mpath;\
fi
endef

.PHONY: zImage.clean
zImage.clean:
	$(hide) echo "rm arch/arm/boot/zImage"
	$(hide) $(kernel_RM_CMD)
        
.PHONY: kernel.clean
kernel.clean:
	$(hide) echo "Cleaning kernel.."
	$(hide) $(kernel_MAKE_CMD) clean
        
.PHONY: kernel.distclean
kernel.distclean:
	$(hide) echo "Distcleaning kernel.."
	$(hide) $(kernel_MAKE_CMD) distclean
        
$(LOCAL_PATH)/.config: $(LOCAL_PATH)/arch/$(TARGET_ARCH)/configs/$(TARGET_KERNEL_CONFIG)
	$(hide) echo "Configuring kernel with $(TARGET_KERNEL_CONFIG).."
	$(hide) $(kernel_MAKE_CMD) $(TARGET_KERNEL_CONFIG)
        
.PHONY: kernel.config
kernel.config: $(LOCAL_PATH)/.config

$(LOCAL_PATH)/arch/arm/boot/zImage:zImage.clean $(LOCAL_PATH)/.config
	$(hide) echo "Building kernel.."
	$(hide) $(kernel_MAKE_CMD) zImage
	$(hide) echo "Building kernel modules.."
	$(hide) $(kernel_MAKE_CMD) modules
	$(hide) echo "Install kernel modules to system/lib/modules ..."
	$(hide) $(kernel_MAKE_CMD) INSTALL_MOD_PATH=../../$(TARGET_OUT) modules_install
	$(hide) $(mv-modules)
	$(hide) $(clean-module-folder)

.PHONY: kernel.image
kernel.image: $(LOCAL_PATH)/arch/arm/boot/zImage

$(LOCAL_PATH)/modules.order: $(LOCAL_PATH)/.config kernel.image
	$(hide) echo "Building kernel modules.."
	$(hide) $(kernel_MAKE_CMD) modules
	$(hide) $(kernel_MAKE_CMD) INSTALL_MOD_PATH=../../$(TARGET_OUT) modules_install
	$(mv-modules)
	$(clean-module-folder)

.PHONY: kernel.modules
kernel.modules: $(LOCAL_PATH)/modules.order

.PHONY: kernel
kernel: kernel.image kernel.modules

.PHONY: kernel.rebuild
kernel.rebuild: $(LOCAL_PATH)/.config
	$(hide) echo "Rebuilding kernel.."
	$(hide) $(kernel_MAKE_CMD) zImage
	$(hide) $(kernel_MAKE_CMD) modules

.PHONY: kernel.rebuild_all
kernel.rebuild_all: kernel.clean kernel

$(LOCAL_PATH)/%.ko: kernel.modules;
endif
