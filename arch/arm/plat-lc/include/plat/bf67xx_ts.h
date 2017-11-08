/*
 * File:         byd_bfxxxx_ts.h
 *
 * Created:	     2012-10-07
 * Depend on:    byd_bfxxxx_ts.c
 * Description:  BYD TouchScreen IC driver for Android
 *
 * Copyright (C) 2012 BYD Company Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __BFXXXX_TS_H__
#define __BFXXXX_TS_H__
//#include "../../../arch/arm/mach-lc186x/board/board-lc1860.h"

/*******************************************************************************
* 第一部分：基本设置
*     如果采用原始驱动增加BYD扩展功能（参数下载、在线烧录、在线调试，如某些CTS15
*  项目），请忽略5-7项。
*******************************************************************************/
/* 1. 调试模式开关，用于显示调试log. 正常工作模式（如量产时）必须注销 */
	  #define TS_DEBUG_MSG

/* 2. 不同于全功能模式，基本功能模式驱动不包含扩展功能（如参数下载、在线固件更
      新等），只能在驱动的调试阶段或某些要求不高的COF方案（参数在IC的flash中）
      使用。 */
	//#define CONFIG_DRIVER_BASIC_FUNCTION

/* 3. 触屏IC选择 */
	//#define CONFIG_CTS01  //CTS01: BF6852A1, BF6852A2, BF6852C, BF6853A1. include CTS11C
	//#define CONFIG_CTS02  //CTS02: BF6862A1, BF6863A1
	//#define CONFIG_CTS12  //CTS12: BF6713A1, BF6931A2, BF6932A2
	  #define CONFIG_CTS13  //CTS13: BF6721A1, BF6722A1, BF6711A1, BF6712A1
	//#define CONFIG_CTS15  //CTS15: BF6751A1, BF6752A1, BF6731

/* 4. 驱动移植方式
      如果不是采用BYD驱动，而是原始驱动增加BYD扩展功能（多见于CTS15跑原始协议） */
	//#define CONFIG_NON_BYD_DRIVER

    /* 用于IC固件版本2.0版本以下的旧版固件芯片（旧BF685X坐标寄存器仅3字节） */
	//#define CONFIG_FIRMWARE_PRE_VERSION_2_0

/* 5. Android版本选择, Android 4.0及以上需开启 */
	  #define CONFIG_ANDROID_4_ICS
    /* 对Android 4.1及以上版本，必须开启触摸设备协议B（4.0版本也可用协议B） */
	  #define CONFIG_REPORT_PROTOCOL_B // 不适用于BF685X旧版（坐标3字节存储）
    /* 触摸设备协议B防止屏幕留点错误（TOUCH_FLAG==0x80时，报全抬起） */
	#define CONFIG_REMOVE_POINT_STAY // 仅适用于byd driver


/* 6. 近距支持开关  PROXIMITY support     */
	//#define CONFIG_TP_PROXIMITY_SENSOR_SUPPORT

/* 7. 手势支持根据具体手势打开下面相应映射按键 gesture function  */
	//#define BYD_GESTURE 1

	 #define BYD_TS_GESTURE_KEYS { \
		KEY_U,\
		KEY_D, \
		KEY_L,  \
		KEY_R, \
		KEY_W, \
		KEY_M, \
		KEY_C, \
		KEY_O, \
		KEY_Z, \
		KEY_POWER,\
	  }

/* 8. 按键定义（如KEY_BACK, KEY_MENU, KEY_HOME, KEY_SEARCH, KEY_SEND, ...等，请
      参见 "linux/input.h" 文件
      注：1. 根据实际触摸屏按键布置，调整下列大括号中按键定义顺序与实际位置相对应
          2. 如果没有用到按键，只需注销大括号中的所有按键即可 */
	  #define BYD_TS_KEYS { \
		 KEY_MENU,   \
		/* KEY_HOME,  */ \
		 KEY_BACK,   \
		/* KEY_SEARCH */ \
	  }

/* 7. 版本信息，如下格式：
      IC编码-IC封装码-IC固件版本号-驱动版本号-客户编码-项目编码-参数版本编码 */
	  #define BYD_TS_DRIVER_DESCRIPTION "BYD Touchscreen Driver, Version: " \
	      "CTS13C-BF67XX-COF2.10-DRV2.10-CLIENT0000-PROJ00-PARA1.0"
	  #define BYD_TS_DRIVER_VERSION_CODE	0x0210 // 版本 2.10


/*******************************************************************************
* 第二部分：IC分类设置
*******************************************************************************/
#ifndef CONFIG_DRIVER_BASIC_FUNCTION

/*版本比较宏， 用于控制在线升级，如果打开此宏，只有有新版本出现才会进行升级*/
 //#define VERSION_IDENTIFY

#endif // ndef CONFIG_DRIVER_BASIC_FUNCTION

/*******************************************************************************
* 第三部分：通讯设置 - 平台相关
*	如果采用原始驱动增加BYD扩展功能（如某些CTS15），请将该部分的定义指向原始
*代码所用的定义，或改动原始驱动代码采用该部分的定义，请注意本头文件和原始代码
*头文件的引用顺序。
*	将 I2C设备挂载到平台的 I2C总线，需要告知平台设备名BYD_TS_NAME，I2C从地址
*BYD_TS_SLAVE_ADDRESS，驱动需要被告知平台分配给本设备的中断号EINT_NUM。
* 注：
*如果 I2C地址（有时也包括中断号IRQ ）已经在平台的设备配置文件中正确定义
*（通常定义在类如"arch/arm/mach-xxxx/mach-xxxx.c"文件的"i2c_board_info"结
*构中），就无需再在这里重复定义。在这里使用这些定义仅适用于你需要覆盖平台
*定义值的情况
*******************************************************************************/

/* 1. 配置中断 */

   /* 中断端口号，可选
      中断端口号可用在函数gpio_to_irq()中得到中断号IRQ（见下EINT_NUM定义）、用
      在函数byd_ts_platform_init()中（见后）进行中断请求端口配置，或用于互电容
      raw data显示，否则请省略 */
	  #define GPIO_TS_EINT  MFP_PIN_GPIO(162)//BF686x_INT_PIN //S5PV210_GPH0(1) // for s5pv210 // 29 for boardcom

   /* 中断号IRQ，可选，可定义为一常量或函数宏定义
      如果在平台的设备配置文件中没有定义中断号IRQ，这里必须给出定义；如果平台已
      经定义，这里的定义将会将其覆盖 */
	#define EINT_NUM	gpio_to_irq(GPIO_TS_EINT) // 也可以是个常数

   /* 中断触发类型，需与IC给定的一致，如IRQF_TRIGGER_FALLING, IRQF_TRIGGER_LOW,
      IRQF_TRIGGER_RISING, IRQF_TRIGGER_HIGH */
	  #define EINT_TRIGGER_TYPE  IRQF_TRIGGER_FALLING

/* 2. I2C 从地址，可选
      如果在平台的设备配置文件中已经定义正确的 I2C从地址（一般芯片使用默认地址
      BYD_TS_DEFAULT_ADDRESS 7位0x2c/8位0x58；备用地址BYD_TS_ALTERNATE_ADDRESS
       0x43/0x86，仅用于bf672x系列）请不必在这里重复定义。在这里开启该定义仅适
      用于你需要覆盖平台定义值的情况。 */
	//#define BYD_TS_SLAVE_ADDRESS	0x2c // 0x2c/0x58, 0x43/0x86(backup), 0x5d/0xba(old mc)

   /* 对于 COB方案的bf672x系列互电容IC，芯片 P<4>/INT0中断口可以用作 I2C地址选
      择（该中断为主机到触屏芯片的中断，COF 方案请省略）。如果默认地址与总线上
      的设备冲突，请将设备从地址(见上面BYD_TS_SLAVE_ADDRESS)改为备用地址，同时
      打开端口GPIO_TS_ADDR的定义，采用该端口的中断及电平通知设备进行地址选择。 */
	//#define GPIO_TS_ADDR  S5PV210_GPH1(7)  // for s5pv210

/* 3. 设备ID及驱动名（请不要修改这里的定义，以免调试工具无法使用）
      如果采用的是 BYD驱动而不是原始驱动扩展方式（见上述“驱动移植方式”），需
      要修改 I2C设备配置（通常在平台的设备配置文件中）所定义的 I2C设备名称与
      这里的定义一致。如果“驱动移植方式”采用的是原始驱动扩展方式，请保留这里
      的定义，原始的 I2C设备配置可以不做修改 */
	  #define BYD_TS_NAME		"bfxxxx_ts"

/* 4.  I2C一次传输最大传输字节数
      标准I2C 总线协议无最大传输字节数的限制，请不要给出该定义。对于非标情况：
      某些MTK平台:8，Linux smbus协议: 32，请注意“最大传输字节数”必须为 4的
      倍数 */
	//#define I2C_PACKET_SIZE  8 // 32

/* 5. 通讯端口配置（如果上述“驱动移植方式”采用的是原始驱动扩展方式，请忽略）
      某些平台需要编写代码对某些端口（如中断、 I2C或休眠唤醒端口）进行初始化配
      置，请将代码加入如下的byd_ts_platform_init函数中： */

#ifndef CONFIG_NON_BYD_DRIVER // 采用BYD驱动请继续配置以下部分（否则请忽略）：

#include <linux/gpio.h>         // gpio_request(), gpio_to_irq()
//#include <linux/irq.h>          // irq_set_irq_type(), IRQ_TYPE_...

#ifdef GPIO_TS_ADDR
static void byd_ts_set_backup_addr(struct i2c_client *client);
#endif // def GPIO_TS_ADDR

/*******************************************************************************
* Function    :  byd_ts_platform_init
* Description :  platform specfic configration.
*                This function is usually used for interrupt pin or I2C pin
                 configration during initialization of TP driver.
* Parameters  :  client, - i2c client or NULL if not appropriate
* Return      :  Optional IRQ number or status if <= 0
*******************************************************************************/
static int byd_ts_platform_init(struct i2c_client *client) {
    int irq = 0;

	printk("%s: entered \n", __FUNCTION__);
#ifdef GPIO_TS_ADDR
    /* config port for I2C address selection, this is for COB only */
    byd_ts_set_backup_addr(client);
#endif

    /* config port for IRQ */
    if(gpio_request(GPIO_TS_EINT, "ts-interrupt") != 0) {
		printk("%s:GPIO_TS_EINT gpio_request() error\n", __FUNCTION__);
		return -EIO;
	}
    gpio_direction_input(GPIO_TS_EINT);     // IO configer as input port
	//s3c_gpio_setpull(GPIO_TS_EINT, S3C_GPIO_PULL_UP); //gpio_pull_updown(GPIO_TS_EINT, 1);
	//msleep(20);

#ifdef EINT_NUM
    /* (Re)define IRQ if not given or value of "client->irq" is not correct */
    irq = EINT_NUM;
    /* if the IRQ number is already given in the "i2c_board_info" at
     platform end, this will cause it being overrided. */
    if (client != NULL) {
        client->irq = irq;
    }
#endif

    /** warning: irq_set_irq_type() call may cause an unexpected interrupt **/
    //irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);   // IRQ_TYPE_LEVEL_LOW

    /* Returned IRQ is used only for adding device to the bus by the driver.
       otherwise, the irq would not necessarilly to be returned here (or,
       a status zero can be returned instead) */
    return irq;
}


/*******************************************************************************
                            关于通讯配置的更多说明

      有两种方法可以将I2C设备挂载到平台的I2C总线:

      1. 平台端挂载

         以上给出的挂载方法就是平台端挂载，通过在平台的设备配置文件中给定总线的
      "i2c_board_info"结构列表中添加一条设备记录（给出设备名、I2C地址、有时还包
      括中断号）就可以将设备”静态地“（编译时就可以确定挂载）添加到该总线上：

         {
          I2C_BOARD_INFO(BYD_TS_NAME, BYD_TS_SLAVE_ADDRESS),
          .irq = EINT_NUM // optional
         },

         有些平台上采用了某些非常规的做法，如IRQ 也可能会定义在"i2c_board_info"
      的结构"platform_data"中：

          struct platform_data {
              u16  irq;
          };

         这种方法是非标准的做法，请不要采纳。

      2. 设备端挂载（驱动内挂载）

         在驱动初始时，I2C设备”动态地“挂载。也就是，I2C设备实例化是在驱动运行
      时而不是在代码编译时确定）。此时设备是通过驱动将自己挂载到平台的I2C总线上

      注：
      1. 请勿使用同一设备名且同时使用两种方法进行设备挂载。
      2. 如果使用设备端挂载，设备所挂载的总线号BUS_NUM、地址BYD_TS_SLAVE_ADDRESS
         有时还有中断号EINT_NUM必须在上述通讯设置中明确地定义
                        关于COB方案I2C从地址选择

          对于 COF方案 I2C地址已经烧写到芯片的 flash中， I2C地址冲突时只需重新
      烧写该地址就可以了。 COB方案（ I2C地址在 flash中的值为0xff）采用bf672x系
      列互电容IC芯片，芯片PA<4>/INT0中断口可以用作 I2C地址选择：对于新方案，采
      用端口直接硬件上拉或下拉，芯片上电并延时后检查 PA4端口的电平，低位使用备
      用地址0x43(0x86)，否则使用默认地址0x2c(0x58)；对于敦泰兼容方案，默认 PA4
      端口设电阻上拉， I2C上电工作使用默认地址。 PA4同时又配置成中断口（主机到
      芯片的中断），当有地址冲突时，由驱动控制产生中断并设置端口电平表示地址选
      择：在驱动初始化时将 PA4端口由低拉高产生上升沿中断，随后 PA4端口拉低并维
      持低电平表示备用地址启用。如果由于某种原因芯片自己复位，芯片也需去检测
       PA4端口电平状态确定所用地址。默认情况下 PA4端口必须保证硬件上拉以及维持
      端口的高电平。

*******************************************************************************/

/*    BUS_NUM 为平台主板上 I2C总线的编号，TP设备将挂载到该总线上。该宏定义同时
      还表明了 I2C设备的挂载方法。如果给出该定义表示设备端挂载，否则表示平台端
      挂载 */
	//#define BUS_NUM		1

#endif // ifndef CONFIG_NON_BYD_DRIVER

#endif
