ifeq ($(CONFIG_COMIP_PROJECT_FOURMODE)$(CONFIG_COMIP_PROJECT_FIVEMODE)$(CONFIG_COMIP_BOARD_LC1860_ZEUS),y)
zreladdr-y      := 0x07808000
params_phys-y   := 0x07800100
initrd_phys-y   := 0x08800000
else
zreladdr-y	:= 0x06408000
params_phys-y	:= 0x06400100
initrd_phys-y	:= 0x07400000
endif

