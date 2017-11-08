#ifndef __ASM_ARCH_COMIP_USB_H
#define __ASM_ARCH_COMIP_USB_H

//bit0: usb0vbus stay up
#define USB_PD_FLAGS_USB0VBUS 	(0x00000001)
#ifdef CONFIG_USB_COMIP_HSIC
/** Enumeration for COMIP HW tyep */
typedef enum _usb_core_type{
       OTG_HW = 0,             /*OTG host*/
       HOST_HW = 1,    /*only host*/
       HSIC_HW = 2             /*HSIC host*/
}usb_core_type;
#endif


struct comip_udc_platform_data {
	unsigned int flag;		//bit0: usb0vbus
	unsigned int reserved;
};

struct comip_hcd_platform_data {
	int (*usb_power_set)(int onoff);
	unsigned int hub_power;
	unsigned int hub_reset;
	unsigned int eth_power;
	unsigned int eth_reset;
	unsigned int reserved;
};

struct comip_otg_platform_data {
	unsigned int id_gpio;
	unsigned int debounce_interval;
	int (*power)(int);
};

struct comip_usb_platform_data {
	struct comip_udc_platform_data udc;
	struct comip_hcd_platform_data hcd;
	struct comip_otg_platform_data otg;
};

extern void comip_set_usb_info(struct comip_usb_platform_data *info);

#ifdef CONFIG_USB_COMIP_OTG
extern int comip_otg_hostmode(void);
#else
static inline int comip_otg_hostmode(void)
{
	return 0;
}
#endif

extern int comip_usb_power_set(int onoff);

#endif	/*__ASM_ARCH_COMIP_USB_H */
