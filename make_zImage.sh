#! /bin/sh

PHONE_BOARD_VERSION=`echo "$1"|tr "a-z" "A-Z"`
BUILD_MASS_PRODUCTION=$2
KERNEL_CFLAGS=""
# delete the old config file
rm .config

if [ ! -e ./.config ] ; then
	if [ "$PHONE_BOARD_VERSION" = "YL8150S_EVB" ] ; then	
		echo "Select YL8150S EVB board"	
		make yl8150s_evb_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LC1813_EVB2" ] ; then
		echo "Select LC1813 EVB2 board"
		make lc1813_evb2_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "UST802_V1_1" ] ; then
		echo "Select UST802 phone board verison is v1.1"
		make ust802_phone_v1_1_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LC1860_MENTOR" ] ; then
		echo "Select LC1860 MENTOR"
		make lc1860_mentor_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "TL920_V1_0" ] ; then
		echo "Select TL920 phone board version is v1.0"
		make tl920_phone_v1_0_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LC1860_EVB" ] ; then
		echo "Select LC1860 EVB board"
		make lc1860_evb_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LC1860_EVB2" ] ; then
		echo "Select LC1860 EVB2 board"
		make lc1860_evb2_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LC1860_EVB3" ] ; then
		echo "Select LC1860 EVB3 board"
		make lc1860_evb3_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "USL90_V1_0" ] ; then
		echo "Select USL90 phone board version is v1.0"
		make usl90_phone_v1_0_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LTE26007_V1_0" ] ; then
		echo "Select LTE26007 phone board version is v1.0"
		make lte26007_phone_v1_0_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LTE26007_V1_1" ] ; then
		echo "Select LTE26007 phone board version is v1.1"
		make lte26007_phone_v1_1_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LTE26007_V1_2" ] ; then
		echo "Select LTE26007 phone board version is v1.2"
		make lte26007_phone_v1_2_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "A310T_V1_0" ] ; then
		echo "Select A310T phone board version is v1.0"
		make a310t_phone_v1_0_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "A310T_V1_1" ] ; then
		echo "Select A310T phone board version is v1.1"
		make a310t_phone_v1_1_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "M9206_V1_0" ] ; then
		echo "Select m9206 pad board version is v1.0"
		make m9206_v1_0_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LX70A_V1_0" ] ; then
		echo "Select lx70a pad board version is v1.1"
		make lx70a_v1_0_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "K706_V1_0" ] ; then
		echo "Select k706 pad board version is v1.1"
		make k706_v1_0_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "LC1860_PHONE" ] ; then
		echo "Select LC1860 phone board"
		make lc1860_phone_defconfig
	elif [ "$PHONE_BOARD_VERSION" = "SMARTMODULE_V1_0" ] ; then
		echo "Select smartmodule phone board"
		make smartmodule_v1_0_defconfig
	else
		echo "Error message:please selct phone version between
		YL8150S_EVB,
		LC1813_EVB2, UST802_V1_1, TL920_V1_0,
		LC1860_MENTOR, LC1860_EVB, LC1860_EVB2, LC1860_EVB3, USL90_V1_0, LTE26007_V1_0, A310T_V1_0, A310T_V1_1, LTE26007_V1_1,
		M9206_V1_0, LX70A_V1_0,K706_V1_0, LC1860_PHONE, SMARTMODULE_V1_0, LTE26007_V1_2"

		exit 0
	fi
fi

# delete the old rootfs
rm -rf ./rootfs_test
if [ "$PHONE_BOARD_VERSION" = "LC1860_MENTOR" ]; then
	if [ ! -e ./rootfs_test ];then
    	tar zxf ./scripts/rootfs_test.tgz
	fi
fi


if [ "$BUILD_MASS_PRODUCTION" = "" ]; then
	KERNEL_CFLAGS=KCFLAGS=-DBUILD_MASS_PRODUCTION
elif [ "$BUILD_MASS_PRODUCTION" = "mass" ]; then
	KERNEL_CFLAGS=KCFLAGS=-DBUILD_MASS_PRODUCTION
elif [ "$BUILD_MASS_PRODUCTION" = "cmcc" ]; then
	:
else
	echo "Error message: please select compile production version : mass or cmcc "
	echo ""
	exit 0
fi

make ARCH=arm CROSS_COMPILE=arm-eabi- -j8 -s ${KERNEL_CFLAGS}
cp arch/arm/boot/zImage ./zImage
