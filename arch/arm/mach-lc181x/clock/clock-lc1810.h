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

static struct clk_range clk_range1_1_8 = {1, 1, 8};
static struct clk_range clk_range1_1_16 = {1, 1, 16};
static struct clk_range clk_range1_2_16 = {1, 2, 16};
static struct clk_range clk_range1_2_32 = {1, 2, 32};
static struct clk_range clk_range1_2_256 = {1, 2, 256};
static struct clk_range clk_range3_3_6 = {3, 3, 6};

static struct clk_table clk_com_uart_table[] = {
	{4000000 * 16, 260000000, 0, 32,   65},
	{2000000 * 16, 260000000, 0, 16,   65},
	{1000000 * 16, 260000000, 0, 8,    65},
	{921600 * 16,  260000000, 0, 4608, 40625},
	{460800 * 16,  260000000, 0, 2304, 40625},
	{230400 * 16,  260000000, 0, 1152, 40625},
	{115200 * 16,  260000000, 0, 576,  40625},
	{57600 * 16,   260000000, 0, 288,  40625},
	{38400 * 16,   260000000, 0, 192,  40625},

	{115200 * 16,  234000000, 0, 384,  24375},
	{921600 * 16,  234000000, 0, 3072, 24375},

	{4000000 * 16, 208000000, 0, 8, 13},
	{2000000 * 16, 208000000, 0, 4, 13},
	{1000000 * 16, 208000000, 0, 2, 13},
	{921600 * 16,  208000000, 0, 1152, 8125},
	{460800 * 16,  208000000, 0, 576, 8125},
	{230400 * 16,  208000000, 0, 288, 8125},
	{115200 * 16,  208000000, 0, 144, 8125},
	{57600 * 16,   208000000, 0, 72,  8125},
	{38400 * 16,   208000000, 0, 48,  8125},
	{0, 0, 0, 0, 0},
};

static struct clk_table clk_com_pcm1_table[] = {
	{64000, 260000000, 0, 20, 40625},
	{64000, 234000000, 0, 24, 43875},
	{64000, 208000000, 0, 2, 3250},
	{0, 0, 0, 0, 0},
};

static struct clk_table clk_com_clkout_table[] = {
	{2000000,  260000000, 0, 2,    130},
	{12288000, 260000000, 0, 3840, 40625},
	{13000000, 260000000, 0, 2,    20},
	{24576000, 260000000, 0, 7680, 40625},
	{2048000,  260000000, 0, 640, 40625},
	{11289600, 260000000, 0, 3528, 40625},
	{26000000, 260000000, 0, 2,    10},

	{2000000,  234000000, 0,    2,   117},
	{2048000,  234000000, 0,  256, 14625},
	{24576000, 234000000, 0, 8189, 39000},
	{11059200, 234000000, 0, 3840, 40625},
	{11289600, 234000000, 0,  784,  8125},
	{13000000, 234000000, 0,    1,     9},
	{12288000, 234000000, 0, 1536, 14625},
	{26000000, 234000000, 0,   26,   117},

	{2000000,  208000000, 0, 2,    104},
	{12288000, 208000000, 0, 192,  1625},
	{13000000, 208000000, 0, 13,	104},
	{24576000, 208000000, 0, 384,  1625},
	{2048000,  208000000, 0, 32,   1625},
	{11289600, 208000000, 0, 882, 8125},
	{26000000, 208000000, 0, 2,   8},
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

static int comip_clk_enable_generic(struct clk * clk);
static void comip_clk_disable_generic(struct clk * clk);
static void comip_clk_init_generic(struct clk *clk);
static int comip_clk_set_rate_generic(struct clk *clk, unsigned long rate);
static void comip_clk_init_by_table(struct clk *clk);
static int comip_clk_set_rate_by_table(struct clk *clk,unsigned long rate);
static void comip_clk_pll_init(struct clk *clk);
static void comip_clk_a9_init(struct clk *clk);
static int comip_clk_a9_set_rate(struct clk *clk, unsigned long rate);
static void comip_clk_a9_peri_init(struct clk *clk);
static int comip_clk_a9_peri_set_rate(struct clk *clk, unsigned long rate);
static void comip_clk_a9_acp_init(struct clk *clk);
static int comip_clk_a9_acp_set_rate(struct clk *clk, unsigned long rate);
static void comip_clk_clkout_init(struct clk *clk);
static int comip_clk_clkout_enable(struct clk *clk);
static void comip_clk_clkout_disable(struct clk *clk);
static int comip_clk_clkout_set_rate(struct clk *clk, unsigned long rate);
static int comip_clk_clkout_set_parent(struct clk *clk, struct clk *parent);
static void comip_clk_timer_init(struct clk *clk);
static int comip_clk_timer_enable(struct clk *clk);
static void comip_clk_timer_disable(struct clk *clk);
static int comip_clk_timer_set_rate(struct clk *clk, unsigned long rate);
static int comip_clk_timer_set_parent(struct clk *clk, struct clk *parent);
static void comip_clk_usb_init(struct clk *clk);
static int comip_clk_usb_set_rate(struct clk *clk, unsigned long rate);
static void comip_clk_ssi_init(struct clk *clk);
static int comip_clk_ssi_set_rate(struct clk *clk, unsigned long rate);
static void comip_clk_sdmmc_init(struct clk *clk);
static int comip_clk_sdmmc_set_rate(struct clk *clk, unsigned long rate);
static int comip_clk_lcd_set_rate(struct clk *clk, unsigned long rate);
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

/*Clock Source. */
static struct clk ddr_pwr_pll_mclk = {
	.name = "ddr_pwr_pll_mclk",
	.parent = &osc_clk,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_PLLCTL),
	.init = comip_clk_pll_init,
};

/*Secondary Div Clock Source*/
static struct clk fraction_in_clk = {
	.name = "fraction_in_clk",
	.parent = &ddr_pwr_pll_mclk,
	.cust = &clk_range1_1_8,
	.flag = CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_DIVCLKCTL),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.init = &comip_clk_init_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk com_i2c_ssi_clk = {
	.name = "i2c_ssi_clk",
	.parent = &ddr_pwr_pll_mclk,
	.cust = &clk_range1_1_8,
	.flag = CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_DIVCLKCTL),
	.divclk_bit = 8,
	.divclk_mask = 0x7,
	.init = &comip_clk_init_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk com_apb_pclk_work = {
	.name = "com_apb_pclk_work",
	.parent = &ddr_pwr_pll_mclk,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_APBCLKCTL),
};

static struct clk com_apb_pclk_idle = {
	.name = "com_apb_pclk_idle",
	.parent = &ddr_pwr_pll_mclk,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_APBCLKCTL),
};

/* Application Unit */
static struct clk com_uart_clk = {
	.name = "uart_clk",
	.id = 3,
	.parent = &fraction_in_clk,
	.cust = &clk_com_uart_table,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN),
	.mclk_bit = 9,
	.mclk_we_bit = 25,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_COMUARTCLKCTL),
	.init = &comip_clk_init_by_table,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_by_table,
};

static struct clk com_pcm1_clk = {
	.name = "pcm1_clk",
	.parent = &fraction_in_clk,
	.cust = &clk_com_pcm1_table,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN),
	.mclk_bit = 3,
	.mclk_we_bit = 19,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 8,
	.ifclk_we_bit = 24, 
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCM1CLKCTL),
	.init = &comip_clk_init_by_table,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_by_table,
};

static struct clk com_clkout_clk = {
	.name = "clkout3_clk",
	.parent = &fraction_in_clk,
	.cust = &clk_com_clkout_table,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN),
	.mclk_bit = 8,
	.mclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKOUTCTL),
	.init = &comip_clk_init_by_table,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_by_table,
};

static struct clk com_i2c_clk = {
	.name = "i2c_clk",
	.id = 4,
	.parent = &com_i2c_ssi_clk,
	.cust = &clk_range1_1_16,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN),
	.mclk_bit = 2,
	.mclk_we_bit = 18,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 4,
	.ifclk_we_bit = 20,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_I2CCLKCTL),
	.divclk_bit = 0,
	.divclk_mask = 0xf,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk phy_ddr_clk = {
	.name = "phy_ddr_clk",
	.parent = &ddr_pwr_pll_mclk,
	.cust = &clk_range1_1_8,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 0,
	.ifclk_we_bit = 16,
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_DDRCLKCTL),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk ctrl_ddr_clk = {
	.name = "ctrl_ddr_clk",
	.parent = &ddr_pwr_pll_mclk,
	.cust = &clk_range1_1_8,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17, 
	.divclk_reg = (void __iomem *)io_p2v(DDR_PWR_DDRCLKCTL),
	.divclk_bit = 0,
	.divclk_mask = 0x7,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk mfp_clk = {
	.name = "mfp_clk",
	.parent = &clk_32k,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 2,
	.ifclk_we_bit = 18, 
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk gpio0_clk = {
	.name = "gpio0_clk",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN),
	.mclk_bit = 7,
	.mclk_we_bit = 23,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 6,
	.ifclk_we_bit = 22,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk gpio1_clk = {
	.name = "gpio1_clk",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(DDR_PWR_CLKEN),
	.mclk_bit = 6,
	.mclk_we_bit = 22,
	.ifclk_reg = (void __iomem *)io_p2v(DDR_PWR_PCLKEN),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk *ddr_pwr_clks[] = {
	&clk_32k,
	&osc_clk,
	&ddr_pwr_pll_mclk,
	&fraction_in_clk,
	&com_i2c_ssi_clk,
	&com_apb_pclk_work,
	&com_apb_pclk_idle,
#if !defined(CONFIG_MENTOR_DEBUG)
	&com_uart_clk,
#endif
	&com_pcm1_clk,
	&com_clkout_clk,
	&com_i2c_clk,
	&phy_ddr_clk,
	&ctrl_ddr_clk,
	&mfp_clk,
	&gpio0_clk,
	&gpio1_clk,
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
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_PLL0CFG_CTL),
	.init = &comip_clk_pll_init,
};

static struct clk pll1_out = {
	.name = "pll1_out",
	.parent = &osc_clk,
	.flag = CLK_RATE_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_PLL1CFG_CTL),
	.init = &comip_clk_pll_init,
};

static struct clk pll1_mclk = {
	.name = "pll1_mclk",
	.parent = &pll1_out,
	.fix_div = 4,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
};

static struct clk pll1_mclk_div2 = {
	.name = "pll1_mclk_div2",
	.parent = &pll1_mclk,
	.fix_div = 2,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
};

static struct clk pll1_mclk_div13 = {
	.name = "pll1_mclk_div13",
	.parent = &pll1_mclk,
	.fix_div = 13,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
};

static struct clk pll2_clk_in = {
	.name = "pll2_clk_in",
	.parent = &pll1_mclk_div13,
	.fix_div = 2,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
};

static struct clk pll2_out = {
	.name = "pll2_out",
	.parent = &pll2_clk_in,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_PLL2_CTL),
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_PLL2CFG_CTL),
	.init = &comip_clk_pll_init,
};

static struct clk bus_mclk = {
	.name = "bus_mclk",
	.parent = &pll1_out,
	.flag = CLK_ALWAYS_ENABLED,
	.cust = &clk_range3_3_6,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
};

static struct clk ctl_pclk = {
	.name = "ctl_pclk",
	.parent = &bus_mclk,
	.fix_div = 2,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_CTL),
};

static struct clk data_pclk = {
	.name = "data_pclk",
	.parent = &bus_mclk,
	.fix_div = 2,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_CTL),
};

static struct clk sec_pclk = {
	.name = "sec_pclk",
	.parent = &bus_mclk,
	.fix_div = 2,
	.flag = CLK_RATE_DIV_FIXED | CLK_ALWAYS_ENABLED,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_CTL),
};

/* Application Unit. */
static struct clk a9_clk = {
	.name = "a9_clk",
	.parent = &pll0_out,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_A9_CLK_CTL),
	.grclk_bit = 0,	
#if !defined(CONFIG_MENTOR_DEBUG)
	.grclk_mask = 0xf,
#else
	.grclk_mask = 0x1f,
#endif
	.grclk_we_bit = 16,
	.init = &comip_clk_a9_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_a9_set_rate,
};

static struct clk a9_peri_clk = {
	.name = "a9_peri_clk",
	.parent = &a9_clk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_A9_CLK_CTL),
	.grclk_bit = 8,
	.grclk_mask = 0x7,
	.grclk_we_bit = 18,
	.init = &comip_clk_a9_peri_init,
	.set_rate = &comip_clk_a9_peri_set_rate,
};

static struct clk a9_acp_axi_clk = {
	.name = "a9_acp_axi_clk",
	.parent = &a9_clk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_A9_CLK_CTL),
	.grclk_bit = 12,
	.grclk_mask = 0x7,
	.grclk_we_bit = 19,
	.init = &comip_clk_a9_acp_init,
	.set_rate = &comip_clk_a9_acp_set_rate,
};

static struct clk gpu_clk = {
	.name = "gpu_clk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_GPU_CLK_CTL),
	.mclk_bit = 9,
	.mclk_we_bit = 19,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_GPU_CLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk peri_gpu_pclk = {
	.name = "peri_gpu_pclk",
	.parent = &gpu_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_GPU_CLK_CTL),
	.mclk_bit = 8,
	.mclk_we_bit = 18,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk on2_encode_clk = {
	.name = "on2_encode_clk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk on2_decode_clk = {
	.name = "on2_decode_clk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2CLK_CTL),
	.divclk_bit = 12,
	.divclk_mask = 0x3,
	.divclk_we_bit = 19,
	.grclk_bit = 8,
	.grclk_mask = 0xf,
	.grclk_we_bit = 18,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk on2_e0_clk = {
	.name = "on2_e0_clk",
	.parent = &on2_encode_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2_CLK_EN),
	.mclk_bit = 0,
	.mclk_we_bit = 8,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk on2_e1_clk = {
	.name = "on2_e1_clk",
	.parent = &on2_encode_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2_CLK_EN),
	.mclk_bit = 1,
	.mclk_we_bit = 9,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk on2_ebus_clk = {
	.name = "on2_ebus_clk",
	.parent = &on2_encode_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2_CLK_EN),
	.mclk_bit = 4,
	.mclk_we_bit = 12,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk on2_d_clk = {
	.name = "on2_d_clk",
	.parent = &on2_decode_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ON2_CLK_EN),
	.mclk_bit = 2,
	.mclk_we_bit = 10,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk ddr_axi_clk = {
	.name = "ddr_axi_clk",
	.parent = &pll1_out,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_DDRAXICLK_CTL),
};

static struct clk ddr_axi_gpv_aclk = {
	.name = "ddr_axi_gpv_aclk",
	.parent = &ddr_axi_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_DDRAXICLK_CTL),
};

static struct clk nfc_clk = {
	.name = "nfc_clk",
	.parent = &pll1_mclk,
};

static struct clk hpi_clk = {
	.name = "hpi_clk",
	.parent = &pll1_mclk,
};

static struct clk usbotg_12m_clk = {
	.name = "usbotg_12m_clk",
	.parent = &pll1_mclk_div13,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLK_EN),
	.mclk_bit = 0,
	.mclk_we_bit = 8,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN1),
	.ifclk_bit = 0,
	.ifclk_we_bit = 16,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLKDIV_CTL),
	.init = &comip_clk_usb_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_usb_set_rate,
};

static struct clk usbhost_12m_clk = {
	.name = "usbhost_12m_clk",
	.parent = &pll1_mclk_div13,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLK_EN),
	.mclk_bit = 1,
	.mclk_we_bit = 9,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN1),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLKDIV_CTL),
	.init = &comip_clk_usb_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_usb_set_rate,
};

static struct clk usbhsic_12m_clk= {
	.name = "usbhsic_12m_clk",
	.parent = &pll1_mclk_div13,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLK_EN),
	.mclk_bit = 2,
	.mclk_we_bit = 10,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN1),
	.ifclk_bit = 2,
	.ifclk_we_bit = 18,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_USBCLKDIV_CTL),
	.init = &comip_clk_usb_init,	
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_usb_set_rate,
};

static struct clk lcdc_axi_clk = {
	.name = "lcdc_axi_clk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDCAXICLK_CTL),
	.mclk_bit = 8,
	.mclk_we_bit = 18,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN1),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDCAXICLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk lcdc0_clk = {
	.name = "lcdc0_clk",
	.parent = &pll1_mclk,
	.cust = &clk_range1_2_32,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDC0CLK_CTL),
	.mclk_bit = 12,
	.mclk_we_bit = 18,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN1),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_LCDC0CLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x1f,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_lcd_set_rate,
};

static struct clk hdmi_pix_clk = {
	.name = "hdmi_pix_clk",
	.parent = &pll2_out,
	.cust = &clk_range1_2_32,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_HDMIPIXCLK_CTL),
	.mclk_bit = 12,
	.mclk_we_bit = 18,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_HDMIPIXCLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x1f,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk hdmi_isfr_clk = {
	.name = "hdmi_isfr_clk",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_DISPCLK_EN),
	.mclk_bit = 0,
	.mclk_we_bit = 8,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk dsi_cfg_clk = {
	.name = "dsi_cfg_clk",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_DISPCLK_EN),
	.mclk_bit = 1,
	.mclk_we_bit = 9,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk dsi_ref_clk = {
	.name = "dsi_ref_clk",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_DISPCLK_EN),
	.mclk_bit = 2,
	.mclk_we_bit = 10,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk isp_cphy_cfg_clk = {
	.name = "isp_cphy_cfg_clk",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL0),
	.mclk_bit = 11,
	.mclk_we_bit = 21,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk isp_axi_clk = {
	.name = "isp_axi_clk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL0),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk isp_p_sclk = {
	.name = "isp_p_sclk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL0),
	.mclk_bit = 8,
	.mclk_we_bit = 18,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL1),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk isp_sclk2 = {
	.name = "isp_sclk2",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL0),
	.mclk_bit = 10,
	.mclk_we_bit = 20,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL1),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk isp_hclk = {
	.name = "isp_hclk",
	.parent = &bus_mclk,	
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ISPCLK_CTL0),
	.mclk_bit = 9,
	.mclk_we_bit = 19,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

/* CTL_APB */
static struct clk i2c0_clk = {
	.name = "i2c_clk",
	.id = 0,
	.parent = &pll1_mclk,
	.cust = &clk_range1_2_32,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLAPBMCLK_EN),
	.mclk_bit = 1,
	.mclk_we_bit = 9,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 3,
	.ifclk_we_bit = 19,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2CCLK_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0x1f,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk i2c1_clk = {
	.name = "i2c_clk",
	.id = 1,
	.parent = &pll1_mclk,
	.cust = &clk_range1_2_32,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLAPBMCLK_EN),
	.mclk_bit = 2,
	.mclk_we_bit = 10,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 4,
	.ifclk_we_bit = 20,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2CCLK_CTL),
	.divclk_bit = 8,
	.divclk_mask = 0x1f,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk i2c2_clk = {
	.name = "i2c_clk",
	.id = 2,
	.parent = &pll1_mclk,
	.cust = &clk_range1_2_32,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SECAPBMCLK_EN),
	.mclk_bit = 1,
	.mclk_we_bit = 9,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_EN),
	.ifclk_bit = 0,
	.ifclk_we_bit = 16,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2CCLK_CTL),
	.divclk_bit = 16,
	.divclk_mask = 0x1f,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk i2c3_clk = {
	.name = "i2c_clk",
	.id = 3,
	.parent = &pll1_mclk,
	.cust = &clk_range1_2_32,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLAPBMCLK_EN),
	.mclk_bit = 3,
	.mclk_we_bit = 11,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2CCLK_CTL),
	.divclk_bit = 24,
	.divclk_mask = 0x1f,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk pwm_clk = {
	.name = "pwm_clk",
	.parent = &osc_clk,
	.cust = &clk_range1_2_256,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLAPBMCLK_EN),
	.mclk_bit = 4,
	.mclk_we_bit = 12,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 6,
	.ifclk_we_bit = 22,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_PWMCLKDIV_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0xff,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk timer0_clk = {
	.name = "timer0_clk",
	.id = 0,
	.parent = &bus_mclk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER0CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 2,
	.init = &comip_clk_timer_init,
	.enable = &comip_clk_timer_enable,
	.disable = &comip_clk_timer_disable,
	.set_rate = &comip_clk_timer_set_rate,
	.set_parent = &comip_clk_timer_set_parent,
};

static struct clk timer1_clk = {
	.name = "timer1_clk",
	.id = 1,
	.parent = &bus_mclk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER1CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 2,
	.init = &comip_clk_timer_init,
	.enable = &comip_clk_timer_enable,
	.disable = &comip_clk_timer_disable,
	.set_rate = &comip_clk_timer_set_rate,
	.set_parent = &comip_clk_timer_set_parent,
};

static struct clk timer2_clk = {
	.name = "timer2_clk",
	.id = 2,
	.parent = &bus_mclk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER2CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 2,
	.init = &comip_clk_timer_init,
	.enable = &comip_clk_timer_enable,
	.disable = &comip_clk_timer_disable,
	.set_rate = &comip_clk_timer_set_rate,
	.set_parent = &comip_clk_timer_set_parent,
};

static struct clk timer3_clk = {
	.name = "timer3_clk",
	.id = 3,
	.parent = &bus_mclk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER3CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 2,
	.init = &comip_clk_timer_init,
	.enable = &comip_clk_timer_enable,
	.disable = &comip_clk_timer_disable,
	.set_rate = &comip_clk_timer_set_rate,
	.set_parent = &comip_clk_timer_set_parent,
};
#ifdef CONFIG_MENTOR_DEBUG
static struct clk timer4_clk = {
	.name = "timer4_clk",
	.id = 4,
	.parent = &bus_mclk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER4CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 2,
	.init = &comip_clk_timer_init,
	.enable = &comip_clk_timer_enable,
	.disable = &comip_clk_timer_disable,
	.set_rate = &comip_clk_timer_set_rate,
	.set_parent = &comip_clk_timer_set_parent,
};

static struct clk timer5_clk = {
	.name = "timer5_clk",
	.id = 5,
	.parent = &bus_mclk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER5CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 2,
	.init = &comip_clk_timer_init,
	.enable = &comip_clk_timer_enable,
	.disable = &comip_clk_timer_disable,
	.set_rate = &comip_clk_timer_set_rate,
	.set_parent = &comip_clk_timer_set_parent,
};

static struct clk timer6_clk = {
	.name = "timer6_clk",
	.id = 6,
	.parent = &bus_mclk,
	.cust = &timer_mask,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_TIMER6CLKCTL),
	.mclk_bit = 28,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 2,
	.init = &comip_clk_timer_init,
	.enable = &comip_clk_timer_enable,
	.disable = &comip_clk_timer_disable,
	.set_rate = &comip_clk_timer_set_rate,
	.set_parent = &comip_clk_timer_set_parent,
};
#endif

static struct clk wdt_clk = {
	.name = "wdt_clk",
	.parent = &ctl_pclk,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CTLPCLK_EN),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

/* DATA_APB */
static struct clk i2s0_clk = {
	.name = "i2s0_clk",
	.parent = &pll1_mclk,
	.cust = &clk_i2s_table,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_I2SCLKGR_CTL),
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 0,
	.ifclk_we_bit = 16,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2S0CLK_CTL),
	.init = &comip_clk_init_by_table,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_by_table,
};

static struct clk i2s1_clk = {
	.name = "i2s1_clk",
	.parent = &pll1_mclk,
	.cust = &clk_i2s_table,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_I2SCLKGR_CTL),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 17,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_I2S1CLK_CTL),
	.init = &comip_clk_init_by_table,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_by_table,
};

static struct clk ssi0_clk = {
	.name = "ssi_clk",
	.id = 0,
	.parent = &pll1_mclk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKGR_CTL),
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 2,
	.ifclk_we_bit = 18,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKDIV_CTL),
	.divclk_bit = 0,
	.divclk_mask = 0xf,
	.init = &comip_clk_ssi_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_ssi_set_rate,
};

static struct clk ssi1_clk = {
	.name = "ssi_clk",
	.id = 1,
	.parent = &pll1_mclk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKGR_CTL),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 17,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 3,
	.ifclk_we_bit = 19,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKDIV_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0xf,
	.init = &comip_clk_ssi_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_ssi_set_rate,
};

static struct clk ssi2_clk = {
	.name = "ssi_clk",
	.id = 2,
	.parent = &pll1_mclk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKGR_CTL),
	.grclk_bit = 8,
	.grclk_mask = 0xf,
	.grclk_we_bit = 18,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_EN),
	.ifclk_bit = 2,
	.ifclk_we_bit = 18,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKDIV_CTL),
	.divclk_bit = 8,
	.divclk_mask = 0xf,
	.init = &comip_clk_ssi_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_ssi_set_rate,
};

static struct clk ssi3_clk = {
	.name = "ssi_clk",
	.id = 3,
	.parent = &pll1_mclk,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKGR_CTL),
	.grclk_bit = 12,
	.grclk_mask = 0xf,
	.grclk_we_bit = 19,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 4,
	.ifclk_we_bit = 20,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SSICLKDIV_CTL),
	.divclk_bit = 12,
	.divclk_mask = 0xf,
	.init = &comip_clk_ssi_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_ssi_set_rate,
};

static struct clk uart0_clk = {
	.name = "uart_clk",
	.id = 0,
	.parent = &pll1_mclk,
	.cust = &clk_uart_table,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_UARTCLKGR_CTL),
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_UART0CLK_CTL),
	.init = &comip_clk_init_by_table,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_by_table,
};

static struct clk uart1_clk = {
	.name = "uart_clk",
	.id = 1,
	.parent = &pll1_mclk,
	.cust = &clk_uart_table,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_UARTCLKGR_CTL),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit = 17,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 6,
	.ifclk_we_bit = 22,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_UART1CLK_CTL),
	.init = &comip_clk_init_by_table,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_by_table,
};

static struct clk uart2_clk = {
	.name = "uart_clk",
	.id = 2,
	.parent = &pll1_mclk,
	.cust = &clk_uart_table,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_UARTCLKGR_CTL),
	.grclk_bit = 8,
	.grclk_mask = 0xf,
	.grclk_we_bit = 18,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_DATAPCLK_EN),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_UART2CLK_CTL),
	.init = &comip_clk_init_by_table,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_by_table,
};

/* SEC_APB */
static struct clk kbs_clk = {
	.name = "kbs_clk",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_SECAPBMCLK_EN),
	.mclk_bit = 0,
	.mclk_we_bit = 8,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_EN),
	.ifclk_bit = 1,
	.ifclk_we_bit = 17, 
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk sdmmc0_clk = {
	.name = "sdmmc_clk",
	.id = 0,
	.parent = &pll1_out,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLKGR_CTL),
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit  = 16,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_EN),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMC0CLKCTL),
	.divclk_bit = 8,
	.divclk_mask = 0x7,
	.init = &comip_clk_sdmmc_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_sdmmc_set_rate,
};

static struct clk sdmmc1_clk = {
	.name = "sdmmc_clk",
	.id = 1,
	.parent = &pll1_out,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLKGR_CTL),
	.grclk_bit = 4,
	.grclk_mask = 0xf,
	.grclk_we_bit  = 17,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_EN),
	.ifclk_bit = 6,
	.ifclk_we_bit = 22,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMC1CLKCTL),
	.divclk_bit = 8,
	.divclk_mask = 0x7,
	.init = &comip_clk_sdmmc_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_sdmmc_set_rate,
};

static struct clk sdmmc2_clk = {
	.name = "sdmmc_clk",
	.id = 2,
	.parent = &pll1_out,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLKGR_CTL),
	.grclk_bit = 8,
	.grclk_mask = 0xf,
	.grclk_we_bit  = 18,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_EN),
	.ifclk_bit = 7,
	.ifclk_we_bit = 23,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMC2CLKCTL),
	.divclk_bit = 8,
	.divclk_mask = 0x7,
	.init = &comip_clk_sdmmc_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_sdmmc_set_rate,
};

static struct clk sdmmc3_clk = {
	.name = "sdmmc_clk",
	.id = 3,
	.parent = &pll1_out,
	.grclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMCCLKGR_CTL),
	.grclk_bit = 12,
	.grclk_mask = 0xf,
	.grclk_we_bit  = 19,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SECPCLK_EN),
	.ifclk_bit = 8,
	.ifclk_we_bit = 24,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_SDMMC3CLKCTL),
	.divclk_bit = 8,
	.divclk_mask = 0x7,
	.init = &comip_clk_sdmmc_init,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_sdmmc_set_rate,
};

static struct clk atb_clk = {
	.name = "atb_clk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_ATBCLK_CTL),
	.mclk_bit = 8,
	.mclk_we_bit = 18,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_ATBCLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk dbg_pclk = {
	.name = "dbg_pclk",
	.parent = &atb_clk,
	.cust = &clk_range1_2_16,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_A9DBGPCLK_CTL),
	.mclk_bit = 0,
	.mclk_we_bit = 16,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_A9DBGPCLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0xf,
	.divclk_we_bit = 17,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk cs_cfg_pclk = {
	.name = "cs_cfg_pclk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN2),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_CSCFGCLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk cs_ctm_clk = {
	.name = "cs_ctm_clk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN2),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_CSCTMCLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

static struct clk cs_arm0cti_clk = {
	.name = "cs_arm0cti_clk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN2),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_CSARM0CTICLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};

#if defined(CONFIG_CPU_LC1810)
static struct clk cs_m0cti_clk = {
	.name = "cs_m0cti_clk",
	.parent = &pll1_out,
	.cust = &clk_range3_3_6,
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN2),
	.ifclk_bit = 5,
	.ifclk_we_bit = 21,
	.divclk_reg = (void __iomem *)io_p2v(AP_PWR_CSM0CTICLK_CTL),
	.divclk_bit = 4,
	.divclk_mask = 0x3,
	.divclk_we_bit = 17,
	.grclk_bit = 0,
	.grclk_mask = 0xf,
	.grclk_we_bit = 16,
	.init = &comip_clk_init_generic,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
	.set_rate = &comip_clk_set_rate_generic,
};
#endif

static struct clk clkout0_clk = {
	.name = "clkout0_clk",
	.parent = &bus_mclk,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUT0CLKCTL),
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUTSEL),
	.ifclk_bit = 0,
	.init = &comip_clk_clkout_init,
	.enable = &comip_clk_clkout_enable,
	.disable = &comip_clk_clkout_disable,
	.set_rate = &comip_clk_clkout_set_rate,
	.set_parent = &comip_clk_clkout_set_parent,
};

static struct clk clkout1_clk = {
	.name = "clkout1_clk",
	.parent = &bus_mclk,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUT1CLKCTL),
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUTSEL),
	.ifclk_bit = 4,
	.init = &comip_clk_clkout_init,
	.enable = &comip_clk_clkout_enable,
	.disable = &comip_clk_clkout_disable,
	.set_rate = &comip_clk_clkout_set_rate,
	.set_parent = &comip_clk_clkout_set_parent,
};

static struct clk clkout2_clk = {
	.name = "clkout2_clk",
	.parent = &bus_mclk,
	.flag = CLK_INIT_DISABLED,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUT2CLKCTL),
	.ifclk_reg = (void __iomem *)io_p2v(AP_PWR_CLKOUTSEL),
	.ifclk_bit = 8,
	.init = &comip_clk_clkout_init,
	.enable = &comip_clk_clkout_enable,
	.disable = &comip_clk_clkout_disable,
	.set_rate = &comip_clk_clkout_set_rate,
	.set_parent = &comip_clk_clkout_set_parent,
};

static struct clk cp_clk32k = {
	.name = "cp_clk32k",
	.parent = &clk_32k,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CPCLK_CTL),
	.mclk_bit = 0,
	.mclk_we_bit = 8,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk cp_clk26m0 = {
	.name = "cp_clk26m0",
	.parent = &osc_clk,
	.mclk_reg = (void __iomem *)io_p2v(AP_PWR_CPCLK_CTL),
	.mclk_bit = 1,
	.mclk_we_bit = 9,
	.enable = &comip_clk_enable_generic,
	.disable = &comip_clk_disable_generic,
};

static struct clk *ap_pwr_clks[] = {
	&func_mclk,
	&pll0_out,
#if !defined(CONFIG_MENTOR_DEBUG)
	&pll1_out,
#endif
	&pll1_mclk,
	&pll1_mclk_div2,
	&pll1_mclk_div13,
	&pll2_clk_in,
#if !defined(CONFIG_MENTOR_DEBUG)
	&pll2_out,
#endif
	&bus_mclk,
	&ctl_pclk,
	&data_pclk,
	&sec_pclk,
	&a9_clk,
	&a9_peri_clk,
	&a9_acp_axi_clk,
	&gpu_clk,
	&peri_gpu_pclk,
	&on2_encode_clk,
	&on2_decode_clk,
	&on2_e0_clk,
	&on2_e1_clk,
	&on2_ebus_clk,
	&on2_d_clk,
	&ddr_axi_clk,
	&ddr_axi_gpv_aclk,
	&nfc_clk,
	&hpi_clk,
	&usbotg_12m_clk,
	&usbhost_12m_clk,
	&usbhsic_12m_clk,
	&lcdc_axi_clk,
	&lcdc0_clk,
	&hdmi_pix_clk,
	&hdmi_isfr_clk,
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
#ifdef CONFIG_MENTOR_DEBUG	
	&timer4_clk,
	&timer5_clk,
	&timer6_clk,
#endif	
	&wdt_clk, 
	&i2s0_clk,
	&i2s1_clk,
	&ssi0_clk,
	&ssi1_clk,
	&ssi2_clk,
	&ssi3_clk,
	&uart0_clk,
	&uart1_clk,
	&uart2_clk,
	&kbs_clk,
	&sdmmc0_clk,
	&sdmmc1_clk,
	&sdmmc2_clk,
	&sdmmc3_clk,
	&atb_clk,
	&dbg_pclk,
	&cs_cfg_pclk,
	&cs_ctm_clk,
	&cs_arm0cti_clk,
#if defined(CONFIG_CPU_LC1810)
	&cs_m0cti_clk,
#endif
	&clkout0_clk,
	&clkout1_clk,
	&clkout2_clk,
	&cp_clk32k,
	&cp_clk26m0,
};

#endif /*__ARCH_ARM_COMIP_CLOCK_H*/
