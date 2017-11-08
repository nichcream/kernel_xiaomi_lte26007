


#ifndef _LC1132_ADC_H
#define _LC1132_ADC_H


#define ADC_MAX_CHANNELS   (5)

#define LC1132_ADCLDOEN_REG       (0x38)
#define LC1132_LDOADCEN           (1)
#define LC1132_LDOADCDIS          (0)

#define LC1132_ADCCTRL_REG        (0x39)
#define LC1132_ADCMODE_SINGLE     (0 << 4)
#define LC1132_ADCMODE_CONTINUOUS (1 << 4)
#define LC1132_ADCSEL_BAT_VOLT    (0)
#define LC1132_ADCSEL_BAT_TEMP    (1)
#define LC1132_ADCSEL_ADCIN1      (2)
#define LC1132_ADCSEL_ADCIN2      (3)
#define LC1132_ADCSEL_ADCIN3      (4)

#define LC1132_ADCCMD_REG        (0x3A)
#define LC1132_ADCEND            (1 << 4)
#define LC1132_ADCSTART          (1 << 0)

#define LC1132_ADCEN_REG         (0x3B)
#define LC1132_ADCEN             (1)
#define LC1132_ADCDIS            (0)

#define LC1132_ADCDATA0_REG      (0x3C)

#define LC1132_ADCDATA1_REG      (0x3D)

#define LC1132_ADCFORMAT_REG     (0x3E)
#define LC1132_ADCFORMAT_12BIT   (0)
#define LC1132_ADCFORMAT_10BOT   (1)

#define LC1132_IRQEN2_REG      (0x46)
#define ADCCPIR_INTR_EN        (1 << 4)
#define LC1132_IRQMASK2_REG    (0x48)
#define ADCCPIR_INTR_MASK      (1 << 4)
#define LC1132_ADC_INT_MASK    (0x10)
#define LC1132_ADC_INT_ENABLE  (0x10)

enum lc1132_adc_channel{
    ADC_VOLT_CHANNEL0,
    ADC_TEMP_CHANNEL1,
    ADC_IN1_CHANNEL2,
    ADC_IN2_CHANNEL3,
    ADC_IN3_CHANNEL4,
    ADC_CHANNEL_MAX,
};

struct lc1132_adc_conversion_method {
    u8 sel;
    u8 rbase;
    u8 ctrl;
    u8 enable;
    u8 mask;
};

struct lc1132_value{
    int raw_code;
    int raw_channel_value;
    int code;
};

struct lc1132_adc_request{
    int channels;
    int adcsel;
    u16 do_avg;
    u16 method;
    u16 type;
    int active;
    int result_pending;
    int rbuf[ADC_MAX_CHANNELS];
    void (*func_cb)(struct lc1132_adc_request *req);
    struct lc1132_value buf[ADC_MAX_CHANNELS];
};


enum lc1132_conversion_methods{
    LC1132_ADC_SINGLE,
    LC1132_ADC_CONTINUOUS,
    LC1132_ADC_NUM_METHODS
};

enum lc1132_sample_type{
    LC1132_ADC_WAIT,
    LC1132_ADC_IRQ_ONESHOT,
    LC1132_ADC_IRQ_REARM
};


struct lc1132_adc_user_parms {
        int channel;
        int status;
        u16 result;
};
#define LC1132_ADC_IOC_MAGIC '`'
#define LC1132_ADC_IOCX_ADC_RAW_READ _IO(LC1132_ADC_IOC_MAGIC, 0)
#define LC1132_ADC_IOCX_ADC_READ     _IO(LC1132_ADC_IOC_MAGIC+1, 0)

struct lc1132_chnl_calib {
    s32 gain_error;
    s32 offset_error;
};

struct lc1132_ideal_code {
    s16 code1;
    s16 code2;
};

extern int lc1132_adc_conversion(struct lc1132_adc_request *conv);



#endif
