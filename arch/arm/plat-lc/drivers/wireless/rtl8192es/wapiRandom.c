#include "8192cd_cfg.h"
#if defined(CONFIG_RTL_WAPI_SUPPORT)

#ifdef __LINUX_2_6__
#ifdef CONFIG_RTL8672
#include "./romeperf.h"
#else
#if !defined(NOT_RTK_BSP)
#include <net/rtl/rtl_types.h>
#endif
#endif
#include <linux/jiffies.h>
#else
#include "../rtl865x/rtl_types.h"
#endif

#include <linux/random.h>
#include "8192cd.h"
#include "wapi_wai.h"
#include "wapiCrypto.h"
#include "8192cd_util.h"
#include "8192cd_headers.h"

/************************************************************
generate radom number
************************************************************/
#if 0   //Original radom number generator
void
GenerateRandomData(unsigned char * data, unsigned int len)
{
        unsigned int i, num;
        unsigned char *pRu8;
#ifdef __LINUX_2_6__
        srandom32(jiffies);     
#endif

        for (i=0; i<len; i++) {
#ifdef __LINUX_2_6__
                num = random32();
#else
                get_random_bytes(&num, 4);
#endif
                pRu8 = (unsigned char*)&num;
                data[i] = pRu8[0]^pRu8[1]^pRu8[2]^pRu8[3];
        }
}
#else   //Radom number generator for wapi test
#define N 624
#define M 397
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */
//#define S     4357        /*seed*/

/* Tempering parameters */
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)

static unsigned long mt[N]; /* the array for the state vector  */
static int mti=N+1; /* mti==N+1 means mt[N] is not initialized */

/* initializing the array with a NONZERO seed */
void WapiMTgenSeed(unsigned long seed)
{
        /* setting initial seeds to mt[N] using         */
        /* the generator Line 25 of Table 1 in          */
        /* [KNUTH 1981, The Art of Computer Programming */
        /*    Vol. 2 (2nd Ed.), pp102]                  */
        const unsigned long int factor = 1812433253;
        mt[0]= seed & 0xffffffff;
        for (mti=1; mti<N; mti++)
        {
                mt[mti] = (factor * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
                mt[mti] &= 0xffffffff;
        }
}

unsigned long WapiMTgenrand(void)
{
        unsigned long y;
        static unsigned long mag01[2]={0x0, MATRIX_A};
        /* mag01[x] = x * MATRIX_A  for x=0,1 */

        if (mti >= N) { /* generate N words at one time */
                int kk;

                if (mti == N+1)   /* if sgenrand() has not been called, */
                        WapiMTgenSeed(5489);

                for (kk=0;kk<N-M;kk++) {
                        y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
                        mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1];
                }
                for (;kk<N-1;kk++) {
                        y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
                        mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1];
                }
                y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
                mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1];

                mti = 0;
        }

        y = mt[mti++];
        y ^= TEMPERING_SHIFT_U(y);
        y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
        y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
        y ^= TEMPERING_SHIFT_L(y);

        return y;
}

//Note: here len should be =< 32
void GenerateRandomData(unsigned char * data, unsigned int len)
{
        int num,i;
        unsigned char tempBuf[32];
        unsigned long y,seed;

#if  defined (__LINUX_2_6__)  &&  !defined(__LINUX_3_10__)
        srandom32(jiffies);
#endif
#if defined (__LINUX_2_6__)  &&  !defined(__LINUX_3_10__)
        seed = random32();
#elif defined(__LINUX_3_10__)
	seed =prandom_u32();
#else
        get_random_bytes(&seed, 4);
#endif

        WapiMTgenSeed(seed);

        if(len % 4 == 0)
                num = len/4;
        else
                num= len/4+1;

        memset(tempBuf,0,4*num);

        for(i=0;i<num;i++)
        {
                y=WapiMTgenrand();
                memcpy(tempBuf+4*i,&y,4);
        }

        memcpy(data,tempBuf,len);
}
#endif
#endif
