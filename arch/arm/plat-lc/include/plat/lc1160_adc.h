


#ifndef _LC1160_ADC_H
#define _LC1160_ADC_H


#define ADC_MAX_CHANNELS   (5)


#define LC1160_BASEADD_ADC       (0x87)

/*lc1160 adc reg addr*/
#define LC1160_ADCCR1_REG        (LC1160_BASEADD_ADC + 0)
#define LC1160_ADCCR2_REG        (LC1160_BASEADD_ADC + 1)
#define LC1160_ADCDATA0_REG      (LC1160_BASEADD_ADC + 2)
#define LC1160_ADCDATA1_REG      (LC1160_BASEADD_ADC + 3)
#define LC1160_IRQEN2_REG        (LC1160_BASEADD_ADC + 10)
#define LC1160_IRQMASK2_REG      (LC1160_BASEADD_ADC + 12)

/*ADCCR1: A/D Converter Control Register1*/
#define LC1160_ADC_CH0_VREF      (1 << 7)
#define LC1160_ADCEND            (1 << 4)
#define LC1160_ADCSTART          (1 << 3)
#define LC1160_ADCFORMAT_10BOT   (1 << 2)
#define LC1160_ADCFORMAT_12BIT   (0 << 2)
#define LC1160_ADCEN             (1 << 1)
#define LC1160_ADCDIS            (0 << 1)
#define LC1160_LDOADCEN          (1)
#define LC1160_LDOADCDIS         (0)

/*ADCCR2: A/D Converter Control Register2*/
#define LC1160_TS_PCB_ON          (1 << 4)
#define LC1160_ADCMODE_SINGLE     (0 << 3)
#define LC1160_ADCMODE_CONTINUOUS (1 << 3)



#define ADCCPIR_INTR_EN        (1 << 4)
#define ADCCPIR_INTR_MASK      (1 << 4)
#define LC1160_ADC_INT_MASK    (0x10)


enum lc1160_adc_channel{
    ADC_VOLT_CHANNEL0,
    ADC_TEMP_CHANNEL1,
    ADC_IN1_CHANNEL2,
    ADC_IN2_CHANNEL3,
    ADC_IN3_CHANNEL4,
    ADC_CHANNEL_MAX,
};

struct lc1160_adc_conversion_method {
    u8 sel;
    u8 rbase;
    u8 ctrl;
    u8 enable;
    u8 mask;
};

struct lc1160_value{
    int raw_code;
    int raw_channel_value;
    int code;
};

struct lc1160_adc_request{
    int channels;
    int adcsel;
    u16 do_avg;
    u16 method;
    u16 type;
    int active;
    int result_pending;
    int rbuf[ADC_MAX_CHANNELS];
    void (*func_cb)(struct lc1160_adc_request *req);
    struct lc1160_value buf[ADC_MAX_CHANNELS];
};


enum lc1160_conversion_methods{
    LC1160_ADC_SINGLE,
    LC1160_ADC_CONTINUOUS,
    LC1160_ADC_NUM_METHODS
};

enum lc1160_sample_type{
    LC1160_ADC_WAIT,
    LC1160_ADC_IRQ_ONESHOT,
    LC1160_ADC_IRQ_REARM
};


struct lc1160_adc_user_parms {
        int channel;
        int status;
        u16 result;
};
#define LC1160_ADC_IOC_MAGIC '`'
#define LC1160_ADC_IOCX_ADC_RAW_READ _IO(LC1160_ADC_IOC_MAGIC, 0)
#define LC1160_ADC_IOCX_ADC_READ     _IO(LC1160_ADC_IOC_MAGIC+1, 0)

struct lc1160_chnl_calib {
    s32 gain_error;
    s32 offset_error;
};

struct lc1160_ideal_code {
    s16 code1;
    s16 code2;
};

extern int lc1160_adc_conversion(struct lc1160_adc_request *conv);



#endif
