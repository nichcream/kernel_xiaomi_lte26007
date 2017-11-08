/* 
 *linux/arch/arm/mach-181x/headsmp-lc1813.S
 *
 */

#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/cp15.h>
#include <asm/cache.h>
#include <linux/irqchip/arm-gic.h>

#include <mach/suspend.h>
#include <plat/hardware.h>
#include <mach/iomap.h>
#include <mach/io.h>

#include "power-macros.S"

	.text

/*
 *	void comip1813_resume(void)
 */
	.align L1_CACHE_SHIFT
ENTRY(comip1813_resume)
	msr	cpsr_fsxc, #(PSR_F_BIT | PSR_I_BIT | SVC_MODE)
	bl	cpu_state_invalidate
	bl	sram_cpu_pre
	ldr	r3, comip_cpu_resume_phys
	bx	r3
ENDPROC(comip1813_resume)

/*
 *	void comip1813_continue(void)
 */
ENTRY(comip1813_continue)
	msr	cpsr_fsxc, #(PSR_F_BIT | PSR_I_BIT | SVC_MODE)
	bl	cpu_state_invalidate
	ldr	r3, comip_cpu_resume_phys
	bx	r3
ENDPROC(comip1813_continue)
	.align L1_CACHE_SHIFT
 ENTRY(comip_cpu_resume_phys)
	.long	cpu_resume	@FIXED to PA

/*
 *	void comip_secondary_startup(void)
 */
	.align L1_CACHE_SHIFT
ENTRY(comip_secondary_startup)
	msr	cpsr_fsxc, #(PSR_F_BIT | PSR_I_BIT | SVC_MODE)
	bl	cpu_state_invalidate

	cpu_id	r0
	mov	r1, #COMIP_SLP_STATE_UPPING
	ldr	r4, =CPUX_SLP_STATE_BASE
	add	r4, r4, r0
	strb	r1, [r4]
	dsb

	lsl	r2, r0, #COMIP_WAKEUP_ID_OFFSET
	orr	r2, r2, #COMIP_WAKEUP_CMD_WAKING
	ldr	r3, =CPU0_WAKEUP_CMD_ADDR
1:
	ldr	r1, [r3]
	teq	r1, r2
	beq	2f
	wfe
	b	1b
2:
	mov	r1, #COMIP_SLP_STATE_UP
	strb	r1, [r4]
	dsb

	ldr	r3, secondary_startup_addr
	bx	r3
ENDPROC(comip_secondary_startup)
ENTRY(secondary_startup_addr)
	.long	secondary_startup	@FIXED to PA

cpu_state_invalidate:
	clrex				@clear L1cache local exclusive flag
	mrc	p15, 0, r3, c1, c0, 1
	bic	r3, r3, #(1 << 6)
	mcr	p15, 0, r3, c1, c0, 1	@ disable SMP
	isb
	mov	r3, #0
	mcr	p15, 0, r3, c7, c5, 0	@ invalidate BTAC, i-cache
	mcr	p15, 0, r3, c7, c5, 6	@ invalidate branch pred array
	mcr	p15, 0, r3, c8, c7, 0	@ invalidate unified TLB
	dsb
	isb
	mrc	p15, 0, r3, c1, c0, 0
	orr	r3, r3, #CR_I
	mcr	p15, 0, r3, c1, c0, 0	@ enable i-cache
	dsb
	isb
	bx	lr

/*
 *	void sram_cpu_pre(void)
 */
sram_cpu_pre:
#ifdef CONFIG_SLEEP_CODE_IN_RAM
#ifdef CONFIG_DDR_FREQ_ADJUST
	b	ddr_freq_up
#endif
#endif
	bx	lr

/*
 *	void sram_cpu_wfi(unsigned int flag)
 */
ENTRY(sram_cpu_wfi)
	clrex
	mrc	p15, 0, r3, c1, c0, 1
	bic	r3, r3, #(1 << 6)
	mcr	p15, 0, r3, c1, c0, 1
	isb

	dsb
	wfi
	isb

	orr	r3, r3, #(1 << 6)
	mcr	p15, 0, r3, c1, c0, 1
	isb
	bx	lr

	.ltorg

#ifdef CONFIG_SLEEP_CODE_IN_RAM
.macro	DELAY_US, label, val, b=b
	mov	r2, #0
\label:
	add	r2, r2, #1
	cmp	r2, #\val
	bls	\label\b
.endm

/*
 *	comip_ddr2ram(unsigned int flag)
 */
ENTRY(comip_ddr2ram)
#ifdef CONFIG_DDR_FREQ_ADJUST
	b	ddr_freq_down
#endif
	b	sram_cpu_wfi

	.ltorg

#ifdef CONFIG_DDR_FREQ_ADJUST

#define COM_UART_THR	(COM_UART_BASE + 0)
#define COM_UART_DLL	(COM_UART_BASE + 0x00)
#define COM_UART_DLH	(COM_UART_BASE + 0x04)
#define COM_UART_FCR	(COM_UART_BASE + 0x08)
#define COM_UART_TCR	(COM_UART_BASE + 0x0c)
#define COM_UART_TSR	(COM_UART_BASE + 0x14)

#define TO_WAIT_AXI_LOCK	(0x100000)
#define TO_WAIT_DDR_SLEEP	(0x0F0000)
#define TO_WAIT_DLL_LOCK	(0x400000)
#define TO_WAIT_DLL_DELAY	(0x400000)

#define F_ADJ_LIMIT_DOWN	0
#define F_ADJ_LIMIT_UP		0

/*
 *     int ddr_freq_down(void)
 *     return
 *     0: success
 *     1: AXI Lock err
 *     2: DDR Sleep err
 */
ddr_freq_down:
	/* enable  LCDC_AXI_CLK */
	ldr	r3, =AP_PWR_LCDCAXICLK_CTL
	movw	r2, #(1 | 0 << 4 | 1 << 8)
	orr	r2, r2, #(7 << 16)
	str	r2, [r3]
	dsb

	/* clear ddr_pwr */
	ldr	r3, =DDR_PWR_INTR_CLR
	mov	r2, #(1 << 9) @FREQ_ADJ
	str	r2, [r3]
	dsb

	/* step2: enable memctl dfs */
	ldr	r3, =DDR_PWR_MEMCTL_FREQ_ADJ
	mov	r2, #(1 << 0 | F_ADJ_LIMIT_DOWN << 4)
	orr	r2, r2, #(1 << 16 | 1 << 18)
	str	r2, [r3]
	dsb

	/* step3: wait intr for axi(0/2/3) lock*/
	mov	r2, #TO_WAIT_AXI_LOCK
	ldr	r3, =DDR_PWR_INTR_RAW
	mov	r0, #1
1:
	subs	r2, r2,#0x1
	bcc	4f
	ldr	r1, [r3]
	tst	r1, #(1 << 9)
	beq	1b

	/* step4: disable auto dll bypass */
	ldr	r3, =DDR_PWR_MEMCTL_LPCTL2
	ldr	r2, [r3]
	bic	r2, r2, #(3 << 12)
	orr	r2, r2, #(1 << 12) @CFG_BYPASS_MODE:bps200_en
	str	r2, [r3]

	/* step5: bypass 200 */
	ldr	r3, =PHY_DDR_DLLGCR
	ldr	r2, [r3]
	orr	r2, r2, #(1 << 23) @DLL-Bypass-200 enable
	str	r2, [r3]
	dsb

	/* step6: ddr access state*/
	ldr	r3, secondary_startup_addr
	ldr	r3, [r3]

	/* step7: ddr sleep state */
	ldr	r3, =CTRL_DDR_SCTL
	mov	r2, #0x3
	str	r2, [r3]
	dsb

	/* wait for ddr enter sleep */
	mov	r2, #TO_WAIT_DDR_SLEEP
	ldr	r3, =CTRL_DDR_STAT
	mov	r0, #2
2:
	subs	r2, r2,#0x1
	bcc	3f
	ldr	r1, [r3]
	and	r1, r1, #0x07
	teq	r1, #0x5
	bne	2b

	/* step8: close ddr ctl&phy clock */
	ldr	r3, =DDR_PWR_CLKEN1
	mov	r2, #(0 << 0 | 3 << 16)
	str	r2, [r3]
	dsb

	/* step9: */
	ldr	r3, =DDR_PWR_DDRCLKCTL
	adr	r1, DDR_CLK_DIV_VAL
	ldr	r2, [r3]
	str	r2, [r1]
	mov	r2, #0x3 @DDR_CLK_DIV
	str	r2, [r3]
	dsb

	/* step10: dll bypass */
	ldr	r3, =PHY_DDR_PIR
	ldr	r2, [r3]
	orr	r2, r2, #(1 << 17) @DLLBYP
	str	r2, [r3]
	dsb

	/* step11: open ddr ctl&phy clock */
	ldr	r3, =DDR_PWR_CLKEN1
	mov	r2, #(3 << 0)
	orr	r2, r2, #(3 << 16)
	str	r2, [r3]
	dsb

	/* step12: */
	ldr	r3, =CTRL_DDR_TOGCNT1U
	adr	r1, DDR_TOGCNT1U_VAL
	ldr	r2, [r3]
	str	r2, [r1]
	mov	r2, #195
	str	r2, [r3]

	ldr	r3, =CTRL_DDR_TOGCNT100N
	adr	r1, DDR_TOGCNT100N_VAL
	ldr	r2, [r3]
	str	r2, [r1]
	mov	r2, #20
	str	r2, [r3]
	dsb

	/*return value*/
	mov	r0, #0
3:
	/*step13:exit sleep mode*/
	ldr	r3, =CTRL_DDR_SCTL
	mov	r2, #4
	str	r2, [r3]
	dsb

	/* clear intr */
	ldr	r3, =DDR_PWR_INTR_CLR
	mov	r2, #(1<<9)
	str	r2, [r3]
	dsb

	/*~ 1us*/
	DELAY_US 91, 300

	/*step14: dis memctl freq adj */
4:
	ldr	r3, =DDR_PWR_MEMCTL_FREQ_ADJ
	mov	r2, #(0 << 0 | 1 << 16)
	str	r2, [r3]
	dsb

	/* disable  LCDC_AXI_CLK */
	ldr	r3, =AP_PWR_LCDCAXICLK_CTL
	mov	r2, #(0 << 8)
	orr	r2, r2, #(1 << 18)
	str	r2, [r3]
	dsb
	mov	r2, #(0 << 0)
	orr	r2, r2, #(1 << 16)
	str	r2, [r3]
	dsb

	adr	r3, ddr_freq_down_ret
	str	r0, [r3]
	dsb
	b	sram_cpu_wfi

/*
 *     int ddr_freq_up(void)
 *     return
 *     0: success
 *     1: AXI Lock err
 *     2: DDR Sleep err
 *     3: DLL Lock err
 */

ddr_freq_up:
	/*ddr_freq_down is success?*/
	ldr	r3, ddr_freq_down_ret
	cmp	r3, #0
	bxne	lr

	/* enable  LCDC_AXI_CLK */
	ldr	r3, =AP_PWR_LCDCAXICLK_CTL
	movw	r2, #(1 | 0 << 4 | 1 << 8)
	orr	r2, r2, #(7 << 16)
	str	r2, [r3]
	dsb

	/* ddr access state*/
	ldr	r3, secondary_startup_addr
	ldr	r3, [r3]

	/* step1: clear ddr_pwr */
	ldr	r3, =DDR_PWR_INTR_CLR
	mov	r2, #(1<<9)
	str	r2, [r3]
	dsb

	/* step2: enable memctl freq adjust */
	ldr	r3, =DDR_PWR_MEMCTL_FREQ_ADJ
	mov	r2, #(1 << 0 | F_ADJ_LIMIT_UP << 4)
	orr	r2, r2, #(1 << 16 | 1 << 18)
	str	r2, [r3]
	dsb

	/* step3: wait intr for axi(0/2/3) lock*/
	mov	r2, #TO_WAIT_AXI_LOCK
	ldr	r3, =DDR_PWR_INTR_RAW
	mov	r0, #1
1:
	subs	r2, r2,#0x1
	bcc	6f
	ldr	r1, [r3]
	tst	r1, #(1 << 9)
	beq	1b

	/* ddr access state*/
	ldr	r3, secondary_startup_addr
	ldr	r3, [r3]

	/* ddr sleep state */
	ldr	r3, =CTRL_DDR_SCTL
	mov	r2, #0x3
	str	r2, [r3]
	dsb

	/* wait for ddr enter sleep */
	mov	r2, #TO_WAIT_DDR_SLEEP
	ldr	r3, =CTRL_DDR_STAT
	mov	r0, #2
2:
	subs	r2, r2,#0x1
	bcc	5f
	ldr	r1, [r3]
	and	r1, r1, #0x07
	teq	r1, #0x5
	bne	2b

	/* step4: close ddr ctl&phy clock */
	ldr	r3, =DDR_PWR_CLKEN1
	mov	r2, #(0 << 0 | 3 << 16)
	str	r2, [r3]
	dsb

	/* step5: enable dll */
	ldr	r3, =PHY_DDR_PIR
	ldr	r2, [r3]
	bic	r2, r2, #(1 << 17) @DLLBYP
	str	r2, [r3]
	dsb

	/* step7&8: freq 390MHz */
	ldr	r3, =DDR_PWR_DDRCLKCTL
	ldr	r2, DDR_CLK_DIV_VAL @DDR_CLK_DIV
	str	r2, [r3]
	dsb

	ldr	r3, =CTRL_DDR_TOGCNT1U
	ldr	r2, DDR_TOGCNT1U_VAL
	str	r2, [r3]
	dsb

	ldr	r3, =CTRL_DDR_TOGCNT100N
	ldr	r2, DDR_TOGCNT100N_VAL
	str	r2, [r3]
	dsb

	/* step9: relock dll */
	ldr	r3, =PHY_DDR_PIR
	mov	r2, #(1 << 18)
	orr	r2, r2, #0x07
	str	r2, [r3]
	dsb

	/*~ 4us*/
	DELAY_US 90, 1200

	/* wait for dll lock */
	mov	r2, #TO_WAIT_DLL_LOCK
	ldr	r3, =PHY_DDR_PGSR
	mov	r0, #3
4:
	subs	r2, r2,#0x1
	bcc	dll_lock_err
	ldr	r1, [r3]
	tst	r1, #0x2
	beq	4b

	/* step10: enbale ddr ctl&phy clock */
	ldr	r3, =DDR_PWR_CLKEN1
	mov	r2, #0x30000
	orr	r2, r2, #3
	str	r2, [r3]
	dsb

	/*return value*/
	mov	r0, #0
5:
	/* step11: exit sleep mode */
	ldr	r3, =CTRL_DDR_SCTL
	mov	r2, #4
	str	r2, [r3]
	dsb

	/* clear intr */
	ldr	r3, =DDR_PWR_INTR_CLR
	mov	r2, #(1<<9)
	str	r2, [r3]
	dsb

	/*~ 1us*/
	DELAY_US 91, 300

	/* dis memctl freq adj */
6:
	ldr	r3, =DDR_PWR_MEMCTL_FREQ_ADJ
	mov	r2, #(0 << 0 | 1 << 16)
	str	r2, [r3]
	dsb

	/* disable  LCDC_AXI_CLK */
	ldr	r3, =AP_PWR_LCDCAXICLK_CTL
	mov	r2, #(0 << 8)
	orr	r2, r2, #(1 << 18)
	str	r2, [r3]
	dsb
	mov	r2, #(0 << 0)
	orr	r2, r2, #(1 << 16)
	str	r2, [r3]
	dsb

	adr	r3, ddr_freq_up_ret
	str	r0, [r3]
	dsb

	bx	lr

dll_lock_err:
	ldr	r3, =COM_UART_FCR
	mov	r1, #0xb6
	str	r1, [r3]
	dsb
	ldr	r3, =COM_UART_THR
	ldr	r2, =COM_UART_TSR
	mov	r1, #0x45	@ 'E'
dll_lock_loop:
	ldr	r4, [r2]
	tst	r4, #0x20
	beq	dll_lock_loop
	str	r1, [r3]
	dsb

	ldr	r4, =TO_WAIT_DLL_DELAY
dll_delay:
	subs	r4, r4,#0x1
	bgt	dll_delay

	b	dll_lock_loop

DDR_CLK_DIV_VAL:
	.long	1
DDR_TOGCNT1U_VAL:
	.long	390
DDR_TOGCNT100N_VAL:
	.long	39

ENTRY(ddr_freq_down_ret)
	.long	0
ENTRY(ddr_freq_up_ret)
	.long	0

	.ltorg
#endif

ENTRY(comip_ram_code_sz)
	.long	. - comip1813_resume
#endif

	.ltorg


