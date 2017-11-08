/*******************************************************************************
 *  COPYRIGHT LeadCore
 ********************************************************************************
 * Filename  : on2psm.c
 *
 * Description   : definition on2 Power Saving  module.
 *
 * Notes     : nothing
 *
 *******************************************************************************
* Change History:
********************************************************************************
*  1.0.0     L1810_Bug00000243       fuhaidong     2012-6-14    original version, add on2 psm
*  1.0.1     L1810_Bug00000753       zhuxiaoping     2012-7-17    original version, add on2 psm
*********************************************************************************/
#include <plat/on2psm.h>
#include <asm/signal.h>
#include <asm-generic/siginfo.h>
#include <plat/clock.h>
#include <plat/hardware.h>

#include <plat/hx170dec.h>
#include <plat/hx280enc.h>
#include <plat/hx280enc_h1.h>

#include <linux/mutex.h>

#if defined(CONFIG_ARCH_LC186X)
#include <linux/delay.h>
#include <plat/hardware.h>
#endif

#define AP_PWR_IRQ_NUM INT_AP_PWR

#undef USE_SYNC_SIGIO
#define CONFIG_ON2_RESUME

/* and this is our MAJOR; use 0 for dynamic allocation (recommended)*/
static int on2psm_major = 0;
static struct class *on2psm_class;
static on2psm_t on2psm_data;

/*ON2 pwr clk*/
#if defined(CONFIG_ARCH_LC181X)
static struct clk *on2_decode_clk = NULL;
static struct clk *on2_d_clk = NULL;

static struct clk *on2_encode_clk = NULL;
static struct clk *on2_e0_clk = NULL;
static struct clk *on2_e1_clk = NULL;
static struct clk *on2_ebus_clk = NULL;
#elif defined(CONFIG_ARCH_LC186X)
static struct clk *on2_codec_mclk = NULL;
static struct clk *ap_sw1_on2codec_clk = NULL;
static struct clk *ap_peri_sw1_on2codec_clk = NULL;

static struct clk *on2_enc_mclk = NULL;
static struct clk *ap_sw1_on2enc_clk = NULL;
static struct clk *ap_peri_sw1_on2enc_clk = NULL;
#endif

//static spinlock_t on2psm_lock;
static DEFINE_MUTEX(on2psm_mutex);

#undef PDEBUG
#define PDEBUG(fmt, args...)  /* not debugging: nothing */

/*------------------------------------------------------------------------------
	Function name   : on2power_isr
	Description     : interrupt handler, for recognize if on2 power down or power up complete

	Return type     : irqreturn_t
------------------------------------------------------------------------------*/
#ifdef USE_SYNC_SIGIO
irqreturn_t on2psm_isr(int irq, void *dev_id)
{
	unsigned int handled = 0;
	on2psm_t *dev = (on2psm_t *) dev_id;
	u32 irq_status_ap_pwr;

	/* interrupt status register read */
#if defined(CONFIG_CPU_LC1810)
	irq_status_ap_pwr = readl((void __iomem *)io_p2v(AP_PWR_INTST_A9));
#else
	irq_status_ap_pwr = readl((void __iomem *)io_p2v(AP_PWR_INTST_A7));
#endif

	if((irq_status_ap_pwr & (1<<AP_PWR_ON2_PD_INTR)) ||
	   (irq_status_ap_pwr & (1<<AP_PWR_ON2_PU_INTR)))
	{
#if defined(CONFIG_CPU_LC1810)
	    writel(irq_status_ap_pwr & (~(1<<AP_PWR_ON2_PD_INTR))&(~(1<<AP_PWR_ON2_PU_INTR))
			,(void __iomem *)io_p2v(AP_PWR_INTST_A9));   /* clear ON2 PU/PD IRQ */
#else
	    writel(irq_status_ap_pwr & (~(1<<AP_PWR_ON2_PD_INTR))&(~(1<<AP_PWR_ON2_PU_INTR))
			,(void __iomem *)io_p2v(AP_PWR_INTST_A7));   /* clear ON2 PU/PD IRQ */
#endif

	    //put sem
	    if(dev->async_queue)
	        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);

	    PDEBUG("on2psm IRQ handled!\n");
	    handled = 1;
	}

	return IRQ_RETVAL(handled);
}
#endif

#if defined(CONFIG_ARCH_LC181X)
#if defined(CONFIG_MALI400)
extern void mali_on2_status_set(u32 enable);
#endif
static int set_ddr_axi_config(int enable) {
#if defined(CONFIG_MALI400)
	unsigned int  reg_val;
	static u32 last_ddr_pcfg3 = 0;

	if(enable) {
		if(on2psm_data.on2_ddr_axi_config_flag == 0)	{
			reg_val = readl((void __iomem *)io_p2v(CTRL_DDR_PCFG_3));
			/* store CTRL_DDR_PCFG_3 register value for resume */
			last_ddr_pcfg3 = reg_val;
			if(!(reg_val & (1<<5))) {
				mali_on2_status_set(1);
			}
		}
		on2psm_data.on2_ddr_axi_config_flag++;
	} else {
		on2psm_data.on2_ddr_axi_config_flag--;
		if(on2psm_data.on2_ddr_axi_config_flag == 0) {
			if(!(last_ddr_pcfg3 & (1<<5))) {
				mali_on2_status_set(0);
			}
		}

		if(on2psm_data.on2_ddr_axi_config_flag < 0)
			on2psm_data.on2_ddr_axi_config_flag = 0;
	}
#endif
	return 1;
}
#endif
/*------------------------------------------------------------------------------
	Function name   : on2psm_ioctl
	Description     : communication method to/from the user space

	Return type     : int
------------------------------------------------------------------------------*/
static long on2psm_ioctl(struct file *filp,
	                      unsigned int cmd, unsigned long arg)
{
	int err = 0;
	unsigned int power_reg_val = 0;
#if defined(CONFIG_ARCH_LC186X)
	int timeout = 100;
#endif

	PDEBUG("ioctl cmd 0x%08ux\n", cmd);
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if(_IOC_TYPE(cmd) != ON2PSM_IOC_MAGIC)
	    return -ENOTTY;
	if(_IOC_NR(cmd) >ON2PSM_IOC_MAXNR)
	    return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if(_IOC_DIR(cmd) & _IOC_READ)
	    err = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	    err = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));
	if(err)
	    return -EFAULT;

	/*cmd */
	switch (cmd)
	{
		/*For lock any diff process to operate on2*/
		case ON2_MUTEX_LOCK:
		    //get sem when operate;
		    break;
		case ON2_MUTEX_UNLOCK:
   		    //put sem after operate;
		    break;

		/*ON2 Internal ClOCK OPERATE*/
		/*ON2 Frequncy adjust OPERATE*/

#if defined(CONFIG_ARCH_LC181X)
		/*ON2 External ClOCK OPERATE*/
		case ON2_PWR_G1_DEC_CLK_ENABLE:
			//spin_lock(&on2psm_lock);
			mutex_lock(&on2psm_mutex);
			if(on2psm_data.on2_pwr_dec_clk_flag == 0) //in order to play anytime,but get and put
			{
				on2_decode_clk = clk_get(on2psm_data.dev, "on2_decode_clk");
				if(IS_ERR(on2_decode_clk)){
				   printk(KERN_ERR "get on2_decode_clk failed\n");
					 return -ENOTTY;
				}
				clk_set_rate(on2_decode_clk, HX170DEC_CPU_FREQ);
				on2_d_clk = clk_get(on2psm_data.dev, "on2_d_clk");
				if(IS_ERR(on2_d_clk)){
				   printk(KERN_ERR "get on2_d_clk failed\n");
					 return -ENOTTY;
				}
				//clk_enable(on2_d_clk);
				err = set_ddr_axi_config(1);
			}
			on2psm_data.on2_pwr_dec_clk_flag++;
			//spin_unlock(&on2psm_lock);
			mutex_unlock(&on2psm_mutex);
		    break;

		case ON2_PWR_G1_DEC_CLK_DISABLE:
			//spin_lock(&on2psm_lock);
			mutex_lock(&on2psm_mutex);
			on2psm_data.on2_pwr_dec_clk_flag--;
			if(on2psm_data.on2_pwr_dec_clk_flag == 0)
			{
				err = set_ddr_axi_config(0);
				clk_set_rate(on2_decode_clk, HX170DEC_CPU_FREQ_MIN);
				clk_put(on2_decode_clk);
				//clk_disable(on2_d_clk);
				clk_put(on2_d_clk);
				PDEBUG("on2_dec_clk have closed on2_clk_en_reg = 0x%x\n",
				readl((void __iomem *)io_p2v(AP_PWR_ON2_CLK_EN)));//0
			}

			if(on2psm_data.on2_pwr_dec_clk_flag < 0)
				on2psm_data.on2_pwr_dec_clk_flag = 0;

			//spin_unlock(&on2psm_lock);
			mutex_unlock(&on2psm_mutex);
			break;

		case ON2_PWR_7280_ENC_CLK_ENABLE:
			//spin_lock(&on2psm_lock);
			mutex_lock(&on2psm_mutex);

			if(on2psm_data.on2_pwr_enc_bus_clk_flag == 0)
			{
				on2_encode_clk = clk_get(on2psm_data.dev, "on2_encode_clk");
				if (IS_ERR(on2_encode_clk)) {
					printk(KERN_ERR "get on2_encode_clk failed\n");
					return -ENOTTY;
				}
				clk_set_rate(on2_encode_clk, HX280ENC_CPU_FREQ);
				on2_ebus_clk = clk_get(on2psm_data.dev, "on2_ebus_clk");
				if (IS_ERR(on2_ebus_clk)) {
					printk(KERN_ERR "get on2_ebus_clk failed\n");
					return -ENOTTY;
				}
				//clk_enable(on2_ebus_clk);
				err = set_ddr_axi_config(1);
			}
			on2psm_data.on2_pwr_enc_bus_clk_flag++;

			if(on2psm_data.on2_pwr_enc0_clk_flag == 0)
			{
				on2_e0_clk = clk_get(on2psm_data.dev, "on2_e0_clk");
				if (IS_ERR(on2_e0_clk)) {
					printk(KERN_ERR "get on2_e0_clk failed\n");
					return -ENOTTY;
				}
				//clk_enable(on2_e0_clk);
			}
			on2psm_data.on2_pwr_enc0_clk_flag++;
			//spin_unlock(&on2psm_lock);
			mutex_unlock(&on2psm_mutex);

		    break;

		case ON2_PWR_7280_ENC_CLK_DISABLE:
			//spin_lock(&on2psm_lock);
			mutex_lock(&on2psm_mutex);
			on2psm_data.on2_pwr_enc_bus_clk_flag--;
			on2psm_data.on2_pwr_enc0_clk_flag--;
			if(on2psm_data.on2_pwr_enc_bus_clk_flag == 0)
			{
				err = set_ddr_axi_config(0);
				clk_set_rate(on2_encode_clk, HX280ENC_CPU_FREQ_MIN);
				clk_put(on2_encode_clk);
				//clk_disable(on2_ebus_clk);
				clk_put(on2_ebus_clk);
			}
			if(on2psm_data.on2_pwr_enc0_clk_flag == 0)
			{
				//clk_disable(on2_e0_clk);
				clk_put(on2_e0_clk);
				PDEBUG("on2_enc0_clk have closed on2_clk_en_reg = 0x%x\n",
					readl((void __iomem *)io_p2v(AP_PWR_ON2_CLK_EN)));//0 is normal
			}

			if(on2psm_data.on2_pwr_enc_bus_clk_flag < 0)
				on2psm_data.on2_pwr_enc_bus_clk_flag = 0;
			if(on2psm_data.on2_pwr_enc0_clk_flag  < 0)
				on2psm_data.on2_pwr_enc0_clk_flag = 0;

			//spin_unlock(&on2psm_lock);
			mutex_unlock(&on2psm_mutex);

			break;

		case ON2_PWR_H1_ENC_CLK_ENABLE:
			//spin_lock(&on2psm_lock);
			mutex_lock(&on2psm_mutex);
			if(on2psm_data.on2_pwr_enc_bus_clk_flag == 0)
			{
				on2_encode_clk = clk_get(on2psm_data.dev, "on2_encode_clk");
				if (IS_ERR(on2_encode_clk)) {
					printk(KERN_ERR "get on2_encode_clk failed\n");
					return -ENOTTY;
				}
				clk_set_rate(on2_encode_clk, HX280ENC_H1_CPU_FREQ);
				on2_ebus_clk = clk_get(on2psm_data.dev, "on2_ebus_clk");
				if (IS_ERR(on2_ebus_clk)) {
					printk(KERN_ERR "get on2_ebus_clk failed\n");
					return -ENOTTY;
				}
				//clk_enable(on2_ebus_clk);
				err = set_ddr_axi_config(1);
			}
			on2psm_data.on2_pwr_enc_bus_clk_flag++;

			if(on2psm_data.on2_pwr_enc1_clk_flag == 0)
			{
				on2_e1_clk = clk_get(on2psm_data.dev, "on2_e1_clk");
				if (IS_ERR(on2_e1_clk)) {
					printk(KERN_ERR "get on2_e1_clk failed\n");
					return -ENOTTY;
				}
				//clk_enable(on2_e1_clk);
			}
			on2psm_data.on2_pwr_enc1_clk_flag++;
			//spin_unlock(&on2psm_lock);
			mutex_unlock(&on2psm_mutex);
			break;

		case ON2_PWR_H1_ENC_CLK_DISABLE:
			//spin_lock(&on2psm_lock);
			mutex_lock(&on2psm_mutex);
			on2psm_data.on2_pwr_enc_bus_clk_flag--;
			on2psm_data.on2_pwr_enc1_clk_flag--;
			if(on2psm_data.on2_pwr_enc_bus_clk_flag == 0)
			{
				err = set_ddr_axi_config(0);
				clk_set_rate(on2_encode_clk, HX280ENC_H1_CPU_FREQ_MIN);
				clk_put(on2_encode_clk);
				//clk_disable(on2_ebus_clk);
				clk_put(on2_ebus_clk);
			}
			if(on2psm_data.on2_pwr_enc1_clk_flag == 0)
			{
				//clk_disable(on2_e1_clk);
				clk_put(on2_e1_clk);
				PDEBUG("on2_enc1_clk have closed on2_clk_en_reg = 0x%x\n",
					readl((void __iomem *)io_p2v(AP_PWR_ON2_CLK_EN)));//0
			}

			if(on2psm_data.on2_pwr_enc_bus_clk_flag < 0)
				on2psm_data.on2_pwr_enc_bus_clk_flag = 0;
			if(on2psm_data.on2_pwr_enc1_clk_flag  < 0)
				on2psm_data.on2_pwr_enc1_clk_flag = 0;

			//spin_unlock(&on2psm_lock);
			mutex_unlock(&on2psm_mutex);
			break;
#elif defined(CONFIG_ARCH_LC186X)
		/*ON2 External ClOCK OPERATE*/
		case ON2_PWR_G1_DEC_CLK_ENABLE:
		case ON2_PWR_7280_ENC_CLK_ENABLE:
			mutex_lock(&on2psm_mutex);
			if(on2psm_data.on2_pwr_codec_clk_flag == 0) //in order to play anytime,but get and put
			{
				on2_codec_mclk = clk_get(on2psm_data.dev, "on2_codec_mclk");
				if(IS_ERR(on2_codec_mclk)){
					printk(KERN_ERR "get on2_codec_mclk failed\n");
					return -ENOTTY;
				}
				clk_set_rate(on2_codec_mclk, HX170DEC_CPU_FREQ);
				clk_enable(on2_codec_mclk);
			}
			on2psm_data.on2_pwr_codec_clk_flag++;

			if(on2psm_data.on2_pwr_sw1_codec_clk_flag == 0)
			{
				ap_sw1_on2codec_clk = clk_get(on2psm_data.dev, "ap_sw1_on2codec_clk");
				if(IS_ERR(ap_sw1_on2codec_clk)){
					printk(KERN_ERR "get ap_sw1_on2codec_clk failed\n");
					return -ENOTTY;
				}
				clk_enable(ap_sw1_on2codec_clk);
			}
			on2psm_data.on2_pwr_sw1_codec_clk_flag++;

			if(on2psm_data.on2_pwr_peri_sw1_codec_clk_flag == 0)
			{
				ap_peri_sw1_on2codec_clk = clk_get(on2psm_data.dev, "ap_peri_sw1_on2codec_clk");
				if(IS_ERR(ap_peri_sw1_on2codec_clk)){
					printk(KERN_ERR "get ap_peri_sw1_on2codec_clk failed\n");
					return -ENOTTY;
				}
				clk_enable(ap_peri_sw1_on2codec_clk);
			}
			on2psm_data.on2_pwr_peri_sw1_codec_clk_flag++;
			mutex_unlock(&on2psm_mutex);
		    break;

		case ON2_PWR_G1_DEC_CLK_DISABLE:
		case ON2_PWR_7280_ENC_CLK_DISABLE:
			mutex_lock(&on2psm_mutex);
			on2psm_data.on2_pwr_codec_clk_flag--;
			on2psm_data.on2_pwr_sw1_codec_clk_flag--;
			on2psm_data.on2_pwr_peri_sw1_codec_clk_flag--;
			if(on2psm_data.on2_pwr_codec_clk_flag == 0)
			{
				clk_set_rate(on2_codec_mclk, HX170DEC_CPU_FREQ_MIN);
				clk_disable(on2_codec_mclk);
				clk_put(on2_codec_mclk);
				PDEBUG("on2_codec_mclk have closed AP_PWR_ON2CLK_CTL1 reg = 0x%x\n",
					readl((void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL1)));//EN: bit 8
			}

			if(on2psm_data.on2_pwr_codec_clk_flag == 0)
			{
				clk_disable(ap_sw1_on2codec_clk);
				clk_put(ap_sw1_on2codec_clk);
			}
			if(on2psm_data.on2_pwr_peri_sw1_codec_clk_flag == 0)
			{
				clk_disable(ap_peri_sw1_on2codec_clk);
				clk_put(ap_peri_sw1_on2codec_clk);

			}
			PDEBUG("sw1_on2codec_clk/peri_sw1_on2code_clk have closed, AP_PWR_CLK_EN4 reg = 0x%x\n",
					readl((void __iomem *)io_p2v(AP_PWR_CLK_EN4)));//peri: bit 6, sw1: bit 7

			if(on2psm_data.on2_pwr_codec_clk_flag < 0)
				on2psm_data.on2_pwr_codec_clk_flag = 0;
			if(on2psm_data.on2_pwr_sw1_codec_clk_flag < 0)
				on2psm_data.on2_pwr_sw1_codec_clk_flag = 0;
			if(on2psm_data.on2_pwr_peri_sw1_codec_clk_flag < 0)
				on2psm_data.on2_pwr_peri_sw1_codec_clk_flag = 0;

			mutex_unlock(&on2psm_mutex);
			break;

		case ON2_PWR_H1_ENC_CLK_ENABLE:
			mutex_lock(&on2psm_mutex);
			if(on2psm_data.on2_pwr_enc_clk_flag == 0)
			{
				on2_enc_mclk = clk_get(on2psm_data.dev, "on2_enc_mclk");
				if (IS_ERR(on2_enc_mclk)) {
					printk(KERN_ERR "get on2_enc_mclk failed\n");
					return -ENOTTY;
				}
				clk_set_rate(on2_enc_mclk, HX280ENC_H1_CPU_FREQ);
				clk_enable(on2_enc_mclk);
			}
			on2psm_data.on2_pwr_enc_clk_flag++;

			if(on2psm_data.on2_pwr_sw1_enc_clk_flag == 0)
			{
				ap_sw1_on2enc_clk = clk_get(on2psm_data.dev, "ap_sw1_on2enc_clk");
				if (IS_ERR(ap_sw1_on2enc_clk)) {
					printk(KERN_ERR "get ap_sw1_on2enc_clk failed\n");
					return -ENOTTY;
				}
				clk_enable(ap_sw1_on2enc_clk);
			}
			on2psm_data.on2_pwr_sw1_enc_clk_flag++;

			if(on2psm_data.on2_pwr_peri_sw1_enc_clk_flag == 0)
			{
				ap_peri_sw1_on2enc_clk = clk_get(on2psm_data.dev, "ap_peri_sw1_on2enc_clk");
				if (IS_ERR(ap_peri_sw1_on2enc_clk)) {
					printk(KERN_ERR "get ap_peri_sw1_on2enc_clk failed\n");
					return -ENOTTY;
				}
				clk_enable(ap_peri_sw1_on2enc_clk);
			}
			on2psm_data.on2_pwr_peri_sw1_enc_clk_flag++;
			mutex_unlock(&on2psm_mutex);
			break;

		case ON2_PWR_H1_ENC_CLK_DISABLE:
			mutex_lock(&on2psm_mutex);
			on2psm_data.on2_pwr_enc_clk_flag--;
			on2psm_data.on2_pwr_sw1_enc_clk_flag--;
			on2psm_data.on2_pwr_peri_sw1_enc_clk_flag--;
			if(on2psm_data.on2_pwr_enc_clk_flag == 0)
			{
				clk_set_rate(on2_enc_mclk, HX280ENC_H1_CPU_FREQ_MIN);
				clk_disable(on2_enc_mclk);
				clk_put(on2_enc_mclk);
				PDEBUG("on2_enc_mclk have closed, AP_PWR_ON2CLK_CTL0 reg = 0x%x\n",
					readl((void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL0)));//EN: bit 8
			}

			if(on2psm_data.on2_pwr_sw1_enc_clk_flag == 0)
			{
				clk_disable(ap_sw1_on2enc_clk);
				clk_put(ap_sw1_on2enc_clk);
			}
			if(on2psm_data.on2_pwr_peri_sw1_enc_clk_flag == 0)
			{
				clk_disable(ap_peri_sw1_on2enc_clk);
				clk_put(ap_peri_sw1_on2enc_clk);
			}
			PDEBUG("sw1_on2enc_clk/peri_sw1_on2enc_clk have closed, AP_PWR_CLK_EN4 reg = 0x%x\n",
					readl((void __iomem *)io_p2v(AP_PWR_CLK_EN4)));//peri: bit 8, sw1: bit 9

			if(on2psm_data.on2_pwr_enc_clk_flag < 0)
				on2psm_data.on2_pwr_enc_clk_flag = 0;
			if(on2psm_data.on2_pwr_sw1_enc_clk_flag < 0)
				on2psm_data.on2_pwr_sw1_enc_clk_flag = 0;
			if(on2psm_data.on2_pwr_peri_sw1_enc_clk_flag < 0)
				on2psm_data.on2_pwr_peri_sw1_enc_clk_flag = 0;

			mutex_unlock(&on2psm_mutex);
			break;
#endif

		/*ON2 POWER OPERATE*/
		case ON2POWER_UP_UPDATEFLAG:
			//power up ON2 and update on2power flag, will get sem to wait
			//spin_lock(&on2psm_lock);
			mutex_lock(&on2psm_mutex);
			if(on2psm_data.on2_pwr_power_on_flag == 0)
			{
				/*AP_PWR_ON2_PD_CTL config*/
				power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
				power_reg_val = (1<< AP_PWR_PD_EN_WE)|(power_reg_val&(~(1<<AP_PWR_PD_EN)));
				/*clear power down en */
				writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
				power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
				power_reg_val |= (1<<AP_PWR_WK_UP_WE)|(1<<AP_PWR_WK_UP);
				/*on2 power up*/
				writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
#if defined(CONFIG_ARCH_LC186X)
				while((readl(io_p2v(AP_PWR_PDFSM_ST1)) &
						(0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)) != 0 && timeout-- > 0)
					udelay(10);
#endif
			}
#if defined(CONFIG_ARCH_LC181X)
			PDEBUG("on2 have open power,reg = 0x%x\n",readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL)));//0x6 normal
#elif defined(CONFIG_ARCH_LC186X)
			PDEBUG("on2 power up,reg = 0x%x\n",
					(readl(io_p2v(AP_PWR_PDFSM_ST1)) & (0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)));//0x0 normal
#endif
			on2psm_data.on2_pwr_power_on_flag++;
			//spin_unlock(&on2psm_lock);
			mutex_unlock(&on2psm_mutex);
			break;

		case ON2POWER_DOWN_UPDATEFLAG:
		    //power down ON2 and update on2power flag, will get sem to wait
			//spin_lock(&on2psm_lock);
			mutex_lock(&on2psm_mutex);
			on2psm_data.on2_pwr_power_on_flag--;
			if(on2psm_data.on2_pwr_power_on_flag == 0)
			{
				/*AP_PWR_ON2_PD_CTL config*/
				power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
				power_reg_val = (1<< AP_PWR_PD_MK_WE)|(1<< AP_PWR_WK_UP_WE)|
								(power_reg_val&(~(1<<AP_PWR_PD_MK))&(~(1<< AP_PWR_WK_UP)));
				/*clear PD mask and clear wakeup en */
				writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
				power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
				power_reg_val |= (1<<AP_PWR_PD_EN_WE)|(1<<AP_PWR_PD_EN);
				/*on2 power down*/
				writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
			}
#if defined(CONFIG_ARCH_LC181X)
			PDEBUG("on2 have close power,reg = 0x%x\n",readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL)));//0x5 normal
#elif defined(CONFIG_ARCH_LC186X)
			PDEBUG("on2 power down,reg = 0x%x\n",
					(readl(io_p2v(AP_PWR_PDFSM_ST1)) & (0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)));//0x7 normal
#endif

			if(on2psm_data.on2_pwr_power_on_flag < 0)
				on2psm_data.on2_pwr_power_on_flag = 0;
			//spin_unlock(&on2psm_lock);
			mutex_unlock(&on2psm_mutex);
			break;

		/*Clear any flag, when some crash happen*/
		case ON2_RESET_FLAG:
			PDEBUG("on2 codec reset \n");
			break;
		/*other cmd case*/
		default:
		    break;
	}

#if defined(CONFIG_ARCH_LC181X)
	PDEBUG("on2psm-ioctl power= %d,dec_clk=%d,enc0_clk=%d,enc1_clk=%d,enc_bus=%d\n",
		on2psm_data.on2_pwr_power_on_flag,
		on2psm_data.on2_pwr_dec_clk_flag,
		on2psm_data.on2_pwr_enc0_clk_flag,
		on2psm_data.on2_pwr_enc1_clk_flag,
		on2psm_data.on2_pwr_enc_bus_clk_flag);
#elif defined(CONFIG_ARCH_LC186X)
	PDEBUG("on2psm-ioctl power= %d,codec_clk=%d,sw1_codec_clk=%d,peri_codec_clk=%d,enc_clk=%d,sw1_enc_clk=%d,peri_enc_clk=%d\n",
		on2psm_data.on2_pwr_power_on_flag,
		on2psm_data.on2_pwr_codec_clk_flag,
		on2psm_data.on2_pwr_sw1_codec_clk_flag,
		on2psm_data.on2_pwr_peri_sw1_codec_clk_flag,
		on2psm_data.on2_pwr_enc_clk_flag,
		on2psm_data.on2_pwr_sw1_enc_clk_flag,
		on2psm_data.on2_pwr_peri_sw1_enc_clk_flag);
#endif

	return 0;
}

/*------------------------------------------------------------------------------
	Function name   : on2psm_open
	Description     : open method

	Return type     : int
------------------------------------------------------------------------------*/
static int on2psm_open(struct inode *inode, struct file *filp)
{
	unsigned int power_reg_val = 0;
#if defined(CONFIG_ARCH_LC186X)
	int timeout = 100;
#endif
	filp->private_data = (void *) &on2psm_data;

#if defined(CONFIG_ARCH_LC186X)
	//make on2 must have power
	if((readl(io_p2v(AP_PWR_PDFSM_ST1)) & (0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)) != 0x0)
	{
		power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
		power_reg_val |= (1 << AP_PWR_WK_UP_WE) | (1 << AP_PWR_WK_UP);
		/*on2 power up*/
		writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
		while((readl(io_p2v(AP_PWR_PDFSM_ST1)) &
				(0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)) != 0x0 && timeout-- > 0)
			udelay(10);

	}
	PDEBUG("on2psm_open on2_power_on = 0x%x\n",
			(readl(io_p2v(AP_PWR_PDFSM_ST1)) & (0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)));//0x0 normal

#elif defined(CONFIG_ARCH_LC181X)
	//make on2 must have power
	/*AP_PWR_ON2_PD_CTL config*/
	power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
	if((power_reg_val&(1<<AP_PWR_WK_UP))==0)
	{
		power_reg_val = (1<< AP_PWR_PD_EN_WE)|(power_reg_val&(~(1<<AP_PWR_PD_EN)));
		/*clear power down en */
		writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
		power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
		power_reg_val |= (1<<AP_PWR_WK_UP_WE)|(1<<AP_PWR_WK_UP);
		/*on2 power up*/
		writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
	}

	PDEBUG("on2psm_open on2_power_on = 0x%x\n",readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL)));//0x6 normal
#endif

	PDEBUG("dev opened\n");
	return 0;
}

#ifdef USE_SYNC_SIGIO
/*------------------------------------------------------------------------------
	Function name   : on2psm_fasync
	Description     : Method for signing up for async notify

	Return type     : int
------------------------------------------------------------------------------*/
static int on2psm_fasync(int fd, struct file *filp, int mode)
{
	on2psm_t *dev = (on2psm_t *) filp->private_data;

	PDEBUG("fasync called\n");

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}
#endif

/*------------------------------------------------------------------------------
	Function name   : on2psm_release
	Description     : Release driver

	Return type     : int
------------------------------------------------------------------------------*/
static int on2psm_release(struct inode *inode, struct file *filp)
{
#ifdef USE_SYNC_SIGIO
	if(filp->f_flags & FASYNC) {
	    /* remove this filp from the asynchronusly notified filp's */
	    on2psm_fasync(-1, filp, 0);
	}
#endif

	PDEBUG("dev closed\n");
	return 0;
}

/* VFS methods */
static struct file_operations on2psm_fops = {
  open:on2psm_open,
  release:on2psm_release,
  unlocked_ioctl:on2psm_ioctl,
#ifdef USE_SYNC_SIGIO
  fasync:on2psm_fasync,
#endif
};

/*when be transfered*/
int __init on2psm_probe(struct platform_device *pdev)
{
	int result;
	unsigned int power_reg_val = 0;
	//unsigned int clk_usercout = 0;

	PDEBUG("on2psm_probe\n");

	//spin_lock_init(&on2psm_lock);

#if defined(CONFIG_ARCH_LC181X)
	on2psm_data.on2_pwr_dec_clk_flag = 0;
	on2psm_data.on2_pwr_enc0_clk_flag = 0;
	on2psm_data.on2_pwr_enc1_clk_flag = 0;
	on2psm_data.on2_pwr_enc_bus_clk_flag = 0;
	on2psm_data.on2_pwr_power_on_flag = 0;

	on2psm_data.on2_ddr_axi_config_flag = 0;
#elif defined(CONFIG_ARCH_LC186X)
	on2psm_data.on2_pwr_codec_clk_flag = 0; /* 7280 & G1 */
	on2psm_data.on2_pwr_sw1_codec_clk_flag = 0;
	on2psm_data.on2_pwr_peri_sw1_codec_clk_flag = 0;

	on2psm_data.on2_pwr_enc_clk_flag = 0; /* H1 */
	on2psm_data.on2_pwr_sw1_enc_clk_flag = 0;
	on2psm_data.on2_pwr_peri_sw1_enc_clk_flag = 0;

	on2psm_data.on2_pwr_power_on_flag = 0;
#endif

	on2psm_data.irq = AP_PWR_IRQ_NUM;
	on2psm_data.async_queue = NULL;
	on2psm_data.dev = &pdev->dev;

	/*close on2 codec pwr clk and power, but "3 on2driver probe func"have close clk, so the user count will zero */
#if 0
	PDEBUG("ON2 clk close \n");
	/*dec clk disable and put*/
	on2_d_clk = clk_get(on2psm_data.dev, "on2_d_clk");
	if(IS_ERR(on2_d_clk)){
	   printk(KERN_ERR "get on2_d_clk failed\n");
		 return -ENOTTY;
	}
	clk_usercout = clk_get_usecount(on2_d_clk);
	if(clk_usercout > 0)
	{
		for(i = 0;i<clk_usercout;i++)
			clk_disable(on2_d_clk);
	}
	/*enc bus clk disable and put*/
	on2_ebus_clk = clk_get(on2psm_data.dev, "on2_ebus_clk");
	if (IS_ERR(on2_ebus_clk)) {
		printk(KERN_ERR "get on2_ebus_clk failed\n");
		return -ENOTTY;
	}
	clk_usercout = clk_get_usecount(on2_ebus_clk);
	if(clk_usercout > 0)
	{
		for(i = 0;i<clk_usercout;i++)
			clk_disable(on2_ebus_clk);
	}
	/*enc0 clk disable and put*/
	on2_e0_clk = clk_get(on2psm_data.dev, "on2_e0_clk");
	if (IS_ERR(on2_e0_clk)) {
		printk(KERN_ERR "get on2_e0_clk failed\n");
		return -ENOTTY;
	}
	clk_usercout = clk_get_usecount(on2_e0_clk);
	if(clk_usercout > 0)
	{
		for(i = 0;i<clk_usercout;i++)
			clk_disable(on2_e0_clk);
	}
	/*enc1 clk disable and put*/
	on2_e1_clk = clk_get(on2psm_data.dev, "on2_e1_clk");
	if (IS_ERR(on2_e1_clk)) {
		printk(KERN_ERR "get on2_e1_clk failed\n");
		return -ENOTTY;
	}
	clk_usercout = clk_get_usecount(on2_e1_clk);
	if(clk_usercout > 0)
	{
		for(i = 0;i<clk_usercout;i++)
			clk_disable(on2_e1_clk);
	}
	clk_put(on2_d_clk);
	clk_put(on2_ebus_clk);
	clk_put(on2_e0_clk);
	clk_put(on2_e1_clk);
#endif
	/*AP_PWR_ON2_PD_CTL config*/
	PDEBUG("ON2 power down \n");
	/*AP_PWR_ON2_PD_CTL config*/
	power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
	power_reg_val = (1<< AP_PWR_PD_MK_WE)|(1<< AP_PWR_WK_UP_WE)|
					(power_reg_val&(~(1<<AP_PWR_PD_MK))&(~(1<< AP_PWR_WK_UP)));
	/*clear PD mask and clear wakeup en */
	writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
	power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
	power_reg_val |= (1<<AP_PWR_PD_EN_WE)|(1<<AP_PWR_PD_EN);
	/*on2 power down*/
	writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));

#ifdef USE_SYNC_SIGIO
	/* get the IRQ line */
	result = request_irq(on2psm_data.irq, on2psm_isr,
	                     IRQF_DISABLED | IRQF_SHARED,
	                     "on2psm", (void *) &on2psm_data);
	if(result != 0) {
	    printk(KERN_ERR "on2psm: request interrupt failed\n");
	    goto error_request_irq;
	}
#endif
	/*register device driver*/
	result = register_chrdev(on2psm_major, "on2psm", &on2psm_fops);
	if(result < 0) {
	    printk(KERN_ERR "on2psm: register_chrdev failed\n");
	    goto error_reserve_io;
	}
	on2psm_major = result;

	/*create device node when load*/
	on2psm_class = class_create(THIS_MODULE, "on2psm");
	if (IS_ERR(on2psm_class)) {
	    printk(KERN_ERR "on2psm: class_create failed\n");
	    goto error_class_create;
	}
	device_create(on2psm_class, NULL, MKDEV(on2psm_major, 0), NULL, "on2psm");

#if defined(CONFIG_ARCH_LC181X)
	printk(KERN_INFO "on2psm: probe successfully. Major=%d,on2_clk_en_reg=0x%x,power=0x%x\n",
		on2psm_major,
		readl((void __iomem *)io_p2v(AP_PWR_ON2_CLK_EN)),//0
		readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL)));//0x5
#elif defined(CONFIG_ARCH_LC186X)
	printk(KERN_INFO "on2psm: probe successfully. power = 0x%x\n",
		(readl(io_p2v(AP_PWR_PDFSM_ST1)) & (0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)));//0x7
#endif
	return 0;

error_class_create:
	unregister_chrdev(on2psm_major, "on2psm");
error_reserve_io:
#ifdef USE_SYNC_SIGIO
	free_irq(on2psm_data.irq, (void *) &on2psm_data);
#endif
#ifdef USE_SYNC_SIGIO
error_request_irq:
#endif
	printk(KERN_INFO "on2psm: probe failed\n");
	return result;
}

void __exit on2psm_remove(struct platform_device *pdev)
{
	//on2psm_t *dev = (on2psm_t *) &on2psm_data;

	/*unregister device, and free irq*/
	device_destroy(on2psm_class, MKDEV(on2psm_major, 0));
	class_destroy(on2psm_class);
	unregister_chrdev(on2psm_major, "on2psm");

#ifdef USE_SYNC_SIGIO
	free_irq(dev->irq, (void *) dev);
#endif
	return;
}

#ifdef CONFIG_ON2_RESUME
static int on2_par_suspend(struct device *dev)
{
	unsigned int power_reg_val = 0;
#if 0
	unsigned int clk_usercout = 0;
	int i = 0;

	/*disable clk*/
	/*dec clk disable and put*/
	clk_usercout = clk_get_usecount(on2_d_clk);
	if(clk_usercout > 0)//1,because just enable 1
	{
		PDEBUG("on2_d_clk flag = %d \n",on2psm_data.on2_pwr_dec_clk_flag);
		for(i = 0;i<clk_usercout;i++)
			clk_disable(on2_d_clk);
		clk_put(on2_decode_clk);
		clk_put(on2_d_clk);
	}
	/*enc bus clk disable and put*/
	clk_usercout = clk_get_usecount(on2_ebus_clk);
	if(clk_usercout > 0)
	{
		PDEBUG("on2_ebus_clk flag = %d \n",on2psm_data.on2_pwr_enc_bus_clk_flag);
		for(i = 0;i<clk_usercout;i++)
			clk_disable(on2_ebus_clk);
		clk_put(on2_encode_clk);
		clk_put(on2_ebus_clk);
	}
	/*enc0 clk disable and put*/
	clk_usercout = clk_get_usecount(on2_e0_clk);
	if(clk_usercout > 0)
	{
		PDEBUG("on2_e0_clk flag = %d \n",on2psm_data.on2_pwr_enc0_clk_flag);
		for(i = 0;i<clk_usercout;i++)
			clk_disable(on2_e0_clk);
		clk_put(on2_e0_clk);
	}
	/*enc1 clk disable and put*/
	clk_usercout = clk_get_usecount(on2_e1_clk);
	if(clk_usercout > 0)
	{
		PDEBUG("on2_e1_clk flag = %d \n",on2psm_data.on2_pwr_enc1_clk_flag);
		for(i = 0;i<clk_usercout;i++)
			clk_disable(on2_e1_clk);
		clk_put(on2_e1_clk);
	}
#endif

	/*power down*/
	power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
	if((on2psm_data.on2_pwr_power_on_flag > 0)||(power_reg_val&(1<<AP_PWR_WK_UP)))
	{
		PDEBUG("on2_power flag = %d, need to close \n",on2psm_data.on2_pwr_power_on_flag);
		/*AP_PWR_ON2_PD_CTL config*/
		power_reg_val = (1<< AP_PWR_PD_MK_WE)|(1<< AP_PWR_WK_UP_WE)|
						(power_reg_val&(~(1<<AP_PWR_PD_MK))&(~(1<< AP_PWR_WK_UP)));
		/*clear PD mask and clear wakeup en */
		writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
		power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
		power_reg_val |= (1<<AP_PWR_PD_EN_WE)|(1<<AP_PWR_PD_EN);
		/*on2 power down*/
		writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
	}

	/*reset flag*/
    /* FIX L1810_Bug00000753 BEGIN DATE:2012-7-17 AUTHOR NAME ZHUXIAOPING */
    /*
	on2psm_data.on2_pwr_dec_clk_flag = 0;
	on2psm_data.on2_pwr_enc0_clk_flag = 0;
	on2psm_data.on2_pwr_enc1_clk_flag = 0;
	on2psm_data.on2_pwr_enc_bus_clk_flag = 0;
	on2psm_data.on2_pwr_power_on_flag = 0;
        */
    /* FIX L1810_Bug00000753 END DATE:2012-7-17 AUTHOR NAME ZHUXIAOPING */
#if defined(CONFIG_ARCH_LC181X)
	PDEBUG("on2_par_suspend on2_clk_en_reg=0x%x,on2_power_reg=0x%x\n",
		readl((void __iomem *)io_p2v(AP_PWR_ON2_CLK_EN)),
		readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL)));
#elif defined(CONFIG_ARCH_LC186X)
	PDEBUG("on2_par_suspend, on2_power_reg = 0x%x\n",
		(readl(io_p2v(AP_PWR_PDFSM_ST1)) & (0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)));//0x7
#endif
	return 0;
}

/*when video can play, after sleep,here need to resume*/
static int on2_par_resume(struct device *dev)
{
	/* FIX L1810_Bug00000753 BEGIN DATE:2012-7-17 AUTHOR NAME ZHUXIAOPING */
	unsigned int power_reg_val = 0;
#if defined(CONFIG_ARCH_LC186X)
	int timeout = 100;
#endif

	PDEBUG("on2_par_resume \n");

#if defined(CONFIG_ARCH_LC181X)
	if(on2psm_data.on2_pwr_dec_clk_flag > 0)
	{
		on2_decode_clk = clk_get(on2psm_data.dev, "on2_decode_clk");
		if(IS_ERR(on2_decode_clk)){
		    printk(KERN_ERR "get on2_decode_clk failed\n");
			return -ENOTTY;
		}
		clk_set_rate(on2_decode_clk, HX170DEC_CPU_FREQ);
		on2_d_clk = clk_get(on2psm_data.dev, "on2_d_clk");
		if(IS_ERR(on2_d_clk)){
		    printk(KERN_ERR "get on2_d_clk failed\n");
	        return -ENOTTY;
		}
		//clk_enable(on2_d_clk);
	}

	if(on2psm_data.on2_pwr_enc_bus_clk_flag > 0)
	{
		on2_encode_clk = clk_get(on2psm_data.dev, "on2_encode_clk");
		if (IS_ERR(on2_encode_clk)) {
			printk(KERN_ERR "get on2_encode_clk failed\n");
			return -ENOTTY;
		}
		clk_set_rate(on2_encode_clk,HX280ENC_CPU_FREQ);
		on2_ebus_clk = clk_get(on2psm_data.dev, "on2_ebus_clk");
		if (IS_ERR(on2_ebus_clk)) {
			printk(KERN_ERR "get on2_ebus_clk failed\n");
			return -ENOTTY;
		}
		//clk_enable(on2_ebus_clk);
	}

	if(on2psm_data.on2_pwr_enc0_clk_flag > 0)
	{
		on2_e0_clk = clk_get(on2psm_data.dev, "on2_e0_clk");
		if (IS_ERR(on2_e0_clk)) {
			printk(KERN_ERR "get on2_e0_clk failed\n");
			return -ENOTTY;
		}
		//clk_enable(on2_e0_clk);
	}

	if(on2psm_data.on2_pwr_enc1_clk_flag > 0)
	{
		on2_e1_clk = clk_get(on2psm_data.dev, "on2_e1_clk");
		if (IS_ERR(on2_e1_clk)) {
			printk(KERN_ERR "get on2_e1_clk failed\n");
			return -ENOTTY;
		}
		//clk_enable(on2_e1_clk);
	}
#elif defined(CONFIG_ARCH_LC186X)
	if(on2psm_data.on2_pwr_codec_clk_flag > 0)
	{
		on2_codec_mclk = clk_get(on2psm_data.dev, "on2_codec_mclk");
		if(IS_ERR(on2_codec_mclk)){
			printk(KERN_ERR "get on2_codec_mclk failed\n");
			return -ENOTTY;
		}
		clk_set_rate(on2_codec_mclk, HX170DEC_CPU_FREQ);
		clk_enable(on2_codec_mclk);
	}
	if(on2psm_data.on2_pwr_sw1_codec_clk_flag > 0)
	{
		ap_sw1_on2codec_clk = clk_get(on2psm_data.dev, "ap_sw1_on2codec_clk");
		if(IS_ERR(ap_sw1_on2codec_clk)){
			printk(KERN_ERR "get ap_sw1_on2codec_clk failed\n");
			return -ENOTTY;
		}
		clk_enable(ap_sw1_on2codec_clk);
	}
	if(on2psm_data.on2_pwr_peri_sw1_codec_clk_flag > 0)
	{
		ap_peri_sw1_on2codec_clk = clk_get(on2psm_data.dev, "ap_peri_sw1_on2codec_clk");
		if(IS_ERR(ap_peri_sw1_on2codec_clk)){
			printk(KERN_ERR "get ap_peri_sw1_on2codec_clk failed\n");
			return -ENOTTY;
		}
		clk_enable(ap_peri_sw1_on2codec_clk);
	}
	if(on2psm_data.on2_pwr_enc_clk_flag > 0)
	{
		on2_enc_mclk = clk_get(on2psm_data.dev, "on2_enc_mclk");
		if (IS_ERR(on2_enc_mclk)) {
			printk(KERN_ERR "get on2_enc_mclk failed\n");
			return -ENOTTY;
		}
		clk_set_rate(on2_enc_mclk, HX280ENC_H1_CPU_FREQ);
		clk_enable(on2_enc_mclk);
	}
	if(on2psm_data.on2_pwr_sw1_enc_clk_flag > 0)
	{
		ap_sw1_on2enc_clk = clk_get(on2psm_data.dev, "ap_sw1_on2enc_clk");
		if (IS_ERR(ap_sw1_on2enc_clk)) {
			printk(KERN_ERR "get ap_sw1_on2enc_clk failed\n");
			return -ENOTTY;
		}
		clk_enable(ap_sw1_on2enc_clk);
	}
	if(on2psm_data.on2_pwr_peri_sw1_enc_clk_flag > 0)
	{
		ap_peri_sw1_on2enc_clk = clk_get(on2psm_data.dev, "ap_peri_sw1_on2enc_clk");
		if (IS_ERR(ap_peri_sw1_on2enc_clk)) {
			printk(KERN_ERR "get ap_peri_sw1_on2enc_clk failed\n");
			return -ENOTTY;
		}
		clk_enable(ap_peri_sw1_on2enc_clk);
	}
#endif

	if(on2psm_data.on2_pwr_power_on_flag > 0)
	{
		/*AP_PWR_ON2_PD_CTL config*/
		power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
		power_reg_val = (1<< AP_PWR_PD_EN_WE)|(power_reg_val&(~(1<<AP_PWR_PD_EN)));
		/*clear power down en */
		writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
		power_reg_val = readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
		power_reg_val |= (1<<AP_PWR_WK_UP_WE)|(1<<AP_PWR_WK_UP);
		/*on2 power up*/
		writel(power_reg_val ,(void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL));
#if defined(CONFIG_ARCH_LC186X)
		while((readl(io_p2v(AP_PWR_PDFSM_ST1)) &
				(0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)) != 0x0 && timeout-- > 0)
			udelay(10);
#endif
	}
    /* FIX L1810_Bug00000753 END DATE:2012-7-17 AUTHOR NAME ZHUXIAOPING */
#if defined(CONFIG_ARCH_LC181X)
	PDEBUG("on2 have open power,reg = 0x%x\n", readl((void __iomem *)io_p2v(AP_PWR_ON2_PD_CTL)));
#elif defined(CONFIG_ARCH_LC186X)
	PDEBUG("on2_par_resume, power=%d, on2_power_reg = 0x%x\n",on2psm_data.on2_pwr_power_on_flag,
		(readl(io_p2v(AP_PWR_PDFSM_ST1)) & (0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)));//0x0
#endif
	return 0;
}

static const struct dev_pm_ops on2_pm_ops = {
	.suspend = on2_par_suspend,
	.resume  = on2_par_resume,
};
#define ON2_PAR_PM_OPS (&on2_pm_ops)

#else
#define ON2_PAR_PM_OPS NULL
#endif  /* CONFIG_PM */


static struct platform_device on2psm_device = {
	.name = "on2psm",
	.id = -1,
};

static struct platform_driver on2psm_driver = {
	.driver = {
	    .name = "on2psm",
	    .owner = THIS_MODULE,
	    .pm = ON2_PAR_PM_OPS
	},
	.remove = __exit_p(on2psm_remove),
};

static int __init on2psm_init(void)
{
	platform_device_register(&on2psm_device);
	return platform_driver_probe(&on2psm_driver, on2psm_probe);
}

static void __exit on2psm_exit(void)
{
	platform_driver_unregister(&on2psm_driver);
	platform_device_unregister(&on2psm_device);
}

module_init(on2psm_init);
module_exit(on2psm_exit);

MODULE_AUTHOR("Haidong Fu<fuhaidong@leadcoretech.com>");
MODULE_DESCRIPTION("ON2 PSM driver");
MODULE_LICENSE("GPL");
