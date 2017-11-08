ifeq ($(CONFIG_VIDEO_COMIP_STUB),y)
obj-$(CONFIG_VIDEO_COMIP) += comip-videostub.o comip-videobuf.o
else
obj-$(CONFIG_VIDEO_COMIP) += comip-video.o comip-videobuf.o \
	comip-aecgc-ctrl.o comip-aecgc-samsung.o comip-aecgc-galaxycore.o comip-aecgc-byd.o
obj-$(CONFIG_VIDEO_COMIP_DEBUG) += camera-debug.o
obj-$(CONFIG_VIDEO_COMIP_ISP) += comip-isp.o isp-ctrl.o isp-i2c.o
obj-$(CONFIG_VIDEO_COMIP_ISP2) += comip-isp2.o isp2-ctrl.o isp-i2c.o
obj-$(CONFIG_VIDEO_COMIP_CSI) += comip-csi.o
obj-$(CONFIG_VIDEO_COMIP_CSI2) += comip-csi2.o
endif
obj-y += sensors/
