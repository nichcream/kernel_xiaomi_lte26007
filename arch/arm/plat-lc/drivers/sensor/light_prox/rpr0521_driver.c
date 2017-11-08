/* drivers/i2c/chips/rpr521_driver.c - ROHM RPR521 Linux kernel driver
 *
 * Copyright (C) 2012
 * Written by Andy Mi <andy-mi@rohm.com.cn>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
/*
 *  This is Linux kernel modules for ambient light + proximity sensor
 *  Revision History
 *  2012-7-19:  Ver. 1.0        New release together with a porting guide.
 *  2012-8-14:  Ver. 1.1        Added calibration and set thresholds methods. Besides, the thresholds are automatically changed if a ps int is triggered to avoid constant interrupts.
 *  2014-1-09:  Ver. 1.2        Modified some functions for rpr521
 */
#include <linux/debugfs.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <plat/rpr0521_driver.h>


#define CONFIG_ALSPS_COMIP_DEBUG
#ifdef CONFIG_ALSPS_COMIP_DEBUG
#define ALSPS_PRINT1(fmt,args...)	printk(KERN_INFO fmt, ##args)
#define ALSPS_PRINT2(fmt,args...)
#define ALSPS_PRINT3(fmt,args...)
#else
#define ALSPS_PRINT1(fmt,args...)	printk(KERN_DEBUG fmt, ##args)
#define ALSPS_PRINT2(fmt,args...)
#define ALSPS_PRINT3(fmt,args...)
#endif

/*************** Global Data ******************/
/* parameter for als calculation */
#define COEFFICIENT               (4)
const unsigned long data0_coefficient[COEFFICIENT] = {839, 935, 666, 405};
const unsigned long data1_coefficient[COEFFICIENT] = {0, 520,  278,  144};
const unsigned long judge_coefficient[COEFFICIENT] = { 184,  1107,  1955, 2810};

/* mode control table */
#define MODE_CTL_FACTOR (16)
static const struct MCTL_TABLE {
    short ALS;
    short PS;
} mode_table[MODE_CTL_FACTOR] = {
    {  0,   0},   /*  0 */
    {  0,  10},   /*  1 */
    {  0,  40},   /*  2 */
    {  0, 100},   /*  3 */
    {  0, 400},   /*  4 */
    {100,  50},   /*  5 */
    {100, 100},   /*  6 */
    {100, 400},   /*  7 */
    {400,   0},   /*  8 */
    {400, 100},   /*  9 */
    {400,   0},   /* 10 */
    {400, 400},   /* 11 */
    {  50,  50},   /* 12 */
    {  0,   0},   /* 13 */
    {  0,   0},   /* 14 */
    {  0,   0}    /* 15 */
};

/* gain table */
#define GAIN_FACTOR (16)
static const struct GAIN_TABLE {
    unsigned char DATA0; /*grace modify in 2014.4.21*/
    unsigned char DATA1; /*grace modify in 2014.4.21*/
} gain_table[GAIN_FACTOR] = {
    {  1,   1},   /*  0 */
    {  0,   0},   /*  1 */
    {  0,   0},   /*  2 */
    {  0,   0},   /*  3 */
    {  2,   1},   /*  4 */
    {  2,   2},   /*  5 */
    {  0,   0},   /*  6 */
    {  0,   0},   /*  7 */
    {  0,   0},   /*  8 */
    {  0,   0},   /*  9 */
    { 64,  64},   /* 10 */
    {  0,   0},   /* 11 */
    {  0,   0},   /* 12 */
    {  0,   0},   /* 13 */
    {128,  64},   /* 14 */
    {128, 128}    /* 15 */
};

/*************** Functions ******************/
/******************************************************************************
 * NAME       : rpr521_set_enable
 * FUNCTION   : set measurement time according to enable
 * REMARKS    : this function will overwrite the work mode. if it is called improperly,
 *                         you may shutdown some part unexpectedly. please check als_ps->enable first.
 *                         I assume it is run in normal mode. If you want low noise mode, the code should be modified.
 *******************************************************************************
 */
static int rpr521_set_enable(struct i2c_client *client, int enable)
{
    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);
    int ret;
    if(enable > 0xFb) {
        printk(KERN_ERR "%s: invalid measurement time setting.\n", __func__);
        return -EINVAL;
    } else {
        //mutex_lock(&als_ps->update_lock);
        ret = i2c_smbus_write_byte_data(client, REG_MODECONTROL, enable);
        //mutex_unlock(&als_ps->update_lock);

        als_ps->enable = enable;
        als_ps->als_time = mode_table[(enable & 0xF)].ALS;
        als_ps->ps_time = mode_table[(enable & 0xF)].PS;

        return ret;
    }
}

static int rpr521_set_ps_threshold_low(struct i2c_client *client, int threshold)
{
    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);
    int ret;
    unsigned short write_data;

    /* check whether the parameter is valid */
    if(threshold > REG_PSTL_MAX) {
        printk(KERN_ERR "%s: exceed maximum possible value.\n", __func__);
        return -EINVAL;
    }

    /* write register to rpr521 via i2c */
    write_data = CONVERT_TO_BE(threshold);
    mutex_lock(&als_ps->update_lock);
    ret = i2c_smbus_write_i2c_block_data(client, REG_PSTL, sizeof(write_data), (unsigned char *)&write_data);
    mutex_unlock(&als_ps->update_lock);
    if(ret < 0) {
        printk(KERN_ERR "%s: write i2c fail.\n", __func__);
        return ret;
    }
    als_ps->ps_th_l = threshold;    /*Update the value after successful i2c write to avoid difference.*/

    return 0;
}

static int rpr521_set_ps_threshold_high(struct i2c_client *client, int threshold)
{
    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);
    int ret;
    unsigned short write_data;

    /* check whether the parameter is valid */
    if(threshold > REG_PSTH_MAX) {
        printk(KERN_ERR "%s: exceed maximum possible value.\n", __func__);
        return -EINVAL;
    }
    /* write register to rpr521 via i2c */
    write_data = CONVERT_TO_BE(threshold);
    mutex_lock(&als_ps->update_lock);
    ret = i2c_smbus_write_i2c_block_data(client, REG_PSTH, sizeof(write_data), (unsigned char *)&write_data);
    mutex_unlock(&als_ps->update_lock);
    if(ret < 0) {
        printk(KERN_ERR "%s: write i2c fail.\n", __func__);
        return ret;
    }
    als_ps->ps_th_h = threshold;    /*Update the value after successful i2c write to avoid difference.*/

    return 0;
}

static int rpr521_calibrate(struct i2c_client *client)
{
    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);
    int average;
    unsigned int i, tmp, ps_th_h, ps_th_l;

    average = 0;
    rpr521_set_enable(client, PS_EN|PS10MS);        /*grace modify in 2014.5.6*/

    for(i = 0; i < 20; i ++) {
        mdelay(20); /*grace modify in 2014.5.6*/
        tmp = i2c_smbus_read_word_data(client, REG_PSDATA);
        if(tmp < 0) {
            printk(KERN_ERR "%s: i2c read ps data fail. \n", __func__);
            goto err_exit;
        }
        average += tmp & 0xFFF; /* 12 bit data*/
    }
    average /= 20;
    ps_th_h = average + THRES_TOLERANCE + THRES_DEFAULT_DIFF;
    ps_th_l = average + THRES_TOLERANCE;

    if(ps_th_h < 0) {
        printk(KERN_ERR "%s: high threshold is less than 0.\n", __func__);
        goto err_exit;
    }
    if(ps_th_h > REG_PSTH_MAX) {
        printk(KERN_ERR "%s: high threshold is greater than maximum allowed value.\n", __func__);
        goto err_exit;
    }
    if(ps_th_l < 0) {
        printk(KERN_ERR "%s: low threshold is less than 0.\n", __func__);
        goto err_exit;
    }
    if(ps_th_l > REG_PSTL_MAX) {
        printk(KERN_ERR "%s: low threshold is greater than maximum allowed value.\n", __func__);
        goto err_exit;
    }

    if(!(rpr521_set_ps_threshold_high(client, ps_th_h)))
        als_ps->ps_th_h_back = ps_th_h;
    else
        goto err_exit;
    if(!(rpr521_set_ps_threshold_low(client, ps_th_l)))
        als_ps->ps_th_l_back = ps_th_l;
    else
        goto err_exit;
    rpr521_set_enable(client, PS_ALS_SET_MODE_CONTROL);     /*grace modify in 2014.5.6*/
    return 0;

err_exit:
    rpr521_set_enable(client, PS_ALS_SET_MODE_CONTROL);     /*grace modify in 2014.5.6*/
    return -1;

}

#if _FUNCTION_USED_     /*masked because they are claimed but not used, which may cause error when compilling if the warning level is high enough. These functions provides some methods.*/
static int rpr521_set_persist(struct i2c_client *client, int persist)
{
    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);
    int ret;

    /* check whether the parameter is valid */
    if(persist > PERSISTENCE_MAX) {
        printk(KERN_ERR "%s: exceed maximum possible value.\n", __func__);
        return -EINVAL;
    }

    /* write register to rpr521 via i2c */
    mutex_lock(&als_ps->update_lock);
    ret = i2c_smbus_write_byte_data(client, REG_PERSISTENCE, persist);
    mutex_unlock(&als_ps->update_lock);
    if(ret < 0) {
        printk(KERN_ERR "%s: write i2c fail.\n", __func__);
        return ret;
    }
    als_ps->persistence = persist;  /*Update the value after successful i2c write to avoid difference.*/

    return 0;
}

static int rpr521_set_control(struct i2c_client *client, int control)
{
    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);
    int ret;
    unsigned char gain, led_current;

    if(control > REG_ALSPSCTL_MAX) {
        printk(KERN_ERR "%s: exceed maximum possible value.\n", __func__);
        return -EINVAL;
    }

    gain = (control & 0x3C) >> 2;   /*gain setting values*/
    led_current = control & 0x03;           /*led current setting value*/

    if(!((gain == ALSGAIN_X1X1) || (gain == ALSGAIN_X1X2) || (gain == ALSGAIN_X2X2) || (gain == ALSGAIN_X64X64)
            || (gain == ALSGAIN_X128X64) || (gain == ALSGAIN_X128X128))) {
        printk(KERN_ERR "%s: invalid gain setting. \n", __func__);
        return -EINVAL;
    }

    mutex_lock(&als_ps->update_lock);
    ret = i2c_smbus_write_byte_data(client, REG_ALSPSCONTROL, control);
    mutex_unlock(&als_ps->update_lock);
    if(ret < 0) {
        printk(KERN_ERR "%s: write i2c fail.\n", __func__);
        return ret;
    }
    als_ps->control = control;
    als_ps->gain0 = gain_table[gain].DATA0;
    als_ps->gain1 = gain_table[gain].DATA1;
    als_ps->ledcurrent = led_current;

    return ret;
}
#endif

/******************************************************************************
 * NAME       : long_long_divider
 * FUNCTION   : calc divider of unsigned long long int or unsgined long
 * REMARKS    :
 ******************************************************************************
 */
static int long_long_divider(long long data, unsigned long base_divier, unsigned long *answer, unsigned long long *overplus)
{
    volatile long long divier;
    volatile long      unit_sft;

    if ((data < 0) || (base_divier == 0)) {
        *answer   = 0;
        *overplus = 0;
        return (CALC_ERROR);
    }

    divier = base_divier;
    if (data > MASK_LONG) {
        unit_sft = 0;
        while ((data > divier) && (divier > 0)) {
            unit_sft++;
            divier = divier << 1;
        }
        while ((data > base_divier) && (unit_sft > 0)) {
            if (data > divier) {
                *answer += 1 << unit_sft;
                data    -= divier;
            }
            unit_sft--;
            divier = divier >> 1;
        }
        *overplus = data;
    } else {
        *answer = (unsigned long)(data & MASK_LONG) / base_divier;
        /* calculate over plus and shift 16bit */
        *overplus = (unsigned long long)(data - (*answer * base_divier));
    }

    return (0);
}

/******************************************************************************
 * NAME       : calc_rohm_als_data
 * FUNCTION   : calculate illuminance data for rpr521
 * REMARKS    : final_data is 1000 times, which is defined as CUT_UNIT, of the actual lux value
 ******************************************************************************
 */
static int calc_rohm_als_data(unsigned short data0, unsigned short data1, unsigned char gain0, unsigned char gain1, unsigned short time)
{
#define DECIMAL_BIT      (15)
#define JUDGE_FIXED_COEF (1000)
#define MAX_OUTRANGE     (65535)
#define MAXRANGE_NMODE   (0xFFFF)
#define MAXSET_CASE      (4)

    int                result; /*grace modified in 2014.4.2*/
    int                final_data;
    CALC_DATA          calc_data;
    CALC_ANS           calc_ans;
    unsigned long      calc_judge;
    unsigned char      set_case;
    unsigned long      div_answer;
    unsigned long long div_overplus;
    unsigned long long overplus;
    unsigned long      max_range;

    /* set the value of measured als data */
    calc_data.als_data0  = data0;
    calc_data.als_data1  = data1;
    calc_data.gain_data0 = gain0;

    /* set max range */
    if (calc_data.gain_data0 == 0) {
        /* issue error value when gain is 0 */
        return (CALC_ERROR);
    } else {
        max_range = MAX_OUTRANGE / calc_data.gain_data0;
    }

    /* calculate data */
    if (calc_data.als_data0 == MAXRANGE_NMODE) {
        calc_ans.positive = max_range;
        calc_ans.decimal  = 0;
    } else {
        /* get the value which is measured from power table */
        calc_data.als_time = time;
        if (calc_data.als_time == 0) {
            /* issue error value when time is 0 */
            return (CALC_ERROR);
        }

        calc_judge = calc_data.als_data1 * JUDGE_FIXED_COEF;
        if (calc_judge < (calc_data.als_data0 * judge_coefficient[0])) {
            set_case = 0;
        } else if (calc_judge < (calc_data.als_data0 * judge_coefficient[1])) {
            set_case = 1;
        } else if (calc_judge < (calc_data.als_data0 * judge_coefficient[2])) {
            set_case = 2;
        } else if (calc_judge < (calc_data.als_data0 * judge_coefficient[3])) {
            set_case = 3;
        } else {
            set_case = MAXSET_CASE;
        }
        calc_ans.positive = 0;
        if (set_case >= MAXSET_CASE) {
            calc_ans.decimal = 0;   /*which means that lux output is 0*/
        } else {
            calc_data.gain_data1 = gain1;
            if (calc_data.gain_data1 == 0) {
                /* issue error value when gain is 0 */
                return (CALC_ERROR);
            }
            calc_data.data0      = (unsigned long long )(data0_coefficient[set_case] * calc_data.als_data0) * calc_data.gain_data1;
            calc_data.data1      = (unsigned long long )(data1_coefficient[set_case] * calc_data.als_data1) * calc_data.gain_data0;
            if(calc_data.data0 < calc_data.data1) { /*In this case, data will be less than 0. As data is unsigned long long, it will become extremely big.*/
                return (CALC_ERROR);
            } else {
                calc_data.data       = (calc_data.data0 - calc_data.data1);
            }
            calc_data.dev_unit   = calc_data.gain_data0 * calc_data.gain_data1 * calc_data.als_time * 10;   /*24 bit at max (128 * 128 * 100 * 10)*/
            if (calc_data.dev_unit == 0) {
                /* issue error value when dev_unit is 0 */
                return (CALC_ERROR);
            }

            /* calculate a positive number */
            div_answer   = 0;
            div_overplus = 0;
            result = long_long_divider(calc_data.data, calc_data.dev_unit, &div_answer, &div_overplus);
            if (result == CALC_ERROR) {
                return (result);
            }
            calc_ans.positive = div_answer;
            /* calculate a decimal number */
            calc_ans.decimal = 0;
            overplus         = div_overplus;
            if (calc_ans.positive < max_range) {
                if (overplus != 0) {
                    overplus     = overplus << DECIMAL_BIT;
                    div_answer   = 0;
                    div_overplus = 0;
                    result = long_long_divider(overplus, calc_data.dev_unit, &div_answer, &div_overplus);
                    if (result == CALC_ERROR) {
                        return (result);
                    }
                    calc_ans.decimal = div_answer;
                }
            }

            else {
                calc_ans.positive = max_range;
            }
        }
    }

    final_data = calc_ans.positive * CUT_UNIT + ((calc_ans.decimal * CUT_UNIT) >> DECIMAL_BIT);

    return (final_data);

#undef DECIMAL_BIT
#undef JUDGE_FIXED_COEF
#undef MAX_OUTRANGE
#undef MAXRANGE_NMODE
#undef MAXSET_CASE
}


/******************************************************************************
 * NAME       : calculate_ps_data
 * FUNCTION   : calculate proximity data for rpr521
 * REMARKS    : 12 bit output
 ******************************************************************************
 */
static int calc_rohm_ps_data(unsigned short ps_reg_data)
{
    return (ps_reg_data & 0xFFF);
}

static unsigned int rpr521_als_data_to_level(unsigned int als_data)
{
#if 0
#define ALS_LEVEL_NUM 15
    int als_level[ALS_LEVEL_NUM]  = { 0, 50, 100, 150,  200,  250,  300, 350, 400,  450,  550, 650, 750, 900, 1100};
    int als_value[ALS_LEVEL_NUM]  = { 0, 50, 100, 150,  200,  250,  300, 350, 400,  450,  550, 650, 750, 900, 1100};
    unsigned char idx;

    for(idx = 0; idx < ALS_LEVEL_NUM; idx ++) {
        if(als_data < als_value[idx]) {
            break;
        }
    }
    if(idx >= ALS_LEVEL_NUM) {
        printk(KERN_ERR "rpr521 als data to level: exceed range.\n");
        idx = ALS_LEVEL_NUM - 1;
    }

    return als_level[idx];
#undef ALS_LEVEL_NUM
#else
    return als_data;
#endif
}

static void rpr521_reschedule_work(struct ALS_PS_DATA *als_ps,
                                   unsigned long delay)
{
    unsigned long flags;

    spin_lock_irqsave(&als_ps->update_lock.wait_lock, flags);

    /*
          * If work is already scheduled then subsequent schedules will not
          * change the scheduled time that's why we have to cancel it first.
          */
    __cancel_delayed_work(&als_ps->dwork);
    schedule_delayed_work(&als_ps->dwork, delay);

    spin_unlock_irqrestore(&als_ps->update_lock.wait_lock, flags);
}

/* ALS polling routine */
static void rpr521_als_polling_work_handler(struct work_struct *work)
{
    struct ALS_PS_DATA *als_ps = container_of(work, struct ALS_PS_DATA, als_dwork.work);
    struct i2c_client *client=als_ps->client;
    int tmp = 0;

    schedule_delayed_work(&als_ps->als_dwork, msecs_to_jiffies(als_ps->als_poll_delay));    /* restart timer*/

    tmp = i2c_smbus_read_word_data(client, REG_ALSDATA0);
    if(tmp < 0) {
        printk(KERN_ERR "%s: i2c read data0 fail. \n", __func__);
    }
    als_ps->als_data0_raw = (unsigned short)tmp;
    tmp = i2c_smbus_read_word_data(client, REG_ALSDATA1);
    if(tmp < 0) {
        printk(KERN_ERR "%s: i2c read data1 fail. \n", __func__);
    }
    als_ps->als_data1_raw = (unsigned short)tmp;

    /*Theorically it is not necesssary to do so, but I just want to avoid any potential error.  -- Andy 2012.6.6*/
    tmp = i2c_smbus_read_byte_data(client, REG_ALSPSCONTROL);
    if(tmp < 0) {
        printk(KERN_ERR "%s: i2c read gain fail. \n", __func__);
    }
    tmp = (tmp & 0x3C) >> 2;
    als_ps->gain0 = gain_table[tmp].DATA0;
    als_ps->gain1 = gain_table[tmp].DATA1;

    tmp = i2c_smbus_read_byte_data(client, REG_MODECONTROL);
    ALSPS_PRINT2("REG_MODECONTROL=%x\n", tmp);

    if(tmp < 0) {
        printk(KERN_ERR "%s: i2c read time fail. \n", __func__);
    }
    tmp = tmp & 0xF;
    als_ps->als_time = mode_table[tmp].ALS;

    als_ps->als_data = calc_rohm_als_data(als_ps->als_data0_raw, als_ps->als_data1_raw, als_ps->gain0, als_ps->gain1, als_ps->als_time);
    if(als_ps->als_data == 0)
        als_ps->als_data ++;

    als_ps->als_level = rpr521_als_data_to_level(als_ps->als_data);

    ALSPS_PRINT2("rpr521 als report: data0 = %d, data1 = %d, gain0 = %d, gain1 = %d, time = %d, lux = %d, level = %d.\n", als_ps->als_data0_raw,
                 als_ps->als_data1_raw, als_ps->gain0, als_ps->gain1, als_ps->als_time, als_ps->als_data, als_ps->als_level);

    tmp = i2c_smbus_read_word_data(client, REG_PSDATA);
    ALSPS_PRINT2("REG_PSDATA=%x\n", tmp);
    if(als_ps->als_data != CALC_ERROR) {
        input_report_abs(als_ps->input_dev_als, ABS_MISC, als_ps->als_level); /* report als data. maybe necessary to convert to lux level*/
        input_sync(als_ps->input_dev_als);
    }
}


/* PS interrupt routine */
static void rpr521_ps_int_work_handler(struct work_struct *work)
{
    struct ALS_PS_DATA *als_ps = container_of((struct delayed_work *)work, struct ALS_PS_DATA, dwork);
    struct i2c_client *client=als_ps->client;
    int tmp;

    ALSPS_PRINT2("rpr521_ps_int_work_handler\n");

    tmp =  i2c_smbus_read_byte_data(client, REG_INTERRUPT);
    if(tmp < 0) {
        printk(KERN_ERR "%s: i2c read interrupt status fail. \n", __func__);
        return;
    }
    /*Interrupt is caused by PS*/
    if(tmp & PS_INT_MASK) {
        tmp = i2c_smbus_read_byte_data(client, REG_ALSPSCONTROL);
        if(tmp < 0) {
            printk(KERN_ERR "%s: i2c read led current fail. \n", __func__);
            goto err_exit;
        }
        als_ps->ledcurrent = tmp & 0x3;

        tmp = i2c_smbus_read_word_data(client, REG_PSDATA);
        if(tmp < 0) {
            printk(KERN_ERR "%s: i2c read ps data fail. \n", __func__);
            goto err_exit;
        }
        als_ps->ps_data_raw = (unsigned short)tmp;

        als_ps->ps_data = calc_rohm_ps_data(als_ps->ps_data_raw);

        if(als_ps->ps_data > als_ps->ps_th_h) {
            tmp = i2c_smbus_read_byte_data(client, REG_PERSISTENCE);
            if(tmp < 0) {
                printk(KERN_ERR "%s: i2c read led current fail. \n", __func__);
                goto err_exit;
            }
            if ((tmp>>6) == INFRARED_LOW) {
                als_ps->ps_direction = 0;
#ifdef _AUTO_THRESHOLD_CHANGE_
                rpr521_set_ps_threshold_high(client, REG_PSTH_MAX);
                rpr521_set_ps_threshold_low(client, als_ps->ps_th_l_back);
#endif
            } else {
                goto err_exit;
            }
        } else if(als_ps->ps_data < als_ps->ps_th_l) {
            als_ps->ps_direction = 1;
#ifdef _AUTO_THRESHOLD_CHANGE_
            rpr521_set_ps_threshold_high(client, als_ps->ps_th_h_back);
            rpr521_set_ps_threshold_low(client, 0);
#endif
        }

        ALSPS_PRINT2("rpr521 ps report: raw_data = %d, data = %d, direction = %d. \n",
                     als_ps->ps_data_raw, als_ps->ps_data, als_ps->ps_direction);

        input_report_abs(als_ps->input_dev_ps, ABS_DISTANCE, als_ps->ps_direction);
        input_sync(als_ps->input_dev_ps);
    } else {
        printk(KERN_ERR "%s: unknown interrupt source.\n", __func__);
    }

    //enable_irq(client->irq);
err_exit:
    enable_irq(client->irq);
}

/* assume this is ISR */
static irqreturn_t rpr521_interrupt(int vec, void *info)
{
    struct i2c_client *client=(struct i2c_client *)info;
    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);
    disable_irq_nosync(client->irq);
    ALSPS_PRINT2("%s\n", __func__);
    rpr521_reschedule_work(als_ps, 0);

    return IRQ_HANDLED;
}

/*************** SysFS Support ******************/
static ssize_t rpr521_show_enable_ps_sensor(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);

    return sprintf(buf, "%d\n", als_ps->enable_ps_sensor);
}

static ssize_t rpr521_store_enable_ps_sensor(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);
    struct i2c_client *client = als_ps->client;

    unsigned long val = simple_strtoul(buf, NULL, 10);
    int tmp;

    printk(KERN_INFO "rpr521 enable PS sensor -> %ld \n", val);

    if ((val != 0) && (val != 1)) {
        printk("%s:store unvalid value=%ld\n", __func__, val);
        return count;
    }

    if(val == 1) {
        /*turn on p sensor*/
        if (als_ps->enable_ps_sensor == 0) {
            als_ps->enable_ps_sensor = 1;

            tmp = i2c_smbus_read_byte_data(client, REG_MODECONTROL);
            ALSPS_PRINT2("%s: tmp=%x\n", __func__, tmp);
            tmp = tmp | 0x40;
            rpr521_set_enable(client, tmp); /*PS on and ALS off*/
        }
    } else {
        if(als_ps->enable_ps_sensor == 1) {
            als_ps->enable_ps_sensor = 0;

            tmp = i2c_smbus_read_byte_data(client, REG_MODECONTROL);
            tmp = tmp & (~0x40);
            rpr521_set_enable(client, tmp); /*PS on and ALS off*/
        }
    }
    return count;
}

static ssize_t rpr521_show_enable_als_sensor(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);


    return sprintf(buf, "%d\n", als_ps->enable_als_sensor);
}

static ssize_t rpr521_store_enable_als_sensor(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);
    struct i2c_client *client = als_ps->client;

    unsigned long val = simple_strtoul(buf, NULL, 10);
    unsigned long flags;
    int tmp;

    printk(KERN_INFO "rpr521 enable ALS sensor -> %ld\n", val);

    if ((val != 0) && (val != 1)) {
        ALSPS_PRINT2("%s: enable als sensor=%ld\n", __func__, val);
        return count;
    }

    if(val == 1) {
        /*turn on light  sensor*/
        if (als_ps->enable_als_sensor==0) {
            als_ps->enable_als_sensor = 1;

            tmp = i2c_smbus_read_byte_data(client, REG_MODECONTROL);
            ALSPS_PRINT2("%s: tmp=%x\n", __func__, tmp);
            tmp = tmp | 0x80;
            rpr521_set_enable(client, tmp); /*PS on and ALS off*/
        }

        spin_lock_irqsave(&als_ps->update_lock.wait_lock, flags);
        __cancel_delayed_work(&als_ps->als_dwork);
        schedule_delayed_work(&als_ps->als_dwork, msecs_to_jiffies(als_ps->als_poll_delay));    /* 125ms*/
        spin_unlock_irqrestore(&als_ps->update_lock.wait_lock, flags);
    } else {
        if(als_ps->enable_als_sensor == 1) {
            als_ps->enable_als_sensor = 0;

            tmp = i2c_smbus_read_byte_data(client, REG_MODECONTROL);
            tmp = tmp & (~0x80);
            rpr521_set_enable(client, tmp); /*PS on and ALS off*/
        }

        spin_lock_irqsave(&als_ps->update_lock.wait_lock, flags);
        __cancel_delayed_work(&als_ps->als_dwork);
        spin_unlock_irqrestore(&als_ps->update_lock.wait_lock, flags);
    }

    return count;
}

static ssize_t rpr521_show_als_poll_delay(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);

    return sprintf(buf, "%d\n", als_ps->als_poll_delay*1000000);    /*return in micro-second*/
}

static ssize_t rpr521_store_als_poll_delay(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);

    unsigned long val = simple_strtoul(buf, NULL, 10);

    unsigned long flags;
    /* leadcore  add  val=val/1000000  */
    if (val < PS_ALS_SET_MIN_DELAY_TIME * 1000000)
        val = PS_ALS_SET_MIN_DELAY_TIME * 1000000;

    als_ps->als_poll_delay = val /1000000;  /*convert us => ms*/

    if (als_ps->enable_als_sensor == 1) { /*grace modified in 2013.10.09*/

        /* we need this polling timer routine for sunlight canellation */
        spin_lock_irqsave(&als_ps->update_lock.wait_lock, flags);

        /*
         * If work is already scheduled then subsequent schedules will not
         * change the scheduled time that's why we have to cancel it first.
         */
        __cancel_delayed_work(&als_ps->als_dwork);
        schedule_delayed_work(&als_ps->als_dwork, msecs_to_jiffies(als_ps->als_poll_delay));    /* 125ms*/

        spin_unlock_irqrestore(&als_ps->update_lock.wait_lock, flags);

    }
    return count;
}

static ssize_t rpr521_show_als_data(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);
    struct i2c_client *client = als_ps->client;
    int tmp;
    int tmp1;
    tmp = i2c_smbus_read_word_data(client, REG_ALSDATA0);
    tmp1 = i2c_smbus_read_word_data(client, REG_ALSDATA1);

    return sprintf(buf, "%d %d\n", tmp, tmp1);
}

static ssize_t rpr521_show_ps_data(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);
    struct i2c_client *client = als_ps->client;
    int tmp = 0;
    tmp = i2c_smbus_read_word_data(client, REG_PSDATA);
    return sprintf(buf, "%d\n", tmp);
}

static ssize_t rpr521_show_ps_thres_high(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);
    struct i2c_client *client = als_ps->client;

    int ps_data = 0, ps_high = 0, ps_low = 0;
    ps_data = i2c_smbus_read_word_data(client, REG_PSDATA);
    if(ps_data < 0) {
        printk(KERN_ERR "%s: i2c read led current fail. \n", __func__);
        return -EINVAL;
    }

    ps_high = i2c_smbus_read_word_data(client, REG_PSTH);
    if(ps_high < 0) {
        printk(KERN_ERR "%s: i2c read led current fail. \n", __func__);
        return -EINVAL;
    }

    ps_low = i2c_smbus_read_word_data(client, REG_PSTL);
    if(ps_low < 0) {
        printk(KERN_ERR "%s: i2c read led current fail. \n", __func__);
        return -EINVAL;
    }
    return sprintf(buf, "%d %d %d\n", ps_data, ps_high, ps_low);
}

static ssize_t rpr521_store_ps_thres_high(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);
    struct i2c_client *client = als_ps->client;


    unsigned long val = simple_strtoul(buf, NULL, 10);

    if(!(rpr521_set_ps_threshold_high(client, val)))
        als_ps->ps_th_h_back = als_ps->ps_th_h;

    return count;
}

static ssize_t rpr521_show_ps_thres_low(struct device *dev,
                                        struct device_attribute *attr, char *buf)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);

    return sprintf(buf, "%d\n", als_ps->ps_th_l);
}

static ssize_t rpr521_store_ps_thres_low(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);
    struct i2c_client *client = als_ps->client;

    unsigned long val = simple_strtoul(buf, NULL, 10);

    if(!(rpr521_set_ps_threshold_low(client, val)))
        als_ps->ps_th_l_back = als_ps->ps_th_l;

    return count;
}

static ssize_t rpr521_show_ps_calib(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);

    return sprintf(buf, "%d\t%d\n", als_ps->ps_th_h, als_ps->ps_th_l);
}

static ssize_t rpr521_store_ps_calib(struct device *dev,
                                     struct device_attribute *attr, const char *buf, size_t count)
{
#define SET_LOW_THRES   1
#define SET_HIGH_THRES  2
#define SET_BOTH_THRES  3

    struct ALS_PS_DATA *als_ps = dev_get_drvdata(dev);
    struct i2c_client *client = als_ps->client;

    unsigned long val = simple_strtoul(buf, NULL, 10);
    unsigned int i, tmp, ps_th_h, ps_th_l;
    int average;    /*This should be signed to avoid error.*/

    switch(val) {
    case SET_LOW_THRES:
        /*Take 20 average for noise. use noise + THRES_TOLERANCE as low threshold.
        If high threshold is lower than the new low threshold + THRES_DIFF, make the high one equal low + THRES_DIFF
        Please make sure that there is NO object above the sensor, otherwise it may cause the high threshold too high to trigger which make the LCD NEVER shutdown.
        If the noise is too large, larger than 4065, it will return -1. If so, the mechanical design MUST be redo. It is quite unlikely.
                  */
        average = 0;
        ps_th_h = als_ps->ps_th_h_back;
        ps_th_l = als_ps->ps_th_l_back;
        for(i = 0; i < 20; i ++) {
            tmp = i2c_smbus_read_word_data(client, REG_PSDATA);
            if(tmp < 0) {
                printk(KERN_ERR "%s: i2c read ps data fail. \n", __func__);
                return -1;
            }
            average += tmp & 0xFFF; /*12 bit data*/
        }
        average /= 20;          /*This is the average noise*/
        ps_th_l = average + THRES_TOLERANCE;
        if(ps_th_l > REG_PSTL_MAX) {
            return -1;
        }
        if(ps_th_l < 0) {
            return -1;
        }
        if(ps_th_h < ps_th_l + THRES_DIFF) {
            ps_th_h = ps_th_l + THRES_DIFF; /*It will not be minus or an error should have occured earlier.*/
            if(ps_th_h > REG_PSTH_MAX) {
                return -1;
            }
            if(!rpr521_set_ps_threshold_high(client, ps_th_h))
                als_ps->ps_th_h_back = ps_th_h;
            else
                return -1;
        }
        if(!rpr521_set_ps_threshold_low(client, ps_th_l))
            als_ps->ps_th_l_back = ps_th_l;
        else
            return -1;
        break;

    case SET_HIGH_THRES:
        /*Take 20 average for signal. use signal -THRES_TOLERANCE as high threshold.
        If low threshold is higher than the new high one - THRES_DIFF, make the low one equal high - THRES_DIFF
        	   Please make sure that there IS AN object above the sensor, otherwise it will be a disaster. The high threshold will be too low which will cause the panel ALWAYS shutdown
        	   Customer can use their own standard to set the test scenario. For example, a 18% grey card @ 2cm, or another example, a 90% white card 4cm, and etc.
        	   If the signal is too weak, less than 30, it will return -1. If so, the mechanical design MUST be redo. It shall not happen very frequently.
        	 */
        average = 0;
        ps_th_h = als_ps->ps_th_h_back;
        ps_th_l = als_ps->ps_th_l_back;
        for(i = 0; i < 20; i ++) {
            tmp = i2c_smbus_read_word_data(client, REG_PSDATA);
            if(tmp < 0) {
                printk(KERN_ERR "%s: i2c read ps data fail. \n", __func__);
                return -1;
            }
            average += tmp & 0xFFF; /*12 bit data*/
        }
        average /= 20;          /*This is the average signal*/
        ps_th_h = average - THRES_TOLERANCE;
        if(ps_th_h > REG_PSTH_MAX) {
            return -1;
        }
        if(ps_th_h < 0) {
            return -1;
        }
        if(ps_th_l > ps_th_h - THRES_DIFF) {
            ps_th_l = ps_th_h - THRES_DIFF; /*Given that REG_PSTH_MAX = REG_PSTL+MAX, it will not be greater than REG_PSTL_MAX or an error should have occured earlier.*/
            if(ps_th_l < 0) {
                return -1;
            }
            if(!rpr521_set_ps_threshold_low(client, ps_th_l))
                als_ps->ps_th_l_back = ps_th_l;
            else
                return -1;
        }
        if(!rpr521_set_ps_threshold_high(client, ps_th_h))
            als_ps->ps_th_h_back = ps_th_h;
        else
            return -1;
        break;

    case SET_BOTH_THRES:    /*Take 20 average for noise. use noise + PS_ALS_SET_PS_TL as low threshold, noise + PS_ALS_SET_PS_TH as high threshold*/
        rpr521_calibrate(client);
        break;

    default:
        return -EINVAL; /*NOT supported!*/
    }

    return count;

#undef SET_BOTH_THRES
#undef SET_HIGH_THRES
#undef SET_LOW_THRES
}

static DEVICE_ATTR(delay,  S_IRUGO | S_IWUGO,
                   rpr521_show_als_poll_delay, rpr521_store_als_poll_delay);

static DEVICE_ATTR(enable,  S_IRUGO | S_IWUGO ,
                   rpr521_show_enable_als_sensor, rpr521_store_enable_als_sensor);

static struct device_attribute ps_enable_attribute = __ATTR(enable,S_IRUGO | S_IWUGO ,
        rpr521_show_enable_ps_sensor,rpr521_store_enable_ps_sensor);

static DEVICE_ATTR(ps_thres_high,  S_IRUGO | S_IWUGO ,
                   rpr521_show_ps_thres_high, rpr521_store_ps_thres_high);

static DEVICE_ATTR(ps_thres_low,  S_IRUGO | S_IWUGO ,
                   rpr521_show_ps_thres_low, rpr521_store_ps_thres_low);

static DEVICE_ATTR(ps_calib,  S_IRUGO | S_IWUGO ,
                   rpr521_show_ps_calib, rpr521_store_ps_calib);
static DEVICE_ATTR(als_data, S_IRUGO | S_IWUGO, rpr521_show_als_data, NULL);
static DEVICE_ATTR(ps_data, S_IRUGO | S_IWUGO, rpr521_show_ps_data, NULL);

static struct attribute *rpr521_als_attributes[] = {
    &dev_attr_enable.attr,
    &dev_attr_delay.attr,
    &dev_attr_als_data.attr,
    NULL
};
static struct attribute *rpr521_ps_attributes[] = {
    &ps_enable_attribute.attr,
    &dev_attr_ps_thres_high.attr,
    &dev_attr_ps_thres_low.attr,
    &dev_attr_ps_calib.attr,
    &dev_attr_ps_data.attr,
    NULL
};

static const struct attribute_group rpr521_als_attr_group = {
    .attrs = rpr521_als_attributes,
};
static const struct attribute_group rpr521_ps_attr_group = {
    .attrs = rpr521_ps_attributes,
};


/*************** Initialze Functions ******************/
static int rpr521_init_client(struct i2c_client *client)
{
    struct init_func_write_data {
        unsigned char mode_ctl;
        unsigned char psals_ctl;
        unsigned char persist;
        unsigned char reserved0;
        unsigned char reserved1;
        unsigned char reserved2;
        unsigned char reserved3;
        unsigned char reserved4;
        unsigned char reserved5;
        unsigned char intr;
        unsigned char psth_hl;
        unsigned char psth_hh;
        unsigned char psth_ll;
        unsigned char psth_lh;
        unsigned char alsth_hl;
        unsigned char alsth_hh;
        unsigned char alsth_ll;
        unsigned char alsth_lh;
    } write_data;
    int result;
    unsigned char gain;

    unsigned char mode_ctl    = PS_ALS_SET_MODE_CONTROL;
    unsigned char psals_ctl   = PS_ALS_SET_ALSPS_CONTROL;
    unsigned char persist     = PS_ALS_SET_INTR_PERSIST;
    unsigned char intr        = PS_ALS_SET_INTR;
    unsigned short psth_upper  = PS_ALS_SET_PS_TH;
    unsigned short psth_low    = PS_ALS_SET_PS_TL;
    unsigned short alsth_upper = PS_ALS_SET_ALS_TH;
    unsigned short alsth_low   = PS_ALS_SET_ALS_TL;

    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);


    /* execute software reset */
    result =  i2c_smbus_write_byte_data(client, REG_SYSTEMCONTROL, 0xC0);
    if (result != 0) {
        return (result);
    }

    write_data.mode_ctl  = mode_ctl;
    write_data.psals_ctl = psals_ctl;
    write_data.persist   = persist;
    write_data.reserved0 = 0;
    write_data.reserved1 = 0;
    write_data.reserved2 = 0;
    write_data.reserved3 = 0;
    write_data.reserved4 = 0;
    write_data.reserved5 = 0;
    write_data.intr      = intr;
    write_data.psth_hl   = CONVERT_TO_BE(psth_upper) & MASK_CHAR;
    write_data.psth_hh   = CONVERT_TO_BE(psth_upper) >> 8;
    write_data.psth_ll   = CONVERT_TO_BE(psth_low) & MASK_CHAR;
    write_data.psth_lh   = CONVERT_TO_BE(psth_low) >> 8;
    write_data.alsth_hl  = CONVERT_TO_BE(alsth_upper) & MASK_CHAR;
    write_data.alsth_hh  = CONVERT_TO_BE(alsth_upper) >> 8;
    write_data.alsth_ll  = CONVERT_TO_BE(alsth_low) & MASK_CHAR;
    write_data.alsth_lh  = CONVERT_TO_BE(alsth_low) >> 8;
    result               = i2c_smbus_write_i2c_block_data(client, REG_MODECONTROL, sizeof(write_data), (unsigned char *)&write_data);

    if(result < 0) {
        printk(KERN_ERR "%s: i2c write fail. \n", __func__);
        return result;
    }

    gain = (psals_ctl & 0x3C) >> 2;

    als_ps->enable = mode_ctl;
    als_ps->als_time = mode_table[(mode_ctl & 0xF)].ALS;
    als_ps->ps_time = mode_table[(mode_ctl & 0xF)].PS;
    als_ps->persistence = persist;
    als_ps->ps_th_l = psth_low;
    als_ps->ps_th_h = psth_upper;
    als_ps->als_th_l = alsth_low;
    als_ps->als_th_h = alsth_upper;
    als_ps->control = psals_ctl;
    als_ps->gain0 = gain_table[gain].DATA0;
    als_ps->gain1 = gain_table[gain].DATA1;
    als_ps->ledcurrent = psals_ctl & 0x03;

    als_ps->enable_als_sensor = 0;
    als_ps->enable_ps_sensor = 0;

#ifdef _INIT_CALIB_
    rpr521_calibrate(client);
#else
    als_ps->ps_th_h_back = als_ps->ps_th_h;
    als_ps->ps_th_l_back = als_ps->ps_th_l;
#endif

    return (result);
}

/*********** I2C init/probing/exit functions ****************/

static struct i2c_driver rpr521_driver;

static int __init rpr521_probe(struct i2c_client *client,
                               const struct i2c_device_id *id)
{
#define ROHM_PSALS_ALSMAX (65535)
#define ROHM_PSALS_PSMAX  (4095)

    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    struct ALS_PS_DATA *als_ps;
    int err = 0;

    printk("rpr521_probe------------------GRACE.\n");
    printk("%s started.\n",__func__);

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
        err = -EIO;
        goto exit;
    }

    als_ps = kzalloc(sizeof(struct ALS_PS_DATA), GFP_KERNEL);
    if (!als_ps) {
        err = -ENOMEM;
        goto exit;
    }
    als_ps->client = client;
    i2c_set_clientdata(client, als_ps);
    mutex_init(&als_ps->update_lock);
    INIT_DELAYED_WORK(&als_ps->dwork, rpr521_ps_int_work_handler);
    INIT_DELAYED_WORK(&als_ps->als_dwork, rpr521_als_polling_work_handler);

    /* Initialize the rpr521 chip */
    err = rpr521_init_client(client);
    if (err)
        goto exit_kfree;

    als_ps->als_poll_delay = PS_ALS_SET_MIN_DELAY_TIME;

    /* Register to Input Device */
    als_ps->input_dev_als = input_allocate_device();
    if (!als_ps->input_dev_als) {
        err = -ENOMEM;
        printk("%s: Failed to allocate input device als\n", __func__);
        goto exit_free_irq;
    }

    als_ps->input_dev_ps = input_allocate_device();
    if (!als_ps->input_dev_ps) {
        err = -ENOMEM;
        printk("%s: Failed to allocate input device ps\n", __func__);
        goto exit_free_dev_als;
    }

    input_set_drvdata(als_ps->input_dev_ps, als_ps);
    input_set_drvdata(als_ps->input_dev_als, als_ps);

    set_bit(EV_ABS, als_ps->input_dev_als->evbit);
    set_bit(EV_ABS, als_ps->input_dev_ps->evbit);

    input_set_abs_params(als_ps->input_dev_als, ABS_MISC, 0, ROHM_PSALS_ALSMAX, 0, 0);
    input_set_abs_params(als_ps->input_dev_ps, ABS_DISTANCE, 0, ROHM_PSALS_PSMAX, 0, 0);

    als_ps->input_dev_als->name = "rpr521light";
    als_ps->input_dev_ps->name = "rpr521proximity";
    als_ps->input_dev_als->id.bustype = BUS_I2C;
    als_ps->input_dev_als->dev.parent =&als_ps->client->dev;
    als_ps->input_dev_ps->id.bustype = BUS_I2C;
    als_ps->input_dev_ps->dev.parent =&als_ps->client->dev;

    err = input_register_device(als_ps->input_dev_als);
    if (err) {
        err = -ENOMEM;
        printk("%s: Unable to register input device als: %s\n", __func__,
               als_ps->input_dev_als->name);
        goto exit_free_dev_ps;
    }
    err = input_register_device(als_ps->input_dev_ps);
    if (err) {
        err = -ENOMEM;
        printk("%s: Unable to register input device ps: %s\n", __func__,
               als_ps->input_dev_ps->name);
        goto exit_unregister_dev_als;
    }

    /* Register sysfs hooks */
    err = sysfs_create_group(&als_ps->input_dev_ps->dev.kobj, &rpr521_ps_attr_group);
    if (err) {
        printk("%s sysfs_create_groupX\n", __func__);
        goto exit_unregister_dev_ps;
    }
    err = sysfs_create_group(&als_ps->input_dev_als->dev.kobj, &rpr521_als_attr_group);
    if (err < 0) {
        printk(KERN_ERR "%s:could not create sysfs group for ps\n", __func__);
        return err;
    }

    printk("%s support ver. %s enabled\n", __func__, DRIVER_VERSION);
    if (request_irq(client->irq, rpr521_interrupt, IRQF_TRIGGER_FALLING,
                    RPR521_DRV_NAME, (void *)client)) {
        printk("%s Could not allocate rpr521_INT !\n", __func__);

        goto exit_kfree;
    }

    printk(KERN_INFO "%s: INT No. %d", __func__, client->irq);
    return 0;


exit_unregister_dev_ps:
    input_unregister_device(als_ps->input_dev_ps);
exit_unregister_dev_als:
    printk("%s exit_unregister_dev_als:\n", __func__);
    input_unregister_device(als_ps->input_dev_als);
exit_free_dev_ps:
    input_free_device(als_ps->input_dev_ps);
exit_free_dev_als:
    input_free_device(als_ps->input_dev_als);
exit_free_irq:
    free_irq(client->irq, client);
exit_kfree:
    kfree(als_ps);
exit:
    return err;

#undef ROHM_PSALS_ALSMAX
#undef ROHM_PSALS_PSMAX
}

static int __exit rpr521_remove(struct i2c_client *client)
{
    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);

    input_unregister_device(als_ps->input_dev_als);
    input_unregister_device(als_ps->input_dev_ps);

    input_free_device(als_ps->input_dev_als);
    input_free_device(als_ps->input_dev_ps);

    free_irq(client->irq, client);

    sysfs_remove_group(&als_ps->input_dev_als->dev.kobj, &rpr521_als_attr_group);

    sysfs_remove_group(&als_ps->input_dev_ps->dev.kobj,&rpr521_ps_attr_group);

    /* Power down the device */
    rpr521_set_enable(client, 0);

    kfree(als_ps);

    return 0;
}

static int rpr521_suspend(struct i2c_client *client, pm_message_t mesg)
{
    ALSPS_PRINT2("%s\n", __func__);
    disable_irq(client->irq);
    return rpr521_set_enable(client, PS_ALS_SET_MODE_CONTROL);
}

static int rpr521_resume(struct i2c_client *client)
{
    struct ALS_PS_DATA *als_ps = i2c_get_clientdata(client);
    int tmp = PS_ALS_SET_MODE_CONTROL;

    ALSPS_PRINT2("%s \n", __func__);

    enable_irq(client->irq);
    if(als_ps->enable_als_sensor == 1)
        tmp = tmp | ALS_EN;
    if(als_ps->enable_ps_sensor == 1)
        tmp = tmp | PS_EN;
    return rpr521_set_enable(client, tmp);
}


MODULE_DEVICE_TABLE(i2c, rpr521_id);

static const struct i2c_device_id rpr521_id[] = {
    { "rpr521", 0 },
    { }
};

static struct i2c_driver rpr521_driver = {
    .driver = {
        .name   = RPR521_DRV_NAME,
        .owner  = THIS_MODULE,
    },
    .suspend = rpr521_suspend,
    .resume = rpr521_resume,
    .probe  = rpr521_probe,
    .remove = __exit_p(rpr521_remove),
    .id_table = rpr521_id,
};

static int __init rpr521_init(void)
{
    return i2c_add_driver(&rpr521_driver);
}

static void __exit rpr521_exit(void)
{
    i2c_del_driver(&rpr521_driver);
}

MODULE_AUTHOR("Andy Mi @ ROHM");
MODULE_DESCRIPTION("rpr521 ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(rpr521_init);
module_exit(rpr521_exit);
