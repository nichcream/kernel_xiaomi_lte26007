
# for SDIO interrupts, remove macro GPIO_BASED_IRQ

# using GPIO based interrupts
EXTRA_CFLAGS += -DMOP_WORKAROUND -DPIGGYBACK -DGPIO_BASED_IRQ -DCONFIG_CW1200_USE_STE_EXTENSIONS

#etf_driver-objs := ./sh_ta.o ./sdio_wrapper.o ./sh_adap.o ./sbus.o ./fwio.o ./hwio.o
#obj-m += etf_driver.o
etf_driver-y := sh_ta.o sdio_wrapper.o sh_adap.o sbus.o fwio.o hwio.o
obj-m += etf_driver.o

