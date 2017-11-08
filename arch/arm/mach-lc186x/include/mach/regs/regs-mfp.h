#ifndef __ASM_ARCH_REGS_MFP_H
#define __ASM_ARCH_REGS_MFP_H

#define MFP_REG(x)		(((x) < 256) ? (MUX_PIN_BASE + ((x) * 4)) : (MUX_PIN_BASE + 0x480 + (((x)-256) * 4)))

#define MFP_AF_MASK		(0x3 << 0)
#define MFP_AF(x)		(((x) << 0) & MFP_AF_MASK)

#define MFP_PULL_DOWN_MASK	(0x1 << 2)
#define MFP_PULL_UP_MASK	(0x1 << 3)
#define MFP_PULL_MASK		(0x3 << 2)
#define MFP_PULL(x)		(((x) << 2) & MFP_PULL_MASK)

#define MFP_DS_MASK		(0x7 << 4)
#define MFP_DS(x)		(((x) << 4) & MFP_DS_MASK)

#define MUXPIN_PCM_IO_SEL  (MUX_PIN_BASE + 0x4d8)

#define MUXPIN_PVDD_VOL_CTRL_BASE    (MUX_PIN_BASE + 0x4C0)
#define MUXPIN_PVDD3_VOL_CTRL        (MUX_PIN_BASE + 0x4C0)
#define MUXPIN_PVDD4_VOL_CTRL        (MUX_PIN_BASE + 0x4C4)
#define MUXPIN_PVDD8_VOL_CTRL        (MUX_PIN_BASE + 0x4C8)
#define MUXPIN_PVDD9_VOL_CTRL        (MUX_PIN_BASE + 0x4CC)
#define MUXPIN_PVDD10_VOL_CTRL       (MUX_PIN_BASE + 0x4D0)

#define PVDDx_VOL_CTRL_1_8V    (1800)/*mV*/
#define PVDDx_VOL_CTRL_2_5V    (2500)/*mV*/
#define PVDDx_VOL_CTRL_3_3V    (3300)/*mV*/

#define PVDD4_VOL_CTRL_1_8V    0x0
#define PVDD4_VOL_CTRL_3_3V    0x2
#endif /* __ASM_ARCH_REGS_MFP_H */
