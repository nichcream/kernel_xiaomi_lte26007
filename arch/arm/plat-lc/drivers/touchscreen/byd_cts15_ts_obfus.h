// #include "byd_cts15_ts_obfus.h"

/*******
open CUSTOMER_USED can adjust customer TP
**************/
//#define CUSTOMER_USED

/*******
add prooject ID
**************/
#define PREJECT1_ID  0
//#define PREJECT2_ID  1
//#define PREJECT3_ID  2

/*identify iquipment type */
extern int ctp_cob_bf675x;
struct fw_data
{
	unsigned int len_size;
	unsigned char *BYD_CTS15;
};

/* Variable   : Buf0 - buffer containing IC code
 *              Len_Bufcust - size of Bufcust
 * Accessed by: byd_ts_download()
 */
	unsigned char Bufmodules[]=
	{
		#include "byd_cts15_ts_modules.txt"
	};
	int Len_Bufmoudles = sizeof(Bufmodules);

	unsigned char Bufcust[]=
	{
		//#include "byd_cts15_ts_custom.txt"
		#include "byd_cts15_ts.txt"

	};
//#ifdef CONFIG_PROJECT_IDENTIFY
	unsigned char  D701C_BYD6751_CTS15_TS16[]={
		//#include "byd_cts15_ts.txt"
	};

	unsigned char  D711HC_BYD6751_CTS15_TS16[]={
		//#include "d711hc_byd_cts15_ts16.txt"
			};

	unsigned char  D711HC_OGS_BYD6751_CTS15_TS16[]={
		//#include "D711HC_OGS_BYD6751_CTS15_TS16.txt"
			};
const struct fw_data bf675x_data[] = {
	{sizeof(Bufcust), Bufcust},  //0
	{sizeof(D701C_BYD6751_CTS15_TS16), D701C_BYD6751_CTS15_TS16},   //  1
//	 {sizeof(D711HC_BYD6751_CTS15_TS16), D711HC_BYD6751_CTS15_TS16},  // 2
//	 {sizeof(D711HC_OGS_BYD6751_CTS15_TS16), D711HC_OGS_BYD6751_CTS15_TS16},  // 2

};
//#endif
//#ifndef CONFIG_PROJECT_IDENTIFY
#if 0
/*************************
bf675x_data
*******************/

	unsigned char Bufcust[]=
	{
		//#include "byd_cts15_ts_custom.txt"

	};

	unsigned char  BYD6751_CTS15_TS1[]={
		#include "BYD6751_CTS15_TS1.txt"
	};

	unsigned char  BYD6751_CTS15_TS2[]={
		//#include "BYD6751_CTS15_TS2.txt"
	};

	unsigned char  BYD6751_CTS15_TS3[]={
		//#include "BYD6751_CTS15_TS3.txt"
	};

	unsigned char  BYD6751_CTS15_TS4[]={
		//#include "BYD6751_CTS15_TS4.txt"
	};

	unsigned char  BYD6751_CTS15_TS5[]={
		//#include "BYD6751_CTS15_TS5.txt"
	};

	unsigned char  BYD6751_CTS15_TS6[]={
		//#include "BYD6751_CTS15_TS6.txt"
	};

	unsigned char  BYD6751_CTS15_TS7[]={
		//#include "BYD6751_CTS15_TS7.txt"
	};

	unsigned char  BYD6751_CTS15_TS8[]={
		//#include "BYD6751_CTS15_TS8.txt"
	};

	unsigned char  BYD6751_CTS15_TS9[]={
		//#include "BYD6751_CTS15_TS9.txt"
	};

	unsigned char  BYD6751_CTS15_TS10[]={
		//#include "BYD6751_CTS15_TS10.txt"
	};

	unsigned char  BYD6751_CTS15_TS11[]={
		//#include "BYD6751_CTS15_TS11.txt"
	};

	unsigned char  BYD6751_CTS15_TS12[]={
		//#include "BYD6751_CTS15_TS12.txt"
	};

	unsigned char  BYD6751_CTS15_TS13[]={
		//#include "BYD6751_CTS15_TS13.txt"
	};

	unsigned char  BYD6751_CTS15_TS14[]={
		//#include "BYD6751_CTS15_TS14.txt"
	};

	unsigned char  BYD6751_CTS15_TS15[]={
		//#include "BYD6751_CTS15_TS15.txt"
	};

	unsigned char  BYD6751_CTS15_TS16[]={
		#include "BYD6751_CTS15_TS1.txt"
	};




const struct fw_data bf675x_data[] = {
	{sizeof(Bufcust), Bufcust},  //0
	{sizeof(BYD6751_CTS15_TS1), BYD6751_CTS15_TS1},   //  1
	{sizeof(BYD6751_CTS15_TS2), BYD6751_CTS15_TS2},  // 2
	{sizeof(BYD6751_CTS15_TS3), BYD6751_CTS15_TS3},  // 2
	{sizeof(BYD6751_CTS15_TS4), BYD6751_CTS15_TS4},
	{sizeof(BYD6751_CTS15_TS5), BYD6751_CTS15_TS5},
	{sizeof(BYD6751_CTS15_TS6), BYD6751_CTS15_TS6},
	{sizeof(BYD6751_CTS15_TS7), BYD6751_CTS15_TS7},
	{sizeof(BYD6751_CTS15_TS8), BYD6751_CTS15_TS8},
	{sizeof(BYD6751_CTS15_TS9), BYD6751_CTS15_TS9},
	{sizeof(BYD6751_CTS15_TS10), BYD6751_CTS15_TS10},
	{sizeof(BYD6751_CTS15_TS11), BYD6751_CTS15_TS11},
	{sizeof(BYD6751_CTS15_TS12), BYD6751_CTS15_TS12},
	{sizeof(BYD6751_CTS15_TS13), BYD6751_CTS15_TS13},
	{sizeof(BYD6751_CTS15_TS14), BYD6751_CTS15_TS14},
	{sizeof(BYD6751_CTS15_TS15), BYD6751_CTS15_TS15},
	{sizeof(BYD6751_CTS15_TS16), BYD6751_CTS15_TS16},
};
#endif

