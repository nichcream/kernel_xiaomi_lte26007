#
# Makefile for ST-Ericsson CG2900 connectivity combo controller
#

ccflags-$(CONFIG_CG2900) :=	-Iarch/arm/plat-lc/drivers/wireless/cg2900/include \
				-Iarch/arm/mach-comip 

obj-$(CONFIG_CG2900)	+= 	devices-cg2900.o \
			board_non_ste.o \
			board-ux500-cg2900.o \
			clock-cg2900.o 

obj-$(CONFIG_CG2900)		+= mfd/
obj-$(CONFIG_CG2900)		+= bluetooth/
