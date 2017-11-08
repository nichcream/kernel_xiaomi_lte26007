#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <mach/timer.h>
#include <mach/gpio.h>
#include <plat/mfp.h>
#include <linux/gpio.h>
#include <plat/cpu.h>
#include <linux/platform_device.h>
#include <plat/comip-battery.h>
#include <plat/bq2022a-batid.h>
#include <linux/sched.h>

struct bq2022a_data {
    spinlock_t bqlock;
    int sdq_pin;
    int batttery_type;
    int bat_id_present;
    int bat_id_invalid;
};

/*bat type data number defined*/
enum{
    BAT_TYPE_XWD_61 = 0x61,
    BAT_TYPE_GY_62,
    BAT_TYPE_XWD_63,
    BAT_TYPE_XWD_64,
    BAT_TYPE_DS_65,
    BAT_TYPE_XX_66,
    BAT_TYPE_AAC_67,
    BAT_TYPE_GY_68,
    BAT_TYPE_DS_69,
    BAT_TYPE_XWD_6A
};


static struct bq2022a_data *g_bq2022a = NULL;
static DEFINE_MUTEX(bq2022a_mutex);


/*Creates the Reset signal to initiate SDQ communication*/
/******************************************************************************
Description : Detects if a device responds to Reset signal
Arguments : PresenceTimer - Sets timeout if no device present
InputData - Actual state of GPIO
GotPulse - States if a pulse was detected
Returns : GotPulse
******************************************************************************/
static int bq2022a_sdq_reset_and_detect(struct bq2022a_data *data)
{
    int PresenceTimer = 300;
    static volatile u8 InputData;
    static volatile u8 GotPulse = 0;
    /*SDQ_HIGH;
    SDQ_OUTPUT;
    SDQ_LOW;
    udelay(500);
    SDQ_HIGH;*/
    //gpio_set_value(data->sdq_pin, 1);
    gpio_direction_output(data->sdq_pin, 0);
    gpio_set_value(data->sdq_pin, 0);
    /* Reset time should be > 480 usec */
    udelay(800);
    /*SDQ_INPUT;*/
    gpio_direction_input(data->sdq_pin);
    udelay(60);
    while ((PresenceTimer > 0) && (GotPulse == 0)) {
        InputData = gpio_get_value(data->sdq_pin);  //Capture state of the SDQ bus
        if (InputData == 0) {   // If SDQ bus got pulled low
            GotPulse = 1;   // then the device responded
        } else {            // If SDQ bus is still high
            GotPulse = 0;   // then the device did not respond
            --PresenceTimer;    // Decrease timeout counter

        }
    }
    udelay(200);    // Delay before attempting command
    if(!GotPulse){
        pr_err("bq2022a: !!!sdq is not responded !!!\n");
    }
    return GotPulse;    // Return if device detected or not

}

/******************************************************************************
unsigned char SDQ_readByte(void)
Description : Reads 8 bits on the SDQ line and returns the byte value.
Arguments : Databyte - Byte value returned by SDQ slave
MaskByte - Used to seperate each bit
i - Used for 8 time loop
Returns : DataByte
******************************************************************************/
static int bq2022a_sdq_read_byte_data(struct bq2022a_data *data)
{
    u8 result = 0x00;
    u8 mask = 0, i = 0;
    unsigned long flags;

    //_DINT();//disable the irq
    spin_lock_irqsave(&data->bqlock, flags);
    //_DINT();  //disable the irq

    for (i = 0; i < 8; i++) {   // Select one bit at a time
        gpio_direction_output(data->sdq_pin, 0);/*SDQ_OUTPUT LOW*/
        udelay(7);
        gpio_direction_input(data->sdq_pin);/*SDQ_INPUT;*/
        udelay(7);
        mask = gpio_get_value(data->sdq_pin);/*SDQ_READ, Capture state of the SDQ bus*/
        udelay(65);// 65 Wait for end of read bit cycle

        //gpio_set_value(data->sdq_pin, 1);/*SDQ_HIGH;*/
        mask <<= i;                 // Determine bit position in byte
        result = (result | mask); // Keep adding bits to form the byte
    }
    udelay(200);    // 200us Delay before attempting command

    //_EINT();
    spin_unlock_irqrestore(&data->bqlock, flags);
    return result;  // Return byte value read
}

static int bq2022a_sdq_write_byte_data(struct bq2022a_data *data,u8 cmd)
{
    unsigned char mask = 1;
    int i;
    unsigned long flags;

    //_DINT();
    spin_lock_irqsave(&data->bqlock, flags);
    //SDQ_HIGH;
    //SDQ_OUTPUT;
    //SDQ_LOW;
    //gpio_set_value(data->sdq_pin, 1);/*SDQ_HIGH;*/
    gpio_direction_output(data->sdq_pin, 1);/*SDQ_OUTPUT*/

    for (i = 0; i < 8; i++) {
        gpio_set_value(data->sdq_pin, 0);/*SDQ_LOW*/
        udelay(4);
        if (mask & cmd) {
            udelay(7);
            //SDQ_HIGH;
            gpio_set_value(data->sdq_pin, 1);
            udelay(100);
        } else {
            udelay(100);
            //SDQ_HIGH;
            gpio_set_value(data->sdq_pin, 1);
            udelay(7);
        }
        udelay(7);
        mask <<= 1;
    }

    //_EINT();
    spin_unlock_irqrestore(&data->bqlock, flags);

    return 0;
}

/*read/match/search/skip rom*/
static int bq2022a_init_rom_command(struct bq2022a_data *data,u8 cmd)
{
    int ret = 0,i = 0;

    /*reset and presence pulse*/
    for (i = 0; i < 10; i++) {
        ret = bq2022a_sdq_reset_and_detect(data);
        if(ret){
            break;
        }
    }

    /*set ROM Command*/
    bq2022a_sdq_write_byte_data(data,cmd);

    return ret;
}

static void bq2022a_read_memory_status_command(struct bq2022a_data *data,u8 cmd)
{

    /*send MEMORY or status cmd*/
    bq2022a_sdq_write_byte_data(data,cmd);
    /*Write Address Low Byte*/
    bq2022a_sdq_write_byte_data(data,LOW_BYTE);
    /*Write Address High Byte*/
    bq2022a_sdq_write_byte_data(data,HIGH_BYTE);

    return;
}

static const unsigned char con_bat_id[] = {
    [0] = 0xed, [1] = 0x21, [2] = 0x4c, [3] = 0xe5,
    [4] = 0xed, [5] = 0xa9, [6] = 0x4b, [7] = 0x2e,
};

int bq2022a_read_bat_id_module(void)
{
    struct bq2022a_data *data = g_bq2022a;
    u8 val = 0,i = 0;
    int memory_status = 0;

    mutex_lock(&bq2022a_mutex);

    /* Skip ROM Command*/
    bq2022a_init_rom_command(data,SKIP_ROM);
    /*READ MEMORY/Page CRC*/
    bq2022a_read_memory_status_command(data,READ_MEMORY_PAGE);

    /*Read CRC, one page, one CRC, one page is 32 bytes*/
    val = bq2022a_sdq_read_byte_data(data);

    for (i = 0; i < 8; i++) {
        /*Read Data to Verify Write 8 time*/
        val = bq2022a_sdq_read_byte_data(data);
        if (val != con_bat_id[i]){
            data->bat_id_present = -ENODEV;
            pr_err("bq2022a: sends eight read time slots[%d]  = 0x%x\n",i,val);
            data->batttery_type = BAT_ID_NONE;
            gpio_direction_output(data->sdq_pin, 1);
            goto exit;
        }
    }
    data->bat_id_present = 0;

    /*Read CRC, one page, one CRC, one page is 32 bytes*/
    memory_status = bq2022a_sdq_read_byte_data(data);
    gpio_direction_output(data->sdq_pin, 1);

    switch(memory_status){
    case BAT_TYPE_XWD_61:
        data->batttery_type = BAT_ID_1;
        break;
    case BAT_TYPE_GY_62:
        data->batttery_type = BAT_ID_2;
        break;
    case BAT_TYPE_XWD_63:
        data->batttery_type = BAT_ID_3;
        break;
    case BAT_TYPE_XWD_64:
        data->batttery_type = BAT_ID_4;
        break;
    case BAT_TYPE_DS_65:
        data->batttery_type = BAT_ID_5;
        break;
    case BAT_TYPE_XX_66:
        data->batttery_type = BAT_ID_6;
        break;
    case BAT_TYPE_AAC_67:
        data->batttery_type = BAT_ID_7;
        break;
    case BAT_TYPE_GY_68:
        data->batttery_type = BAT_ID_8;
        break;
    case BAT_TYPE_DS_69:
        data->batttery_type = BAT_ID_9;
        break;
    case BAT_TYPE_XWD_6A:
        data->batttery_type = BAT_ID_10;
        break;
    default:
        data->batttery_type = BAT_ID_NONE;
        pr_err("bq2022a: !!!batttery_type = 0x%x \n",memory_status);
        break;
    }

    mutex_unlock(&bq2022a_mutex);
    return data->batttery_type;

exit:
    mutex_unlock(&bq2022a_mutex);
    return -ENODEV;
}

int bq2022a_read_bat_id(void)
{
    int ret = 0,i = 0;
    struct bq2022a_data *data = g_bq2022a;

    if(data->bat_id_invalid) {
         return 0;
    }

    if(g_bq2022a == NULL) {
        printk("bq2022a_read_bat_id: g_bq2022a is null");
        return -ENODEV;
    }

    if(data->bat_id_present == 0){
        return data->bat_id_present;
    }

    ret = bq2022a_read_bat_id_module();
    if(ret < 0){
        pr_err("bq2022a: read family code Error and Need read again\n");
        msleep(10);
        for(i = 0; i< 2; i++){
            ret = bq2022a_read_bat_id_module();
            if(ret >= 0){
                break;
            }
        }
    }

    return data->bat_id_present;
}
EXPORT_SYMBOL_GPL(bq2022a_read_bat_id);

int comip_battery_id_type(void)
{
    struct bq2022a_data *data = g_bq2022a;
    int ret = 0,i = 0;

    if(g_bq2022a == NULL || data->bat_id_invalid) {
        data->batttery_type = BAT_ID_NONE;
        goto exit;
    }
    if(data->batttery_type != BAT_ID_NONE){
        goto exit;
    }
    ret = bq2022a_read_bat_id_module();
    if(ret < 0){
        pr_err("bq2022a: repeat read family code\n");
        msleep(400);
        for(i = 0; i< 2; i++){
            ret = bq2022a_read_bat_id_module();
            if(ret >= 0){
                break;
            }
        }
    }

exit:
    return data->batttery_type;
}
EXPORT_SYMBOL_GPL(comip_battery_id_type);


/*sysfs interface*/
static ssize_t bq2022a_show_battery_name(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    enum battery_id_type val = 0;
    const char* bat_name;
    int ret = 0;

    ret = bq2022a_read_bat_id_module();
    if(ret >= 0){
        val = ret;
    }
    bat_name = comip_battery_name(val);
    return sprintf(buf, "%s\n", bat_name);
}
static DEVICE_ATTR(bat_name, S_IWUSR | S_IRUGO, bq2022a_show_battery_name, NULL);

static struct attribute *bq2022a_attributes[] = {
    &dev_attr_bat_name.attr,
    NULL,
};

static const struct attribute_group bq2022a_attr_group = {
    .attrs = bq2022a_attributes,
};

static int __init bq2022a_probe(struct platform_device *pdev)
{
    int ret;
    struct bq2022a_platform_data *pdata = pdev->dev.platform_data;
    struct bq2022a_data *data;

    data = (struct bq2022a_data *)
            kzalloc(sizeof(struct bq2022a_data), GFP_KERNEL);
    if (data == NULL)
        return -ENOMEM;

    if(cpu_is_lc1860_eco1()||cpu_is_lc1860_eco2()){
        data->sdq_pin = pdata->sdq_gpio;
    }else{
        data->sdq_pin = pdata->sdq_pin;
    }
    if(pdata->bat_id_invalid) {
        data->bat_id_invalid = pdata->bat_id_invalid();
    }
    g_bq2022a = data;
    data->bat_id_present = -1;
    data->batttery_type = 0;

    ret = gpio_request(data->sdq_pin, "data->sdq_pin");
    if (ret){
        dev_err(&pdev->dev,"request bq2022a_gpio failed, %d\n", data->sdq_pin);
    }
    gpio_direction_output(data->sdq_pin, 1);

    spin_lock_init(&data->bqlock);

    ret = sysfs_create_group(&pdev->dev.kobj, &bq2022a_attr_group);
    if (ret) {
        dev_err(&pdev->dev, "could not create sysfs files\n");
    }

    dev_info(&pdev->dev, "bq2022a_probe finished\n");

    return 0;
}


static int __exit bq2022a_remove(struct platform_device *pdev)
{
    struct bq2022a_platform_data *pdata = pdev->dev.platform_data;
    gpio_free(pdata->sdq_pin);
    return 0;
}

static struct platform_driver bq2022a_driver = {
    .driver = {
        .name = "bq2022a",
        .owner = THIS_MODULE,
    },
    .remove = __exit_p(bq2022a_remove),
};
static int __init bq2022a_init(void)
{
    return platform_driver_probe(&bq2022a_driver, bq2022a_probe);
}

static void __exit bq2022a_exit(void)
{
    return platform_driver_unregister(&bq2022a_driver);
}


fs_initcall(bq2022a_init);
module_exit(bq2022a_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("bq2022a-batid driver");
