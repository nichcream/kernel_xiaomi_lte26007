#ifndef _COMIP_SMMU_H
#define _COMIP_SMMU_H

#include <linux/device.h>

struct comip_smmu_data{
	unsigned int hw_id;
	unsigned int stream_id;
};

#if defined(CONFIG_COMIP_IOMMU)
extern int ion_iommu_attach_dev(struct device *dev);
extern int ion_iommu_detach_dev(struct device *dev);
#else
static inline int ion_iommu_attach_dev(struct device *dev) {return 0;}
static inline int ion_iommu_detach_dev(struct device *dev) {return 0;}
#endif

#endif
