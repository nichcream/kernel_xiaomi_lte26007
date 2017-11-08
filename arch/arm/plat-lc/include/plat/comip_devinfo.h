#ifndef __COMIP_DEVINFO_H
#define __COMIP_DEVINFO_H

#if defined(CONFIG_DEVINFO_COMIP)
extern int comip_devinfo_register(struct attribute **attributes, int count);
#else
static inline int comip_devinfo_register(struct attribute **attributes, int count)
{
	return 0;
}
#endif

#endif /* __COMIP_DEVINFO_H */
