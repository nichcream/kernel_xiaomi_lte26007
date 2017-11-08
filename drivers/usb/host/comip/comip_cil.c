/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/comip_cil.c $
 * $Revision: #182 $
 * $Date: 2011/05/17 $
 * $Change: 1774110 $
 *
 * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================== */

/** @file
 *
 * The Core Interface Layer provides basic services for accessing and
 * managing the COMIP_otg hardware. These services are used by both the
 * Host Controller Driver and the Peripheral Controller Driver.
 *
 * The CIL manages the memory map for the core so that the HCD and PCD
 * don't have to do this separately. It also handles basic tasks like
 * reading/writing the registers and data FIFOs in the controller.
 * Some of the data access functions provide encapsulation of several
 * operations required to perform a task, such as writing multiple
 * registers to start a transfer. Finally, the CIL performs basic
 * services that are not specific to either the host or device modes
 * of operation. These services include management of the OTG Host
 * Negotiation Protocol (HNP) and Session Request Protocol (SRP). A
 * Diagnostic API is also provided to allow testing of the controller
 * hardware.
 *
 * The Core Interface Layer has the following requirements:
 * - Provides basic controller operations.
 * - Minimal use of OS services. 
 * - The OS services used will be abstracted by using inline functions
 *   or macros.
 *
 */

#include "comip_os.h"
#include "comip_otg_regs.h"
#include "comip_cil.h"
#include <linux/slab.h>
#include <plat/hardware.h>
#include <mach/comip-regs.h>
#include <linux/clk.h>
#include <linux/usb/gadget.h>
#include <linux/types.h>
#include <linux/sched.h>

extern int comip_otg_hostmode(void);
static int comip_setup_params(comip_core_if_t * core_if);


static void comip_otg_clk_enable(comip_core_if_t *core_if)
{
	if(!core_if->clk_enabled) {
		clk_enable(core_if->clk);
		core_if->clk_enabled = 1;
	}
}
static void comip_otg_clk_disable(comip_core_if_t *core_if)
{
	if(core_if->clk_enabled) {
		clk_disable(core_if->clk);
		core_if->clk_enabled = 0;
	}
}
/**
 * This function is called to initialize the COMIP_otg CSR data
 * structures. The register addresses in the device and host
 * structures are initialized from the base address supplied by the
 * caller. The calling function must make the OS calls to get the
 * base address of the COMIP_otg controller registers. The core_params
 * argument holds the parameters that specify how the core should be
 * configured.
 *
 * @param reg_base_addr Base address of COMIP_otg core registers
 *
 */
void set_usb_init_reg(comip_core_if_t *core_if)
{
    grstctl_t grstctl = {.d32 = 0 };
    gahbcfg_data_t gahbcfg = {.d32 = 0 };
    gusbcfg_data_t gusbcfg = {.d32 = 0 };
    int val;
    int timeout= 10;

#ifndef CONFIG_USB_COMIP_HSIC
    comip_otg_clk_enable(core_if);
    mdelay(1);

	val = readl((void __iomem *)io_p2v(AP_PWR_PDFSM_ST1));
	printk("LZS %s: %d, AP_PWR_PDFSM_ST1=0x%08X\n", __func__, __LINE__, val);

	/*set USB PHY reset */
	writel(0x01, (void __iomem *)io_p2v(CTL_POR_OTGPHY_CTRL));
	/*disable usb12_clk*/

	comip_otg_clk_disable(core_if);
	/*set USB PHY suspend */
    writel(0x00, (void __iomem *)io_p2v(CTL_OTGPHY_SUSPENDM_CTRL));
    mdelay(10);
	/*set USB PHY no reset */
    writel(0x00, (void __iomem *)io_p2v(CTL_POR_OTGPHY_CTRL));
    comip_otg_clk_enable(core_if);
    mdelay(1);
	/*set USB PHY no suspend */
    writel(0x01, (void __iomem *)io_p2v(CTL_OTGPHY_SUSPENDM_CTRL));
#endif
    mdelay(10);
	/*set USB CORE no reset */
    writel(0x01, (void __iomem *)io_p2v(CTL_OTG_CORE_RST_CTRL));
    mdelay(1);

	/*set USB0_ID detect */
	val = readl((void __iomem *)io_p2v(CTL_OTGPHY_CHARGE_CTRL));
	val |= 0x02;
    writel(val, (void __iomem *)io_p2v(CTL_OTGPHY_CHARGE_CTRL));


	/*Soft reset */
#if 1
	grstctl.d32 = readl(&core_if->core_global_regs->grstctl); //0xA0400010
    grstctl.b.csftrst = 1;
    grstctl.b.hsftrst = 1;
    COMIP_WRITE_REG32(&core_if->core_global_regs->grstctl, grstctl.d32);
    do{
    	mdelay(20);
    	grstctl.d32 = readl(&core_if->core_global_regs->grstctl);
		printk("LZS %s: %d, grstctl=0x%08x\n", __func__, __LINE__, grstctl.d32);
  	}
  	while ( ((!(grstctl.b.ahbidle == 1)) || grstctl.b.csftrst == 1) 
  		&& ((timeout--) > 0) );
#endif

    gahbcfg.d32 = readl(&core_if->core_global_regs->gahbcfg);
    gahbcfg.b.glblintrmsk = 1;
    gahbcfg.b.ptxfemplvl = 1;
    COMIP_WRITE_REG32(&core_if->core_global_regs->gahbcfg, gahbcfg.d32);
    mdelay(10);    
    
    gusbcfg.d32 = readl(&core_if->core_global_regs->gusbcfg);
    gusbcfg.b.toutcal = 5;
    gusbcfg.b.phyif = 1;
    gusbcfg.b.usbtrdtim = 5;
    gusbcfg.b.force_host_mode = 1;
    COMIP_WRITE_REG32(&core_if->core_global_regs->gusbcfg, gusbcfg.d32);
    mdelay(10);
	
}
void comip_cil_uninit(comip_core_if_t *core_if)
{
    gusbcfg_data_t gusbcfg = {.d32 = 0 };
    //hcfg_data_t hcfg = {.d32 = 0};

	/* clear all interrupts, include endpoints and U2DMAs */
	COMIP_WRITE_REG32(&core_if->core_global_regs->gintsts, 0xFFFFFFFF);

	/* mask all interrupt */
	COMIP_WRITE_REG32(&core_if->core_global_regs->gintmsk, 0x00);

	gusbcfg.d32 = readl(&core_if->core_global_regs->gusbcfg);
    gusbcfg.b.force_host_mode = 0;
	COMIP_WRITE_REG32(&core_if->core_global_regs->gusbcfg, gusbcfg.d32);
	mdelay(20);
#ifndef CONFIG_USB_COMIP_HSIC
	/*enter suspend*/
	writel(0x00, (void __iomem *)io_p2v(CTL_OTGPHY_SUSPENDM_CTRL));
	mdelay(10);

	/*reset phy */
	writel(0x01, (void __iomem *)io_p2v(CTL_POR_OTGPHY_CTRL));
	mdelay(10);

	/*disable usb12_clk*/
	if(core_if->clk)
	comip_otg_clk_disable(core_if);
#endif	
	mdelay(10);
}
void w_wakeup_detected(void *p)
{
    comip_core_if_t *core_if = (comip_core_if_t *) p;
    /*
     * Clear the Resume after 70ms. (Need 20 ms minimum. Use 70 ms
     * so that OPT tests pass with all PHYs).
     */
    hprt0_data_t hprt0 = {.d32 = 0 };

    hprt0.d32 = comip_read_hprt0(core_if);
    COMIP_DEBUGPL(DBG_ANY, "Resume: HPRT0=%0x\n", hprt0.d32);
//      mdelay(70);
    hprt0.b.prtres = 0; /* Resume */
    COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);
    COMIP_DEBUGPL(DBG_ANY, "Clear Resume: HPRT0=%0x\n",
            readl(core_if->host_if->hprt0));

    cil_hcd_resume(core_if);

    /** Change to L0 state*/
    core_if->lx_state = COMIP_L0;
}

comip_core_if_t *comip_cil_init(const uint32_t * reg_base_addr, struct clk *clk)
{
    comip_core_if_t *core_if = 0;
    comip_host_if_t *host_if = 0;
    uint8_t *reg_base = (uint8_t *) reg_base_addr;
    int i = 0;

    COMIP_DEBUGPL(DBG_CILV, "%s(%p)\n", __func__, reg_base_addr);
    core_if = kzalloc(sizeof(comip_core_if_t), GFP_KERNEL);

    if (core_if == NULL) {
        COMIP_DEBUGPL(DBG_CIL,
                "Allocation of comip_core_if_t failed\n");
        return 0;
    }
    core_if->core_global_regs = (comip_core_global_regs_t *) reg_base;

    /*
     * Allocate the Host Mode structures.
     */
    host_if = kzalloc(sizeof(comip_host_if_t), GFP_KERNEL);

    if (host_if == NULL) {
        COMIP_DEBUGPL(DBG_CIL,
                "Allocation of comip_host_if_t failed\n");
        kfree(core_if);
        return 0;
    }

    host_if->host_global_regs = (comip_host_global_regs_t *)
        (reg_base + COMIP_HOST_GLOBAL_REG_OFFSET);

    host_if->hprt0 =
        (uint32_t *) (reg_base + COMIP_HOST_PORT_REGS_OFFSET);

    for (i = 0; i < MAX_EPS_CHANNELS; i++) {
        host_if->hc_regs[i] = (comip_hc_regs_t *)
            (reg_base + COMIP_HOST_CHAN_REGS_OFFSET +
             (i * COMIP_CHAN_REGS_OFFSET));
        COMIP_DEBUGPL(DBG_CILV, "hc_reg[%d]->hcchar=%p\n",
                i, &host_if->hc_regs[i]->hcchar);
    }

    host_if->num_host_channels = MAX_EPS_CHANNELS;
    core_if->host_if = host_if;
    for (i = 0; i < MAX_EPS_CHANNELS; i++) {
        core_if->data_fifo[i] =
            (uint32_t *) (reg_base + COMIP_DATA_FIFO_OFFSET +
                  (i * COMIP_DATA_FIFO_SIZE));
        COMIP_DEBUGPL(DBG_CILV, "data_fifo[%d]=0x%08lx\n",
                i, (unsigned long)core_if->data_fifo[i]);
    }

    core_if->pcgcctl = (uint32_t *) (reg_base + COMIP_PCGCCTL_OFFSET);

	core_if->clk = clk;
	core_if->clk_enabled = 0;

		/* set usb init reg */
    //set_usb_init_reg(core_if);

    /* Initiate lx_state to L3 disconnected state */
    core_if->lx_state = COMIP_L3;
    /*
     * Store the contents of the hardware configuration registers here for
     * easy access later.
     */
     core_if->hwcfg1.d32= 0x2aaa5554; //endpoint direction
    core_if->hwcfg2.d32	= 0x239ff870; //include hub support bit
    core_if->hwcfg3.d32 = 0x162044e8; //include hsic config
    core_if->hwcfg4.d32 = 0xdff04020; // include dms mode config
	core_if->hptxfsiz.d32 = 0x00000000;

    COMIP_DEBUGPL(DBG_CILV, "hwcfg1=%08x\n", core_if->hwcfg1.d32);
    COMIP_DEBUGPL(DBG_CILV, "hwcfg2=%08x\n", core_if->hwcfg2.d32);
    COMIP_DEBUGPL(DBG_CILV, "hwcfg3=%08x\n", core_if->hwcfg3.d32);
    COMIP_DEBUGPL(DBG_CILV, "hwcfg4=%08x\n", core_if->hwcfg4.d32);
    COMIP_DEBUGPL(DBG_CILV, "hptxfsiz=%08x\n", core_if->hptxfsiz.d32);

    //core_if->hcfg.d32 =
        //readl(&core_if->host_if->host_global_regs->hcfg);
 
    //COMIP_DEBUGPL(DBG_CILV, "hcfg=%08x\n", core_if->hcfg.d32);
    COMIP_DEBUGPL(DBG_CILV, "dcfg=%08x\n", core_if->dcfg.d32);

    COMIP_DEBUGPL(DBG_CILV, "op_mode=%0x\n", core_if->hwcfg2.b.op_mode);
    COMIP_DEBUGPL(DBG_CILV, "arch=%0x\n", core_if->hwcfg2.b.architecture);
    COMIP_DEBUGPL(DBG_CILV, "num_dev_ep=%d\n", core_if->hwcfg2.b.num_dev_ep);
    COMIP_DEBUGPL(DBG_CILV, "num_host_chan=%d\n",
            core_if->hwcfg2.b.num_host_chan);
    COMIP_DEBUGPL(DBG_CILV, "nonperio_tx_q_depth=0x%0x\n",
            core_if->hwcfg2.b.nonperio_tx_q_depth);
    COMIP_DEBUGPL(DBG_CILV, "host_perio_tx_q_depth=0x%0x\n",
            core_if->hwcfg2.b.host_perio_tx_q_depth);
    COMIP_DEBUGPL(DBG_CILV, "dev_token_q_depth=0x%0x\n",
            core_if->hwcfg2.b.dev_token_q_depth);

    COMIP_DEBUGPL(DBG_CILV, "Total FIFO SZ=%d\n",
            core_if->hwcfg3.b.dfifo_depth);
    COMIP_DEBUGPL(DBG_CILV, "xfer_size_cntr_width=%0x\n",
            core_if->hwcfg3.b.xfer_size_cntr_width);
    /*
     * Create new workqueue and init works
     */
    core_if->wq_otg = COMIP_WORKQ_ALLOC("comip");
    if (core_if->wq_otg == 0) {
        COMIP_WARN("COMIP_WORKQ_ALLOC failed\n");
        COMIP_FREE(host_if);
        COMIP_FREE(core_if);
        return 0;
    }
    
    core_if->snpsid = 0x4f54294a;
    COMIP_PRINTF("Core Release: %x.%x%x%x\n",
           (core_if->snpsid >> 12 & 0xF),
           (core_if->snpsid >> 8 & 0xF),
           (core_if->snpsid >> 4 & 0xF), (core_if->snpsid & 0xF));
           
    core_if->wkp_timer = COMIP_TIMER_ALLOC("Wake Up Timer",
                         w_wakeup_detected, core_if);
    if (core_if->wkp_timer == 0) {
        COMIP_WARN("COMIP_TIMER_ALLOC failed\n");
        COMIP_FREE(host_if);
        COMIP_WORKQ_FREE(core_if->wq_otg);
        COMIP_FREE(core_if);
        return 0;
    }

    if (comip_setup_params(core_if)) {
        COMIP_WARN("Error while setting core params\n");
    }

    core_if->hibernation_suspend = 0;

	/** ADP initialization */
	//comip_adp_init(core_if);
	
    return core_if;
}

/**
 * This function frees the structures allocated by comip_cil_init().
 *
 * @param core_if The core interface pointer returned from
 *        comip_cil_init().
 *
 */
void comip_cil_remove(comip_core_if_t * core_if)
{
    /* Disable all interrupts */
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gahbcfg, 1, 0);
    COMIP_WRITE_REG32(&core_if->core_global_regs->gintmsk, 0);

    if (core_if->wq_otg) {
        COMIP_WORKQ_WAIT_WORK_DONE(core_if->wq_otg, 500);
        COMIP_WORKQ_FREE(core_if->wq_otg);
    }
	
    if (core_if->host_if) {
        COMIP_FREE(core_if->host_if);
    }

    /** Remove ADP Timers  */
    if (core_if->core_params) {
        COMIP_FREE(core_if->core_params);
    }
    if (core_if->wkp_timer) {
        COMIP_TIMER_FREE(core_if->wkp_timer);
    }
    COMIP_FREE(core_if);
}

/**
 * This function enables the controller's Global Interrupt in the AHB Config
 * register.
 *
 * @param core_if Programming view of COMIP_otg controller.
 */
void comip_enable_global_interrupts(comip_core_if_t * core_if)
{
    gahbcfg_data_t ahbcfg = {.d32 = 0 };
    ahbcfg.b.glblintrmsk = 1;   /* Enable interrupts */
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gahbcfg, 0, ahbcfg.d32);
}

/**
 * This function disables the controller's Global Interrupt in the AHB Config
 * register.
 *
 * @param core_if Programming view of COMIP_otg controller.
 */
void comip_disable_global_interrupts(comip_core_if_t * core_if)
{
    gahbcfg_data_t ahbcfg = {.d32 = 0 };
    ahbcfg.b.glblintrmsk = 1;   /* Disable interrupts */
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gahbcfg, ahbcfg.d32, 0);
}
/**
 * This function initializes the commmon interrupts, used in both
 * device and host modes.
 *
 * @param core_if Programming view of the COMIP_otg controller
 *
 */
static void comip_enable_common_interrupts(comip_core_if_t * core_if)
{
    comip_core_global_regs_t *global_regs = core_if->core_global_regs;
    gintmsk_data_t intr_mask = {.d32 = 0 };

    /* Clear any pending OTG Interrupts */
    COMIP_WRITE_REG32(&global_regs->gotgint, 0xFFFFFFFF);

    /* Clear any pending interrupts */
    COMIP_WRITE_REG32(&global_regs->gintsts, 0xFFFFFFFF);

    /*
     * Enable the interrupts in the GINTMSK.
     */
    //intr_mask.b.modemismatch = 1;
    //intr_mask.b.otgintr = 1;

    if (!core_if->dma_enable) {
        intr_mask.b.rxstsqlvl = 1;
    }
    
    //intr_mask.b.wkupintr = 1;
    intr_mask.b.disconnect = 0;
    //intr_mask.b.usbsuspend = 1;
    //intr_mask.b.sessreqintr = 1;
#ifdef CONFIG_USB_COMIP_LPM
    if (core_if->core_params->lpm_enable) {
        intr_mask.b.lpmtranrcvd = 1;
    }
#endif
    COMIP_WRITE_REG32(&global_regs->gintmsk, intr_mask.d32);
}

/*
 * The restore operation is modified to support Synopsys Emulated Powerdown and
 * Hibernation. This function is for exiting from Host mode hibernation by
 * Host Initiated Resume/Reset and Device Initiated Remote-Wakeup.
 * @param core_if Programming view of COMIP_otg controller.
 * @param rem_wakeup - indicates whether resume is initiated by Device or Host.
 * @param reset - indicates whether resume is initiated by Reset.
 */
int comip_host_hibernation_restore(comip_core_if_t * core_if,
                     int rem_wakeup, int reset)
{
    gpwrdn_data_t gpwrdn = {.d32 = 0 };
    hprt0_data_t hprt0 = {.d32 = 0 };

    int timeout = 2000;

    COMIP_DEBUGPL(DBG_HCD, "%s called\n", __FUNCTION__);
    /* Switch-on voltage to the core */
    gpwrdn.b.pwrdnswtch = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gpwrdn, gpwrdn.d32, 0);
    udelay(10);

    /* Reset core */
    gpwrdn.d32 = 0;
    gpwrdn.b.pwrdnrstn = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gpwrdn, gpwrdn.d32, 0);
    udelay(10);

    /* Assert Restore signal */
    gpwrdn.d32 = 0;
    gpwrdn.b.restore = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gpwrdn, 0, gpwrdn.d32);
    udelay(10);

    /* Disable power clamps */
    gpwrdn.d32 = 0;
    gpwrdn.b.pwrdnclmp = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gpwrdn, gpwrdn.d32, 0);

    if (!rem_wakeup) {
        udelay(50);
    }

    /* Deassert Reset core */
    gpwrdn.d32 = 0;
    gpwrdn.b.pwrdnrstn = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gpwrdn, 0, gpwrdn.d32);
    udelay(10);

    /* Disable PMU interrupt */
    gpwrdn.d32 = 0;
    gpwrdn.b.pmuintsel = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gpwrdn, gpwrdn.d32, 0);

    gpwrdn.d32 = 0;
    gpwrdn.b.connect_det_msk = 1;
    gpwrdn.b.srp_det_msk = 1;
    gpwrdn.b.disconn_det_msk = 1;
    gpwrdn.b.rst_det_msk = 1;
    gpwrdn.b.lnstchng_msk = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gpwrdn, gpwrdn.d32, 0);

    /* Indicates that we are going out from hibernation */
    core_if->hibernation_suspend = 0;

    /* Set Restore Essential Regs bit in PCGCCTL register */
    restore_essential_regs(core_if, rem_wakeup, 1);

    /* Wait a little for seeing new value of variable hibernation_suspend if
     * Restore done interrupt received before polling */
    udelay(10);

    if (core_if->hibernation_suspend == 0) {
        /* Wait For Restore_done Interrupt. This mechanism of polling the
         * interrupt is introduced to avoid any possible race conditions
         */
        do {
            gintsts_data_t gintsts;
            gintsts.d32 = readl(&core_if->core_global_regs->gintsts);
            if (gintsts.b.restoredone) {
                gintsts.d32 = 0;
                gintsts.b.restoredone = 1;
                COMIP_WRITE_REG32(&core_if->core_global_regs->gintsts, gintsts.d32);
                COMIP_DEBUGPL(DBG_HCD,"Restore Done Interrupt seen\n");    
                break;
            }
            udelay(10);
        } while (--timeout);
        if (!timeout) {
            COMIP_WARN("Restore Done interrupt wasn't generated\n");
        }
    }

    /* Set the flag's value to 0 again after receiving restore done interrupt */
    core_if->hibernation_suspend = 0;

    /* This step is not described in functional spec but if not wait for this
     * delay, mismatch interrupts occurred because just after restore core is
     * in Device mode(gintsts.curmode == 0) */
    mdelay(100);

    /* Clear all pending interrupts */
    COMIP_WRITE_REG32(&core_if->core_global_regs->gintsts, 0xFFFFFFFF);

    /* De-assert Restore */
    gpwrdn.d32 = 0;
    gpwrdn.b.restore = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gpwrdn, gpwrdn.d32, 0);
    udelay(10);

    /* Restore GUSBCFG and HCFG */
    COMIP_WRITE_REG32(&core_if->core_global_regs->gusbcfg,
            core_if->gr_backup->gusbcfg_local);
    COMIP_WRITE_REG32(&core_if->host_if->host_global_regs->hcfg,
            core_if->hr_backup->hcfg_local);

    /* De-assert Wakeup Logic */
    gpwrdn.d32 = 0;
    gpwrdn.b.pmuactv = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gpwrdn, gpwrdn.d32, 0);
    udelay(10);

    /* Start the Resume operation by programming HPRT0 */
    hprt0.d32 = core_if->hr_backup->hprt0_local;
    hprt0.b.prtpwr = 1;
    hprt0.b.prtena = 0;
    hprt0.b.prtsusp = 0;
    COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);

    COMIP_PRINTF("Resume Starts Now\n");
    if (!reset) {       // Indicates it is Resume Operation
        hprt0.d32 = core_if->hr_backup->hprt0_local;
        hprt0.b.prtres = 1;
        hprt0.b.prtpwr = 1;
        hprt0.b.prtena = 0;
        hprt0.b.prtsusp = 0;
        COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);

        if (!rem_wakeup)
            hprt0.b.prtres = 0;
        /* Wait for Resume time and then program HPRT again */
        mdelay(100);
        COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);

    } else {        // Indicates it is Reset Operation
        hprt0.d32 = core_if->hr_backup->hprt0_local;
        hprt0.b.prtrst = 1;
        hprt0.b.prtpwr = 1;
        hprt0.b.prtena = 0;
        hprt0.b.prtsusp = 0;
        COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);
        /* Wait for Reset time and then program HPRT again */
        mdelay(60);
        hprt0.b.prtrst = 0;
        COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);
    }
    /* Clear all interrupt status */
    hprt0.d32 = comip_read_hprt0(core_if);
    hprt0.b.prtconndet = 1;
    hprt0.b.prtenchng = 1;
    COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);

    /* Clear all pending interupts */
    COMIP_WRITE_REG32(&core_if->core_global_regs->gintsts, 0xFFFFFFFF);

    /* Restore global registers */
    comip_restore_global_regs(core_if);
    /* Restore host global registers */
    comip_restore_host_regs(core_if, reset);

    /* The core will be in ON STATE */
    core_if->lx_state = COMIP_L0;
    COMIP_PRINTF("Hibernation recovery is complete here\n");
    return 0;
}

/** Saves some register values into system memory. */
int comip_save_global_regs(comip_core_if_t * core_if)
{
    struct comip_global_regs_backup *gr;
    int i;

    gr = core_if->gr_backup;
    if (!gr) {
        gr = COMIP_ALLOC(sizeof(*gr));
        if (!gr) {
            return -ENOMEM;
        }
        core_if->gr_backup = gr;
    }

    gr->gotgctl_local = readl(&core_if->core_global_regs->gotgctl);
    gr->gintmsk_local = readl(&core_if->core_global_regs->gintmsk);
    gr->gahbcfg_local = readl(&core_if->core_global_regs->gahbcfg);
    gr->gusbcfg_local = readl(&core_if->core_global_regs->gusbcfg);
    gr->grxfsiz_local = readl(&core_if->core_global_regs->grxfsiz);
    gr->gnptxfsiz_local = readl(&core_if->core_global_regs->gnptxfsiz);
    gr->hptxfsiz_local = readl(&core_if->core_global_regs->hptxfsiz);
#ifdef CONFIG_USB_COMIP_LPM
    gr->glpmcfg_local = readl(&core_if->core_global_regs->glpmcfg);
#endif
    gr->gi2cctl_local = readl(&core_if->core_global_regs->gi2cctl);
    gr->pcgcctl_local = readl(core_if->pcgcctl);
    gr->gdfifocfg_local =
        readl(&core_if->core_global_regs->gdfifocfg);
    for (i = 0; i < MAX_EPS_CHANNELS; i++) {
        gr->dtxfsiz_local[i] =
            readl(&(core_if->core_global_regs->dtxfsiz[i]));
    }

    COMIP_DEBUGPL(DBG_ANY, "===========Backing Global registers==========\n");
    COMIP_DEBUGPL(DBG_ANY, "Backed up gotgctl   = %08x\n", gr->gotgctl_local);
    COMIP_DEBUGPL(DBG_ANY, "Backed up gintmsk   = %08x\n", gr->gintmsk_local);
    COMIP_DEBUGPL(DBG_ANY, "Backed up gahbcfg   = %08x\n", gr->gahbcfg_local);
    COMIP_DEBUGPL(DBG_ANY, "Backed up gusbcfg   = %08x\n", gr->gusbcfg_local);
    COMIP_DEBUGPL(DBG_ANY, "Backed up grxfsiz   = %08x\n", gr->grxfsiz_local);
    COMIP_DEBUGPL(DBG_ANY, "Backed up gnptxfsiz = %08x\n",
            gr->gnptxfsiz_local);
    COMIP_DEBUGPL(DBG_ANY, "Backed up hptxfsiz  = %08x\n",
            gr->hptxfsiz_local);
#ifdef CONFIG_USB_COMIP_LPM
    COMIP_DEBUGPL(DBG_ANY, "Backed up glpmcfg   = %08x\n", gr->glpmcfg_local);
#endif
    COMIP_DEBUGPL(DBG_ANY, "Backed up gi2cctl   = %08x\n", gr->gi2cctl_local);
    COMIP_DEBUGPL(DBG_ANY, "Backed up pcgcctl   = %08x\n", gr->pcgcctl_local);
    COMIP_DEBUGPL(DBG_ANY,"Backed up gdfifocfg   = %08x\n",gr->gdfifocfg_local);

    return 0;
}

/** Saves GINTMSK register before setting the msk bits. */
int comip_save_gintmsk_reg(comip_core_if_t * core_if)
{
    struct comip_global_regs_backup *gr;

    gr = core_if->gr_backup;
    if (!gr) {
        gr = COMIP_ALLOC(sizeof(*gr));
        if (!gr) {
            return -ENOMEM;
        }
        core_if->gr_backup = gr;
    }

    gr->gintmsk_local = readl(&core_if->core_global_regs->gintmsk);

    COMIP_DEBUGPL(DBG_ANY,"=============Backing GINTMSK registers============\n");
    COMIP_DEBUGPL(DBG_ANY, "Backed up gintmsk   = %08x\n", gr->gintmsk_local);

    return 0;
}

int comip_save_host_regs(comip_core_if_t * core_if)
{
    struct comip_host_regs_backup *hr;
    int i;

    hr = core_if->hr_backup;
    if (!hr) {
        hr = COMIP_ALLOC(sizeof(*hr));
        if (!hr) {
            return -ENOMEM;
        }
        core_if->hr_backup = hr;
    }

    hr->hcfg_local =
        readl(&core_if->host_if->host_global_regs->hcfg);
    hr->haintmsk_local =
        readl(&core_if->host_if->host_global_regs->haintmsk);
    for (i = 0; i < comip_get_param_host_channels(core_if); ++i) {
        hr->hcintmsk_local[i] =
            readl(&core_if->host_if->hc_regs[i]->hcintmsk);
    }
    hr->hprt0_local = readl(core_if->host_if->hprt0);
    hr->hfir_local =
        readl(&core_if->host_if->host_global_regs->hfir);

    COMIP_DEBUGPL(DBG_ANY,
            "=============Backing Host registers===============\n");
    COMIP_DEBUGPL(DBG_ANY, "Backed up hcfg     = %08x\n",
            hr->hcfg_local);
    COMIP_DEBUGPL(DBG_ANY, "Backed up haintmsk = %08x\n", hr->haintmsk_local);
    for (i = 0; i < comip_get_param_host_channels(core_if); ++i) {
        COMIP_DEBUGPL(DBG_ANY, "Backed up hcintmsk[%02d]=%08x\n", i,
                hr->hcintmsk_local[i]);
    }
    COMIP_DEBUGPL(DBG_ANY, "Backed up hprt0           = %08x\n",
            hr->hprt0_local);
    COMIP_DEBUGPL(DBG_ANY, "Backed up hfir           = %08x\n",
            hr->hfir_local);

    return 0;
}

int comip_restore_global_regs(comip_core_if_t *core_if)
{
    struct comip_global_regs_backup *gr;
    int i;

    gr = core_if->gr_backup;
    if (!gr) {
        return -EINVAL;
    }
   
    COMIP_WRITE_REG32(&core_if->core_global_regs->gotgctl, gr->gotgctl_local);
    COMIP_WRITE_REG32(&core_if->core_global_regs->gintmsk, gr->gintmsk_local);
    COMIP_WRITE_REG32(&core_if->core_global_regs->gusbcfg, gr->gusbcfg_local);
    COMIP_WRITE_REG32(&core_if->core_global_regs->gahbcfg, gr->gahbcfg_local);
    COMIP_WRITE_REG32(&core_if->core_global_regs->grxfsiz, gr->grxfsiz_local);
    COMIP_WRITE_REG32(&core_if->core_global_regs->gnptxfsiz,
            gr->gnptxfsiz_local);
    COMIP_WRITE_REG32(&core_if->core_global_regs->hptxfsiz,
            gr->hptxfsiz_local);
    COMIP_WRITE_REG32(&core_if->core_global_regs->gdfifocfg,
            gr->gdfifocfg_local);
    for (i = 0; i < MAX_EPS_CHANNELS; i++) {
        COMIP_WRITE_REG32(&core_if->core_global_regs->dtxfsiz[i],
                gr->dtxfsiz_local[i]);
    }

    COMIP_WRITE_REG32(&core_if->core_global_regs->gintsts, 0xFFFFFFFF);
    COMIP_WRITE_REG32(core_if->host_if->hprt0, 0x0000100A);
    COMIP_WRITE_REG32(&core_if->core_global_regs->gahbcfg,
            (gr->gahbcfg_local));
    return 0;
}

int comip_restore_host_regs(comip_core_if_t * core_if, int reset)
{
    struct comip_host_regs_backup *hr;
    int i;
    hr = core_if->hr_backup;

    if (!hr) {
        return -EINVAL;
    }

    COMIP_WRITE_REG32(&core_if->host_if->host_global_regs->hcfg, hr->hcfg_local);
    //if (!reset)
    //{
    //      COMIP_WRITE_REG32(&core_if->host_if->host_global_regs->hfir, hr->hfir_local);
    //}

    COMIP_WRITE_REG32(&core_if->host_if->host_global_regs->haintmsk,
            hr->haintmsk_local);
    for (i = 0; i < comip_get_param_host_channels(core_if); ++i) {
        COMIP_WRITE_REG32(&core_if->host_if->hc_regs[i]->hcintmsk,
                hr->hcintmsk_local[i]);
    }

    return 0;
}

int restore_lpm_i2c_regs(comip_core_if_t * core_if)
{
    struct comip_global_regs_backup *gr;

    gr = core_if->gr_backup;

    /* Restore values for LPM and I2C */
#ifdef CONFIG_USB_COMIP_LPM
    COMIP_WRITE_REG32(&core_if->core_global_regs->glpmcfg, gr->glpmcfg_local);
#endif
    COMIP_WRITE_REG32(&core_if->core_global_regs->gi2cctl, gr->gi2cctl_local);

    return 0;
}

int restore_essential_regs(comip_core_if_t * core_if, int rmode, int is_host)
{
    struct comip_global_regs_backup *gr;
    pcgcctl_data_t pcgcctl = {.d32 = 0 };
    gahbcfg_data_t gahbcfg = {.d32 = 0 };
    gusbcfg_data_t gusbcfg = {.d32 = 0 };
    gintmsk_data_t gintmsk = {.d32 = 0 };

    /* Restore LPM and I2C registers */
    restore_lpm_i2c_regs(core_if);

    /* Set PCGCCTL to 0 */
    COMIP_WRITE_REG32(core_if->pcgcctl, 0x00000000);

    gr = core_if->gr_backup;
    /* Load restore values for [31:14] bits */
    COMIP_WRITE_REG32(core_if->pcgcctl,
            ((gr->pcgcctl_local & 0xffffc000) | 0x00020000));

    /* Umnask global Interrupt in GAHBCFG and restore it */
    gahbcfg.d32 = gr->gahbcfg_local;
    gahbcfg.b.glblintrmsk = 1;
    COMIP_WRITE_REG32(&core_if->core_global_regs->gahbcfg, gahbcfg.d32);

    /* Clear all pending interupts */
    COMIP_WRITE_REG32(&core_if->core_global_regs->gintsts, 0xFFFFFFFF);

    /* Unmask restore done interrupt */
    gintmsk.b.restoredone = 1;
    COMIP_WRITE_REG32(&core_if->core_global_regs->gintmsk, gintmsk.d32);

    /* Restore GUSBCFG and HCFG/DCFG */
    gusbcfg.d32 = core_if->gr_backup->gusbcfg_local;
    COMIP_WRITE_REG32(&core_if->core_global_regs->gusbcfg, gusbcfg.d32);

    if (is_host) {
        hcfg_data_t hcfg = {.d32 = 0 };
        hcfg.d32 = core_if->hr_backup->hcfg_local;
        COMIP_WRITE_REG32(&core_if->host_if->host_global_regs->hcfg,
                hcfg.d32);

        /* Load restore values for [31:14] bits */
        pcgcctl.d32 = gr->pcgcctl_local & 0xffffc000;
        pcgcctl.d32 = gr->pcgcctl_local | 0x00020000;

        if (rmode)
            pcgcctl.b.restoremode = 1;
        COMIP_WRITE_REG32(core_if->pcgcctl, pcgcctl.d32);
        udelay(10);

        /* Load restore values for [31:14] bits and set EssRegRestored bit */
        pcgcctl.d32 = gr->pcgcctl_local | 0xffffc000;
        pcgcctl.d32 = gr->pcgcctl_local & 0xffffc000;
        pcgcctl.b.ess_reg_restored = 1;
        if (rmode)
            pcgcctl.b.restoremode = 1;
        COMIP_WRITE_REG32(core_if->pcgcctl, pcgcctl.d32);
    } else {

        /* Load restore values for [31:14] bits */
        pcgcctl.d32 = gr->pcgcctl_local & 0xffffc000;
        pcgcctl.d32 = gr->pcgcctl_local | 0x00020000;
        if (!rmode) {
            pcgcctl.d32 |= 0x208;
        }
        COMIP_WRITE_REG32(core_if->pcgcctl, pcgcctl.d32);
        udelay(10);

        /* Load restore values for [31:14] bits */
        pcgcctl.d32 = gr->pcgcctl_local & 0xffffc000;
        pcgcctl.d32 = gr->pcgcctl_local | 0x00020000;
        pcgcctl.b.ess_reg_restored = 1;
        if (!rmode)
            pcgcctl.d32 |= 0x208;
        COMIP_WRITE_REG32(core_if->pcgcctl, pcgcctl.d32);
    }

    return 0;
}

/**
 * Initializes the FSLSPClkSel field of the HCFG register depending on the PHY
 * type.
 */
static void init_fslspclksel(comip_core_if_t * core_if)
{
    uint32_t val;
    hcfg_data_t hcfg;

    if (((core_if->hwcfg2.b.hs_phy_type == 2) &&
         (core_if->hwcfg2.b.fs_phy_type == 1) &&
         (core_if->core_params->ulpi_fs_ls)) ||
        (core_if->core_params->phy_type == COMIP_PHY_TYPE_PARAM_FS)) {
        /* Full speed PHY */
        val = COMIP_HCFG_48_MHZ;
    } else {
        /* High speed PHY running at full speed or high speed */
        val = COMIP_HCFG_30_60_MHZ;
    }

    COMIP_DEBUGPL(DBG_CIL, "Initializing HCFG.FSLSPClkSel to 0x%1x\n", val);
    hcfg.d32 = readl(&core_if->host_if->host_global_regs->hcfg);
    hcfg.b.fslspclksel = val;
    COMIP_WRITE_REG32(&core_if->host_if->host_global_regs->hcfg, hcfg.d32);
}

/**
 * This function calculates the number of IN EPS
 * using GHWCFG1 and GHWCFG2 registers values
 *
 * @param core_if Programming view of the COMIP_otg controller
 */
#if 0
static uint32_t calc_num_in_eps(comip_core_if_t * core_if)
{
    uint32_t num_in_eps = 0;
    uint32_t num_eps = core_if->hwcfg2.b.num_dev_ep;
    uint32_t hwcfg1 = core_if->hwcfg1.d32 >> 3;
    uint32_t num_tx_fifos = core_if->hwcfg4.b.num_in_eps;
    int i;

    for (i = 0; i < num_eps; ++i) {
        if (!(hwcfg1 & 0x1))
            num_in_eps++;

        hwcfg1 >>= 2;
    }

    if (core_if->hwcfg4.b.ded_fifo_en) {
        num_in_eps =
            (num_in_eps > num_tx_fifos) ? num_tx_fifos : num_in_eps;
    }

    return num_in_eps;
}

/**
 * This function calculates the number of OUT EPS
 * using GHWCFG1 and GHWCFG2 registers values
 *
 * @param core_if Programming view of the COMIP_otg controller
 */
static uint32_t calc_num_out_eps(comip_core_if_t * core_if)
{
    uint32_t num_out_eps = 0;
    uint32_t num_eps = core_if->hwcfg2.b.num_dev_ep;
    uint32_t hwcfg1 = core_if->hwcfg1.d32 >> 2;
    int i;

    for (i = 0; i < num_eps; ++i) {
        if (!(hwcfg1 & 0x1))
            num_out_eps++;

        hwcfg1 >>= 2;
    }
    return num_out_eps;
}
#endif
/**
 * This function initializes the COMIP_otg controller registers and
 * prepares the core for device mode or host mode operation.
 *
 * @param core_if Programming view of the COMIP_otg controller
 *
 */
void comip_core_init(comip_core_if_t * core_if)
{
    comip_core_global_regs_t *global_regs = core_if->core_global_regs;
    gahbcfg_data_t ahbcfg = {.d32 = 0 };
    gusbcfg_data_t usbcfg = {.d32 = 0 };
    gi2cctl_data_t i2cctl = {.d32 = 0 };

    COMIP_DEBUGPL(DBG_CILV, "comip_core_init(%p)\n", core_if);

    /* Common Initialization */
    usbcfg.d32 = readl(&global_regs->gusbcfg);

    /* Program the ULPI External VBUS bit if needed */
    usbcfg.b.ulpi_ext_vbus_drv =
        (core_if->core_params->phy_ulpi_ext_vbus ==
         COMIP_PHY_ULPI_EXTERNAL_VBUS) ? 1 : 0;

    /* Set external TS Dline pulsing */
    usbcfg.b.term_sel_dl_pulse =
        (core_if->core_params->ts_dline == 1) ? 1 : 0;
    COMIP_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);

    /* Reset the Controller */
    comip_core_reset(core_if);
    
    core_if->power_down = core_if->core_params->power_down;
    core_if->otg_sts = 0;

    COMIP_DEBUGPL(DBG_CIL, "num_dev_perio_in_ep=%d\n",
            core_if->hwcfg4.b.num_dev_perio_in_ep);

    core_if->total_fifo_size = core_if->hwcfg3.b.dfifo_depth;
    core_if->rx_fifo_size = readl(&global_regs->grxfsiz);
    core_if->nperio_tx_fifo_size =
        readl(&global_regs->gnptxfsiz) >> 16;

    COMIP_DEBUGPL(DBG_CIL, "Total FIFO SZ=%d\n", core_if->total_fifo_size);
    COMIP_DEBUGPL(DBG_CIL, "Rx FIFO SZ=%d\n", core_if->rx_fifo_size);
    COMIP_DEBUGPL(DBG_CIL, "NP Tx FIFO SZ=%d\n",
            core_if->nperio_tx_fifo_size);

    /* This programming sequence needs to happen in FS mode before any other
     * programming occurs */
    if ((core_if->core_params->speed == COMIP_SPEED_PARAM_FULL) &&
        (core_if->core_params->phy_type == COMIP_PHY_TYPE_PARAM_FS)) {
        /* If FS mode with FS PHY */

        /* core_init() is now called on every switch so only call the
         * following for the first time through. */
        if (!core_if->phy_init_done) {
            core_if->phy_init_done = 1;
            COMIP_DEBUGPL(DBG_CIL, "FS_PHY detected\n");
            usbcfg.d32 = readl(&global_regs->gusbcfg);
            usbcfg.b.physel = 1;
            COMIP_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);

            /* Reset after a PHY select */
            comip_core_reset(core_if);
        }

        /* Program DCFG.DevSpd or HCFG.FSLSPclkSel to 48Mhz in FS.      Also
         * do this on HNP Dev/Host mode switches (done in dev_init and
         * host_init). */
        if (comip_is_host_mode(core_if)) {
            init_fslspclksel(core_if);
        } 

        if (core_if->core_params->i2c_enable) {
            COMIP_DEBUGPL(DBG_CIL, "FS_PHY Enabling I2c\n");
            /* Program GUSBCFG.OtgUtmifsSel to I2C */
            usbcfg.d32 = readl(&global_regs->gusbcfg);
            usbcfg.b.otgutmifssel = 1;
            COMIP_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);

            /* Program GI2CCTL.I2CEn */
            i2cctl.d32 = readl(&global_regs->gi2cctl);
            i2cctl.b.i2cdevaddr = 1;
            i2cctl.b.i2cen = 0;
            COMIP_WRITE_REG32(&global_regs->gi2cctl, i2cctl.d32);
            i2cctl.b.i2cen = 1;
            COMIP_WRITE_REG32(&global_regs->gi2cctl, i2cctl.d32);
        }

    } /* endif speed == COMIP_SPEED_PARAM_FULL */
    else {
        /* High speed PHY. */
        if (!core_if->phy_init_done) {
            core_if->phy_init_done = 1;
            /* HS PHY parameters.  These parameters are preserved
             * during soft reset so only program the first time.  Do
             * a soft reset immediately after setting phyif.  */

            if (core_if->core_params->phy_type == 2) {
                /* ULPI interface */
                usbcfg.b.ulpi_utmi_sel = 1;
                usbcfg.b.phyif = 0;
                usbcfg.b.ddrsel =
                    core_if->core_params->phy_ulpi_ddr;
            } else if (core_if->core_params->phy_type == 1) {
                /* UTMI+ interface */
                usbcfg.b.ulpi_utmi_sel = 0;
                if (core_if->core_params->phy_utmi_width == 16) {
                    usbcfg.b.phyif = 1;

                } else {
                    usbcfg.b.phyif = 0;
                }
            } else {
                COMIP_ERROR("FS PHY TYPE\n");
            }
            COMIP_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);
            /* Reset after setting the PHY parameters */
            comip_core_reset(core_if);
        }
    }

    if ((core_if->hwcfg2.b.hs_phy_type == 2) &&
        (core_if->hwcfg2.b.fs_phy_type == 1) &&
        (core_if->core_params->ulpi_fs_ls)) {
        COMIP_DEBUGPL(DBG_CIL, "Setting ULPI FSLS\n");
        usbcfg.d32 = readl(&global_regs->gusbcfg);
        usbcfg.b.ulpi_fsls = 1;
        usbcfg.b.ulpi_clk_sus_m = 1;
        COMIP_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);
    } else {
        usbcfg.d32 = readl(&global_regs->gusbcfg);
        usbcfg.b.ulpi_fsls = 0;
        usbcfg.b.ulpi_clk_sus_m = 0;
        COMIP_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);
    }

    /* Program the GAHBCFG Register. */
    switch (core_if->hwcfg2.b.architecture) {

    case COMIP_SLAVE_ONLY_ARCH:
        COMIP_DEBUGPL(DBG_CIL, "Slave Only Mode\n");
        ahbcfg.b.nptxfemplvl_txfemplvl =
            COMIP_GAHBCFG_TXFEMPTYLVL_HALFEMPTY;
        ahbcfg.b.ptxfemplvl = COMIP_GAHBCFG_TXFEMPTYLVL_HALFEMPTY;
        core_if->dma_enable = 0;
        core_if->dma_desc_enable = 0;
        break;

    case COMIP_EXT_DMA_ARCH:
        COMIP_DEBUGPL(DBG_CIL, "External DMA Mode\n");
        {
            uint8_t brst_sz = core_if->core_params->dma_burst_size;
            ahbcfg.b.hburstlen = 0;
            while (brst_sz > 1) {
                ahbcfg.b.hburstlen++;
                brst_sz >>= 1;
            }
        }
        core_if->dma_enable = (core_if->core_params->dma_enable != 0);
        core_if->dma_desc_enable =
            (core_if->core_params->dma_desc_enable != 0);
        break;

    case COMIP_INT_DMA_ARCH:
        COMIP_DEBUGPL(DBG_CIL, "Internal DMA Mode\n");
        /* Old value was COMIP_GAHBCFG_INT_DMA_BURST_INCR - done for 
          Host mode ISOC in issue fix - vahrama */
        ahbcfg.b.hburstlen = COMIP_GAHBCFG_INT_DMA_BURST_INCR4;
        core_if->dma_enable = (core_if->core_params->dma_enable != 0);
        core_if->dma_desc_enable =
            (core_if->core_params->dma_desc_enable != 0);
        break;

    }
    if (core_if->dma_enable) {
        if (core_if->dma_desc_enable) {
            COMIP_PRINTF("Using Descriptor DMA mode\n");
        } else {
            COMIP_PRINTF("Using Buffer DMA mode\n");

        }
    } else {
        COMIP_PRINTF("Using Slave mode\n");
        core_if->dma_desc_enable = 0;
    }

    ahbcfg.b.dmaenable = core_if->dma_enable;
    COMIP_WRITE_REG32(&global_regs->gahbcfg, ahbcfg.d32);

    core_if->en_multiple_tx_fifo = core_if->hwcfg4.b.ded_fifo_en;

    core_if->pti_enh_enable = core_if->core_params->pti_enable != 0;
    core_if->multiproc_int_enable = core_if->core_params->mpi_enable;
    COMIP_PRINTF("Periodic Transfer Interrupt Enhancement - %s\n",
           ((core_if->pti_enh_enable) ? "enabled" : "disabled"));
    COMIP_PRINTF("Multiprocessor Interrupt Enhancement - %s\n",
           ((core_if->multiproc_int_enable) ? "enabled" : "disabled"));

#ifdef CONFIG_USB_COMIP_LPM
    if (core_if->core_params->lpm_enable) {
        glpmcfg_data_t lpmcfg = {.d32 = 0 };

        /* To enable LPM support set lpm_cap_en bit */
        lpmcfg.b.lpm_cap_en = 1;

        /* Make AppL1Res ACK */
        lpmcfg.b.appl_resp = 1;

        /* Retry 3 times */
        lpmcfg.b.retry_count = 3;

        COMIP_MODIFY_REG32(&core_if->core_global_regs->glpmcfg,
                 0, lpmcfg.d32);

    }
#endif
    if (core_if->core_params->ic_usb_cap) {
        gusbcfg_data_t gusbcfg = {.d32 = 0 };
        gusbcfg.b.ic_usb_cap = 1;
        COMIP_MODIFY_REG32(&core_if->core_global_regs->gusbcfg,
                 0, gusbcfg.d32);
    }
    {
        gotgctl_data_t gotgctl = {.d32 = 0 };
        gotgctl.b.otgver = core_if->core_params->otg_ver;
        COMIP_MODIFY_REG32(&core_if->core_global_regs->gotgctl, 0,
                 gotgctl.d32);
        /* Set OTG version supported */
        core_if->otg_ver = core_if->core_params->otg_ver;
        COMIP_PRINTF("OTG VER PARAM: %d, OTG VER FLAG: %d\n",
               core_if->core_params->otg_ver, core_if->otg_ver);
    }   
		/* Enable common interrupts */
    comip_enable_common_interrupts(core_if);
    
}

/**
 * This function enables the Host mode interrupts.
 *
 * @param core_if Programming view of COMIP_otg controller
 */
void comip_enable_host_interrupts(comip_core_if_t * core_if)
{
    comip_core_global_regs_t *global_regs = core_if->core_global_regs;
    gintmsk_data_t intr_mask = {.d32 = 0 };

    COMIP_DEBUGPL(DBG_CIL, "%s()\n", __func__);

    /* Disable all interrupts. */
    COMIP_WRITE_REG32(&global_regs->gintmsk, 0);

    /* Clear any pending interrupts. */
    COMIP_WRITE_REG32(&global_regs->gintsts, 0xFFFFFFFF);

    /* Enable the common interrupts */
    comip_enable_common_interrupts(core_if);

    /*
     * Enable host mode interrupts without disturbing common
     * interrupts.
     */

    /* Do not need sof interrupt for Descriptor DMA */
    if (!core_if->dma_desc_enable)
        intr_mask.b.sofintr = 1;
    intr_mask.b.disconnect = 1;
    intr_mask.b.portintr = 1;
    intr_mask.b.hcintr = 1;

    COMIP_MODIFY_REG32(&global_regs->gintmsk, intr_mask.d32, intr_mask.d32);
}

/**
 * This function disables the Host Mode interrupts.
 *
 * @param core_if Programming view of COMIP_otg controller
 */
void comip_disable_host_interrupts(comip_core_if_t * core_if)
{
    comip_core_global_regs_t *global_regs = core_if->core_global_regs;
    gintmsk_data_t intr_mask = {.d32 = 0 };

    COMIP_DEBUGPL(DBG_CILV, "%s()\n", __func__);

    /*
     * Disable host mode interrupts without disturbing common
     * interrupts.
     */
    intr_mask.b.sofintr = 1;
    intr_mask.b.portintr = 1;
    intr_mask.b.hcintr = 1;
    intr_mask.b.ptxfempty = 1;
    intr_mask.b.nptxfempty = 1;

    COMIP_MODIFY_REG32(&global_regs->gintmsk, intr_mask.d32, 0);
}

/**
 * This function initializes the COMIP_otg controller registers for
 * host mode.
 *
 * This function flushes the Tx and Rx FIFOs and it flushes any entries in the
 * request queues. Host channels are reset to ensure that they are ready for
 * performing transfers.
 *
 * @param core_if Programming view of COMIP_otg controller
 *
 */
void comip_core_host_init(comip_core_if_t * core_if)
{
    comip_core_global_regs_t *global_regs = core_if->core_global_regs;
    comip_host_if_t *host_if = core_if->host_if;
    comip_core_params_t *params = core_if->core_params;
    hprt0_data_t hprt0 = {.d32 = 0 };
    fifosize_data_t nptxfifosize;
    fifosize_data_t ptxfifosize;
    uint16_t rxfsiz, nptxfsiz, hptxfsiz;
    gdfifocfg_data_t gdfifocfg = {.d32 = 0 };
    int i;
    hcchar_data_t hcchar;
    hcfg_data_t hcfg;
    hfir_data_t hfir;
    comip_hc_regs_t *hc_regs;
    int num_channels;
    gotgctl_data_t gotgctl = {.d32 = 0 };

    COMIP_DEBUGPL(DBG_CILV, "%s(%p)\n", __func__, core_if);

    /* Restart the Phy Clock */
    COMIP_WRITE_REG32(core_if->pcgcctl, 0);

    /* Initialize Host Configuration Register */
    init_fslspclksel(core_if);
    if (core_if->core_params->speed == COMIP_SPEED_PARAM_FULL) {
        hcfg.d32 = readl(&host_if->host_global_regs->hcfg);
        hcfg.b.fslssupp = 1;
        COMIP_WRITE_REG32(&host_if->host_global_regs->hcfg, hcfg.d32);

    }

    /* This bit allows dynamic reloading of the HFIR register
     * during runtime. This bit needs to be programmed during 
     * initial configuration and its value must not be changed
     * during runtime.*/
    if (core_if->core_params->reload_ctl == 1) {
        hfir.d32 = readl(&host_if->host_global_regs->hfir);
        hfir.b.hfirrldctrl = 1;
        COMIP_WRITE_REG32(&host_if->host_global_regs->hfir, hfir.d32);
    }

    if (core_if->core_params->dma_desc_enable) {
        uint8_t op_mode = core_if->hwcfg2.b.op_mode;
        if (!
            (core_if->hwcfg4.b.desc_dma
             && (core_if->snpsid >= OTG_CORE_REV_2_90a)
             && ((op_mode == COMIP_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG)
             || (op_mode == COMIP_HWCFG2_OP_MODE_SRP_ONLY_CAPABLE_OTG)
             || (op_mode ==
                 COMIP_HWCFG2_OP_MODE_NO_HNP_SRP_CAPABLE_OTG)
             || (op_mode == COMIP_HWCFG2_OP_MODE_SRP_CAPABLE_HOST)
             || (op_mode ==
                 COMIP_HWCFG2_OP_MODE_NO_SRP_CAPABLE_HOST)))) {

            COMIP_ERROR("Host can't operate in Descriptor DMA mode.\n"
                  "Either core version is below 2.90a or "
                  "GHWCFG2, GHWCFG4 registers' values do not allow Descriptor DMA in host mode.\n"
                  "To run the driver in Buffer DMA host mode set dma_desc_enable "
                  "module parameter to 0.\n");
            return;
        }
        hcfg.d32 = readl(&host_if->host_global_regs->hcfg);
        hcfg.b.descdma = 1;
        COMIP_WRITE_REG32(&host_if->host_global_regs->hcfg, hcfg.d32);
    }

    /* Configure data FIFO sizes */
    if (core_if->hwcfg2.b.dynamic_fifo && params->enable_dynamic_fifo) {
        COMIP_DEBUGPL(DBG_CIL, "Total FIFO Size=%d\n",
                core_if->total_fifo_size);
        COMIP_DEBUGPL(DBG_CIL, "Rx FIFO Size=%d\n",
                params->host_rx_fifo_size);
        COMIP_DEBUGPL(DBG_CIL, "NP Tx FIFO Size=%d\n",
                params->host_nperio_tx_fifo_size);
        COMIP_DEBUGPL(DBG_CIL, "P Tx FIFO Size=%d\n",
                params->host_perio_tx_fifo_size);

        /* Rx FIFO */
        COMIP_DEBUGPL(DBG_CIL, "initial grxfsiz=%08x\n",
                readl(&global_regs->grxfsiz));
        COMIP_WRITE_REG32(&global_regs->grxfsiz,
                params->host_rx_fifo_size);
        COMIP_DEBUGPL(DBG_CIL, "new grxfsiz=%08x\n",
                readl(&global_regs->grxfsiz));

        /* Non-periodic Tx FIFO */
        COMIP_DEBUGPL(DBG_CIL, "initial gnptxfsiz=%08x\n",
                readl(&global_regs->gnptxfsiz));
        nptxfifosize.b.depth = params->host_nperio_tx_fifo_size;
        nptxfifosize.b.startaddr = params->host_rx_fifo_size;
        COMIP_WRITE_REG32(&global_regs->gnptxfsiz, nptxfifosize.d32);
        COMIP_DEBUGPL(DBG_CIL, "new gnptxfsiz=%08x\n",
                readl(&global_regs->gnptxfsiz));

        /* Periodic Tx FIFO */
        COMIP_DEBUGPL(DBG_CIL, "initial hptxfsiz=%08x\n",
                readl(&global_regs->hptxfsiz));
        ptxfifosize.b.depth = params->host_perio_tx_fifo_size;
        ptxfifosize.b.startaddr =
            nptxfifosize.b.startaddr + nptxfifosize.b.depth;
        COMIP_WRITE_REG32(&global_regs->hptxfsiz, ptxfifosize.d32);
        COMIP_DEBUGPL(DBG_CIL, "new hptxfsiz=%08x\n",
                readl(&global_regs->hptxfsiz));
        
        if (core_if->en_multiple_tx_fifo) {
            /* Global DFIFOCFG calculation for Host mode - include RxFIFO, NPTXFIFO and HPTXFIFO */
            gdfifocfg.d32 = readl(&global_regs->gdfifocfg);
            rxfsiz = (readl(&global_regs->grxfsiz) & 0x0000ffff);
            nptxfsiz = (readl(&global_regs->gnptxfsiz) >> 16);
            hptxfsiz = (readl(&global_regs->hptxfsiz) >> 16);
            gdfifocfg.b.epinfobase = rxfsiz + nptxfsiz + hptxfsiz;
            COMIP_WRITE_REG32(&global_regs->gdfifocfg, gdfifocfg.d32);
        }
    }

    /* TODO - check this */
    /* Clear Host Set HNP Enable in the OTG Control Register */
    gotgctl.b.hstsethnpen = 1;
    COMIP_MODIFY_REG32(&global_regs->gotgctl, gotgctl.d32, 0);
    /* Make sure the FIFOs are flushed. */
    comip_flush_tx_fifo(core_if, 0x10 /* all TX FIFOs */ );
    comip_flush_rx_fifo(core_if);

    /* Clear Host Set HNP Enable in the OTG Control Register */
    gotgctl.b.hstsethnpen = 1;
    COMIP_MODIFY_REG32(&global_regs->gotgctl, gotgctl.d32, 0);

    if (!core_if->core_params->dma_desc_enable) {
        /* Flush out any leftover queued requests. */
        num_channels = core_if->core_params->host_channels;

        for (i = 0; i < num_channels; i++) {
            hc_regs = core_if->host_if->hc_regs[i];
            hcchar.d32 = readl(&hc_regs->hcchar);
            hcchar.b.chen = 0;
            hcchar.b.chdis = 1;
            hcchar.b.epdir = 0;
            COMIP_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);
        }

        /* Halt all channels to put them into a known state. */
        for (i = 0; i < num_channels; i++) {
            int count = 0;
            hc_regs = core_if->host_if->hc_regs[i];
            hcchar.d32 = readl(&hc_regs->hcchar);
            hcchar.b.chen = 1;
            hcchar.b.chdis = 1;
            hcchar.b.epdir = 0;
            COMIP_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);
            COMIP_DEBUGPL(DBG_HCDV, "%s: Halt channel %d\n", __func__, i);
            do {
                hcchar.d32 = readl(&hc_regs->hcchar);
                if (++count > 1000) {
                    COMIP_ERROR ("%s: Unable to clear halt on channel %d.\n", __func__, i);
                    break;
                }
                udelay(1);
            } while (hcchar.b.chen);
        }
    }

    /* Turn on the vbus power. */  
    hprt0.d32 = comip_read_hprt0(core_if);
    COMIP_PRINTF("Init: Power Port (%d)\n", hprt0.b.prtpwr);
    if (hprt0.b.prtpwr == 0) {
        hprt0.b.prtpwr = 1;
        COMIP_WRITE_REG32(host_if->hprt0, hprt0.d32);
    }

    comip_enable_host_interrupts(core_if);
}

/**
 * Prepares a host channel for transferring packets to/from a specific
 * endpoint. The HCCHARn register is set up with the characteristics specified
 * in _hc. Host channel interrupts that may need to be serviced while this
 * transfer is in progress are enabled.
 *
 * @param core_if Programming view of COMIP_otg controller
 * @param hc Information needed to initialize the host channel
 */
void comip_hc_init(comip_core_if_t * core_if, comip_hc_t * hc)
{
    uint32_t intr_enable;
    hcintmsk_data_t hc_intr_mask;
    gintmsk_data_t gintmsk = {.d32 = 0 };
    hcchar_data_t hcchar;
    hcsplt_data_t hcsplt;

    uint8_t hc_num = hc->hc_num;
    comip_host_if_t *host_if = core_if->host_if;
    comip_hc_regs_t *hc_regs = host_if->hc_regs[hc_num];

    /* Clear old interrupt conditions for this host channel. */
    hc_intr_mask.d32 = 0xFFFFFFFF;
    hc_intr_mask.b.reserved14_31 = 0;
    COMIP_WRITE_REG32(&hc_regs->hcint, hc_intr_mask.d32);

    /* Enable channel interrupts required for this transfer. */
    hc_intr_mask.d32 = 0;
    hc_intr_mask.b.chhltd = 1;
    if (core_if->dma_enable) {
        /* For Descriptor DMA mode core halts the channel on AHB error. Interrupt is not required */
        if (!core_if->dma_desc_enable)
            hc_intr_mask.b.ahberr = 1;
        else {
            if (hc->ep_type == COMIP_EP_TYPE_ISOC)
                hc_intr_mask.b.xfercompl = 1;
        }

        if (hc->error_state && !hc->do_split &&
            hc->ep_type != COMIP_EP_TYPE_ISOC) {
            hc_intr_mask.b.ack = 1;
            if (hc->ep_is_in) {
                hc_intr_mask.b.datatglerr = 1;
                if (hc->ep_type != COMIP_EP_TYPE_INTR) {
                    hc_intr_mask.b.nak = 1;
                }
            }
        }
    } else {
        switch (hc->ep_type) {
        case COMIP_EP_TYPE_CONTROL:
        case COMIP_EP_TYPE_BULK:
            hc_intr_mask.b.xfercompl = 1;
            hc_intr_mask.b.stall = 1;
            hc_intr_mask.b.xacterr = 1;
            hc_intr_mask.b.datatglerr = 1;
            if (hc->ep_is_in) {
                hc_intr_mask.b.bblerr = 1;
            } else {
                hc_intr_mask.b.nak = 1;
                hc_intr_mask.b.nyet = 1;
                if (hc->do_ping) {
                    hc_intr_mask.b.ack = 1;
                }
            }

            if (hc->do_split) {
                hc_intr_mask.b.nak = 1;
                if (hc->complete_split) {
                    hc_intr_mask.b.nyet = 1;
                } else {
                    hc_intr_mask.b.ack = 1;
                }
            }

            if (hc->error_state) {
                hc_intr_mask.b.ack = 1;
            }
            break;
        case COMIP_EP_TYPE_INTR:
            hc_intr_mask.b.xfercompl = 1;
            hc_intr_mask.b.nak = 1;
            hc_intr_mask.b.stall = 1;
            hc_intr_mask.b.xacterr = 1;
            hc_intr_mask.b.datatglerr = 1;
            hc_intr_mask.b.frmovrun = 1;

            if (hc->ep_is_in) {
                hc_intr_mask.b.bblerr = 1;
            }
            if (hc->error_state) {
                hc_intr_mask.b.ack = 1;
            }
            if (hc->do_split) {
                if (hc->complete_split) {
                    hc_intr_mask.b.nyet = 1;
                } else {
                    hc_intr_mask.b.ack = 1;
                }
            }
            break;
        case COMIP_EP_TYPE_ISOC:
            hc_intr_mask.b.xfercompl = 1;
            hc_intr_mask.b.frmovrun = 1;
            hc_intr_mask.b.ack = 1;

            if (hc->ep_is_in) {
                hc_intr_mask.b.xacterr = 1;
                hc_intr_mask.b.bblerr = 1;
            }
            break;
        }
    }
    COMIP_WRITE_REG32(&hc_regs->hcintmsk, hc_intr_mask.d32);

    /* Enable the top level host channel interrupt. */
    intr_enable = (1 << hc_num);
    COMIP_MODIFY_REG32(&host_if->host_global_regs->haintmsk, 0, intr_enable);

    /* Make sure host channel interrupts are enabled. */
    gintmsk.b.hcintr = 1;
    COMIP_MODIFY_REG32(&core_if->core_global_regs->gintmsk, 0, gintmsk.d32);

    /*
     * Program the HCCHARn register with the endpoint characteristics for
     * the current transfer.
     */
    hcchar.d32 = 0;
    hcchar.b.devaddr = hc->dev_addr;
    hcchar.b.epnum = hc->ep_num;
    hcchar.b.epdir = hc->ep_is_in;
    hcchar.b.lspddev = (hc->speed == COMIP_EP_SPEED_LOW);
    hcchar.b.eptype = hc->ep_type;
    hcchar.b.mps = hc->max_packet;

    COMIP_WRITE_REG32(&host_if->hc_regs[hc_num]->hcchar, hcchar.d32);

    COMIP_DEBUGPL(DBG_HCDV, "%s: Channel %d\n", __func__, hc->hc_num);
    COMIP_DEBUGPL(DBG_HCDV, "   Dev Addr: %d\n", hcchar.b.devaddr);
    COMIP_DEBUGPL(DBG_HCDV, "   Ep Num: %d\n", hcchar.b.epnum);
    COMIP_DEBUGPL(DBG_HCDV, "   Is In: %d\n", hcchar.b.epdir);
    COMIP_DEBUGPL(DBG_HCDV, "   Is Low Speed: %d\n", hcchar.b.lspddev);
    COMIP_DEBUGPL(DBG_HCDV, "   Ep Type: %d\n", hcchar.b.eptype);
    COMIP_DEBUGPL(DBG_HCDV, "   Max Pkt: %d\n", hcchar.b.mps);
    COMIP_DEBUGPL(DBG_HCDV, "   Multi Cnt: %d\n", hcchar.b.multicnt);

    /*
     * Program the HCSPLIT register for SPLITs
     */
    hcsplt.d32 = 0;
    if (hc->do_split) {
        COMIP_DEBUGPL(DBG_HCDV, "Programming HC %d with split --> %s\n",
                hc->hc_num,
                hc->complete_split ? "CSPLIT" : "SSPLIT");
        hcsplt.b.compsplt = hc->complete_split;
        hcsplt.b.xactpos = hc->xact_pos;
        hcsplt.b.hubaddr = hc->hub_addr;
        hcsplt.b.prtaddr = hc->port_addr;
        COMIP_DEBUGPL(DBG_HCDV, "    comp split %d\n", hc->complete_split);
        COMIP_DEBUGPL(DBG_HCDV, "    xact pos %d\n", hc->xact_pos);
        COMIP_DEBUGPL(DBG_HCDV, "    hub addr %d\n", hc->hub_addr);
        COMIP_DEBUGPL(DBG_HCDV, "    port addr %d\n", hc->port_addr);
        COMIP_DEBUGPL(DBG_HCDV, "    is_in %d\n", hc->ep_is_in);
        COMIP_DEBUGPL(DBG_HCDV, "    Max Pkt: %d\n", hcchar.b.mps);
        COMIP_DEBUGPL(DBG_HCDV, "    xferlen: %d\n", hc->xfer_len);
    }
    COMIP_WRITE_REG32(&host_if->hc_regs[hc_num]->hcsplt, hcsplt.d32);

}

/**
 * Attempts to halt a host channel. This function should only be called in
 * Slave mode or to abort a transfer in either Slave mode or DMA mode. Under
 * normal circumstances in DMA mode, the controller halts the channel when the
 * transfer is complete or a condition occurs that requires application
 * intervention.
 *
 * In slave mode, checks for a free request queue entry, then sets the Channel
 * Enable and Channel Disable bits of the Host Channel Characteristics
 * register of the specified channel to intiate the halt. If there is no free
 * request queue entry, sets only the Channel Disable bit of the HCCHARn
 * register to flush requests for this channel. In the latter case, sets a
 * flag to indicate that the host channel needs to be halted when a request
 * queue slot is open.
 *
 * In DMA mode, always sets the Channel Enable and Channel Disable bits of the
 * HCCHARn register. The controller ensures there is space in the request
 * queue before submitting the halt request.
 *
 * Some time may elapse before the core flushes any posted requests for this
 * host channel and halts. The Channel Halted interrupt handler completes the
 * deactivation of the host channel.
 *
 * @param core_if Controller register interface.
 * @param hc Host channel to halt.
 * @param halt_status Reason for halting the channel.
 */
void comip_hc_halt(comip_core_if_t * core_if,
             comip_hc_t * hc, comip_halt_status_e halt_status)
{
    gnptxsts_data_t nptxsts;
    hptxsts_data_t hptxsts;
    hcchar_data_t hcchar;
    comip_hc_regs_t *hc_regs;
    comip_core_global_regs_t *global_regs;
    comip_host_global_regs_t *host_global_regs;

    hc_regs = core_if->host_if->hc_regs[hc->hc_num];
    global_regs = core_if->core_global_regs;
    host_global_regs = core_if->host_if->host_global_regs;

    COMIP_ASSERT(!(halt_status == COMIP_HC_XFER_NO_HALT_STATUS),
           "halt_status = %d\n", halt_status);

    if (halt_status == COMIP_HC_XFER_URB_DEQUEUE ||
        halt_status == COMIP_HC_XFER_AHB_ERR) {
        /*
         * Disable all channel interrupts except Ch Halted. The QTD
         * and QH state associated with this transfer has been cleared
         * (in the case of URB_DEQUEUE), so the channel needs to be
         * shut down carefully to prevent crashes.
         */
        hcintmsk_data_t hcintmsk;
        hcintmsk.d32 = 0;
        hcintmsk.b.chhltd = 1;
        COMIP_WRITE_REG32(&hc_regs->hcintmsk, hcintmsk.d32);

        /*
         * Make sure no other interrupts besides halt are currently
         * pending. Handling another interrupt could cause a crash due
         * to the QTD and QH state.
         */
        COMIP_WRITE_REG32(&hc_regs->hcint, ~hcintmsk.d32);

        /*
         * Make sure the halt status is set to URB_DEQUEUE or AHB_ERR
         * even if the channel was already halted for some other
         * reason.
         */
        hc->halt_status = halt_status;

        hcchar.d32 = readl(&hc_regs->hcchar);
        if (hcchar.b.chen == 0) {
            /*
             * The channel is either already halted or it hasn't
             * started yet. In DMA mode, the transfer may halt if
             * it finishes normally or a condition occurs that
             * requires driver intervention. Don't want to halt
             * the channel again. In either Slave or DMA mode,
             * it's possible that the transfer has been assigned
             * to a channel, but not started yet when an URB is
             * dequeued. Don't want to halt a channel that hasn't
             * started yet.
             */
            return;
        }
    }
    if (hc->halt_pending) {
        /*
         * A halt has already been issued for this channel. This might
         * happen when a transfer is aborted by a higher level in
         * the stack.
         */
#ifdef DEBUG
        COMIP_PRINTF
            ("*** %s: Channel %d, _hc->halt_pending already set ***\n",
             __func__, hc->hc_num);

#endif
        return;
    }

    hcchar.d32 = readl(&hc_regs->hcchar);

    /* No need to set the bit in DDMA for disabling the channel */
    //TODO check it everywhere channel is disabled          
    if (!core_if->core_params->dma_desc_enable)
        hcchar.b.chen = 1;
    hcchar.b.chdis = 1;

    if (!core_if->dma_enable) {
        /* Check for space in the request queue to issue the halt. */
        if (hc->ep_type == COMIP_EP_TYPE_CONTROL ||
            hc->ep_type == COMIP_EP_TYPE_BULK) {
            nptxsts.d32 = readl(&global_regs->gnptxsts);
            if (nptxsts.b.nptxqspcavail == 0) {
                hcchar.b.chen = 0;
            }
        } else {
            hptxsts.d32 =
                readl(&host_global_regs->hptxsts);
            if ((hptxsts.b.ptxqspcavail == 0)
                || (core_if->queuing_high_bandwidth)) {
                hcchar.b.chen = 0;
            }
        }
    }
    COMIP_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);

    hc->halt_status = halt_status;

    if (hcchar.b.chen) {
        hc->halt_pending = 1;
        hc->halt_on_queue = 0;
    } else {
        hc->halt_on_queue = 1;
    }

    COMIP_DEBUGPL(DBG_HCDV, "%s: Channel %d\n", __func__, hc->hc_num);
    COMIP_DEBUGPL(DBG_HCDV, "   hcchar: 0x%08x\n", hcchar.d32);
    COMIP_DEBUGPL(DBG_HCDV, "   halt_pending: %d\n", hc->halt_pending);
    COMIP_DEBUGPL(DBG_HCDV, "   halt_on_queue: %d\n", hc->halt_on_queue);
    COMIP_DEBUGPL(DBG_HCDV, "   halt_status: %d\n", hc->halt_status);

    return;
}

/**
 * Clears the transfer state for a host channel. This function is normally
 * called after a transfer is done and the host channel is being released.
 *
 * @param core_if Programming view of COMIP_otg controller.
 * @param hc Identifies the host channel to clean up.
 */
void comip_hc_cleanup(comip_core_if_t * core_if, comip_hc_t * hc)
{
    comip_hc_regs_t *hc_regs;

    hc->xfer_started = 0;

    /*
     * Clear channel interrupt enables and any unhandled channel interrupt
     * conditions.
     */
    hc_regs = core_if->host_if->hc_regs[hc->hc_num];
    COMIP_WRITE_REG32(&hc_regs->hcintmsk, 0);
    COMIP_WRITE_REG32(&hc_regs->hcint, 0xFFFFFFFF);
#ifdef DEBUG
    COMIP_TIMER_CANCEL(core_if->hc_xfer_timer[hc->hc_num]);
#endif
}

/**
 * Sets the channel property that indicates in which frame a periodic transfer
 * should occur. This is always set to the _next_ frame. This function has no
 * effect on non-periodic transfers.
 *
 * @param core_if Programming view of COMIP_otg controller.
 * @param hc Identifies the host channel to set up and its properties.
 * @param hcchar Current value of the HCCHAR register for the specified host
 * channel.
 */
static inline void hc_set_even_odd_frame(comip_core_if_t * core_if,
                     comip_hc_t * hc, hcchar_data_t * hcchar)
{
    if (hc->ep_type == COMIP_EP_TYPE_INTR ||
        hc->ep_type == COMIP_EP_TYPE_ISOC) {
        hfnum_data_t hfnum;
        hfnum.d32 =
            readl(&core_if->host_if->host_global_regs->hfnum);

        /* 1 if _next_ frame is odd, 0 if it's even */
        hcchar->b.oddfrm = (hfnum.b.frnum & 0x1) ? 0 : 1;
#ifdef DEBUG
        if (hc->ep_type == COMIP_EP_TYPE_INTR && hc->do_split
            && !hc->complete_split) {
            switch (hfnum.b.frnum & 0x7) {
            case 7:
                core_if->hfnum_7_samples++;
                core_if->hfnum_7_frrem_accum += hfnum.b.frrem;
                break;
            case 0:
                core_if->hfnum_0_samples++;
                core_if->hfnum_0_frrem_accum += hfnum.b.frrem;
                break;
            default:
                core_if->hfnum_other_samples++;
                core_if->hfnum_other_frrem_accum +=
                    hfnum.b.frrem;
                break;
            }
        }
#endif
    }
}

#ifdef DEBUG
void hc_xfer_timeout(void *ptr)
{
    hc_xfer_info_t *xfer_info = NULL;
    int hc_num = 0;

    if (ptr)
        xfer_info = (hc_xfer_info_t *) ptr;

    if (!xfer_info->hc) {
        COMIP_ERROR("xfer_info->hc = %p\n", xfer_info->hc);
        return;
    }

    hc_num = xfer_info->hc->hc_num;
    COMIP_WARN("%s: timeout on channel %d\n", __func__, hc_num);
    COMIP_WARN("   start_hcchar_val 0x%08x\n",
         xfer_info->core_if->start_hcchar_val[hc_num]);
}
#endif
void set_pid_isoc(comip_hc_t * hc)
{
    /* Set up the initial PID for the transfer. */
    if (hc->speed == COMIP_EP_SPEED_HIGH) {
        if (hc->ep_is_in) {
            if (hc->multi_count == 1) {
                hc->data_pid_start = COMIP_HC_PID_DATA0;
            } else if (hc->multi_count == 2) {
                hc->data_pid_start = COMIP_HC_PID_DATA1;
            } else {
                hc->data_pid_start = COMIP_HC_PID_DATA2;
            }
        } else {
            if (hc->multi_count == 1) {
                hc->data_pid_start = COMIP_HC_PID_DATA0;
            } else {
                hc->data_pid_start = COMIP_HC_PID_MDATA;
            }
        }
    } else {
        hc->data_pid_start = COMIP_HC_PID_DATA0;
    }
}

/**
 * This function does the setup for a data transfer for a host channel and
 * starts the transfer. May be called in either Slave mode or DMA mode. In
 * Slave mode, the caller must ensure that there is sufficient space in the
 * request queue and Tx Data FIFO.
 *
 * For an OUT transfer in Slave mode, it loads a data packet into the
 * appropriate FIFO. If necessary, additional data packets will be loaded in
 * the Host ISR.
 *
 * For an IN transfer in Slave mode, a data packet is requested. The data
 * packets are unloaded from the Rx FIFO in the Host ISR. If necessary,
 * additional data packets are requested in the Host ISR.
 *
 * For a PING transfer in Slave mode, the Do Ping bit is set in the HCTSIZ
 * register along with a packet count of 1 and the channel is enabled. This
 * causes a single PING transaction to occur. Other fields in HCTSIZ are
 * simply set to 0 since no data transfer occurs in this case.
 *
 * For a PING transfer in DMA mode, the HCTSIZ register is initialized with
 * all the information required to perform the subsequent data transfer. In
 * addition, the Do Ping bit is set in the HCTSIZ register. In this case, the
 * controller performs the entire PING protocol, then starts the data
 * transfer.
 *
 * @param core_if Programming view of COMIP_otg controller.
 * @param hc Information needed to initialize the host channel. The xfer_len
 * value may be reduced to accommodate the max widths of the XferSize and
 * PktCnt fields in the HCTSIZn register. The multi_count value may be changed
 * to reflect the final xfer_len value.
 */
void comip_hc_start_transfer(comip_core_if_t * core_if, comip_hc_t * hc)
{
    hcchar_data_t hcchar;
    hctsiz_data_t hctsiz;
    uint16_t num_packets;
    uint32_t max_hc_xfer_size = core_if->core_params->max_transfer_size;
    uint16_t max_hc_pkt_count = core_if->core_params->max_packet_count;
    comip_hc_regs_t *hc_regs = core_if->host_if->hc_regs[hc->hc_num];

    hctsiz.d32 = 0;

    if (hc->do_ping) {
        if (!core_if->dma_enable) {
            comip_hc_do_ping(core_if, hc);
            hc->xfer_started = 1;
            return;
        } else {
            hctsiz.b.dopng = 1;
        }
    }

    if (hc->do_split) {
        num_packets = 1;

        if (hc->complete_split && !hc->ep_is_in) {
            /* For CSPLIT OUT Transfer, set the size to 0 so the
             * core doesn't expect any data written to the FIFO */
            hc->xfer_len = 0;
        } else if (hc->ep_is_in || (hc->xfer_len > hc->max_packet)) {
            hc->xfer_len = hc->max_packet;
        } else if (!hc->ep_is_in && (hc->xfer_len > 188)) {
            hc->xfer_len = 188;
        }

        hctsiz.b.xfersize = hc->xfer_len;
    } else {
        /*
         * Ensure that the transfer length and packet count will fit
         * in the widths allocated for them in the HCTSIZn register.
         */
        if (hc->ep_type == COMIP_EP_TYPE_INTR ||
            hc->ep_type == COMIP_EP_TYPE_ISOC) {
            /*
             * Make sure the transfer size is no larger than one
             * (micro)frame's worth of data. (A check was done
             * when the periodic transfer was accepted to ensure
             * that a (micro)frame's worth of data can be
             * programmed into a channel.)
             */
            uint32_t max_periodic_len =
                hc->multi_count * hc->max_packet;
            if (hc->xfer_len > max_periodic_len) {
                hc->xfer_len = max_periodic_len;
            } else {
            }
        } else if (hc->xfer_len > max_hc_xfer_size) {
            /* Make sure that xfer_len is a multiple of max packet size. */
            hc->xfer_len = max_hc_xfer_size - hc->max_packet + 1;
        }

        if (hc->xfer_len > 0) {
            num_packets =
                (hc->xfer_len + hc->max_packet -
                 1) / hc->max_packet;
            if (num_packets > max_hc_pkt_count) {
                num_packets = max_hc_pkt_count;
                hc->xfer_len = num_packets * hc->max_packet;
            }
        } else {
            /* Need 1 packet for transfer length of 0. */
            num_packets = 1;
        }

        if (hc->ep_is_in) {
            /* Always program an integral # of max packets for IN transfers. */
            hc->xfer_len = num_packets * hc->max_packet;
        }

        if (hc->ep_type == COMIP_EP_TYPE_INTR ||
            hc->ep_type == COMIP_EP_TYPE_ISOC) {
            /*
             * Make sure that the multi_count field matches the
             * actual transfer length.
             */
            hc->multi_count = num_packets;
        }

        if (hc->ep_type == COMIP_EP_TYPE_ISOC)
            set_pid_isoc(hc);

        hctsiz.b.xfersize = hc->xfer_len;
    }

    hc->start_pkt_count = num_packets;
    hctsiz.b.pktcnt = num_packets;
    hctsiz.b.pid = hc->data_pid_start;
    COMIP_WRITE_REG32(&hc_regs->hctsiz, hctsiz.d32);

    COMIP_DEBUGPL(DBG_HCDV, "%s: Channel %d\n", __func__, hc->hc_num);
    COMIP_DEBUGPL(DBG_HCDV, "   Xfer Size: %d\n", hctsiz.b.xfersize);
    COMIP_DEBUGPL(DBG_HCDV, "   Num Pkts: %d\n", hctsiz.b.pktcnt);
    COMIP_DEBUGPL(DBG_HCDV, "   Start PID: %d\n", hctsiz.b.pid);

    if (core_if->dma_enable) {
        comip_dma_t dma_addr;
        if (hc->align_buff) {
            dma_addr = hc->align_buff;
        } else {
            dma_addr = ((uint32_t)hc->xfer_buff & 0xffffffff);
        }
        COMIP_WRITE_REG32(&hc_regs->hcdma, dma_addr);
    }

    /* Start the split */
    if (hc->do_split) {
        hcsplt_data_t hcsplt;
        hcsplt.d32 = readl(&hc_regs->hcsplt);
        hcsplt.b.spltena = 1;
        COMIP_WRITE_REG32(&hc_regs->hcsplt, hcsplt.d32);
    }

    hcchar.d32 = readl(&hc_regs->hcchar);
    hcchar.b.multicnt = hc->multi_count;
    hc_set_even_odd_frame(core_if, hc, &hcchar);
#ifdef DEBUG
    core_if->start_hcchar_val[hc->hc_num] = hcchar.d32;
    if (hcchar.b.chdis) {
        COMIP_WARN("%s: chdis set, channel %d, hcchar 0x%08x\n",
             __func__, hc->hc_num, hcchar.d32);
    }
#endif

    /* Set host channel enable after all other setup is complete. */
    hcchar.b.chen = 1;
    hcchar.b.chdis = 0;
    COMIP_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);

    hc->xfer_started = 1;
    hc->requests++;

    if (!core_if->dma_enable && !hc->ep_is_in && hc->xfer_len > 0) {
        /* Load OUT packet into the appropriate Tx FIFO. */
        comip_hc_write_packet(core_if, hc);
    }
#ifdef DEBUG
    if (hc->ep_type != COMIP_EP_TYPE_INTR) {
        core_if->hc_xfer_info[hc->hc_num].core_if = core_if;
        core_if->hc_xfer_info[hc->hc_num].hc = hc;

        /* Start a timer for this transfer. */
        COMIP_TIMER_SCHEDULE(core_if->hc_xfer_timer[hc->hc_num], 10000);
    }
#endif
}

/**
 * This function does the setup for a data transfer for a host channel
 * and starts the transfer in Descriptor DMA mode.
 *
 * Initializes HCTSIZ register. For a PING transfer the Do Ping bit is set.
 * Sets PID and NTD values. For periodic transfers
 * initializes SCHED_INFO field with micro-frame bitmap.
 *
 * Initializes HCDMA register with descriptor list address and CTD value
 * then starts the transfer via enabling the channel.
 *
 * @param core_if Programming view of COMIP_otg controller.
 * @param hc Information needed to initialize the host channel.
 */
void comip_hc_start_transfer_ddma(comip_core_if_t * core_if, comip_hc_t * hc)
{
    comip_hc_regs_t *hc_regs = core_if->host_if->hc_regs[hc->hc_num];
    hcchar_data_t hcchar;
    hctsiz_data_t hctsiz;
    hcdma_data_t hcdma;

    hctsiz.d32 = 0;

    if (hc->do_ping)
        hctsiz.b_ddma.dopng = 1;

    if (hc->ep_type == COMIP_EP_TYPE_ISOC)
        set_pid_isoc(hc);

    /* Packet Count and Xfer Size are not used in Descriptor DMA mode */
    hctsiz.b_ddma.pid = hc->data_pid_start;
    hctsiz.b_ddma.ntd = hc->ntd - 1;    /* 0 - 1 descriptor, 1 - 2 descriptors, etc. */
    hctsiz.b_ddma.schinfo = hc->schinfo;    /* Non-zero only for high-speed interrupt endpoints */

    COMIP_DEBUGPL(DBG_HCDV, "%s: Channel %d\n", __func__, hc->hc_num);
    COMIP_DEBUGPL(DBG_HCDV, "   Start PID: %d\n", hctsiz.b.pid);
    COMIP_DEBUGPL(DBG_HCDV, "   NTD: %d\n", hctsiz.b_ddma.ntd);

    COMIP_WRITE_REG32(&hc_regs->hctsiz, hctsiz.d32);

    hcdma.d32 = 0;
    hcdma.b.dma_addr = ((uint32_t) hc->desc_list_addr) >> 11;

    /* Always start from first descriptor. */
    hcdma.b.ctd = 0;
    COMIP_WRITE_REG32(&hc_regs->hcdma, hcdma.d32);

    hcchar.d32 = readl(&hc_regs->hcchar);
    hcchar.b.multicnt = hc->multi_count;

#ifdef DEBUG
    core_if->start_hcchar_val[hc->hc_num] = hcchar.d32;
    if (hcchar.b.chdis) {
        COMIP_WARN("%s: chdis set, channel %d, hcchar 0x%08x\n",
             __func__, hc->hc_num, hcchar.d32);
    }
#endif

    /* Set host channel enable after all other setup is complete. */
    hcchar.b.chen = 1;
    hcchar.b.chdis = 0;

    COMIP_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);

    hc->xfer_started = 1;
    hc->requests++;

#ifdef DEBUG
    if ((hc->ep_type != COMIP_EP_TYPE_INTR)
        && (hc->ep_type != COMIP_EP_TYPE_ISOC)) {
        core_if->hc_xfer_info[hc->hc_num].core_if = core_if;
        core_if->hc_xfer_info[hc->hc_num].hc = hc;
        /* Start a timer for this transfer. */
        COMIP_TIMER_SCHEDULE(core_if->hc_xfer_timer[hc->hc_num], 10000);
    }
#endif

}

/**
 * This function continues a data transfer that was started by previous call
 * to <code>comip_hc_start_transfer</code>. The caller must ensure there is
 * sufficient space in the request queue and Tx Data FIFO. This function
 * should only be called in Slave mode. In DMA mode, the controller acts
 * autonomously to complete transfers programmed to a host channel.
 *
 * For an OUT transfer, a new data packet is loaded into the appropriate FIFO
 * if there is any data remaining to be queued. For an IN transfer, another
 * data packet is always requested. For the SETUP phase of a control transfer,
 * this function does nothing.
 *
 * @return 1 if a new request is queued, 0 if no more requests are required
 * for this transfer.
 */
int comip_hc_continue_transfer(comip_core_if_t * core_if, comip_hc_t * hc)
{
    COMIP_DEBUGPL(DBG_HCDV, "%s: Channel %d\n", __func__, hc->hc_num);

    if (hc->do_split) {
        /* SPLITs always queue just once per channel */
        return 0;
    } else if (hc->data_pid_start == COMIP_HC_PID_SETUP) {
        /* SETUPs are queued only once since they can't be NAKed. */
        return 0;
    } else if (hc->ep_is_in) {
        /*
         * Always queue another request for other IN transfers. If
         * back-to-back INs are issued and NAKs are received for both,
         * the driver may still be processing the first NAK when the
         * second NAK is received. When the interrupt handler clears
         * the NAK interrupt for the first NAK, the second NAK will
         * not be seen. So we can't depend on the NAK interrupt
         * handler to requeue a NAKed request. Instead, IN requests
         * are issued each time this function is called. When the
         * transfer completes, the extra requests for the channel will
         * be flushed.
         */
        hcchar_data_t hcchar;
        comip_hc_regs_t *hc_regs =
            core_if->host_if->hc_regs[hc->hc_num];

        hcchar.d32 = readl(&hc_regs->hcchar);
        hc_set_even_odd_frame(core_if, hc, &hcchar);
        hcchar.b.chen = 1;
        hcchar.b.chdis = 0;
        COMIP_DEBUGPL(DBG_HCDV, "   IN xfer: hcchar = 0x%08x\n",
                hcchar.d32);
        COMIP_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);
        hc->requests++;
        return 1;
    } else {
        /* OUT transfers. */
        if (hc->xfer_count < hc->xfer_len) {
            if (hc->ep_type == COMIP_EP_TYPE_INTR ||
                hc->ep_type == COMIP_EP_TYPE_ISOC) {
                hcchar_data_t hcchar;
                comip_hc_regs_t *hc_regs;
                hc_regs = core_if->host_if->hc_regs[hc->hc_num];
                hcchar.d32 = readl(&hc_regs->hcchar);
                hc_set_even_odd_frame(core_if, hc, &hcchar);
            }

            /* Load OUT packet into the appropriate Tx FIFO. */
            comip_hc_write_packet(core_if, hc);
            hc->requests++;
            return 1;
        } else {
            return 0;
        }
    }
}

/**
 * Starts a PING transfer. This function should only be called in Slave mode.
 * The Do Ping bit is set in the HCTSIZ register, then the channel is enabled.
 */
void comip_hc_do_ping(comip_core_if_t * core_if, comip_hc_t * hc)
{
    hcchar_data_t hcchar;
    hctsiz_data_t hctsiz;
    comip_hc_regs_t *hc_regs = core_if->host_if->hc_regs[hc->hc_num];

    COMIP_DEBUGPL(DBG_HCDV, "%s: Channel %d\n", __func__, hc->hc_num);

    hctsiz.d32 = 0;
    hctsiz.b.dopng = 1;
    hctsiz.b.pktcnt = 1;
    COMIP_WRITE_REG32(&hc_regs->hctsiz, hctsiz.d32);

    hcchar.d32 = readl(&hc_regs->hcchar);
    hcchar.b.chen = 1;
    hcchar.b.chdis = 0;
    COMIP_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);
}

/*
 * This function writes a packet into the Tx FIFO associated with the Host
 * Channel. For a channel associated with a non-periodic EP, the non-periodic
 * Tx FIFO is written. For a channel associated with a periodic EP, the
 * periodic Tx FIFO is written. This function should only be called in Slave
 * mode.
 *
 * Upon return the xfer_buff and xfer_count fields in _hc are incremented by
 * then number of bytes written to the Tx FIFO.
 */
void comip_hc_write_packet(comip_core_if_t * core_if, comip_hc_t * hc)
{
    uint32_t i;
    uint32_t remaining_count;
    uint32_t byte_count;
    uint32_t dword_count;

    uint32_t *data_buff = (uint32_t *) (hc->xfer_buff);
    uint32_t *data_fifo = core_if->data_fifo[hc->hc_num];

    remaining_count = hc->xfer_len - hc->xfer_count;
    if (remaining_count > hc->max_packet) {
        byte_count = hc->max_packet;
    } else {
        byte_count = remaining_count;
    }

    dword_count = (byte_count + 3) / 4;

    if ((((uint32_t)data_buff) & 0x3) == 0) {
        /* xfer_buff is DWORD aligned. */
        for (i = 0; i < dword_count; i++, data_buff++) {
            COMIP_WRITE_REG32(data_fifo, *data_buff);
        }
    } else {
        /* xfer_buff is not DWORD aligned. */
        for (i = 0; i < dword_count; i++, data_buff++) {
            uint32_t data;
            data =
                (data_buff[0] | data_buff[1] << 8 | data_buff[2] <<
                 16 | data_buff[3] << 24);
            COMIP_WRITE_REG32(data_fifo, data);
        }
    }

    hc->xfer_count += byte_count;
    hc->xfer_buff += byte_count;
}

/**
 * Calculates and gets the frame Interval value of HFIR register according PHY 
 * type and speed.The application can modify a value of HFIR register only after
 * the Port Enable bit of the Host Port Control and Status register 
 * (HPRT.PrtEnaPort) has been set.
*/

uint32_t calc_frame_interval(comip_core_if_t * core_if)
{
    gusbcfg_data_t usbcfg;
    hwcfg2_data_t hwcfg2;
    hprt0_data_t hprt0;
    int clock = 60;     // default value
    usbcfg.d32 = readl(&core_if->core_global_regs->gusbcfg);
    hwcfg2.d32 = readl(&core_if->core_global_regs->ghwcfg2);
    hprt0.d32 = readl(core_if->host_if->hprt0);
    if (!usbcfg.b.physel && usbcfg.b.ulpi_utmi_sel && !usbcfg.b.phyif)
        clock = 60;
    if (usbcfg.b.physel && hwcfg2.b.fs_phy_type == 3)
        clock = 48;
    if (!usbcfg.b.phylpwrclksel && !usbcfg.b.physel &&
        !usbcfg.b.ulpi_utmi_sel && usbcfg.b.phyif)
        clock = 30;
    if (!usbcfg.b.phylpwrclksel && !usbcfg.b.physel &&
        !usbcfg.b.ulpi_utmi_sel && !usbcfg.b.phyif)
        clock = 60;
    if (usbcfg.b.phylpwrclksel && !usbcfg.b.physel &&
        !usbcfg.b.ulpi_utmi_sel && usbcfg.b.phyif)
        clock = 48;
    if (usbcfg.b.physel && !usbcfg.b.phyif && hwcfg2.b.fs_phy_type == 2)
        clock = 48;
    if (usbcfg.b.physel && hwcfg2.b.fs_phy_type == 1)
        clock = 48;
    if (hprt0.b.prtspd == 0)
        /* High speed case */
        return 125 * clock;
    else
        /* FS/LS case */
        return 1000 * clock;
}

#ifdef DEBUG
void dump_msg(const u8 * buf, uint32_t length)
{
    uint32_t start, num, i;
    char line[52], *p;

    if (length >= 512)
        return;
    start = 0;
    while (length > 0) {
        num = length < 16u ? length : 16u;
        p = line;
        for (i = 0; i < num; ++i) {
            if (i == 8)
                *p++ = ' ';
            COMIP_SPRINTF(p, " %02x", buf[i]);
            p += 3;
        }
        *p = 0;
        COMIP_PRINTF("%6x: %s\n", start, line);
        buf += num;
        start += num;
        length -= num;
    }
}
#else
static inline void dump_msg(const u8 * buf, uint32_t length)
{
}
#endif

/**
 * This function writes a packet into the Tx FIFO associated with the
 * EP. For non-periodic EPs the non-periodic Tx FIFO is written.  For
 * periodic EPs the periodic Tx FIFO associated with the EP is written
 * with all packets for the next micro-frame.
 *
 * @param core_if Programming view of COMIP_otg controller.
 * @param ep The EP to write packet for.
 * @param dma Indicates if DMA is being used.
 */
void comip_ep_write_packet(comip_core_if_t * core_if, comip_ep_t * ep,
                 int dma)
{
    /**
     * The buffer is padded to DWORD on a per packet basis in
     * slave/dma mode if the MPS is not DWORD aligned. The last
     * packet, if short, is also padded to a multiple of DWORD.
     *
     * ep->xfer_buff always starts DWORD aligned in memory and is a
     * multiple of DWORD in length
     *
     * ep->xfer_len can be any number of bytes
     *
     * ep->xfer_count is a multiple of ep->maxpacket until the last
     *  packet
     *
     * FIFO access is DWORD */

    uint32_t i;
    uint32_t byte_count;
    uint32_t dword_count;
    uint32_t *fifo;
    uint32_t *data_buff = (uint32_t *) ep->xfer_buff;

    COMIP_DEBUGPL((DBG_PCDV | DBG_CILV), "%s(%p,%p)\n", __func__, core_if,
            ep);
    if (ep->xfer_count >= ep->xfer_len) {
        COMIP_WARN("%s() No data for EP%d!!!\n", __func__, ep->num);
        return;
    }

    /* Find the byte length of the packet either short packet or MPS */
    if ((ep->xfer_len - ep->xfer_count) < ep->maxpacket) {
        byte_count = ep->xfer_len - ep->xfer_count;
    } else {
        byte_count = ep->maxpacket;
    }

    /* Find the DWORD length, padded by extra bytes as neccessary if MPS
     * is not a multiple of DWORD */
    dword_count = (byte_count + 3) / 4;

#ifdef VERBOSE
    dump_msg(ep->xfer_buff, byte_count);
#endif

    /**@todo NGS Where are the Periodic Tx FIFO addresses
     * intialized?  What should this be? */

    fifo = core_if->data_fifo[ep->num];

    COMIP_DEBUGPL((DBG_PCDV | DBG_CILV), "fifo=%p buff=%p *p=%08x bc=%d\n",
            fifo, data_buff, *data_buff, byte_count);

    if (!dma) {
        for (i = 0; i < dword_count; i++, data_buff++) {
            COMIP_WRITE_REG32(fifo, *data_buff);
        }
    }

    ep->xfer_count += byte_count;
    ep->xfer_buff += byte_count;
    ep->dma_addr += byte_count;
}

/**
 * This function reads a packet from the Rx FIFO into the destination
 * buffer. To read SETUP data use comip_read_setup_packet.
 *
 * @param core_if Programming view of COMIP_otg controller.
 * @param dest    Destination buffer for the packet.
 * @param bytes  Number of bytes to copy to the destination.
 */
void comip_read_packet(comip_core_if_t * core_if,
             uint8_t * dest, uint16_t bytes)
{
    int i;
    int word_count = (bytes + 3) / 4;

    volatile uint32_t *fifo = core_if->data_fifo[0];
    uint32_t *data_buff = (uint32_t *) dest;

    /**
     * @todo Account for the case where _dest is not dword aligned. This
     * requires reading data from the FIFO into a uint32_t temp buffer,
     * then moving it into the data buffer.
     */

    COMIP_DEBUGPL((DBG_PCDV | DBG_CILV), "%s(%p,%p,%d)\n", __func__,
            core_if, dest, bytes);

    for (i = 0; i < word_count; i++, data_buff++) {
        *data_buff = readl(fifo);
    }

    return;
}
/**
 * This functions reads the SPRAM and prints its content
 *
 * @param core_if Programming view of COMIP_otg controller.
 */
void comip_dump_spram(comip_core_if_t * core_if)
{
    volatile uint8_t *addr, *start_addr, *end_addr;

    COMIP_PRINTF("SPRAM Data:\n");
    start_addr = (void *)core_if->core_global_regs;
    COMIP_PRINTF("Base Address: 0x%8lX\n", (unsigned long)start_addr);
    start_addr += 0x00028000;
    end_addr = (void *)core_if->core_global_regs;
    end_addr += 0x000280e0;

    for (addr = start_addr; addr < end_addr; addr += 16) {
        COMIP_PRINTF
            ("0x%8lX:\t%2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X\n",
             (unsigned long)addr, addr[0], addr[1], addr[2], addr[3],
             addr[4], addr[5], addr[6], addr[7], addr[8], addr[9],
             addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]
            );
    }

    return;
}

/**
 * This function reads the host registers and prints them
 *
 * @param core_if Programming view of COMIP_otg controller.
 */
void comip_dump_host_registers(comip_core_if_t * core_if)
{
    int i;
    volatile uint32_t *addr;

    COMIP_PRINTF("Host Global Registers\n");
    addr = &core_if->host_if->host_global_regs->hcfg;
    COMIP_PRINTF("HCFG      @0x%08lX : 0x%08X\n",
           (unsigned long)addr, readl(addr));
    addr = &core_if->host_if->host_global_regs->hfir;
    COMIP_PRINTF("HFIR      @0x%08lX : 0x%08X\n",
           (unsigned long)addr, readl(addr));
    addr = &core_if->host_if->host_global_regs->hfnum;
    COMIP_PRINTF("HFNUM     @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->host_if->host_global_regs->hptxsts;
    COMIP_PRINTF("HPTXSTS   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->host_if->host_global_regs->haint;
    COMIP_PRINTF("HAINT     @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->host_if->host_global_regs->haintmsk;
    COMIP_PRINTF("HAINTMSK  @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    if (core_if->dma_desc_enable) {
        addr = &core_if->host_if->host_global_regs->hflbaddr;
        COMIP_PRINTF("HFLBADDR  @0x%08lX : 0x%08X\n",
               (unsigned long)addr, readl(addr));
    }

    addr = core_if->host_if->hprt0;
    COMIP_PRINTF("HPRT0     @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));

    for (i = 0; i < core_if->core_params->host_channels; i++) {
        COMIP_PRINTF("Host Channel %d Specific Registers\n", i);
        addr = &core_if->host_if->hc_regs[i]->hcchar;
        COMIP_PRINTF("HCCHAR    @0x%08lX : 0x%08X\n",
               (unsigned long)addr, readl(addr));
        addr = &core_if->host_if->hc_regs[i]->hcsplt;
        COMIP_PRINTF("HCSPLT    @0x%08lX : 0x%08X\n",
               (unsigned long)addr, readl(addr));
        addr = &core_if->host_if->hc_regs[i]->hcint;
        COMIP_PRINTF("HCINT     @0x%08lX : 0x%08X\n",
               (unsigned long)addr, readl(addr));
        addr = &core_if->host_if->hc_regs[i]->hcintmsk;
        COMIP_PRINTF("HCINTMSK  @0x%08lX : 0x%08X\n",
               (unsigned long)addr, readl(addr));
        addr = &core_if->host_if->hc_regs[i]->hctsiz;
        COMIP_PRINTF("HCTSIZ    @0x%08lX : 0x%08X\n",
               (unsigned long)addr, readl(addr));
        addr = &core_if->host_if->hc_regs[i]->hcdma;
        COMIP_PRINTF("HCDMA     @0x%08lX : 0x%08X\n",
               (unsigned long)addr, readl(addr));
        if (core_if->dma_desc_enable) {
            addr = &core_if->host_if->hc_regs[i]->hcdmab;
            COMIP_PRINTF("HCDMAB    @0x%08lX : 0x%08X\n",
                   (unsigned long)addr, readl(addr));
        }

    }
    return;
}

/**
 * This function reads the core global registers and prints them
 *
 * @param core_if Programming view of COMIP_otg controller.
 */
void comip_dump_global_registers(comip_core_if_t * core_if)
{
    int i, ep_num;
    volatile uint32_t *addr;
    char *txfsiz;

    COMIP_PRINTF("Core Global Registers\n");
    addr = &core_if->core_global_regs->gotgctl;
    COMIP_PRINTF("GOTGCTL   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gotgint;
    COMIP_PRINTF("GOTGINT   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gahbcfg;
    COMIP_PRINTF("GAHBCFG   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gusbcfg;
    COMIP_PRINTF("GUSBCFG   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->grstctl;
    COMIP_PRINTF("GRSTCTL   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gintsts;
    COMIP_PRINTF("GINTSTS   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gintmsk;
    COMIP_PRINTF("GINTMSK   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->grxstsr;
    COMIP_PRINTF("GRXSTSR   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->grxfsiz;
    COMIP_PRINTF("GRXFSIZ   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gnptxfsiz;
    COMIP_PRINTF("GNPTXFSIZ @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gnptxsts;
    COMIP_PRINTF("GNPTXSTS  @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gi2cctl;
    COMIP_PRINTF("GI2CCTL   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gpvndctl;
    COMIP_PRINTF("GPVNDCTL  @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->ggpio;
    COMIP_PRINTF("GGPIO     @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->guid;
    COMIP_PRINTF("GUID      @0x%08lX : 0x%08X\n",
           (unsigned long)addr, readl(addr));
    addr = &core_if->core_global_regs->gsnpsid;
    COMIP_PRINTF("GSNPSID   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->ghwcfg1;
    COMIP_PRINTF("GHWCFG1   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->ghwcfg2;
    COMIP_PRINTF("GHWCFG2   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->ghwcfg3;
    COMIP_PRINTF("GHWCFG3   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->ghwcfg4;
    COMIP_PRINTF("GHWCFG4   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->glpmcfg;
    COMIP_PRINTF("GLPMCFG   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gpwrdn;
    COMIP_PRINTF("GPWRDN    @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->gdfifocfg;
    COMIP_PRINTF("GDFIFOCFG     @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->adpctl;
    COMIP_PRINTF("ADPCTL    @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
    addr = &core_if->core_global_regs->hptxfsiz;
    COMIP_PRINTF("HPTXFSIZ  @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));

    if (core_if->en_multiple_tx_fifo == 0) {
        ep_num = core_if->hwcfg4.b.num_dev_perio_in_ep;
        txfsiz = "DPTXFSIZ";
    } else {
        ep_num = core_if->hwcfg4.b.num_in_eps;
        txfsiz = "DIENPTXF";
    }
    for (i = 0; i < ep_num; i++) {
        addr = &core_if->core_global_regs->dtxfsiz[i];
        COMIP_PRINTF("%s[%d] @0x%08lX : 0x%08X\n", txfsiz, i + 1,
               (unsigned long)addr, readl(addr));
    }
    addr = core_if->pcgcctl;
    COMIP_PRINTF("PCGCCTL   @0x%08lX : 0x%08X\n", (unsigned long)addr,
           readl(addr));
}

/**
 * Flush a Tx FIFO.
 *
 * @param core_if Programming view of COMIP_otg controller.
 * @param num Tx FIFO to flush.
 */
void comip_flush_tx_fifo(comip_core_if_t * core_if, const int num)
{
    comip_core_global_regs_t *global_regs = core_if->core_global_regs;
    volatile grstctl_t greset = {.d32 = 0 };
    int count = 0;

    COMIP_DEBUGPL((DBG_CIL | DBG_PCDV), "Flush Tx FIFO %d\n", num);

    greset.b.txfflsh = 1;
    greset.b.txfnum = num;
    COMIP_WRITE_REG32(&global_regs->grstctl, greset.d32);

    do {
        greset.d32 = readl(&global_regs->grstctl);
        if (++count > 10000) {
            COMIP_WARN("%s() HANG! GRSTCTL=%0x GNPTXSTS=0x%08x\n",
                 __func__, greset.d32,
                 readl(&global_regs->gnptxsts));
            break;
        }
        udelay(1);
    } while (greset.b.txfflsh == 1);

    /* Wait for 3 PHY Clocks */
    udelay(1);
}

/**
 * Flush Rx FIFO.
 *
 * @param core_if Programming view of COMIP_otg controller.
 */
void comip_flush_rx_fifo(comip_core_if_t * core_if)
{
    comip_core_global_regs_t *global_regs = core_if->core_global_regs;
    volatile grstctl_t greset = {.d32 = 0 };
    int count = 0;

    COMIP_DEBUGPL((DBG_CIL | DBG_PCDV), "%s\n", __func__);
    /*
     *
     */
    greset.b.rxfflsh = 1;
    COMIP_WRITE_REG32(&global_regs->grstctl, greset.d32);

    do {
        greset.d32 = readl(&global_regs->grstctl);
        if (++count > 10000) {
            COMIP_WARN("%s() HANG! GRSTCTL=%0x\n", __func__,
                 greset.d32);
            break;
        }
        udelay(1);
    } while (greset.b.rxfflsh == 1);

    /* Wait for 3 PHY Clocks */
    udelay(1);
}

/**
 * Do core a soft reset of the core.  Be careful with this because it
 * resets all the internal state machines of the core.
 */
void comip_core_reset(comip_core_if_t * core_if)
{
    comip_core_global_regs_t *global_regs = core_if->core_global_regs;
    volatile grstctl_t greset = {.d32 = 0 };
    int count = 0;

    COMIP_DEBUGPL(DBG_CILV, "%s\n", __func__);
    /* Wait for AHB master IDLE state. */
    do {
        udelay(10);
        greset.d32 = readl(&global_regs->grstctl);
        if (++count > 100000) {
            COMIP_WARN("%s() HANG! AHB Idle GRSTCTL=%0x\n", __func__,
                 greset.d32);
            return;
        }
    }
    while (greset.b.ahbidle == 0);

    /* Core Soft Reset */
    count = 0;
    greset.b.csftrst = 1;
    COMIP_WRITE_REG32(&global_regs->grstctl, greset.d32);
    do {
        greset.d32 = readl(&global_regs->grstctl);
        if (++count > 10000) {
            COMIP_WARN("%s() HANG! Soft Reset GRSTCTL=%0x\n",
                 __func__, greset.d32);
            break;
        }
        udelay(1);
    }
    while (greset.b.csftrst == 1);

    /* Wait for 3 PHY Clocks */
    mdelay(100);
}

uint8_t comip_is_device_mode(comip_core_if_t * _core_if)
{
    //return (comip_mode(_core_if) != COMIP_HOST_MODE);
    return (!comip_otg_hostmode());
}

uint8_t comip_is_host_mode(comip_core_if_t * _core_if)
{
    //return (comip_mode(_core_if) == COMIP_HOST_MODE);
    return comip_otg_hostmode();
}

/**
 * Register HCD callbacks. The callbacks are used to start and stop
 * the HCD for interrupt processing.
 *
 * @param core_if Programming view of COMIP_otg controller.
 * @param cb the HCD callback structure.
 * @param p pointer to be passed to callback function (usb_hcd*).
 */
void comip_cil_register_hcd_callbacks(comip_core_if_t * core_if,
                    comip_cil_callbacks_t * cb, void *p)
{
    core_if->hcd_cb = cb;
    cb->p = p;
}

static void comip_set_uninitialized(int32_t * p, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        p[i] = -1;
    }
}

static int comip_param_initialized(int32_t val)
{
    return val != -1;
}

static int comip_setup_params(comip_core_if_t * core_if)
{
    core_if->core_params = COMIP_ALLOC(sizeof(*core_if->core_params));
    if (!core_if->core_params) {
        return -ENOMEM;
    }
    comip_set_uninitialized((int32_t *) core_if->core_params,
                  sizeof(*core_if->core_params) /
                  sizeof(int32_t));
    COMIP_PRINTF("Setting default values for core params\n");
    comip_set_param_otg_cap(core_if, comip_param_otg_cap_default);
    comip_set_param_dma_enable(core_if, comip_param_dma_enable_default);
    comip_set_param_dma_desc_enable(core_if,
                      comip_param_dma_desc_enable_default);
    comip_set_param_opt(core_if, comip_param_opt_default);
    comip_set_param_dma_burst_size(core_if,
                     comip_param_dma_burst_size_default);
    comip_set_param_host_support_fs_ls_low_power(core_if,
                               comip_param_host_support_fs_ls_low_power_default);
    comip_set_param_enable_dynamic_fifo(core_if,
                          comip_param_enable_dynamic_fifo_default);
    comip_set_param_data_fifo_size(core_if,
                     comip_param_data_fifo_size_default);
    comip_set_param_host_rx_fifo_size(core_if,
                        comip_param_host_rx_fifo_size_default);
    comip_set_param_host_nperio_tx_fifo_size(core_if,
                           comip_param_host_nperio_tx_fifo_size_default);
    comip_set_param_host_perio_tx_fifo_size(core_if,
                          comip_param_host_perio_tx_fifo_size_default);
    comip_set_param_max_transfer_size(core_if,
                        comip_param_max_transfer_size_default);
    comip_set_param_max_packet_count(core_if,
                       comip_param_max_packet_count_default);
    comip_set_param_host_channels(core_if,
                    comip_param_host_channels_default);
    comip_set_param_dev_endpoints(core_if,
                    comip_param_dev_endpoints_default);
    comip_set_param_phy_type(core_if, comip_param_phy_type_default);
    comip_set_param_speed(core_if, comip_param_speed_default);
    comip_set_param_host_ls_low_power_phy_clk(core_if,
                            comip_param_host_ls_low_power_phy_clk_default);
    comip_set_param_phy_ulpi_ddr(core_if, comip_param_phy_ulpi_ddr_default);
    comip_set_param_phy_ulpi_ext_vbus(core_if,
                        comip_param_phy_ulpi_ext_vbus_default);
    comip_set_param_phy_utmi_width(core_if,
                     comip_param_phy_utmi_width_default);
    comip_set_param_ts_dline(core_if, comip_param_ts_dline_default);
    comip_set_param_i2c_enable(core_if, comip_param_i2c_enable_default);
    comip_set_param_ulpi_fs_ls(core_if, comip_param_ulpi_fs_ls_default);
    comip_set_param_en_multiple_tx_fifo(core_if,
                          comip_param_en_multiple_tx_fifo_default);
    comip_set_param_thr_ctl(core_if, comip_param_thr_ctl_default);
    comip_set_param_mpi_enable(core_if, comip_param_mpi_enable_default);
    comip_set_param_pti_enable(core_if, comip_param_pti_enable_default);
    comip_set_param_lpm_enable(core_if, comip_param_lpm_enable_default);
    comip_set_param_ic_usb_cap(core_if, comip_param_ic_usb_cap_default);
    comip_set_param_tx_thr_length(core_if,
                    comip_param_tx_thr_length_default);
    comip_set_param_rx_thr_length(core_if,
                    comip_param_rx_thr_length_default);
    comip_set_param_ahb_thr_ratio(core_if,
                    comip_param_ahb_thr_ratio_default);
    comip_set_param_power_down(core_if, comip_param_power_down_default);
    comip_set_param_reload_ctl(core_if, comip_param_reload_ctl_default);
    comip_set_param_dev_out_nak(core_if, comip_param_dev_out_nak_default);
    comip_set_param_otg_ver(core_if, comip_param_otg_ver_default);
    comip_set_param_adp_enable(core_if, comip_param_adp_enable_default);
    return 0;
}

uint8_t comip_is_dma_enable(comip_core_if_t * core_if)
{
    return core_if->dma_enable;
}

/* Checks if the parameter is outside of its valid range of values */
#define COMIP_PARAM_TEST(_param_, _low_, _high_) \
        (((_param_) < (_low_)) || \
        ((_param_) > (_high_)))

/* Parameter access functions */
int comip_set_param_otg_cap(comip_core_if_t * core_if, int32_t val)
{
    int valid;
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 2)) {
        COMIP_WARN("Wrong value for otg_cap parameter\n");
        COMIP_WARN("otg_cap parameter must be 0,1 or 2\n");
        retval = -EINVAL;
        goto out;
    }

    valid = 1;
    switch (val) {
    case COMIP_CAP_PARAM_HNP_SRP_CAPABLE:
        if (core_if->hwcfg2.b.op_mode !=
            COMIP_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG)
            valid = 0;
        break;
    case COMIP_CAP_PARAM_SRP_ONLY_CAPABLE:
        if ((core_if->hwcfg2.b.op_mode !=
             COMIP_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG)
            && (core_if->hwcfg2.b.op_mode !=
            COMIP_HWCFG2_OP_MODE_SRP_ONLY_CAPABLE_OTG)
            && (core_if->hwcfg2.b.op_mode !=
            COMIP_HWCFG2_OP_MODE_SRP_CAPABLE_DEVICE)
            && (core_if->hwcfg2.b.op_mode !=
            COMIP_HWCFG2_OP_MODE_SRP_CAPABLE_HOST)) {
            valid = 0;
        }
        break;
    case COMIP_CAP_PARAM_NO_HNP_SRP_CAPABLE:
        /* always valid */
        break;
    }
    if (!valid) {
        if (comip_param_initialized(core_if->core_params->otg_cap)) {
            COMIP_ERROR
                ("%d invalid for otg_cap paremter. Check HW configuration.\n",
                 val);
        }
        val =
            (((core_if->hwcfg2.b.op_mode ==
               COMIP_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG)
              || (core_if->hwcfg2.b.op_mode ==
              COMIP_HWCFG2_OP_MODE_SRP_ONLY_CAPABLE_OTG)
              || (core_if->hwcfg2.b.op_mode ==
              COMIP_HWCFG2_OP_MODE_SRP_CAPABLE_DEVICE)
              || (core_if->hwcfg2.b.op_mode ==
              COMIP_HWCFG2_OP_MODE_SRP_CAPABLE_HOST)) ?
             COMIP_CAP_PARAM_SRP_ONLY_CAPABLE :
             COMIP_CAP_PARAM_NO_HNP_SRP_CAPABLE);
        retval = -EINVAL;
    }

    core_if->core_params->otg_cap = val;
out:
    return retval;
}

int32_t comip_get_param_otg_cap(comip_core_if_t * core_if)
{
    return core_if->core_params->otg_cap;
}

int comip_set_param_opt(comip_core_if_t * core_if, int32_t val)
{
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong value for opt parameter\n");
        return -EINVAL;
    }
    core_if->core_params->opt = val;
    return 0;
}

int32_t comip_get_param_opt(comip_core_if_t * core_if)
{
    return core_if->core_params->opt;
}

int comip_set_param_dma_enable(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong value for dma enable\n");
        return -EINVAL;
    }

    if ((val == 1) && (core_if->hwcfg2.b.architecture == 0)) {
        if (comip_param_initialized(core_if->core_params->dma_enable)) {
            COMIP_ERROR
                ("%d invalid for dma_enable paremter. Check HW configuration.\n",
                 val);
        }
        val = 0;
        retval = -EINVAL;
    }

    core_if->core_params->dma_enable = val;
    if (val == 0) {
        comip_set_param_dma_desc_enable(core_if, 0);
    }
    return retval;
}

int32_t comip_get_param_dma_enable(comip_core_if_t * core_if)
{
    return core_if->core_params->dma_enable;
}

int comip_set_param_dma_desc_enable(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong value for dma_enable\n");
        COMIP_WARN("dma_desc_enable must be 0 or 1\n");
        return -EINVAL;
    }

    if ((val == 1)
        && ((comip_get_param_dma_enable(core_if) == 0)
        || (core_if->hwcfg4.b.desc_dma == 0))) {
        if (comip_param_initialized
            (core_if->core_params->dma_desc_enable)) {
            COMIP_ERROR
                ("%d invalid for dma_desc_enable paremter. Check HW configuration.\n",
                 val);
        }
        val = 0;
        retval = -EINVAL;
    }
    core_if->core_params->dma_desc_enable = val;
    return retval;
}

int32_t comip_get_param_dma_desc_enable(comip_core_if_t * core_if)
{
    return core_if->core_params->dma_desc_enable;
}

int comip_set_param_host_support_fs_ls_low_power(comip_core_if_t * core_if,
                           int32_t val)
{
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong value for host_support_fs_low_power\n");
        COMIP_WARN("host_support_fs_low_power must be 0 or 1\n");
        return -EINVAL;
    }
    core_if->core_params->host_support_fs_ls_low_power = val;
    return 0;
}

int32_t comip_get_param_host_support_fs_ls_low_power(comip_core_if_t *
                               core_if)
{
    return core_if->core_params->host_support_fs_ls_low_power;
}

int comip_set_param_enable_dynamic_fifo(comip_core_if_t * core_if,
                      int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong value for enable_dynamic_fifo\n");
        COMIP_WARN("enable_dynamic_fifo must be 0 or 1\n");
        return -EINVAL;
    }

    if ((val == 1) && (core_if->hwcfg2.b.dynamic_fifo == 0)) {
        if (comip_param_initialized
            (core_if->core_params->enable_dynamic_fifo)) {
            COMIP_ERROR
                ("%d invalid for enable_dynamic_fifo paremter. Check HW configuration.\n",
                 val);
        }
        val = 0;
        retval = -EINVAL;
    }
    core_if->core_params->enable_dynamic_fifo = val;
    return retval;
}

int32_t comip_get_param_enable_dynamic_fifo(comip_core_if_t * core_if)
{
    return core_if->core_params->enable_dynamic_fifo;
}

int comip_set_param_data_fifo_size(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 32, 32768)) {
        COMIP_WARN("Wrong value for data_fifo_size\n");
        COMIP_WARN("data_fifo_size must be 32-32768\n");
        return -EINVAL;
    }

    if (val > core_if->hwcfg3.b.dfifo_depth) {
        if (comip_param_initialized
            (core_if->core_params->data_fifo_size)) {
            COMIP_ERROR
                ("%d invalid for data_fifo_size parameter. Check HW configuration.\n",
                 val);
        }
        val = core_if->hwcfg3.b.dfifo_depth;
        retval = -EINVAL;
    }

    core_if->core_params->data_fifo_size = val;
    return retval;
}

int32_t comip_get_param_data_fifo_size(comip_core_if_t * core_if)
{
    return core_if->core_params->data_fifo_size;
}

int comip_set_param_dev_rx_fifo_size(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 16, 32768)) {
        COMIP_WARN("Wrong value for dev_rx_fifo_size\n");
        COMIP_WARN("dev_rx_fifo_size must be 16-32768\n");
        return -EINVAL;
    }

    if (val > readl(&core_if->core_global_regs->grxfsiz)) {
        if (comip_param_initialized(core_if->core_params->dev_rx_fifo_size)) {
        COMIP_WARN("%d invalid for dev_rx_fifo_size parameter\n", val);
        }
        val = readl(&core_if->core_global_regs->grxfsiz);
        retval = -EINVAL;
    }

    core_if->core_params->dev_rx_fifo_size = val;
    return retval;
}

int32_t comip_get_param_dev_rx_fifo_size(comip_core_if_t * core_if)
{
    return core_if->core_params->dev_rx_fifo_size;
}

int comip_set_param_host_rx_fifo_size(comip_core_if_t * core_if,
                    int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 16, 32768)) {
        COMIP_WARN("Wrong value for host_rx_fifo_size\n");
        COMIP_WARN("host_rx_fifo_size must be 16-32768\n");
        return -EINVAL;
    }

    if (val > readl(&core_if->core_global_regs->grxfsiz)) {
        if (comip_param_initialized
            (core_if->core_params->host_rx_fifo_size)) {
            COMIP_ERROR
                ("%d invalid for host_rx_fifo_size. Check HW configuration.\n",
                 val);
        }
        val = readl(&core_if->core_global_regs->grxfsiz);
        retval = -EINVAL;
    }

    core_if->core_params->host_rx_fifo_size = val;
    return retval;

}

int32_t comip_get_param_host_rx_fifo_size(comip_core_if_t * core_if)
{
    return core_if->core_params->host_rx_fifo_size;
}

int comip_set_param_host_nperio_tx_fifo_size(comip_core_if_t * core_if,
                           int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 16, 32768)) {
        COMIP_WARN("Wrong value for host_nperio_tx_fifo_size\n");
        COMIP_WARN("host_nperio_tx_fifo_size must be 16-32768\n");
        return -EINVAL;
    }

    if (val > (readl(&core_if->core_global_regs->gnptxfsiz) >> 16)) {
        if (comip_param_initialized
            (core_if->core_params->host_nperio_tx_fifo_size)) {
            COMIP_ERROR
                ("%d invalid for host_nperio_tx_fifo_size. Check HW configuration.\n",
                 val);
        }
        val =
            (readl(&core_if->core_global_regs->gnptxfsiz) >>
             16);
        retval = -EINVAL;
    }

    core_if->core_params->host_nperio_tx_fifo_size = val;
    return retval;
}

int32_t comip_get_param_host_nperio_tx_fifo_size(comip_core_if_t * core_if)
{
    return core_if->core_params->host_nperio_tx_fifo_size;
}

int comip_set_param_host_perio_tx_fifo_size(comip_core_if_t * core_if,
                          int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 16, 32768)) {
        COMIP_WARN("Wrong value for host_perio_tx_fifo_size\n");
        COMIP_WARN("host_perio_tx_fifo_size must be 16-32768\n");
        return -EINVAL;
    }

    if (val >
        ((core_if->hptxfsiz.d32)>> 16)) {
        if (comip_param_initialized
            (core_if->core_params->host_perio_tx_fifo_size)) {
            COMIP_ERROR
                ("%d invalid for host_perio_tx_fifo_size. Check HW configuration.\n",
                 val);
        }
        val = (core_if->hptxfsiz.d32) >> 16;
        retval = -EINVAL;
    }

    core_if->core_params->host_perio_tx_fifo_size = val;
    return retval;
}

int32_t comip_get_param_host_perio_tx_fifo_size(comip_core_if_t * core_if)
{
    return core_if->core_params->host_perio_tx_fifo_size;
}

int comip_set_param_max_transfer_size(comip_core_if_t * core_if,
                    int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 2047, 524288)) {
        COMIP_WARN("Wrong value for max_transfer_size\n");
        COMIP_WARN("max_transfer_size must be 2047-524288\n");
        return -EINVAL;
    }

    if (val >= (1 << (core_if->hwcfg3.b.xfer_size_cntr_width + 11))) {
        if (comip_param_initialized
            (core_if->core_params->max_transfer_size)) {
            COMIP_ERROR
                ("%d invalid for max_transfer_size. Check HW configuration.\n",
                 val);
        }
        val =
            ((1 << (core_if->hwcfg3.b.packet_size_cntr_width + 11)) -
             1);
        retval = -EINVAL;
    }

    core_if->core_params->max_transfer_size = val;
    return retval;
}

int32_t comip_get_param_max_transfer_size(comip_core_if_t * core_if)
{
    return core_if->core_params->max_transfer_size;
}

int comip_set_param_max_packet_count(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 15, 511)) {
        COMIP_WARN("Wrong value for max_packet_count\n");
        COMIP_WARN("max_packet_count must be 15-511\n");
        return -EINVAL;
    }

    if (val > (1 << (core_if->hwcfg3.b.packet_size_cntr_width + 4))) {
        if (comip_param_initialized
            (core_if->core_params->max_packet_count)) {
            COMIP_ERROR
                ("%d invalid for max_packet_count. Check HW configuration.\n",
                 val);
        }
        val =
            ((1 << (core_if->hwcfg3.b.packet_size_cntr_width + 4)) - 1);
        retval = -EINVAL;
    }

    core_if->core_params->max_packet_count = val;
    return retval;
}

int32_t comip_get_param_max_packet_count(comip_core_if_t * core_if)
{
    return core_if->core_params->max_packet_count;
}

int comip_set_param_host_channels(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 1, 16)) {
        COMIP_WARN("Wrong value for host_channels\n");
        COMIP_WARN("host_channels must be 1-16\n");
        return -EINVAL;
    }

    if (val > (core_if->hwcfg2.b.num_host_chan + 1)) {
        if (comip_param_initialized
            (core_if->core_params->host_channels)) {
            COMIP_ERROR
                ("%d invalid for host_channels. Check HW configurations.\n",
                 val);
        }
        val = (core_if->hwcfg2.b.num_host_chan + 1);
        retval = -EINVAL;
    }

    core_if->core_params->host_channels = val;
    return retval;
}

int32_t comip_get_param_host_channels(comip_core_if_t * core_if)
{
    return core_if->core_params->host_channels;
}

int comip_set_param_dev_endpoints(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 1, 15)) {
        COMIP_WARN("Wrong value for dev_endpoints\n");
        COMIP_WARN("dev_endpoints must be 1-15\n");
        return -EINVAL;
    }

    if (val > (core_if->hwcfg2.b.num_dev_ep)) {
        if (comip_param_initialized
            (core_if->core_params->dev_endpoints)) {
            COMIP_ERROR
                ("%d invalid for dev_endpoints. Check HW configurations.\n",
                 val);
        }
        val = core_if->hwcfg2.b.num_dev_ep;
        retval = -EINVAL;
    }

    core_if->core_params->dev_endpoints = val;
    return retval;
}

int32_t comip_get_param_dev_endpoints(comip_core_if_t * core_if)
{
    return core_if->core_params->dev_endpoints;
}

int comip_set_param_phy_type(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    int valid = 0;

    if (COMIP_PARAM_TEST(val, 0, 2)) {
        COMIP_WARN("Wrong value for phy_type\n");
        COMIP_WARN("phy_type must be 0,1 or 2\n");
        return -EINVAL;
    }
#ifndef NO_FS_PHY_HW_CHECKS
    if ((val == COMIP_PHY_TYPE_PARAM_UTMI) &&
        ((core_if->hwcfg2.b.hs_phy_type == 1) ||
         (core_if->hwcfg2.b.hs_phy_type == 3))) {
        valid = 1;
    } else if ((val == COMIP_PHY_TYPE_PARAM_ULPI) &&
           ((core_if->hwcfg2.b.hs_phy_type == 2) ||
            (core_if->hwcfg2.b.hs_phy_type == 3))) {
        valid = 1;
    } else if ((val == COMIP_PHY_TYPE_PARAM_FS) &&
           (core_if->hwcfg2.b.fs_phy_type == 1)) {
        valid = 1;
    }
    if (!valid) {
        if (comip_param_initialized(core_if->core_params->phy_type)) {
            COMIP_ERROR
                ("%d invalid for phy_type. Check HW configurations.\n",
                 val);
        }
        if (core_if->hwcfg2.b.hs_phy_type) {
            if ((core_if->hwcfg2.b.hs_phy_type == 3) ||
                (core_if->hwcfg2.b.hs_phy_type == 1)) {
                val = COMIP_PHY_TYPE_PARAM_UTMI;
            } else {
                val = COMIP_PHY_TYPE_PARAM_ULPI;
            }
        }
        retval = -EINVAL;
    }
#endif
    core_if->core_params->phy_type = val;
    return retval;
}

int32_t comip_get_param_phy_type(comip_core_if_t * core_if)
{
    return core_if->core_params->phy_type;
}

int comip_set_param_speed(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong value for speed parameter\n");
        COMIP_WARN("max_speed parameter must be 0 or 1\n");
        return -EINVAL;
    }
    if ((val == 0)
        && comip_get_param_phy_type(core_if) == COMIP_PHY_TYPE_PARAM_FS) {
        if (comip_param_initialized(core_if->core_params->speed)) {
            COMIP_ERROR
                ("%d invalid for speed paremter. Check HW configuration.\n",
                 val);
        }
        val =
            (comip_get_param_phy_type(core_if) ==
             COMIP_PHY_TYPE_PARAM_FS ? 1 : 0);
        retval = -EINVAL;
    }
    core_if->core_params->speed = val;
    return retval;
}

int32_t comip_get_param_speed(comip_core_if_t * core_if)
{
    return core_if->core_params->speed;
}

int comip_set_param_host_ls_low_power_phy_clk(comip_core_if_t * core_if,
                        int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN
            ("Wrong value for host_ls_low_power_phy_clk parameter\n");
        COMIP_WARN("host_ls_low_power_phy_clk must be 0 or 1\n");
        return -EINVAL;
    }

    if ((val == COMIP_HOST_LS_LOW_POWER_PHY_CLK_PARAM_48MHZ)
        && (comip_get_param_phy_type(core_if) == COMIP_PHY_TYPE_PARAM_FS)) {
        if (comip_param_initialized
            (core_if->core_params->host_ls_low_power_phy_clk)) {
            COMIP_ERROR
                ("%d invalid for host_ls_low_power_phy_clk. Check HW configuration.\n",
                 val);
        }
        val =
            (comip_get_param_phy_type(core_if) ==
             COMIP_PHY_TYPE_PARAM_FS) ?
            COMIP_HOST_LS_LOW_POWER_PHY_CLK_PARAM_6MHZ :
            COMIP_HOST_LS_LOW_POWER_PHY_CLK_PARAM_48MHZ;
        retval = -EINVAL;
    }

    core_if->core_params->host_ls_low_power_phy_clk = val;
    return retval;
}

int32_t comip_get_param_host_ls_low_power_phy_clk(comip_core_if_t * core_if)
{
    return core_if->core_params->host_ls_low_power_phy_clk;
}

int comip_set_param_phy_ulpi_ddr(comip_core_if_t * core_if, int32_t val)
{
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong value for phy_ulpi_ddr\n");
        COMIP_WARN("phy_upli_ddr must be 0 or 1\n");
        return -EINVAL;
    }

    core_if->core_params->phy_ulpi_ddr = val;
    return 0;
}

int32_t comip_get_param_phy_ulpi_ddr(comip_core_if_t * core_if)
{
    return core_if->core_params->phy_ulpi_ddr;
}

int comip_set_param_phy_ulpi_ext_vbus(comip_core_if_t * core_if,
                    int32_t val)
{
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong valaue for phy_ulpi_ext_vbus\n");
        COMIP_WARN("phy_ulpi_ext_vbus must be 0 or 1\n");
        return -EINVAL;
    }

    core_if->core_params->phy_ulpi_ext_vbus = val;
    return 0;
}

int32_t comip_get_param_phy_ulpi_ext_vbus(comip_core_if_t * core_if)
{
    return core_if->core_params->phy_ulpi_ext_vbus;
}

int comip_set_param_phy_utmi_width(comip_core_if_t * core_if, int32_t val)
{
    if (COMIP_PARAM_TEST(val, 8, 8) && COMIP_PARAM_TEST(val, 16, 16)) {
        COMIP_WARN("Wrong valaue for phy_utmi_width\n");
        COMIP_WARN("phy_utmi_width must be 8 or 16\n");
        return -EINVAL;
    }

    core_if->core_params->phy_utmi_width = val;
    return 0;
}

int32_t comip_get_param_phy_utmi_width(comip_core_if_t * core_if)
{
    return core_if->core_params->phy_utmi_width;
}

int comip_set_param_ulpi_fs_ls(comip_core_if_t * core_if, int32_t val)
{
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong valaue for ulpi_fs_ls\n");
        COMIP_WARN("ulpi_fs_ls must be 0 or 1\n");
        return -EINVAL;
    }

    core_if->core_params->ulpi_fs_ls = val;
    return 0;
}

int32_t comip_get_param_ulpi_fs_ls(comip_core_if_t * core_if)
{
    return core_if->core_params->ulpi_fs_ls;
}

int comip_set_param_ts_dline(comip_core_if_t * core_if, int32_t val)
{
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong valaue for ts_dline\n");
        COMIP_WARN("ts_dline must be 0 or 1\n");
        return -EINVAL;
    }

    core_if->core_params->ts_dline = val;
    return 0;
}

int32_t comip_get_param_ts_dline(comip_core_if_t * core_if)
{
    return core_if->core_params->ts_dline;
}

int comip_set_param_i2c_enable(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong valaue for i2c_enable\n");
        COMIP_WARN("i2c_enable must be 0 or 1\n");
        return -EINVAL;
    }
#ifndef NO_FS_PHY_HW_CHECK
    if (val == 1 && core_if->hwcfg3.b.i2c == 0) {
        if (comip_param_initialized(core_if->core_params->i2c_enable)) {
            COMIP_ERROR
                ("%d invalid for i2c_enable. Check HW configuration.\n",
                 val);
        }
        val = 0;
        retval = -EINVAL;
    }
#endif

    core_if->core_params->i2c_enable = val;
    return retval;
}

int32_t comip_get_param_i2c_enable(comip_core_if_t * core_if)
{
    return core_if->core_params->i2c_enable;
}

int comip_set_param_en_multiple_tx_fifo(comip_core_if_t * core_if,
                      int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong valaue for en_multiple_tx_fifo,\n");
        COMIP_WARN("en_multiple_tx_fifo must be 0 or 1\n");
        return -EINVAL;
    }

    if (val == 1 && core_if->hwcfg4.b.ded_fifo_en == 0) {
        if (comip_param_initialized
            (core_if->core_params->en_multiple_tx_fifo)) {
            COMIP_ERROR
                ("%d invalid for parameter en_multiple_tx_fifo. Check HW configuration.\n",
                 val);
        }
        val = 0;
        retval = -EINVAL;
    }

    core_if->core_params->en_multiple_tx_fifo = val;
    return retval;
}

int32_t comip_get_param_en_multiple_tx_fifo(comip_core_if_t * core_if)
{
    return core_if->core_params->en_multiple_tx_fifo;
}

int comip_set_param_dev_tx_fifo_size(comip_core_if_t * core_if, int32_t val,
                       int fifo_num)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 4, 768)) {
        COMIP_WARN("Wrong value for dev_tx_fifo_size\n");
        COMIP_WARN("dev_tx_fifo_size must be 4-768\n");
        return -EINVAL;
    }

    if (val >
        (readl(&core_if->core_global_regs->dtxfsiz[fifo_num]))) {
        if (comip_param_initialized
            (core_if->core_params->dev_tx_fifo_size[fifo_num])) {
            COMIP_ERROR
                ("`%d' invalid for parameter `dev_tx_fifo_size_%d'. Check HW configuration.\n",
                 val, fifo_num);
        }
        val = (readl(&core_if->core_global_regs->dtxfsiz[fifo_num]));
        retval = -EINVAL;
    }

    core_if->core_params->dev_tx_fifo_size[fifo_num] = val;
    return retval;
}

int32_t comip_get_param_dev_tx_fifo_size(comip_core_if_t * core_if,
                       int fifo_num)
{
    return core_if->core_params->dev_tx_fifo_size[fifo_num];
}

int comip_set_param_thr_ctl(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 0, 7)) {
        COMIP_WARN("Wrong value for thr_ctl\n");
        COMIP_WARN("thr_ctl must be 0-7\n");
        return -EINVAL;
    }

    if ((val != 0) &&
        (!comip_get_param_dma_enable(core_if) ||
         !core_if->hwcfg4.b.ded_fifo_en)) {
        if (comip_param_initialized(core_if->core_params->thr_ctl)) {
            COMIP_ERROR
                ("%d invalid for parameter thr_ctl. Check HW configuration.\n",
                 val);
        }
        val = 0;
        retval = -EINVAL;
    }

    core_if->core_params->thr_ctl = val;
    return retval;
}

int32_t comip_get_param_thr_ctl(comip_core_if_t * core_if)
{
    return core_if->core_params->thr_ctl;
}

int comip_set_param_lpm_enable(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("Wrong value for lpm_enable\n");
        COMIP_WARN("lpm_enable must be 0 or 1\n");
        return -EINVAL;
    }

    if (val && !core_if->hwcfg3.b.otg_lpm_en) {
        if (comip_param_initialized(core_if->core_params->lpm_enable)) {
            COMIP_ERROR
                ("%d invalid for parameter lpm_enable. Check HW configuration.\n",
                 val);
        }
        val = 0;
        retval = -EINVAL;
    }

    core_if->core_params->lpm_enable = val;
    return retval;
}

int32_t comip_get_param_lpm_enable(comip_core_if_t * core_if)
{
    return core_if->core_params->lpm_enable;
}

int comip_set_param_tx_thr_length(comip_core_if_t * core_if, int32_t val)
{
    if (COMIP_PARAM_TEST(val, 8, 128)) {
        COMIP_WARN("Wrong valaue for tx_thr_length\n");
        COMIP_WARN("tx_thr_length must be 8 - 128\n");
        return -EINVAL;
    }

    core_if->core_params->tx_thr_length = val;
    return 0;
}

int32_t comip_get_param_tx_thr_length(comip_core_if_t * core_if)
{
    return core_if->core_params->tx_thr_length;
}

int comip_set_param_rx_thr_length(comip_core_if_t * core_if, int32_t val)
{
    if (COMIP_PARAM_TEST(val, 8, 128)) {
        COMIP_WARN("Wrong valaue for rx_thr_length\n");
        COMIP_WARN("rx_thr_length must be 8 - 128\n");
        return -EINVAL;
    }

    core_if->core_params->rx_thr_length = val;
    return 0;
}

int32_t comip_get_param_rx_thr_length(comip_core_if_t * core_if)
{
    return core_if->core_params->rx_thr_length;
}

int comip_set_param_dma_burst_size(comip_core_if_t * core_if, int32_t val)
{
    if (COMIP_PARAM_TEST(val, 1, 1) &&
        COMIP_PARAM_TEST(val, 4, 4) &&
        COMIP_PARAM_TEST(val, 8, 8) &&
        COMIP_PARAM_TEST(val, 16, 16) &&
        COMIP_PARAM_TEST(val, 32, 32) &&
        COMIP_PARAM_TEST(val, 64, 64) &&
        COMIP_PARAM_TEST(val, 128, 128) &&
        COMIP_PARAM_TEST(val, 256, 256)) {
        COMIP_WARN("`%d' invalid for parameter `dma_burst_size'\n", val);
        return -EINVAL;
    }
    core_if->core_params->dma_burst_size = val;
    return 0;
}

int32_t comip_get_param_dma_burst_size(comip_core_if_t * core_if)
{
    return core_if->core_params->dma_burst_size;
}

int comip_set_param_pti_enable(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("`%d' invalid for parameter `pti_enable'\n", val);
        return -EINVAL;
    }
    if (val && (core_if->snpsid < OTG_CORE_REV_2_72a)) {
        if (comip_param_initialized(core_if->core_params->pti_enable)) {
            COMIP_ERROR
                ("%d invalid for parameter pti_enable. Check HW configuration.\n",
                 val);
        }
        retval = -EINVAL;
        val = 0;
    }
    core_if->core_params->pti_enable = val;
    return retval;
}

int32_t comip_get_param_pti_enable(comip_core_if_t * core_if)
{
    return core_if->core_params->pti_enable;
}

int comip_set_param_mpi_enable(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("`%d' invalid for parameter `mpi_enable'\n", val);
        return -EINVAL;
    }
    if (val && (core_if->hwcfg2.b.multi_proc_int == 0)) {
        if (comip_param_initialized(core_if->core_params->mpi_enable)) {
            COMIP_ERROR
                ("%d invalid for parameter mpi_enable. Check HW configuration.\n",
                 val);
        }
        retval = -EINVAL;
        val = 0;
    }
    core_if->core_params->mpi_enable = val;
    return retval;
}

int32_t comip_get_param_mpi_enable(comip_core_if_t * core_if)
{
    return core_if->core_params->mpi_enable;
}

int comip_set_param_adp_enable(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("`%d' invalid for parameter `adp_enable'\n", val);
        return -EINVAL;
    }
    if (val && (core_if->hwcfg3.b.adp_supp == 0)) {
        if (comip_param_initialized
            (core_if->core_params->adp_supp_enable)) {
            COMIP_ERROR
                ("%d invalid for parameter adp_enable. Check HW configuration.\n",
                 val);
        }
        retval = -EINVAL;
        val = 0;
    }
    core_if->core_params->adp_supp_enable = val;
    /*Set OTG version 2.0 in case of enabling ADP*/
    comip_set_param_otg_ver(core_if, 1);

    return retval;
}

int32_t comip_get_param_adp_enable(comip_core_if_t * core_if)
{
    return core_if->core_params->adp_supp_enable;
}

int comip_set_param_ic_usb_cap(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("`%d' invalid for parameter `ic_usb_cap'\n", val);
        COMIP_WARN("ic_usb_cap must be 0 or 1\n");
        return -EINVAL;
    }

    if (val && (core_if->hwcfg2.b.otg_enable_ic_usb == 0)) {
        if (comip_param_initialized(core_if->core_params->ic_usb_cap)) {
            COMIP_ERROR
                ("%d invalid for parameter ic_usb_cap. Check HW configuration.\n",
                 val);
        }
        retval = -EINVAL;
        val = 0;
    }
    core_if->core_params->ic_usb_cap = val;
    return retval;
}

int32_t comip_get_param_ic_usb_cap(comip_core_if_t * core_if)
{
    return core_if->core_params->ic_usb_cap;
}

int comip_set_param_ahb_thr_ratio(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    int valid = 1;

    if (COMIP_PARAM_TEST(val, 0, 3)) {
        COMIP_WARN("`%d' invalid for parameter `ahb_thr_ratio'\n", val);
        COMIP_WARN("ahb_thr_ratio must be 0 - 3\n");
        return -EINVAL;
    }

    if (val
        && (core_if->snpsid < OTG_CORE_REV_2_81a
        || !comip_get_param_thr_ctl(core_if))) {
        valid = 0;
    } else if (val
           && ((comip_get_param_tx_thr_length(core_if) / (1 << val)) <
               4)) {
        valid = 0;
    }
    if (valid == 0) {
        if (comip_param_initialized
            (core_if->core_params->ahb_thr_ratio)) {
            COMIP_ERROR
                ("%d invalid for parameter ahb_thr_ratio. Check HW configuration.\n",
                 val);
        }
        retval = -EINVAL;
        val = 0;
    }

    core_if->core_params->ahb_thr_ratio = val;
    return retval;
}

int32_t comip_get_param_ahb_thr_ratio(comip_core_if_t * core_if)
{
    return core_if->core_params->ahb_thr_ratio;
}

int comip_set_param_power_down(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    int valid = 1;

    if (COMIP_PARAM_TEST(val, 0, 2)) {
        COMIP_WARN("`%d' invalid for parameter `power_down'\n", val);
        COMIP_WARN("power_down must be 0 - 2\n");
        return -EINVAL;
    }

    if ((val == 2) && (core_if->snpsid < OTG_CORE_REV_2_91a)) {
        valid = 0;
    }
    if (valid == 0) {
        if (comip_param_initialized(core_if->core_params->power_down)) {
            COMIP_ERROR
                ("%d invalid for parameter power_down. Check HW configuration.\n",
                 val);
        }
        retval = -EINVAL;
        val = 0;
    }
    core_if->core_params->power_down = val;
    return retval;
}

int32_t comip_get_param_power_down(comip_core_if_t * core_if)
{
    return core_if->core_params->power_down;
}

int comip_set_param_reload_ctl(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    int valid = 1;

    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("`%d' invalid for parameter `reload_ctl'\n", val);
        COMIP_WARN("reload_ctl must be 0 or 1\n");
        return -EINVAL;
    }

    if ((val == 1) && (core_if->snpsid < OTG_CORE_REV_2_92a)) {
        valid = 0;
    }
    if (valid == 0) {
        if (comip_param_initialized(core_if->core_params->reload_ctl)) {
            COMIP_ERROR("%d invalid for parameter reload_ctl."
                  "Check HW configuration.\n", val);
        }
        retval = -EINVAL;
        val = 0;
    }
    core_if->core_params->reload_ctl = val;
    return retval;
}

int32_t comip_get_param_reload_ctl(comip_core_if_t * core_if)
{
    return core_if->core_params->reload_ctl;
}

int comip_set_param_dev_out_nak(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;
    int valid = 1;

    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("`%d' invalid for parameter `dev_out_nak'\n", val);
        COMIP_WARN("dev_out_nak must be 0 or 1\n");
        return -EINVAL;
    }

    if ((val == 1) && ((core_if->snpsid < OTG_CORE_REV_2_93a) ||
        !(core_if->core_params->dma_desc_enable))) {
        valid = 0;
    }
    if (valid == 0) {
        if (comip_param_initialized(core_if->core_params->dev_out_nak)) {
            COMIP_ERROR("%d invalid for parameter dev_out_nak."
                "Check HW configuration.\n", val);
        }
        retval = -EINVAL;
        val = 0;
    }
    core_if->core_params->dev_out_nak = val;
    return retval;
}

int32_t comip_get_param_dev_out_nak(comip_core_if_t * core_if)
{
    return core_if->core_params->dev_out_nak;
}

int comip_set_param_otg_ver(comip_core_if_t * core_if, int32_t val)
{
    int retval = 0;

    if (COMIP_PARAM_TEST(val, 0, 1)) {
        COMIP_WARN("`%d' invalid for parameter `otg_ver'\n", val);
        COMIP_WARN
            ("otg_ver must be 0(for OTG 1.3 support) or 1(for OTG 2.0 support)\n");
        return -EINVAL;
    }

    core_if->core_params->otg_ver = val;
    return retval;
}

int32_t comip_get_param_otg_ver(comip_core_if_t * core_if)
{
    return core_if->core_params->otg_ver;
}

uint32_t comip_get_hnpstatus(comip_core_if_t * core_if)
{
    gotgctl_data_t otgctl;
    otgctl.d32 = readl(&core_if->core_global_regs->gotgctl);
    return otgctl.b.hstnegscs;
}

uint32_t comip_get_srpstatus(comip_core_if_t * core_if)
{
    gotgctl_data_t otgctl;
    otgctl.d32 = readl(&core_if->core_global_regs->gotgctl);
    return otgctl.b.sesreqscs;
}

void comip_set_hnpreq(comip_core_if_t * core_if, uint32_t val)
{
    if(core_if->otg_ver == 0) {
        gotgctl_data_t otgctl;
        otgctl.d32 = readl(&core_if->core_global_regs->gotgctl);
        otgctl.b.hnpreq = val;
        COMIP_WRITE_REG32(&core_if->core_global_regs->gotgctl, otgctl.d32);
    } else {
        core_if->otg_sts = val;
    }
}

uint32_t comip_get_gsnpsid(comip_core_if_t * core_if)
{
    return core_if->snpsid;
}

uint32_t comip_get_mode(comip_core_if_t * core_if)
{
    gintsts_data_t gintsts;
    gintsts.d32 = readl(&core_if->core_global_regs->gintsts);
    return gintsts.b.curmode;
}

uint32_t comip_get_hnpcapable(comip_core_if_t * core_if)
{
    gusbcfg_data_t usbcfg;
    usbcfg.d32 = readl(&core_if->core_global_regs->gusbcfg);
    return usbcfg.b.hnpcap;
}

void comip_set_hnpcapable(comip_core_if_t * core_if, uint32_t val)
{
    gusbcfg_data_t usbcfg;
    usbcfg.d32 = readl(&core_if->core_global_regs->gusbcfg);
    usbcfg.b.hnpcap = val;
    COMIP_WRITE_REG32(&core_if->core_global_regs->gusbcfg, usbcfg.d32);
}

uint32_t comip_get_srpcapable(comip_core_if_t * core_if)
{
    gusbcfg_data_t usbcfg;
    usbcfg.d32 = readl(&core_if->core_global_regs->gusbcfg);
    return usbcfg.b.srpcap;
}

void comip_set_srpcapable(comip_core_if_t * core_if, uint32_t val)
{
    gusbcfg_data_t usbcfg;
    usbcfg.d32 = readl(&core_if->core_global_regs->gusbcfg);
    usbcfg.b.srpcap = val;
    COMIP_WRITE_REG32(&core_if->core_global_regs->gusbcfg, usbcfg.d32);
}

uint32_t comip_get_busconnected(comip_core_if_t * core_if)
{
    hprt0_data_t hprt0;
    hprt0.d32 = readl(core_if->host_if->hprt0);
    return hprt0.b.prtconnsts;
}
uint32_t comip_get_prtpower(comip_core_if_t * core_if)
{
    hprt0_data_t hprt0;
    hprt0.d32 = readl(core_if->host_if->hprt0);
    return hprt0.b.prtpwr;

}

uint32_t comip_get_core_state(comip_core_if_t * core_if)
{
    return core_if->hibernation_suspend;
}

void comip_set_prtpower(comip_core_if_t * core_if, uint32_t val)
{
    hprt0_data_t hprt0;
    hprt0.d32 = comip_read_hprt0(core_if);
    hprt0.b.prtpwr = val;
    COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);
}

uint32_t comip_get_prtsuspend(comip_core_if_t * core_if)
{
    hprt0_data_t hprt0;
    hprt0.d32 = readl(core_if->host_if->hprt0);
    return hprt0.b.prtsusp;

}

void comip_set_prtsuspend(comip_core_if_t * core_if, uint32_t val)
{
    hprt0_data_t hprt0;
    hprt0.d32 = comip_read_hprt0(core_if);
    hprt0.b.prtsusp = val;
    COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);
}

uint32_t comip_get_fr_interval(comip_core_if_t * core_if)
{
    hfir_data_t hfir;
    hfir.d32 = readl(&core_if->host_if->host_global_regs->hfir);
    return hfir.b.frint;

}

void comip_set_fr_interval(comip_core_if_t * core_if, uint32_t val)
{
    hfir_data_t hfir;
    uint32_t fram_int;
    fram_int = calc_frame_interval(core_if);
    hfir.d32 = readl(&core_if->host_if->host_global_regs->hfir);
    if (!core_if->core_params->reload_ctl) {
        COMIP_WARN("\nCannot reload HFIR register.HFIR.HFIRRldCtrl bit is"
             "not set to 1.\nShould load driver with reload_ctl=1"
             " module parameter\n");
        return;
    }
    switch (fram_int) {
    case 3750:
        if ((val < 3350) || (val > 4150)) {
            COMIP_WARN("HFIR interval for HS core and 30 MHz"
                 "clock freq should be from 3350 to 4150\n");
            return;
        }
        break;
    case 30000:
        if ((val < 26820) || (val > 33180)) {
            COMIP_WARN("HFIR interval for FS/LS core and 30 MHz"
                 "clock freq should be from 26820 to 33180\n");
            return;
        }
        break;
    case 6000:
        if ((val < 5360) || (val > 6640)) {
            COMIP_WARN("HFIR interval for HS core and 48 MHz"
                 "clock freq should be from 5360 to 6640\n");
            return;
        }
        break;
    case 48000:
        if ((val < 42912) || (val > 53088)) {
            COMIP_WARN("HFIR interval for FS/LS core and 48 MHz"
                 "clock freq should be from 42912 to 53088\n");
            return;
        }
        break;
    case 7500:
        if ((val < 6700) || (val > 8300)) {
            COMIP_WARN("HFIR interval for HS core and 60 MHz"
                 "clock freq should be from 6700 to 8300\n");
            return;
        }
        break;
    case 60000:
        if ((val < 53640) || (val > 65536)) {
            COMIP_WARN("HFIR interval for FS/LS core and 60 MHz"
                 "clock freq should be from 53640 to 65536\n");
            return;
        }
        break;
    default:
        COMIP_WARN("Unknown frame interval\n");
        return;
        break;

    }
    hfir.b.frint = val;
    COMIP_WRITE_REG32(&core_if->host_if->host_global_regs->hfir, hfir.d32);
}

uint32_t comip_get_mode_ch_tim(comip_core_if_t * core_if)
{
    hcfg_data_t hcfg;
    hcfg.d32 = readl(&core_if->host_if->host_global_regs->hcfg);
    return hcfg.b.modechtimen;

}

void comip_set_mode_ch_tim(comip_core_if_t * core_if, uint32_t val)
{
    hcfg_data_t hcfg;
    hcfg.d32 = readl(&core_if->host_if->host_global_regs->hcfg);
    hcfg.b.modechtimen = val;
    COMIP_WRITE_REG32(&core_if->host_if->host_global_regs->hcfg, hcfg.d32);
}

void comip_set_prtresume(comip_core_if_t * core_if, uint32_t val)
{
    hprt0_data_t hprt0;
    hprt0.d32 = comip_read_hprt0(core_if);
    hprt0.b.prtres = val;
    COMIP_WRITE_REG32(core_if->host_if->hprt0, hprt0.d32);
}

uint32_t comip_get_lpm_portsleepstatus(comip_core_if_t * core_if)
{
    glpmcfg_data_t lpmcfg;
    lpmcfg.d32 = readl(&core_if->core_global_regs->glpmcfg);

    COMIP_ASSERT(!
           ((core_if->lx_state == COMIP_L1) ^ lpmcfg.b.prt_sleep_sts),
           "lx_state = %d, lmpcfg.prt_sleep_sts = %d\n",
           core_if->lx_state, lpmcfg.b.prt_sleep_sts);

    return lpmcfg.b.prt_sleep_sts;
}

uint32_t comip_get_lpm_remotewakeenabled(comip_core_if_t * core_if)
{
    glpmcfg_data_t lpmcfg;
    lpmcfg.d32 = readl(&core_if->core_global_regs->glpmcfg);
    return lpmcfg.b.rem_wkup_en;
}

uint32_t comip_get_lpmresponse(comip_core_if_t * core_if)
{
    glpmcfg_data_t lpmcfg;
    lpmcfg.d32 = readl(&core_if->core_global_regs->glpmcfg);
    return lpmcfg.b.appl_resp;
}

void comip_set_lpmresponse(comip_core_if_t * core_if, uint32_t val)
{
    glpmcfg_data_t lpmcfg;
    lpmcfg.d32 = readl(&core_if->core_global_regs->glpmcfg);
    lpmcfg.b.appl_resp = val;
    COMIP_WRITE_REG32(&core_if->core_global_regs->glpmcfg, lpmcfg.d32);
}

uint32_t comip_get_hsic_connect(comip_core_if_t * core_if)
{
    glpmcfg_data_t lpmcfg;
    lpmcfg.d32 = readl(&core_if->core_global_regs->glpmcfg);
    return lpmcfg.b.hsic_connect;
}

void comip_set_hsic_connect(comip_core_if_t * core_if, uint32_t val)
{
    glpmcfg_data_t lpmcfg;
    lpmcfg.d32 = readl(&core_if->core_global_regs->glpmcfg);
    lpmcfg.b.hsic_connect = val;
    COMIP_WRITE_REG32(&core_if->core_global_regs->glpmcfg, lpmcfg.d32);
}

uint32_t comip_get_inv_sel_hsic(comip_core_if_t * core_if)
{
    glpmcfg_data_t lpmcfg;
    lpmcfg.d32 = readl(&core_if->core_global_regs->glpmcfg);
    return lpmcfg.b.inv_sel_hsic;

}

void comip_set_inv_sel_hsic(comip_core_if_t * core_if, uint32_t val)
{
    glpmcfg_data_t lpmcfg;
    lpmcfg.d32 = readl(&core_if->core_global_regs->glpmcfg);
    lpmcfg.b.inv_sel_hsic = val;
    COMIP_WRITE_REG32(&core_if->core_global_regs->glpmcfg, lpmcfg.d32);
}

uint32_t comip_get_gotgctl(comip_core_if_t * core_if)
{
    return readl(&core_if->core_global_regs->gotgctl);
}

void comip_set_gotgctl(comip_core_if_t * core_if, uint32_t val)
{
    COMIP_WRITE_REG32(&core_if->core_global_regs->gotgctl, val);
}

uint32_t comip_get_gusbcfg(comip_core_if_t * core_if)
{
    return readl(&core_if->core_global_regs->gusbcfg);
}

void comip_set_gusbcfg(comip_core_if_t * core_if, uint32_t val)
{
    COMIP_WRITE_REG32(&core_if->core_global_regs->gusbcfg, val);
}

uint32_t comip_get_grxfsiz(comip_core_if_t * core_if)
{
    return readl(&core_if->core_global_regs->grxfsiz);
}

void comip_set_grxfsiz(comip_core_if_t * core_if, uint32_t val)
{
    COMIP_WRITE_REG32(&core_if->core_global_regs->grxfsiz, val);
}

uint32_t comip_get_gnptxfsiz(comip_core_if_t * core_if)
{
    return readl(&core_if->core_global_regs->gnptxfsiz);
}

void comip_set_gnptxfsiz(comip_core_if_t * core_if, uint32_t val)
{
    COMIP_WRITE_REG32(&core_if->core_global_regs->gnptxfsiz, val);
}

uint32_t comip_get_gpvndctl(comip_core_if_t * core_if)
{
    return readl(&core_if->core_global_regs->gpvndctl);
}

void comip_set_gpvndctl(comip_core_if_t * core_if, uint32_t val)
{
    COMIP_WRITE_REG32(&core_if->core_global_regs->gpvndctl, val);
}

uint32_t comip_get_ggpio(comip_core_if_t * core_if)
{
    return readl(&core_if->core_global_regs->ggpio);
}

void comip_set_ggpio(comip_core_if_t * core_if, uint32_t val)
{
    COMIP_WRITE_REG32(&core_if->core_global_regs->ggpio, val);
}

uint32_t comip_get_hprt0(comip_core_if_t * core_if)
{
    return readl(core_if->host_if->hprt0);

}

void comip_set_hprt0(comip_core_if_t * core_if, uint32_t val)
{
    COMIP_WRITE_REG32(core_if->host_if->hprt0, val);
}

uint32_t comip_get_guid(comip_core_if_t * core_if)
{
    return readl(&core_if->core_global_regs->guid);
}

void comip_set_guid(comip_core_if_t * core_if, uint32_t val)
{
    COMIP_WRITE_REG32(&core_if->core_global_regs->guid, val);
}

uint32_t comip_get_hptxfsiz(comip_core_if_t * core_if)
{
    return readl(&core_if->core_global_regs->hptxfsiz);
}

uint16_t comip_get_otg_version(comip_core_if_t * core_if)
{
    return ((core_if->otg_ver == 1) ? (uint16_t)0x0200 : (uint16_t)0x0103);
}

