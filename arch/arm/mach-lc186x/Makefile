#
# Makefile for the linux kernel.
#

# Object file lists.

# Base
obj-y := generic.o devices.o
obj-y += timer/
obj-y += sleep/
obj-y += clock/
obj-y += irq/
obj-y += cpufreq/

# Platform Driver
obj-$(CONFIG_TPZ_COMIP) += drivers/tpz/
# Modem & Bridge
obj-y += drivers/modem/

# Device Driver
obj-$(CONFIG_WATCHDOG_COMIP) += drivers/wdt/

obj-$(CONFIG_SMP) += platsmp/
obj-y += drivers/usb/

# Board
obj-y += board/

# Sound
obj-y += sound/

#smmu
obj-y += drivers/smmu/

#flowcal
obj-y += drivers/flowcal/
