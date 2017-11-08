#
# Makefile for the linux kernel.
#

# Object file lists.

# Base
obj-y := generic.o devices.o
obj-y += cache/
obj-y += timer/
obj-y += drivers/mfp/
obj-y += sleep/
obj-y += clock/
obj-y += irq/
obj-y += cpufreq/
obj-$(CONFIG_SMP) += platsmp/

# Platform Driver
obj-y += drivers/multicore/
obj-$(CONFIG_WATCHDOG_COMIP) += drivers/wdt/
obj-$(CONFIG_TPZ_COMIP) += drivers/tpz/
obj-$(CONFIG_USB_COMIP) += drivers/usb/

# Modem & Bridge
obj-y += drivers/modem/

# Board
obj-y += board/

#sound
obj-y += sound/
