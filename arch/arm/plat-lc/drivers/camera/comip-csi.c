
#include "comip-csi.h"
#include <plat/comip-pmic.h>

//#define COMIP_CSI_DEBUG
#ifdef COMIP_CSI_DEBUG
#define CSI_PRINT(fmt, args...) printk(KERN_ERR "[ISP]" fmt, ##args)
#else
#define CSI_PRINT(fmt, args...)
#endif


/* Reference clock frequency divided by Input Frequency Division Ratio LIMITS */
#define DPHY_DIV_UPPER_LIMIT	800000
#define DPHY_DIV_LOWER_LIMIT	1000
#define MIN_OUTPUT_FREQ		80

static struct {
	uint32_t freq;		//upper margin of frequency range
	uint8_t hs_freq;	//hsfreqrange
	uint8_t vco_range;	//vcorange
}ranges[] = {
	{90, 0x00, 0x01}, {100, 0x10, 0x01}, {110, 0x20, 0x01},
	{125, 0x01, 0x01}, {140, 0x11, 0x01}, {150, 0x21, 0x01},
	{160, 0x02, 0x01}, {180, 0x12, 0x03}, {200, 0x22, 0x03},
	{210, 0x03, 0x03}, {240, 0x13, 0x03}, {250, 0x23, 0x03},
	{270, 0x04, 0x07}, {300, 0x14, 0x07}, {330, 0x24, 0x07},
	{360, 0x15, 0x07}, {400, 0x25, 0x07}, {450, 0x06, 0x07},
	{500, 0x16, 0x07}, {550, 0x07, 0x0f}, {600, 0x17, 0x0f},
	{650, 0x08, 0x0f}, {700, 0x18, 0x0f}, {750, 0x09, 0x0f},
	{800, 0x19, 0x0f}, {850, 0x0A, 0x0f}, {900, 0x1A, 0x0f},
	{950, 0x2A, 0x0f}, {1000, 0x3A, 0x0f},
};

static struct {
	uint32_t loop_div;	// upper limit of loop divider range
	uint8_t cp_current;	// icpctrl
	uint8_t lpf_resistor;	// lpfctrl
}loop_bandwidth[] = {
	{32, 0x06, 0x10}, {64, 0x06, 0x10}, {128, 0x0C, 0x08},
	{256, 0x04, 0x04}, {512, 0x00, 0x01}, {768, 0x01, 0x01},
	{1000, 0x02, 0x01},
};

static void csi_phy_test_clear(unsigned int id, int value)
{
	csi_reg_write_part(CSI_CTRL0(id), value, 0, 1);
}

static void csi_phy_test_clock(unsigned int id, int value)
{
	csi_reg_write_part(CSI_CTRL0(id), value, 1, 1);
}

static void csi_phy_test_data_in(unsigned int id, unsigned char test_data)
{
	csi_reg_writel(test_data, CSI_CTRL1(id));
}

static void csi_phy_test_en(unsigned int id, unsigned char on_falling_edge)
{
	csi_reg_write_part(CSI_CTRL1(id), on_falling_edge, 16, 1);
}

static void csi_phy_write(unsigned int id, unsigned char address,
		unsigned char * data, unsigned char data_length)
{
	unsigned char i = 0;

	if (data != 0) {
		/* set the TESTCLK input high in preparation to latch in the desired test mode */
		csi_phy_test_clock(id, 0);
		/* set the desired test code in the input 8-bit bus TESTDIN[7:0] */
		csi_phy_test_data_in(id, address);
		/* set TESTEN input high  */
		csi_phy_test_en(id, 1);
		/* drive the TESTCLK input low; the falling edge captures the chosen test code into the transceiver */
		csi_phy_test_clock(id, 1);
		csi_phy_test_clock(id, 0);
		/* set TESTEN input low to disable further test mode code latching  */
		csi_phy_test_en(id, 0);
		/* start writing MSB first */
		for (i = data_length; i > 0; i--) {
			/* set TESTDIN[7:0] to the desired test data appropriate to the chosen test mode */
			csi_phy_test_data_in(id, data[i - 1]);
			/* pulse TESTCLK high to capture this test data into the macrocell; repeat these two steps as necessary */
			csi_phy_test_clock(id, 1);
			csi_phy_test_clock(id, 0);
		}
	}
}

static uint8_t csi_phy_read(unsigned int id)
{
	return (csi_reg_readl(CSI_CTRL1(id))& 0xFF00)>>8;
}

//目前只用PHY0，所以只校准PHY0
int csi_phy_software_calibration(unsigned int id)
{
	int retval = 0;
	int i;
	unsigned char testdata[1];
	int AUX_TRIPU,AUX_TRIPD;
	int AUX_TRIP_TEMP;
	int AUX_A,AUX_B;
	int AUX_LAST;

	int Flag_error = 0;
	int Flag_Change = 0;
	int Write_error = 0;

	CSI_PRINT("[CSI]id %d\n",id);
//配置前，提供电压
	testdata[0] = 0<<3;
	csi_phy_write(id,0x20, testdata, 1);
//是否需要稳定一下?
	udelay(100);


//1. Set TESTDIN[4:2] to 000 and wait for 100 ns (settling time).
	testdata[0] = (0<<2) | 0x3;
	csi_phy_write(id,0x21, testdata, 1);
	udelay(100);
	AUX_TRIPU = csi_phy_read(id);
	if((AUX_TRIPU & 0x1F) != ((0<<2)| 0x3))
		Write_error++;
	AUX_TRIPU &= 0x80;	//this bit should typical case is 1
	CSI_PRINT("1.AUX_TRIPU %d	%d\n",AUX_TRIPU,Write_error);
//	2. Sweep TESTDIN[4:2] from 001 to 111.
	AUX_A = 0;
	Flag_Change = 0;
	for(i=1;i<8;i++)
	{
		testdata[0] = (i<<2) | 0x3;
		csi_phy_write(id,0x21, testdata, 1);
		udelay(100);
		AUX_TRIP_TEMP = csi_phy_read(id);
		if((AUX_TRIP_TEMP & 0x1F) != ((i<<2)| 0x3))
			Write_error++;
		AUX_TRIP_TEMP &= 0x80;	//this bit should typical case is 1
	CSI_PRINT("2.AUX_TRIPU %d	%d\n",AUX_TRIPU,Write_error);
	//3. Check TESTDOUT[7].
		if(AUX_TRIPU!=AUX_TRIP_TEMP)
		{
			if(!Flag_Change)
			{
				AUX_A = i;
				AUX_TRIPU = AUX_TRIP_TEMP;
			}
			Flag_Change++;
		}
	}

	if(AUX_A == 0) //not change value throughtout the sweep
	{
		if((AUX_TRIPU & 0x80))
		{
			testdata[0] = (7<<2) | 0x3;
			csi_phy_write(id,0x21, testdata, 1);
			udelay(100);
			AUX_TRIP_TEMP = csi_phy_read(id);
			if((AUX_TRIP_TEMP & 0x1F) != ((7<<2)| 0x3))
				Write_error++;

			AUX_LAST = 0x7;
		}
		if(!(AUX_TRIPU & 0x80))
		{
			testdata[0] = (0<<2) | 0x3;
			csi_phy_write(id,0x21, testdata, 1);
			udelay(100);
			AUX_TRIP_TEMP = csi_phy_read(id);
			if((AUX_TRIP_TEMP & 0x1F) != ((0<<2)| 0x3))
				Write_error++;

			AUX_LAST = 0x0;
		}
		goto ENDP;
	}

//Once TESTDIN[4:2] = '111'
		AUX_TRIPD = csi_phy_read(id);
		AUX_TRIPD &= 0x80;

		if(AUX_TRIPD != AUX_TRIPU)
		{
			testdata[0] = (7<<2) | 0x3;
			csi_phy_write(id,0x21, testdata, 1);
			udelay(100);
			AUX_TRIP_TEMP = csi_phy_read(id);

			if((AUX_TRIP_TEMP & 0x1F) != ((7<<2)| 0x3))
				Write_error++;

			Flag_error = 1;
			AUX_LAST = 0x3;
			goto ENDP;
		}
		CSI_PRINT("[CSI]4.AUX_TRIPU %d\n",AUX_TRIPU);


//5. Sweep TESTDIN[4:2] from 110 down to 000.
	AUX_B = 7;
	Flag_Change = 0;
	for(i=6;i>=0;i--)
	{
		testdata[0] = (i<<2) | 0x3;
		csi_phy_write(id,0x21, testdata, 1);
		udelay(100);
		AUX_TRIP_TEMP = csi_phy_read(id);

		if((AUX_TRIP_TEMP & 0x1F) != ((i<<2)| 0x3))
			Write_error++;
		AUX_TRIP_TEMP &= 0x80;	//this bit should typical case is 1

	//3. Check TESTDOUT[7].
		if(AUX_TRIPD != AUX_TRIP_TEMP)
		{
			if(!Flag_Change)
			{
				AUX_B = i;
				AUX_TRIPD = AUX_TRIP_TEMP;
			}
			Flag_Change++;
		}
	}

	CSI_PRINT("[CSI]5.AUX_TRIPU %d\n",AUX_TRIPU);
//6. Set TESTDIN[4:2] to ROUND_MAX[(AUX_A+AUX_B)/2].

	i = (AUX_A+AUX_B-1)/2 + 1;
	testdata[0] = (i<<2) | 0x3;
	csi_phy_write(id,0x21, testdata, 1);
	udelay(100);
	AUX_TRIP_TEMP = csi_phy_read(id);

	if((AUX_TRIP_TEMP & 0x1F) != ((i<<2)| 0x3))
		Write_error++;

	AUX_LAST = i;

	CSI_PRINT(" [CSI]6.AUX_LAST %d\n",AUX_LAST);

//配置后，关闭电压
	testdata[0] = 1<<3;
	csi_phy_write(id,0x20, testdata, 1);
	udelay(100);
	retval = csi_phy_read(id);
	udelay(100);



ENDP:

	//下面2个值用于debug
	//AUX_LAST 为最后的得出应写入0x20的值
	//Flag_Change 标示比较中是否发生过变化

	if(Write_error)	//写的过程中发生错误
		retval = -1;
	else
		retval = 0;

	return retval;

}

static int csi_phy_configure(unsigned int id, unsigned int lane_output_freq,
		unsigned int reference_freq, unsigned int no_of_bytes)
{
	unsigned int loop_divider = 0;		// (M)
	unsigned int input_divider = 1;		// (N)
	unsigned char data[4];			// maximum data for now are 4 bytes per test mode
	unsigned char i = 0;			//iterator
	unsigned char range = 0;		//ranges iterator
	int flag = 0;
	unsigned int output_freq;		//data lane out freq (bytes)

	output_freq = lane_output_freq;
	if (output_freq < MIN_OUTPUT_FREQ) {
		return -EINVAL;
	}
	// find M and N dividers;  here the >= DPHY_DIV_LOWER_LIMIT is a phy constraint, formula should be above 1 MHz
	for (input_divider = (1 + (reference_freq / DPHY_DIV_UPPER_LIMIT));
		((reference_freq / input_divider) >= DPHY_DIV_LOWER_LIMIT) && (!flag); input_divider++) {
		if (((output_freq * input_divider) % (reference_freq )) == 0) {
			loop_divider = ((output_freq * input_divider) / (reference_freq));	//values found
			if (loop_divider >= 12) {
				flag = 1;
			}
		}
	}
	if ((!flag) || ((reference_freq / input_divider) < DPHY_DIV_LOWER_LIMIT)) {	//value not found
		loop_divider = output_freq / DPHY_DIV_LOWER_LIMIT;
		input_divider = reference_freq / DPHY_DIV_LOWER_LIMIT;
	}
	else {
		input_divider--;
	}
	for (i = 0; (i < (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0]))) && (loop_divider > loop_bandwidth[i].loop_div); i++);

	if (i >= (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0]))) {
		return -EINVAL;
	}
	/* get the PHY in power down mode (shutdownz=0) and reset it (rstz=0) to
	avoid transient periods in PHY operation during re-configuration procedures. */

	/* provide an initial active-high test clear pulse in TESTCLR  */
	csi_phy_test_clear(id, 1);
	csi_phy_test_clear(id, 0);
	/* find ranges */
	for (range = 0; (range < (sizeof(ranges)/sizeof(ranges[0]))) && ((output_freq / 1000) > ranges[range].freq); range++);
	if (range >= (sizeof(ranges)/sizeof(ranges[0]))) {
		return -EINVAL;
	}

	csi_phy_software_calibration(id);

	/* setup digital part */
	/* hs frequency range [7]|[6:1]|[0]*/
	data[0] = (0 << 7) | (ranges[range].hs_freq << 1) | 0;
	csi_phy_write(id, 0x44, data, 1);

	return 0;
}

static int csi_phy_ready(unsigned int id, unsigned int lane_num)
{
	int ready;
	unsigned int i;

	ready = csi_reg_readl(CSI_STAT(id));

	for (i = 0; i < lane_num; i++)
		if (!(ready & (1 << (20+i))))
			return 0;

	return 1;
}

int csi_phy_init(void)
{
	return 0;
}

int csi_phy_release(void)
{

	csi_reg_writel(0x0, CSI_RSTZ(0));
#if defined (CONFIG_VIDEO_COMIP_ISP)
	csi_reg_writel(0x0, CSI_RSTZ(1));
#endif
	return 0;
}

int csi_phy_start(unsigned int id, unsigned int lane_num, unsigned int freq)
{
	int i;
	int retries = 30;

	if (id >= CSI_NUM)
		return -EINVAL;

	pmic_voltage_set(PMIC_POWER_CAMERA_CSI_PHY, 0, PMIC_POWER_VOLTAGE_ENABLE);

	//mdelay(5);

	/* cphyx_mode */
	csi_reg_writel(0x0, CSI_MODE(id));

	/* set lane and reset */
	csi_reg_writel(0x7f, CSI_RSTZ(id));

	/* wait  */
	//mdelay(5);

	csi_phy_configure(id, freq * 1000, 26000, 2);

	for (i = 0; i < retries; i++) {
		if (csi_phy_ready(id, lane_num))
			break;

		msleep(10);
	}

	if (i >= retries) {
		printk(KERN_DEBUG "CSI PHY is not ready");
	}

	return 0;
}

int csi_phy_stop(unsigned int id)
{
	if (id >= CSI_NUM)
		return -EINVAL;

	csi_reg_writel(0x0, CSI_RSTZ(id));
	csi_phy_test_clear(id, 1);
	pmic_voltage_set(PMIC_POWER_CAMERA_CSI_PHY, 0, PMIC_POWER_VOLTAGE_DISABLE);
	return 0;
}

