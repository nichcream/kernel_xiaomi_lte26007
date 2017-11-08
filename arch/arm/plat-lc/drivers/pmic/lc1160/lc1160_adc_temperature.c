/*
 * drivers\pmic\lc1132\comip_adc_temperature.c
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */
 
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <plat/comip-battery.h>

#define PROPRARE_COEFFCIENT    (1000) /*in order to resolve divisor less than zero*/
#define DBB_ADC_SAMPLE_DEFAULT_VALUE  (800)
#define BAT_ADC_SAMPLE_DEFAULT_VALUE  (900)


#define DBB_TEMP_NUM    (25)
#if 0
/*lc1813 dbb temperature from -20 to 100 for 15k registor*/
static int dbb_volt_to_temp[DBB_TEMP_NUM][2] = {
    {2333, -20},{2224, -15},{2101, -10},{1969, -5}, {1828, 0},
    {1682, 5},{1536, 10},{1391, 15},{1252, 20},{1120, 25},
    {997, 30},{884, 35},{781, 40},{689, 45},{607, 50},
    {535, 55},{471, 60},{415, 65},{366, 70},{324, 75},
    {286, 80},{254, 85},{225, 90},{200, 95},{179, 100},
};
#endif
/*lc1860 dbb temperature from -20 to 100for 4.7k registor*/
static int dbb_volt_to_temp[DBB_TEMP_NUM][2] = {
    {2634, -20},{2589, -15},{2536, -10},{2473, -5}, {2400, 0},
    {2317, 5},{2226, 10},{2125, 15},{2018, 20},{1904, 25},
    {1787, 30},{1667, 35},{1547, 40},{1429, 45},{1314, 50},
    {1204, 55},{1099, 60},{1001, 65},{909, 70},{824, 75},
    {747, 80},{676, 85},{611, 90},{553, 95},{501, 100},
};

#define BAT_TEMP_NUM    (19)
/*bat temperature from -20 to 70*/
static int bat_volt_to_temp[BAT_TEMP_NUM][2] = {
    {2295,-20},{2188,-15},{2069,-10},{1940,-5},{1805,0},
    {1665,5},{1524,10},{1384,15},{1249,20},{1120,25},
    {998,30},{886,35},{783,40},{691,45},{607,50},
    {534,55},{468,60},{411,65},{361,70},
};

static int comip_change_volt_to_temp(int volt, int index_min, int index_max, int channel)
{
    int coeffcient = 0;
    int volt1 = 0,volt2 = 0;
    int temp1 = 0,temp2 = 0;
    int temp = 0;

    switch(channel) {
    case DBB_TEMP_ADC_CHNL:
        volt1 = dbb_volt_to_temp[index_min][0];
        temp1 = dbb_volt_to_temp[index_min][1];
        volt2 = dbb_volt_to_temp[index_max][0];
        temp2 = dbb_volt_to_temp[index_max][1];
        break;
    case BAT_TEMP_ADC_CHNL:
        volt1 = bat_volt_to_temp[index_min][0];
        temp1 = bat_volt_to_temp[index_min][1];
        volt2 = bat_volt_to_temp[index_max][0];
        temp2 = bat_volt_to_temp[index_max][1];
        break;
    default:
        pr_err("ADC channel is not exist!\n\r");
        break;
    }

    coeffcient =((volt1 - volt)* PROPRARE_COEFFCIENT)/ (volt1 - volt2);
    temp = ((temp2 - temp1) * coeffcient) / PROPRARE_COEFFCIENT + temp1;
    return temp;
}

static int comip_get_temperature(int volt, int channel, int (*volt_to_temp)[2])
{
    int iLow = 0;
    int iUpper = 0 ;
    int temperature = 0;
    int iMid = 0;
    int *temp_idex = (int *)volt_to_temp;

    if (DBB_TEMP_ADC_CHNL == channel) {
        iUpper = sizeof(dbb_volt_to_temp) / sizeof(dbb_volt_to_temp[0][0]) / 2;
    }else if(BAT_TEMP_ADC_CHNL == channel){
        iUpper = sizeof(bat_volt_to_temp) / sizeof(bat_volt_to_temp[0][0]) / 2;
    }

    if (volt > *(temp_idex + 2 * 0 + 0)) {
        temperature = *(temp_idex + 2 * 0 + 1);
        return temperature;
    } else if (volt < *(temp_idex + 2 * (iUpper - 1) + 0)) {
        temperature = *(temp_idex + 2 * (iUpper - 1) + 1);
        return temperature;
    }

    while (iLow <= iUpper) {
        iMid = (iLow + iUpper) / 2;
        if (*(temp_idex + 2 * iMid + 0) < volt) {
            if (*(temp_idex + 2 * (iMid - 1) + 0) > volt) {
                temperature = comip_change_volt_to_temp(volt,
                        (iMid - 1), iMid, channel);
                goto exit;
            }
            iUpper = iMid - 1;
        } else if (*(temp_idex + 2 * iMid + 0) > volt) {
            if (*(temp_idex + 2 * (iMid + 1) + 0) < volt) {
                temperature = comip_change_volt_to_temp(volt,
                    iMid, (iMid + 1), channel);
                goto exit;
            }
            iLow = iMid + 1;
        } else {
            iUpper = iMid;
            temperature = *(temp_idex + 2 * iMid + 1);
            break;
        }
    }
exit:
    return temperature;
}

int comip_get_thermal_temperature(int channel,int *val)
{
    int adc_val = 0;
    int temp = 0;

    switch(channel){
        case DBB_TEMP_ADC_CHNL:
            adc_val = pmic_get_adc_conversion(channel);
            if (adc_val <= 0){
                pr_err("DBB get ADC value is fail,adc_val[%d]!\n\r", adc_val);
                adc_val = DBB_ADC_SAMPLE_DEFAULT_VALUE;/*if read error, return default value*/
                return (-EINVAL);
            } 
            temp = comip_get_temperature(adc_val,channel,dbb_volt_to_temp);
            *val = temp;
            break;
        case BAT_TEMP_ADC_CHNL:
            adc_val = pmic_get_adc_conversion(channel);
            if (adc_val <= 0){
                pr_err("BAT get ADC value is fail,adc_val[%d]!\n\r", adc_val);
                adc_val = BAT_ADC_SAMPLE_DEFAULT_VALUE;/*if read error, return default value*/
                return (-EINVAL);
            }
            temp = comip_get_temperature(adc_val,channel,bat_volt_to_temp);
            *val = temp;
            break;
        default:
            pr_err("ADC channel is not exist!\n\r");
            return (-EINVAL);
            break;
    }
    return 0;
}
EXPORT_SYMBOL(comip_get_thermal_temperature);


