#
# Makefile for ST-Ericsson CG2900 connectivity combo controller
#

ccflags-y :=					\
	-Idrivers/staging/cg2900/include \
	-Idrivers/staging/cg2900 \
	-Iarch/arm/mach-comip

obj-y	+= cg2900_core.o cg2900_lib.o
export-objs			:= cg2900_core.o cg2900_lib.o

obj-y	+= cg2900_char_devices.o

obj-y	+= cg2900_test.o

obj-y	+= cg2900_chip.o
#obj-$(CONFIG_STLC2690_CHIP)	+= stlc2690_chip.o

obj-y	+= cg2900_audio.o
