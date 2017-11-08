#ifndef __ARCH_ARM_COMIP_CLOCK_H
#define __ARCH_ARM_COMIP_CLOCK_H

struct clk_range {
	unsigned int offset;
	unsigned int min;
	unsigned int max;
};

struct clk_table {
	unsigned int rate;
	unsigned int parent_rate;
	unsigned int gr;
	unsigned int mul;
	unsigned int div;
};

static struct clk_table clk_com_uart_table[] = {
	{4000000 * 16, 265200000, 0, 320,   663},
	{2000000 * 16, 265200000, 0, 160,   663},
	{1000000 * 16, 265200000, 0, 80,    663},
	{921600 * 16,  265200000, 0, 4608, 41437},
	{460800 * 16,  265200000, 0, 2304, 41437},
	{230400 * 16,  265200000, 0, 1152, 41437},
	{115200 * 16,  265200000, 0, 576,  41437},
	{57600 * 16,   265200000, 0, 288,  41437},
	{38400 * 16,   265200000, 0, 192,  41437},
	{4000000 * 16, 213200000, 0, 320,   533},
	{2000000 * 16, 213200000, 0, 160,   533},
	{1000000 * 16, 213200000, 0, 80,    533},
	{921600 * 16,  213200000, 0, 480,  3470},
	{460800 * 16,  213200000, 0, 240,  3470},
	{230400 * 16,  213200000, 0, 120,  3470},
	{115200 * 16,  213200000, 0, 60,   3470},
	{57600 * 16,   213200000, 0, 30,   3470},
	{38400 * 16,   213200000, 0, 20,   3470},
	{4000000 * 16, 208000000, 0, 8,	13},
	{2000000 * 16, 208000000, 0, 4,	13},
	{1000000 * 16, 208000000, 0, 2,	13},
	{921600 * 16,  208000000, 0, 1024,  7222},
	{460800 * 16,  208000000, 0, 512,  7222},
	{230400 * 16,  208000000, 0, 256,  7222},
	{115200 * 16,  208000000, 0, 128,  7222},
	{57600 * 16,   208000000, 0, 64,   7222},
	{38400 * 16,   208000000, 0, 48,   8125},

	{0, 0, 0, 0, 0},
};

static struct clk_table clk_com_pcm_table[] = {
	{64000, 265200000, 0, 8, 16575},
	{64000, 213200000, 0, 5, 8328},
	{64000, 208000000, 0, 1, 1625},
	{25600000, 213200000, 0, 3980, 16575},

	{0, 0, 0, 0, 0},
};

static struct clk_table clk_com_clkout_table[] = {
	{2000000,  265200000, 0, 10,	663},
	{12288000, 265200000, 0, 512,	5525},
	{13000000, 265200000, 0, 5,		51},
	{24576000, 265200000, 0, 1024,	5525},
	{2048000,  265200000, 0, 133,	8611},
	{4096000,  265200000, 0, 320,	10359},
	{11289600, 265200000, 0, 420,	4933},
	{26000000, 265200000, 0, 10,	51},
	{2000000,  213200000, 0, 10,	533},
	{12288000, 213200000, 0, 400,	3470},
	{13000000, 213200000, 0, 5, 	41},
	{24576000, 213200000, 0, 800,	3470},
	{2048000,  213200000, 0, 80,	4164},
	{4096000,  213200000, 0, 160,	4164},
	{11289600, 213200000, 0, 441,	4164},
	{26000000, 213200000, 0, 10,	41},
	{2000000,  208000000, 0, 1,	52},
	{12288000, 208000000, 0, 192,	1625},
	{13000000, 208000000, 0, 1, 	8},
	{24576000, 208000000, 0, 384,	1625},
	{2048000,  208000000, 0, 32,	1625},
	{4096000,  208000000, 0, 64,	1625},
	{11289600, 208000000, 0, 420,	3869},
	{26000000, 208000000, 0, 1,	4},

	{0, 0, 0, 0, 0},
};

static struct clk_table clk_i2s_table[] = {
	{25600000, 312000000, 8, 1024, 6240},
	{19500000, 312000000, 1, 1,    1},
	{0, 0, 0, 0, 0},
};

static struct clk_table clk_uart_table[] = {
	{4000000 * 16, 312000000, 8, 32,   78},
	{3000000 * 16, 312000000, 8, 24,   78},
	{2000000 * 16, 312000000, 8, 16,   78},
	{1000000 * 16, 312000000, 8, 8,    78},
	{921600 * 16,  312000000, 8, 1536, 16250},
	{460800 * 16,  312000000, 8, 768,  16250},
	{230400 * 16,  312000000, 8, 384,  16250},
	{115200 * 16,  312000000, 8, 192,  16250},
	{57600 * 16,   312000000, 8, 96,   16250},
	{38400 * 16,   312000000, 8, 64,   16250},
	{0, 0, 0, 0, 0},
};

static unsigned int timer_mask;

static void clk_top_bus_init(struct clk *clk);
static int clk_top_bus_set_rate(struct clk *clk, unsigned long rate);
static void clk_memctl_phy_init(struct clk *clk);
static int clk_memctl_phy_enable(struct clk * clk);
static void clk_memctl_phy_disable(struct clk * clk);
static int clk_memctl_phy_set_rate(struct clk *clk, unsigned long rate);
static void clk_pllx_init(struct clk *clk);
#ifdef CONFIG_USB_COMIP_HSIC
static int clk_enable_usb(struct clk * clk);
static void clk_disable_usb(struct clk * clk);
#endif
static int clk_enable_generic(struct clk * clk);
static void clk_disable_generic(struct clk * clk);
static int clk_enable_nowe(struct clk * clk);
static void clk_disable_nowe(struct clk * clk);
static void clk_clkout_init(struct clk *clk);
static int clk_clkout_enable(struct clk *clk);
static void clk_clkout_disable(struct clk *clk);
static int clk_clkout_set_rate(struct clk *clk, unsigned long rate);
static int clk_clkout_set_parent(struct clk *clk, struct clk *parent);
static void clk_timer_init(struct clk *clk);
static int clk_timer_enable(struct clk *clk);
static void clk_timer_disable(struct clk *clk);
static int clk_timer_set_rate(struct clk *clk, unsigned long rate);
static int clk_timer_set_parent(struct clk *clk, struct clk *parent);
static void sdiv_phdiv_init(struct clk *clk);
static int sdiv_phdiv_set_rate(struct clk *clk, unsigned long rate);
static void g16plus0_init(struct clk *clk);
static void gdiv_init(struct clk *clk);
static int gdiv_set_rate(struct clk *clk, unsigned long rate);
static void sdiv_g16plus1_init(struct clk *clk);
static int sdiv_g16plus1_set_rate_noen(struct clk *clk, unsigned long rate);
static void g8plus1noen_init(struct clk *clk);
static void g8plus1_sdiv_init(struct clk *clk);
static int g8plus1_sdiv_set_rate(struct clk *clk, unsigned long rate);
static void g16plus1_sdiv_init(struct clk *clk);
static int g16plus1_sdiv_set_rate_noen(struct clk *clk, unsigned long rate);
static void sdiv_init(struct clk *clk);
static void sa7_init(struct clk * clk);
static int sdiv_set_rate(struct clk *clk, unsigned long rate);
static void fdiv_init_by_table(struct clk *clk);
static int fdiv_set_rate_by_table(struct clk *clk,unsigned long rate);
static int ha7_set_rate(struct clk * clk, unsigned long rate);
static int sa7_set_rate(struct clk * clk, unsigned long rate);
static int mets_clk_enable(struct clk * clk);
static void mets_clk_disable(struct clk * clk);


/* Base external input clocks. */
static struct clk clk_32k = {
	.name = "clk_32k",
	.rate = 32768,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
};

/* Typical 26MHz in standalone mode. */
static struct clk osc_clk = {
	.name = "osc_clk",
	.rate = 26000000,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
};

static struct clk ddr_pwr_pll_mclk = {
	.name = "ddr_pwr_pll_mclk",
	.parent = &osc_clk,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_PLLCFG),
	.init = &clk_pllx_init,
};

static struct clk com_pre_clk = {
	.name = "com_pre_clk",
	.parent = &ddr_pwr_pll_mclk,
	.flag = CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_COMPRECLKDIV),
	.divclk_bit = 0,
	.divclk_mask = 0xf,
	.init = &sdiv_init,
	.set_rate = &sdiv_set_rate,

};

static struct clk top_bus_clk = {
	.name = "top_bus_clk",
	.parent = &ddr_pwr_pll_mclk,
	.flag = CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_BUSCLKDIV),
	.init = &clk_top_bus_init,
	.set_rate = &clk_top_bus_set_rate,
};

static struct clk com_i2s_clk = {
	.name = "i2s_clk",
	.id = 0,
	.parent = &com_pre_clk,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN0),
	.mclk_bit = 2,
	.mclk_we_bit = 18,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_I2SCLKDIV),
	.divclk_bit = 0,
	.divclk_mask = 0x1f,
	.init = &sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_set_rate,
};

static struct clk com_uart_clk = {
	.name = "uart_clk",
	.id = 3,
	.parent = &com_pre_clk,
	.cust = &clk_com_uart_table,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN0),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 3,
	.ifclk_we_bit = 19,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_COMUARTCLKDIV),
	.init = &fdiv_init_by_table,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &fdiv_set_rate_by_table,
};

static struct clk com_pcm_clk = {
	.name = "pcm_clk",
	.parent = &com_pre_clk,
	.cust = &clk_com_pcm_table,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN0),
	.mclk_bit = 4,
	.mclk_we_bit = 20,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 4,
	.ifclk_we_bit = 20,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCMCLKDIV),
	.init = &fdiv_init_by_table,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &fdiv_set_rate_by_table,
};

static struct clk com_clkout0_clk = {
	.name = "clkout3_clk",
	.parent = &com_pre_clk,
	.cust = &clk_com_clkout_table,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN1),
	.mclk_bit = 1,
	.mclk_we_bit = 17,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKOUT0DIV),
	.init = &fdiv_init_by_table,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &fdiv_set_rate_by_table,
};

static struct clk com_clkout1_clk = {
	.name = "clkout4_clk",
	.parent = &com_pre_clk,
	.cust = &clk_com_clkout_table,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN1),
	.mclk_bit = 2,
	.mclk_we_bit = 18,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKOUT1DIV),
	.init = &fdiv_init_by_table,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &fdiv_set_rate_by_table,
	.flag = CLK_INIT_DISABLED,
};

static struct clk com_i2c_clk = {
	.name = "i2c_clk",
	.id = 4,
	.parent = &com_pre_clk,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN0),
	.mclk_bit = 3,
	.mclk_we_bit = 19,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 2,
	.ifclk_we_bit = 18,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_I2CCLKDIV),
	.divclk_bit = 0,
	.divclk_mask = 0x1f,
	.init = &sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_set_rate,
};

static struct clk memctl0_phy_clk = {
	.name = "memctl0_phy_clk",
	.parent = &ddr_pwr_pll_mclk,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN0),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN1),
	.ifclk_bit = 3,
	.ifclk_we_bit = 19,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_DDRCLKDIV_NOW),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.init = &clk_memctl_phy_init,
	.enable = &clk_memctl_phy_enable,
	.disable = &clk_memctl_phy_disable,
	.set_rate = &clk_memctl_phy_set_rate,
};

static struct clk memctl1_phy_clk = {
	.name = "memctl1_phy_clk",
	.parent = &ddr_pwr_pll_mclk,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN0),
	.mclk_bit = 1,
	.mclk_we_bit = 17,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN1),
	.ifclk_bit = 4,
	.ifclk_we_bit = 20,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_DDRCLKDIV_NOW),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.init = &clk_memctl_phy_init,
	.enable = &clk_memctl_phy_enable,
	.disable = &clk_memctl_phy_disable,
	.set_rate = &clk_memctl_phy_set_rate,
};

static struct clk a7_tsp_clk = {
	.name = "a7_tsp_clk",
	.parent = &ddr_pwr_pll_mclk,
	.flag = CLK_INIT_ENABLED,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_TSCLKCTL),
	.mclk_bit = 8,
	.mclk_we_bit = 8,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_TSCLKCTL),
	.divclk_bit = 0,
	.divclk_mask = 0xf,
	.enable = &clk_enable_nowe,
	.disable = &clk_disable_nowe,
};

static struct clk gpio_clk = {
	.name = "gpio_clk",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN0),
	.mclk_bit = 7,
	.mclk_we_bit = 23,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk mets_clk = {
	.name = "mets_clk",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_METERCLKCTL),
	.mclk_bit = 8,
	.mclk_we_bit = 8,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_METERCLKCTL),
	.divclk_bit = 0,
	.divclk_mask = 0x1f,
	.init = &sdiv_init,
	.enable = &mets_clk_enable,
	.disable = &mets_clk_disable,
	.set_rate = &sdiv_set_rate,
};

static struct clk *ddr_pwr_clks[] = {
	&clk_32k,
	&osc_clk,
	&ddr_pwr_pll_mclk,
	&com_pre_clk,
	&top_bus_clk,
	&com_i2s_clk,
#if !defined(CONFIG_MENTOR_DEBUG)
	&com_uart_clk,
#endif
	&com_pcm_clk,
	&com_clkout0_clk,
	&com_clkout1_clk,
	&com_i2c_clk,
	&memctl0_phy_clk,
	&memctl1_phy_clk,
	&a7_tsp_clk,
	&gpio_clk,
	&mets_clk,
};

/* AP_PWR. */
/* Base SoC clocks. */
static struct clk func_mclk = {
	.name	= "func_mclk",
	.parent = &osc_clk,
	.rate	= 26000000,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
};

static struct clk pll0_out = {
	.name = "pll0_out",
	.parent = &osc_clk,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_PLL0CFG_CTL0),
	.init = &clk_pllx_init,
};

static struct clk pll1_out = {
	.name = "pll1_out",
	.parent = &osc_clk,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_PLL1CFG_CTL0),
	.init = &clk_pllx_init,
};

static struct clk pll1_mclk = {
	.name = "pll1_mclk",
	.parent = &pll1_out,
	.fix_div = 4,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
};

static struct clk pll0_div13_clk = {
	.name = "pll0_div13_clk",
	.parent = &pll0_out,
	.fix_div = 13,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
};

static struct clk pll1_out_div13 = {
	.name = "pll1_out_div13",
	.parent = &pll1_out,
	.fix_div = 13,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
};


static struct clk bus_mclk0 = {
	.name = "bus_mclk0",
	.parent = &pll1_out,
	.flag = CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_BUSMCLK0_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.divclk_we_bit = 16,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_BUSMCLK0_CTL),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 20,
	.init = &sdiv_g16plus1_init,
};

static struct clk bus_mclk1 = {
	.name = "bus_mclk1",
	.parent = &bus_mclk0,
	.flag = CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_BUSMCLK1_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x7,
	.divclk_we_bit = 20,
	.init = &g8plus1noen_init,
};

static struct clk ctl_pclk = {
	.name = "ctl_pclk",
	.parent = &bus_mclk1,
	.fix_div = 2,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_CTL),
};

static struct clk data_pclk = {
	.name = "data_pclk",
	.parent = &bus_mclk1,
	.fix_div = 2,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_CTL),
};

static struct clk sec_pclk = {
	.name = "sec_pclk",
	.parent = &bus_mclk1,
	.fix_div = 2,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_CTL),
};

/* Application Unit. */
static struct clk ha7_clk = {
	.name = "a7_clk",
	.parent = &pll0_out,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_HA7_CLK_CTL),
	.grclk_bit = 0,
	.grclk_mask = 0x1f,
	.grclk_we_bit = 16,
	.init = &g16plus0_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &ha7_set_rate,
};

static struct clk ha7_pclkdbg_clk = {
	.name = "a7_pclkdbg_clk",
	.parent = &ha7_clk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_HA7_CLK_CTL),
	.grclk_bit = 8,
	.grclk_mask = 0x7,
	.grclk_we_bit = 24,
	.init = &gdiv_init,
	.set_rate = &gdiv_set_rate,
};

static struct clk ha7_atclk_clk = {
	.name = "a7_atclk_clk",
	.parent = &ha7_clk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_HA7_CLK_CTL),
	.grclk_bit = 12,
	.grclk_mask = 0x7,
	.grclk_we_bit = 28,
	.init = &gdiv_init,
	.set_rate = &gdiv_set_rate,
};

static struct clk sa7_clk = {
	.name = "sa7_clk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SA7CLK_CTL0),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SA7CLK_CTL0),
	.divclk_bit = 4,
	.divclk_mask = 0x7,
	.divclk_we_bit = 20,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SA7CLK_CTL0),
	.grclk_bit = 8,
	.grclk_mask = 0xf,
	.grclk_we_bit = 24,
	.init = &sa7_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sa7_set_rate,
};

static struct clk sa7_pclkdbg_clk = {
	.name = "sa7_pclkdbg_clk",
	.parent = &sa7_clk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SA7CLK_CTL1),
	.grclk_bit = 0,
	.grclk_mask = 0x7,
	.grclk_we_bit = 16,
	.init = &gdiv_init,
	.set_rate = &gdiv_set_rate,
};

static struct clk sa7_atclk_clk = {
	.name = "sa7_atclk_clk",
	.parent = &sa7_clk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SA7CLK_CTL1),
	.grclk_bit = 4,
	.grclk_mask = 0x7,
	.grclk_we_bit = 20,
	.init = &gdiv_init,
	.set_rate = &gdiv_set_rate,
};

static struct clk gpu_mclk = {
	.name = "gpu_mclk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_GPUCLK_CTL),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_GPUCLK_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.divclk_we_bit = 16,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_GPUCLK_CTL),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 20,
	.init = &sdiv_g16plus1_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_g16plus1_set_rate_noen,

};

static struct clk peri_sw0_gpu_clk = {
	.name = "peri_sw0_gpu_clk",
	.parent = &gpu_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN4),
	.mclk_bit = 10,
	.mclk_we_bit = 26,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk gpu_adb_aclkm = {
	.name = "gpu_adb_aclkm",
	.parent = &gpu_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN4),
	.mclk_bit = 12,
	.mclk_we_bit = 28,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk on2_enc_mclk = {
	.name = "on2_enc_mclk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL0),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL0),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.divclk_we_bit = 16,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL0),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 20,
	.init = &sdiv_g16plus1_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_g16plus1_set_rate_noen,
};

static struct clk on2_codec_mclk = {
	.name = "on2_codec_mclk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL1),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL1),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.divclk_we_bit = 16,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL1),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 20,
	.init = &sdiv_g16plus1_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_g16plus1_set_rate_noen,
};

static struct clk ap_sw1_on2enc_clk = {
	.name = "ap_sw1_on2enc_clk",
	.parent = &on2_enc_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN4),
	.mclk_bit = 9,
	.mclk_we_bit = 25,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk ap_peri_sw1_on2enc_clk = {
	.name = "ap_peri_sw1_on2enc_clk",
	.parent = &on2_enc_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN4),
	.mclk_bit = 7,
	.mclk_we_bit = 23,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk ap_sw1_on2codec_clk = {
	.name = "ap_sw1_on2codec_clk",
	.parent = &on2_codec_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN4),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk ap_peri_sw1_on2codec_clk = {
	.name = "ap_peri_sw1_on2codec_clk",
	.parent = &on2_codec_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN4),
	.mclk_bit = 6,
	.mclk_we_bit = 22,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk nfc_clk = {
	.name = "nfc_clk",
	.parent = &pll1_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_NFCCLK_CTL),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_NFCCLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0xf,
	.divclk_we_bit = 20,
	.init = &sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_set_rate,
	.flag = CLK_INIT_DISABLED,
};

static struct clk hpi_clk = {
	.name = "hpi_clk",
	.parent = &pll1_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_HPICLK_CTL),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.ifclk_bit = 4,
	.ifclk_we_bit = 20,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_HPICLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0xf,
	.divclk_we_bit = 20,
	.init = &sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_set_rate,
	.flag = CLK_INIT_DISABLED,
};

static struct clk usbotg_12m_clk = {
	.name = "usbotg_12m_clk",
	.parent = &pll1_out_div13,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLKDIV_CTL),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLKDIV_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x7,
	.divclk_we_bit = 20,
	.busclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN2),
	.busclk_bit = 7,
	.busclk_we_bit = 23,
	.init = &sdiv_init,
	#ifdef CONFIG_USB_COMIP_HSIC
	.enable = &clk_enable_usb,
	.disable = &clk_disable_usb,
	#else
	.enable = &clk_enable_generic,
        .disable = &clk_disable_generic,
	#endif
	.set_rate = &sdiv_set_rate,
};
#ifdef CONFIG_USB_COMIP_HSIC
static struct clk usbhsic_12m_clk = {
	.name = "usbhsic_12m_clk",
	.parent = &pll1_out_div13,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLKDIV_CTL),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.ifclk_bit = 6,
	.ifclk_we_bit = 22,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLKDIV_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x7,
	.divclk_we_bit = 20,
	.busclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN2),
	.busclk_bit = 6,
	.busclk_we_bit = 22,
	.init = &sdiv_init,
	.enable = &clk_enable_usb,
	.disable = &clk_disable_usb,
	.set_rate = &sdiv_set_rate,
};
#endif


static struct clk acc2d_aclk = {
	.name = "acc2d_aclk",
	.parent = &bus_mclk0,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.mclk_bit = 15,
	.mclk_we_bit = 31,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk acc2d_hclk = {
	.name = "acc2d_hclk",
	.parent = &bus_mclk0,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.mclk_bit = 14,
	.mclk_we_bit = 30,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk peri_sw1_acc2d_clk = {
	.name = "peri_sw1_acc2d_clk",
	.parent = &bus_mclk0,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN3),
	.mclk_bit = 1,
	.mclk_we_bit = 17,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk sw1_acc2d_clk = {
	.name = "sw1_acc2d_clk",
	.parent = &bus_mclk0,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN3),
	.mclk_bit = 12,
	.mclk_we_bit = 28,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk smmu0_clk = {
	.name = "smmu0_clk",
	.parent = &bus_mclk0,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN5),
	.mclk_bit = 12,
	.mclk_we_bit = 28,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk smmu1_clk = {
	.name = "smmu1_clk",
	.parent = &bus_mclk0,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN5),
	.mclk_bit = 13,
	.mclk_we_bit = 29,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk acc2d_mclk = {
	.name = "acc2d_mclk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ACC2DMCLK_CTL),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ACC2DMCLK_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.divclk_we_bit = 16,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_ACC2DMCLK_CTL),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 20,
	.init = &sdiv_g16plus1_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_g16plus1_set_rate_noen,
};

static struct clk lcdc_axi_clk = {
	.name = "lcdc_axi_clk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDCAXICLK_CTL),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDCAXICLK_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.divclk_we_bit = 16,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDCAXICLK_CTL),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 20,
	.init = &sdiv_g16plus1_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_g16plus1_set_rate_noen,
};

static struct clk lcdc0_clk = {
	.name = "lcdc0_clk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDC0CLK_CTL),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDC0CLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0xf,
	.divclk_we_bit = 20,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDC0CLK_CTL),
	.grclk_bit = 0,
	.grclk_mask = 0x7,
	.grclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN5),
	.ifclk_bit = 14,
	.ifclk_we_bit = 30,
	.init = &g8plus1_sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &g8plus1_sdiv_set_rate,
};

static struct clk lcdc1_clk = {
	.name = "lcdc1_clk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDC1CLK_CTL),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDC1CLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0xf,
	.divclk_we_bit = 20,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDC1CLK_CTL),
	.grclk_bit = 0,
	.grclk_mask = 0x7,
	.grclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN5),
	.ifclk_bit = 14,
	.ifclk_we_bit = 30,
	.init = &g8plus1_sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &g8plus1_sdiv_set_rate,
	.flag = CLK_INIT_DISABLED,
};

static struct clk dsi_cfg_clk = {
	.name = "dsi_cfg_clk",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_DISCLK_CTL_EN),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 9,
	.ifclk_we_bit = 25,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk dsi_ref_clk = {
	.name = "dsi_ref_clk",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_DISCLK_CTL_EN),
	.mclk_bit = 1,
	.mclk_we_bit = 17,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 9,
	.ifclk_we_bit = 25,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk isp_cphy_cfg_clk = {
	.name = "isp_cphy_cfg_clk",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL0),
	.mclk_bit = 9,
	.mclk_we_bit = 25,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk isp_axi_clk = {
	.name = "isp_axi_clk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL0),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL0),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.divclk_we_bit = 16,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL0),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 20,
	.init = &g16plus1_sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &g16plus1_sdiv_set_rate_noen,
};

static struct clk isp_p_sclk = {
	.name = "isp_p_sclk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL1),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL1),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.divclk_we_bit = 16,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL1),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 20,
	.init = &g16plus1_sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &g16plus1_sdiv_set_rate_noen,
};

static struct clk isp_sclk2 = {
	.name = "isp_sclk2",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL1),
	.mclk_bit = 9,
	.mclk_we_bit = 25,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL1),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.divclk_we_bit = 16,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL1),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 20,
	.init = &g16plus1_sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &g16plus1_sdiv_set_rate_noen,

};

static struct clk isp_hclk = {
	.name = "isp_hclk",
	.parent = &bus_mclk0,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN4),
	.mclk_bit = 4,
	.mclk_we_bit = 20,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

/* CTL_APB */
static struct clk i2c0_clk = {
	.name = "i2c_clk",
	.id = 0,
	.parent = &pll1_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLAPBMCLK_EN),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2CCLK_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0x1f,
	.init = &sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_set_rate,
	.flag = CLK_INIT_DISABLED,
};

static struct clk i2c1_clk = {
	.name = "i2c_clk",
	.id = 1,
	.parent = &pll1_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLAPBMCLK_EN),
	.mclk_bit = 1,
	.mclk_we_bit = 17,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 6,
	.ifclk_we_bit = 22,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2CCLK_CTL),
	.divclk_bit = 8,
	.divclk_mask = 0x1f,
	.init = &sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_set_rate,
};

static struct clk i2c2_clk = {
	.name = "i2c_clk",
	.id = 2,
	.parent = &pll1_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLAPBMCLK_EN),
	.mclk_bit = 2,
	.mclk_we_bit = 18,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2CCLK_CTL),
	.divclk_bit = 16,
	.divclk_mask = 0x1f,
	.init = &sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_set_rate,
};

static struct clk i2c3_clk = {
	.name = "i2c_clk",
	.id = 3,
	.parent = &pll1_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLAPBMCLK_EN),
	.mclk_bit = 3,
	.mclk_we_bit = 19,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 8,
	.ifclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2CCLK_CTL),
	.divclk_bit = 24,
	.divclk_mask = 0x1f,
	.init = &sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_set_rate,
};

static struct clk pwm_clk = {
	.name = "pwm_clk",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLAPBMCLK_EN),
	.mclk_bit = 3,
	.mclk_we_bit = 19,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 8,
	.ifclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_PWMCLKDIV_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0xff,
	.init = &sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_set_rate,
};

static struct clk timer0_clk = {
	.name = "timer0_clk",
	.id = 0,
	.parent = &pll0_div13_clk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER0CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 4,
	.init = &clk_timer_init,
	.enable = &clk_timer_enable,
	.disable = &clk_timer_disable,
	.set_rate = &clk_timer_set_rate,
	.set_parent = &clk_timer_set_parent,
};

static struct clk timer1_clk = {
	.name = "timer1_clk",
	.id = 1,
	.parent = &pll0_div13_clk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER1CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 4,
	.init = &clk_timer_init,
	.enable = &clk_timer_enable,
	.disable = &clk_timer_disable,
	.set_rate = &clk_timer_set_rate,
	.set_parent = &clk_timer_set_parent,
};

static struct clk timer2_clk = {
	.name = "timer2_clk",
	.id = 2,
	.parent = &pll0_div13_clk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER2CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 4,
	.init = &clk_timer_init,
	.enable = &clk_timer_enable,
	.disable = &clk_timer_disable,
	.set_rate = &clk_timer_set_rate,
	.set_parent = &clk_timer_set_parent,
};

static struct clk timer3_clk = {
	.name = "timer3_clk",
	.id = 3,
	.parent = &pll0_div13_clk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER3CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 4,
	.init = &clk_timer_init,
	.enable = &clk_timer_enable,
	.disable = &clk_timer_disable,
	.set_rate = &clk_timer_set_rate,
	.set_parent = &clk_timer_set_parent,
};

static struct clk timer4_clk = {
	.name = "timer4_clk",
	.id = 4,
	.parent = &pll0_div13_clk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER4CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 4,
	.init = &clk_timer_init,
	.enable = &clk_timer_enable,
	.disable = &clk_timer_disable,
	.set_rate = &clk_timer_set_rate,
	.set_parent = &clk_timer_set_parent,
};

static struct clk timer5_clk = {
	.name = "timer5_clk",
	.id = 5,
	.parent = &pll0_div13_clk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER5CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 4,
	.init = &clk_timer_init,
	.enable = &clk_timer_enable,
	.disable = &clk_timer_disable,
	.set_rate = &clk_timer_set_rate,
	.set_parent = &clk_timer_set_parent,
};

static struct clk timer6_clk = {
	.name = "timer6_clk",
	.id = 6,
	.parent = &pll0_div13_clk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER6CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 4,
	.init = &clk_timer_init,
	.enable = &clk_timer_enable,
	.disable = &clk_timer_disable,
	.set_rate = &clk_timer_set_rate,
	.set_parent = &clk_timer_set_parent,
};

static struct clk timer7_clk = {
	.name = "timer7_clk",
	.id = 7,
	.parent = &pll0_div13_clk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER7CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 4,
	.init = &clk_timer_init,
	.enable = &clk_timer_enable,
	.disable = &clk_timer_disable,
	.set_rate = &clk_timer_set_rate,
	.set_parent = &clk_timer_set_parent,
};

static struct clk wdt0_clk = {
	.name = "wdt_clk",
	.id = 0,
	.parent = &ctl_pclk,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 0,
	.ifclk_we_bit = 16,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk wdt1_clk = {
	.name = "wdt_clk",
	.id = 1,
	.parent = &ctl_pclk,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk wdt2_clk = {
	.name = "wdt_clk",
	.id = 2,
	.parent = &ctl_pclk,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 2,
	.ifclk_we_bit = 18,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk wdt3_clk = {
	.name = "wdt_clk",
	.id = 3,
	.parent = &ctl_pclk,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 3,
	.ifclk_we_bit = 19,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

/* DATA_APB */
static struct clk i2s0_clk = {
	.name = "i2s_clk",
	.id = 1,
	.parent = &pll1_mclk,
	.cust = &clk_i2s_table,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_I2SCLK_CTL0),
	.mclk_bit = 4,
	.mclk_we_bit = 20,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_I2SCLK_CTL0),
	.grclk_bit = 0,
	.grclk_mask = 0x7,
	.grclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 6,
	.ifclk_we_bit = 22,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2SCLK_CTL1),
	.init = &fdiv_init_by_table,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &fdiv_set_rate_by_table,
	.flag = CLK_INIT_DISABLED,
};

static struct clk ssi0_clk = {
	.name = "ssi_clk",
	.id = 0,
	.parent = &pll1_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLK_CTL0),
	.mclk_bit = 12,
	.mclk_we_bit = 28,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLK_CTL0),
	.grclk_bit = 0,
	.grclk_mask = 0x7,
	.grclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 0,
	.ifclk_we_bit = 16,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKDIV_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0xf,
	.divclk_we_bit = 16,
	.init = &g8plus1_sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &g8plus1_sdiv_set_rate,
	.flag = CLK_INIT_DISABLED,
};

static struct clk ssi1_clk = {
	.name = "ssi_clk",
	.id = 1,
	.parent = &pll1_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLK_CTL0),
	.mclk_bit = 13,
	.mclk_we_bit = 29,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLK_CTL0),
	.grclk_bit = 4,
	.grclk_mask = 0x7,
	.grclk_we_bit = 20,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKDIV_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0xf,
	.divclk_we_bit = 20,
	.init = &g8plus1_sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &g8plus1_sdiv_set_rate,
	.flag = CLK_INIT_DISABLED,
};

static struct clk ssi2_clk = {
	.name = "ssi_clk",
	.id = 2,
	.parent = &pll1_mclk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLK_CTL0),
	.mclk_bit = 14,
	.mclk_we_bit = 30,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLK_CTL0),
	.grclk_bit = 8,
	.grclk_mask = 0x7,
	.grclk_we_bit = 24,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_EN),
	.ifclk_bit = 2,
	.ifclk_we_bit = 18,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKDIV_CTL),
	.divclk_bit = 8,
	.divclk_mask = 0xf,
	.divclk_we_bit = 24,
	.init = &g8plus1_sdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &g8plus1_sdiv_set_rate,
	.flag = CLK_INIT_DISABLED,
};

static struct clk uart0_clk = {
	.name = "uart_clk",
	.id = 0,
	.parent = &pll1_mclk,
	.cust = &clk_uart_table,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_UARTCLK_CTL0),
	.mclk_bit = 12,
	.mclk_we_bit = 28,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_UARTCLK_CTL0),
	.grclk_bit = 0,
	.grclk_mask = 0x7,
	.grclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 3,
	.ifclk_we_bit = 19,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_UART0CLK_CTL1),
	.init = &fdiv_init_by_table,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &fdiv_set_rate_by_table,

};

static struct clk uart1_clk = {
	.name = "uart_clk",
	.id = 1,
	.parent = &pll1_mclk,
	.cust = &clk_uart_table,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_UARTCLK_CTL0),
	.mclk_bit = 13,
	.mclk_we_bit = 29,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_UARTCLK_CTL0),
	.grclk_bit = 4,
	.grclk_mask = 0x7,
	.grclk_we_bit = 20,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 4,
	.ifclk_we_bit = 20,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_UART1CLK_CTL1),
	.init = &fdiv_init_by_table,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &fdiv_set_rate_by_table,
	.flag = CLK_INIT_DISABLED,
};

static struct clk uart2_clk = {
	.name = "uart_clk",
	.id = 2,
	.parent = &pll1_mclk,
	.cust = &clk_uart_table,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_UARTCLK_CTL0),
	.mclk_bit = 14,
	.mclk_we_bit = 30,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_UARTCLK_CTL0),
	.grclk_bit = 8,
	.grclk_mask = 0x7,
	.grclk_we_bit = 24,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_UART2CLK_CTL1),
	.init = &fdiv_init_by_table,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &fdiv_set_rate_by_table,
	.flag = CLK_INIT_DISABLED,
};

/* SEC_APB */
static struct clk kbs_clk = {
	.name = "kbs_clk",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SECAPBMCLK_EN),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_CTL),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk tpz_clk = {
	.name = "tpz_clk",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SECAPBMCLK_EN),
	.mclk_bit = 2,
	.mclk_we_bit = 18,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_CTL),
	.ifclk_bit = 4,
	.ifclk_we_bit = 20,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk sdmmc0_clk = {
	.name = "sdmmc_clk",
	.id = 0,
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLK_CTL0),
	.mclk_bit = 12,
	.mclk_we_bit = 28,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLK_CTL0),
	.grclk_bit = 0,
	.grclk_mask = 0x7,
	.grclk_we_bit  = 16,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMC0CLK_CTL1),
	.divclk_bit = 8,
	.divclk_mask = 0x7,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.ifclk_bit = 2,
	.ifclk_we_bit = 18,
	.busclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN2),
	.busclk_bit = 10,
	.busclk_we_bit = 26,
	.init = &sdiv_phdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_phdiv_set_rate,
};

static struct clk sdmmc1_clk = {
	.name = "sdmmc_clk",
	.id = 1,
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLK_CTL0),
	.mclk_bit = 13,
	.mclk_we_bit = 29,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLK_CTL0),
	.grclk_bit = 4,
	.grclk_mask = 0x7,
	.grclk_we_bit  = 20,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMC1CLK_CTL1),
	.divclk_bit = 8,
	.divclk_mask = 0x7,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17,
	.busclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN2),
	.busclk_bit = 4,
	.busclk_we_bit = 20,
	.init = &sdiv_phdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_phdiv_set_rate,
};

static struct clk sdmmc2_clk = {
	.name = "sdmmc_clk",
	.id = 2,
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLK_CTL0),
	.mclk_bit = 14,
	.mclk_we_bit = 30,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLK_CTL0),
	.grclk_bit = 8,
	.grclk_mask = 0x7,
	.grclk_we_bit  = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMC2CLK_CTL1),
	.divclk_bit = 8,
	.divclk_mask = 0x7,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.ifclk_bit = 0,
	.ifclk_we_bit = 16,
	.busclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN2),
	.busclk_bit = 9,
	.busclk_we_bit = 25,
	.init = &sdiv_phdiv_init,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.set_rate = &sdiv_phdiv_set_rate,
};

static struct clk clkout0_clk = {
	.name = "clkout0_clk",
	.parent = &pll0_div13_clk,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUT1CTL),
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUTSEL),
	.ifclk_bit = 0,
	.init = &clk_clkout_init,
	.enable = &clk_clkout_enable,
	.disable = &clk_clkout_disable,
	.set_rate = &clk_clkout_set_rate,
	.set_parent = &clk_clkout_set_parent,
};

static struct clk clkout1_clk = {
	.name = "clkout1_clk",
	.parent = &pll0_div13_clk,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUT2CTL),
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUTSEL),
	.ifclk_bit = 4,
	.init = &clk_clkout_init,
	.enable = &clk_clkout_enable,
	.disable = &clk_clkout_disable,
	.set_rate = &clk_clkout_set_rate,
	.set_parent = &clk_clkout_set_parent,
};

static struct clk clkout2_clk = {
	.name = "clkout2_clk",
	.parent = &pll0_div13_clk,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUT3CTL),
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUTSEL),
	.ifclk_bit = 8,
	.init = &clk_clkout_init,
	.enable = &clk_clkout_enable,
	.disable = &clk_clkout_disable,
	.set_rate = &clk_clkout_set_rate,
	.set_parent = &clk_clkout_set_parent,
};

static struct clk cp_clk32k = {
	.name = "cp_clk32k",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_32KCLK_EN),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk security_clk32k = {
	.name = "security_clk32k",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_32KCLK_EN),
	.mclk_bit = 1,
	.mclk_we_bit = 17,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.ifclk_bit = 3,
	.ifclk_we_bit = 20,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.flag = CLK_INIT_DISABLED,
};

static struct clk ap_tm32k_clk32k = {
	.name = "ap_tm32k_clk32k",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_32KCLK_EN),
	.mclk_bit = 2,
	.mclk_we_bit = 18,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
};

static struct clk cipher_clk = {
	.name = "cipher_clk",
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN0),
	.ifclk_bit = 8,
	.ifclk_we_bit = 24,
	.busclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN1),
	.busclk_bit = 15,
	.busclk_we_bit = 31,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.flag = CLK_INIT_DISABLED,
};

static struct clk hsic_clk = {
	.name = "hsic_clk",
	.busclk_reg = (void __iomem *)io_p2v(AP_PWR_CLK_EN2),
	.busclk_bit = 6,
	.busclk_we_bit = 22,
	.enable = &clk_enable_generic,
	.disable = &clk_disable_generic,
	.flag = CLK_INIT_DISABLED,
};

static struct clk *ap_pwr_clks[] = {
	&func_mclk,
	&pll0_out,
#if !defined(CONFIG_MENTOR_DEBUG)
	&pll1_out,
#endif
	&pll1_mclk,
	&pll1_out_div13,
	&pll0_div13_clk,
	&bus_mclk0,
	&bus_mclk1,
	&ctl_pclk,
	&data_pclk,
	&sec_pclk,
	&ha7_clk,
	&ha7_pclkdbg_clk,
	&ha7_atclk_clk,
	&sa7_clk,
	&sa7_pclkdbg_clk,
	&sa7_atclk_clk,
	&gpu_mclk,
	&peri_sw0_gpu_clk,
	&gpu_adb_aclkm,
	&on2_enc_mclk,
	&on2_codec_mclk,
	&ap_sw1_on2enc_clk,
	&ap_peri_sw1_on2enc_clk,
	&ap_sw1_on2codec_clk,
	&ap_peri_sw1_on2codec_clk,
	&nfc_clk,
	&hpi_clk,
	&usbotg_12m_clk,
	#ifdef CONFIG_USB_COMIP_HSIC
	&usbhsic_12m_clk,
	#endif
	&smmu0_clk,
	&smmu1_clk,
	&acc2d_mclk,
	&acc2d_aclk,
	&acc2d_hclk,
	&peri_sw1_acc2d_clk,
	&sw1_acc2d_clk,
	&lcdc_axi_clk,
	&lcdc0_clk,
	&lcdc1_clk,
	&dsi_cfg_clk,
	&dsi_ref_clk,
	&isp_cphy_cfg_clk,
	&isp_axi_clk,
	&isp_p_sclk,
	&isp_sclk2,
	&isp_hclk,
	&i2c0_clk,
	&i2c1_clk,
	&i2c2_clk,
	&i2c3_clk,
	&pwm_clk,
	&timer0_clk,
	&timer1_clk,
	&timer2_clk,
	&timer3_clk,
	&timer4_clk,
	&timer5_clk,
	&timer6_clk,
	&timer7_clk,
	&wdt0_clk,
	&wdt1_clk,
	&wdt2_clk,
	&wdt3_clk,
	&i2s0_clk,
	&ssi0_clk,
	&ssi1_clk,
	&ssi2_clk,
	&uart0_clk,
	&uart1_clk,
	&uart2_clk,
	&kbs_clk,
	&tpz_clk,
	&sdmmc0_clk,
	&sdmmc1_clk,
	&sdmmc2_clk,
	&clkout0_clk,
	&clkout1_clk,
	&clkout2_clk,
	&cp_clk32k,
	&security_clk32k,
	&ap_tm32k_clk32k,
	&cipher_clk,
	&hsic_clk,
};

#endif /*__ARCH_ARM_COMIP_CLOCK_H*/
