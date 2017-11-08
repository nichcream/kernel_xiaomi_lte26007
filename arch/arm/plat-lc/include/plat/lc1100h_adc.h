


#ifndef _LC1100H_ADC_H
#define _LC1100H_ADC_H


#define ADC_MAX_CHANNELS   (5)

#define LC1100H_ADCLDOEN_REG      (0x38)
#define LC1100H_LDOADCEN          (1)
#define LC1100H_LDOADCDIS         (0)
#define LC1100H_ADCCTRL_REG       (0x39)
#define LC1100H_ADCMODE_SINGLE    (0 << 4)
#define LC1100H_ADCMODE_CONTINUOUS (1 << 4)
#define LC1100H_ADCSEL_BAT_VOLT         (0)
#define LC1100H_ADCSEL_BAT_TEMP         (1)
#define LC1100H_ADCSEL_ADCIN1           (2)
#define LC1100H_ADCSEL_ADCIN2           (3)
#define LC1100H_ADCSEL_ADCIN3           (4)
#define LC1100H_ADCCMD_REG        (0x3A)
#define LC1100H_ADCEND            (1 << 4)
#define LC1100H_ADCSTART          (1 << 0)
#define LC1100H_ADCEN_REG         (0x3B)
#define LC1100H_ADCEN             (1)
#define LC1100H_ADCDIS            (0)
#define LC1100H_ADCDATA0_REG      (0x3C)
#define LC1100H_ADCDATA1_REG      (0x3D)
#define LC1100H_ADCFORMAT_REG     (0x3E)
#define LC1100H_ADCFORMAT_12BIT   (0)
#define LC1100H_ADCFORMAT_10BOT   (1)

#define LC1100H_IRQEN2_REG      (0x46)
#define ADCCPIR_INTR_EN        (1 << 4)
#define LC1100H_IRQMASK2_REG    (0x48)
#define ADCCPIR_INTR_MASK      (1 << 4)
#define LC1100H_ADC_INT_MASK    (0x10)
#define LC1100H_ADC_INT_ENABLE  (0x10)

enum lc1100h_adc_channel{
    ADC_VOLT_CHANNEL0,
    ADC_TEMP_CHANNEL1,
    ADC_IN1_CHANNEL2,
    ADC_IN2_CHANNEL3,
    ADC_IN3_CHANNEL4,
    ADC_CHANNEL_MAX,
};

struct lc1100h_adc_conversion_method {
    u8 sel;
    u8 rbase;
    u8 ctrl;
    u8 enable;
    u8 mask;
};

struct lc1100h_value{
	int raw_code;
	int raw_channel_value;
	int code;
};

struct lc1100h_adc_request{
	int channels;
	int adcsel;
	u16 do_avg;
	u16 method;
	u16 type;
	int active;
	int result_pending;
	int rbuf[ADC_MAX_CHANNELS];
	void (*func_cb)(struct lc1100h_adc_request *req);
	struct lc1100h_value buf[ADC_MAX_CHANNELS];
};


enum lc1100h_conversion_methods{
	LC1100H_ADC_SINGLE,
	LC1100H_ADC_CONTINUOUS,
	LC1100H_ADC_NUM_METHODS
};

enum lc1100h_sample_type{
	LC1100H_ADC_WAIT,
	LC1100H_ADC_IRQ_ONESHOT,
	LC1100H_ADC_IRQ_REARM
};


struct lc1100h_adc_user_parms {
        int channel;
        int status;
        u16 result;
};
#define LC1100H_ADC_IOC_MAGIC '`'
#define LC1100H_ADC_IOCX_ADC_RAW_READ _IO(LC1100H_ADC_IOC_MAGIC, 0)
#define LC1100H_ADC_IOCX_ADC_READ     _IO(LC1100H_ADC_IOC_MAGIC+1, 0)

struct lc1100h_chnl_calib {
	s32 gain_error;
	s32 offset_error;
};

struct lc1100h_ideal_code {
	s16 code1;
	s16 code2;
};

extern int lc1100h_adc_conversion(struct lc1100h_adc_request *conv);
extern int lc1100h_get_adc_conversion(int channel_no);


#endif
