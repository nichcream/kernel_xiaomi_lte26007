/*
 * File:         byd_bf675x_ts.h
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
#include <plat/mfp.h>

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

/* 3. 驱动移植方式
      如果不是采用BYD驱动，而是原始驱动增加BYD扩展功能（多见于CTS15跑原始协议） */
	//#define CONFIG_NON_BYD_DRIVER

/* 4. Android版本选择, Android 4.0及以上需开启 */
	  #define CONFIG_ANDROID_4_ICS
    /* 对Android 4.1及以上版本，必须开启触摸设备协议B（4.0版本也可用协议B） */
	  #define CONFIG_REPORT_PROTOCOL_B
    /* 触摸设备协议B防止屏幕留点错误（TOUCH_FLAG==0x80时，报全抬起） */
	//#define CONFIG_REMOVE_POINT_STAY // 仅适用于byd driver

/* 5. 按键定义（如KEY_BACK, KEY_MENU, KEY_HOME, KEY_SEARCH, KEY_SEND, ...等，请
      参见 "linux/input.h" 文件
      注：1. 根据实际触摸屏按键布置，调整下列大括号中按键定义顺序与实际位置相对应
          2. 如果没有用到按键，只需注销大括号中的所有按键即可 */
	  #define BYD_TS_KEYS { \
		/* KEY_MENU,  */ \
		/* KEY_HOME,  */ \
		/* KEY_BACK,  */ \
		/* KEY_SEARCH */ \
	  }

/* 6. 版本信息，如下格式：
      IC编码-IC封装码-IC固件版本号-驱动版本号-客户编码-项目编码-参数版本编码 */
	  #define BYD_TS_DRIVER_DESCRIPTION "BYD Touchscreen Driver, Version: " \
	      "CTS15C-BF6751A-COB1.00-DRV1.32-CLIENT0000-PROJ00-PARA1.0"
	  #define BYD_TS_DRIVER_VERSION_CODE	0x0210 // 版本 2.10


/*******************************************************************************
* 第二部分：IC分类设置
*******************************************************************************/
#ifndef CONFIG_DRIVER_BASIC_FUNCTION

/* 1. 参数下载开关
      可以选择(a)...PARAMETER_UPG 和/或(b)...PARAMETER_FILE。对于COB方案，建议
      (a)(b)都选。*/

   /* (a) 默认参数使用驱动编译的参数。对于CTS15跑BYD协议，该定义表示驱动初始化
      时下载包含参数的IC代码byd_cts15_ts.txt（IC代码将会编译进驱动中作为默认的
      代码在开机时下载），在量产阶段请开启该定义（跑原协议的 CTS15无需开启）。*/
	  #define CONFIG_SUPPORT_PARAMETER_UPG
       /* 根据模组ID不同，参数（如下为示例）须定义在不同数组中
          （每个不同的模组对应一个不同的参数.txt 文件，默认走模组1.txt）*/

   /* (b) 使用外部参数文件：如果给出外部参数文件，系统将用该文件中的参数工作，
          上述驱动编译的参数将不再有效。由于在线参数调试必须用到外部参数文件，
          且该文件还可用于量产后参数需要改动但不便拆机的情况，建议无论COF、COB
          方案，都将该宏定义打开 */
	  #define CONFIG_SUPPORT_PARAMETER_FILE
       /* 外部参数文件须按如下方式以规定的位置和命名方式放置。注意：
          外部参数文件仅当下面PARA_DOWNLOAD_FILE及PARA_ALTERNATE_DIR中定义的文
          件在系统中存在时有效。为便于TP效果调试，驱动首先尝试在SD卡中寻找同名
          的文件，如果没有找到，再寻找PARA_DOWNLOAD_FILE定义的文件，这两个位置
          是便于对没有Root过的Android 设备（手机或平板等）进行调试的临时目录，
          最后寻找的目录是系统目录PARA_ALTERNATE_DIR。请注意最终调试结果用于量
          产时，参数文件必须放在系统目录下【但CTS15最好采用驱动编译参数的方法，
          将参数文件留作量产后需要修改时的备用】*/
	  //#define PARA_DOWNLOAD_FILE	"/data/local/tmp/byd_cts1x_ts#.dat"
	  #define PARA_DOWNLOAD_FILE	"/data/local/tmp/byd_cts1x_ts#.bin"
	  #define PARA_ALTERNATE_DIR	"/system/usr"
       /* 文件名中最后的#号将会被模组ID号替换（范围1-20，不使用时默认为1） */

/* 2. IC代码/参数下载
      CTS15 IC固件可以以数组的形式放在头文件中（多用于量产时），也可以放在外部
      文件中。将固件和/或参数 文件放置在SD卡的根目录、如下指定的目录、或上述的
      PARA_ALTERNATE_DIR指定的目录下，驱动会将其下载到TP芯片中. 请注意只有以指
      定的命名方式的文件出现这些位置时才会有效。文件位置和命名方式如下：*/
	  #define FIRM_DOWNLOAD_FILE	"/data/local/tmp/byd_cts1x_ts.bin"
	  #ifdef CONFIG_NON_BYD_DRIVER
	    #include "byd_cts15_ts.h"
	  #else
	    /* CTS-15量产时，如果采用的是 BYD的协议和驱动，在驱动代码的目录下必
	       须有byd_cts15_ts.txt代码文件，且CONFIG_SUPPORT_PARAMETER_UPG宏定
	       义必须开启 */
	  #endif


/* 注：关于参数下载更多说明：
      TP IC 工作参数可以来自选项 (a)驱动固定参数、或选项 (b)外部参数文件。外部
      参数文件优先级最高，当系统发现该文件为存在时，系统将会将该文件中的参数下
      载到IC中工作。其次优先级为选项(a),用驱动编译给定的参数，如果外部参数文件
      没有给出，驱动将会下载这些参数使IC工作（即使配置了选项 (b)）。因此，选项
      (a) 用于参数调试完成后，将调试结果在量产中使用，或用于 COB方案调试时的初
      始参数，选项(b) 可用于在量产后不拆机、不重新烧写手机系统的情况下通过给出
      外部文件改变参数配置。但无论COB或COF方案，选项(b) 都应该打开，否则无法使
      用在线参数调试功能。*/

#endif // ndef CONFIG_DRIVER_BASIC_FUNCTION

/*
******************************************************************************
第三部分：通讯设置 - 平台相关
如果采用原始驱动增加BYD扩展功能（如某些CTS15），请将该部分的定义指向原始
代码所用的定义，或改动原始驱动代码采用该部分的定义，请注意本头文件和原始代码
头文件的引用顺序。
将 I2C设备挂载到平台的 I2C总线，需要告知平台设备名BYD_TS_NAME，I2C从地址
BYD_TS_SLAVE_ADDRESS，驱动需要被告知平台分配给本设备的中断号EINT_NUM。
注：
如果 I2C地址（有时也包括中断号IRQ ）已经在平台的设备配置文件中正确定义
（通常定义在类如"arch/arm/mach-xxxx/mach-xxxx.c"文件的"i2c_board_info"结
构中），就无需再在这里重复定义。在这里使用这些定义仅适用于你需要覆盖平台
定义值的情况
******************************************************************************
*/

/* 1. 配置中断 */

   /* 中断端口号，可选
      中断端口号可用在函数gpio_to_irq()中得到中断号IRQ（见下EINT_NUM定义）、用
      在函数byd_ts_platform_init()中（见后）进行中断请求端口配置，或用于互电容
      raw data显示，否则请省略 */
	  #define GPIO_TS_EINT  MFP_PIN_GPIO(166)//BF686x_INT_PIN //S5PV210_GPH0(1) //S5PV210_GPH0(4)	//EXYNOS4_GPX1(4) //S5PV210_GPH0(5)

   /* 中断号IRQ，可选，可定义为一常量或函数宏定义
      如果在平台的设备配置文件中没有定义中断号IRQ，这里必须给出定义；如果平台已
      经定义，这里的定义将会将其覆盖 */
	  #define EINT_NUM	gpio_to_irq(GPIO_TS_EINT) // 也可以是个常数

   /* 中断触发类型，需与IC给定的一致，如IRQF_TRIGGER_FALLING, IRQF_TRIGGER_LOW,
      IRQF_TRIGGER_RISING, IRQF_TRIGGER_HIGH */
	  #define EINT_TRIGGER_TYPE  IRQF_TRIGGER_RISING

/* 2. I2C 从地址，可选
      如果在平台的设备配置文件中已经定义正确的 I2C从地址（默认地址
      BYD_TS_DEFAULT_ADDRESS 7位0x2c/8位0x58；）请不必在这里重复定义。
      在这里开启该定义仅适用于你需要覆盖平台定义值的情况。 */
	  #define BYD_TS_SLAVE_ADDRESS	0x2c // 0x2c/0x58

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
	//#define I2C_PACKET_SIZE  8 // actually 8 for read, 6 for write // smbus 32

/* 5. 通讯端口配置（如果上述“驱动移植方式”采用的是原始驱动扩展方式，请忽略）
      某些平台需要编写代码对某些端口（如中断、 I2C或休眠唤醒端口）进行初始化配
      置，请将代码加入如下的byd_ts_platform_init函数中： */

#ifndef CONFIG_NON_BYD_DRIVER // 采用BYD驱动请继续配置以下部分（否则请忽略）：

/* 6. WAKEUP端口号，
      采用shutdown端口用于CTS15的硬件方式休眠唤醒，这里没有使用，该端口默认上拉。 */
	  #define WAKE_PORT		MFP_PIN_GPIO(165)//BF686x_RST_PIN //S5PV210_GPD1(0)//---sda0  //S5PV210_GPH0(3)	//EXYNOS4_GPX1(5) //S5PV210_GPH1(7)


#include <linux/gpio.h>         // gpio_request(), gpio_to_irq()
//#include <linux/irq.h>          // irq_set_irq_type(), IRQ_TYPE_...
#include <linux/delay.h>     // mdelay()
static int byd_ts_shutdown_low(void)
{
	gpio_direction_output(WAKE_PORT, 0);
	//gpio_set_value(WAKE_PORT, 0);
	return 0;
}

static int byd_ts_shutdown_high(void)
{
	gpio_direction_output(WAKE_PORT, 1);
	//gpio_set_value(WAKE_PORT, 1);
	return 0;
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

      注
      1. 请勿使用同一设备名且同时使用两种方法进行设备挂载。
      2. 如果使用设备端挂载，设备所挂载的总线号BUS_NUM、地址BYD_TS_SLAVE_ADDRESS
		有时还有中断号EINT_NUM必须在上述通讯设置中明确地定义。

*******************************************************************************/

/*    BUS_NUM 为平台主板上 I2C总线的编号，TP设备将挂载到该总线上。该宏定义同时
      还表明了 I2C设备的挂载方法。如果给出该定义表示设备端挂载，否则表示平台端
      挂载 */
	#define BUS_NUM		2 // 1 // 0

#endif // ifndef CONFIG_NON_BYD_DRIVER

#endif
