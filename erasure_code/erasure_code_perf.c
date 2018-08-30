/**********************************************************************
  Copyright(c) 2011-2015 Intel Corporation All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>		// for memset, memcmp
#include "erasure_code.h"
#include "test.h"

#define CACHED_TEST
#ifdef CACHED_TEST
// Cached test, loop many times over small dataset
# define TEST_SOURCES 32
//# define TEST_LEN(m)  ((128*1024 / m) & ~(64-1))
# define TEST_LEN(m)  ((1024*1024 / m) & ~(64-1))
# define TEST_LOOPS(m)   (10000*m)
# define TEST_TYPE_STR "_warm"
#else
# ifndef TEST_CUSTOM
// Uncached test.  Pull from large mem base.
#  define TEST_SOURCES 32
//#  define GT_L3_CACHE  32*1024*1024	/* some number > last level cache */
#  define GT_L3_CACHE  1024*1024*1024
#  define TEST_LEN(m)  ((GT_L3_CACHE / m) & ~(64-1))
#  define TEST_LOOPS(m)   (50*m)
#  define TEST_TYPE_STR "_cold"
# else
#  define TEST_TYPE_STR "_cus"
#  ifndef TEST_LOOPS
#    define TEST_LOOPS(m) 1000
#  endif
# endif
#endif



#define MMAX TEST_SOURCES
#define KMAX TEST_SOURCES

typedef unsigned char u8;

long long total_encode_time=0;
double total_encode_bandwidth=0;
long long total_decode_time=0;
double total_decode_bandwidth=0;


void test_perf_print(struct perf stop, struct perf start, long long dsize,int type)
{
	long long secs = stop.tv.tv_sec - start.tv.tv_sec;
	long long usecs = secs * 1000000 + stop.tv.tv_usec - start.tv.tv_usec;
        
	printf("runtime = %10lld usecs", usecs);
	if (dsize != 0) {
#if 1 // not bug in printf for 32-bit
		printf(", bandwidth %lld MB in %.4f sec = %.2f MB/s\n", dsize/(1024*1024),
			((double) usecs)/1000000, ((double) dsize) / (double)usecs);
        if(type==1)
        {
           total_encode_time+=usecs; 
           total_encode_bandwidth+=((double) dsize) / (double)usecs;  
        }else if(type==2)
        {
           total_decode_time+=usecs;
           total_decode_bandwidth+=((double) dsize) / (double)usecs;
        } 
#else
		printf(", bandwidth %lld MB ", dsize/(1024*1024));
		printf("in %.4f sec ",(double)usecs/1000000);
		printf("= %.2f MB/s\n", (double)dsize/usecs);
#endif
	}
	else
		printf("\n");
}

void total_perf_print(long long dsize,int type)
{
	
	long long usecs =0;
        if(type==1)
        {
           usecs=total_encode_time; 
           printf("encode total runtime = %10lld usecs \n", usecs);  
        }else if(type==2)
        {
           usecs=total_decode_time;
           printf("decode total runtime = %10lld usecs \n", usecs);
        } 
	
	if (dsize != 0) {
#if 1 // not bug in printf for 32-bit
		//printf(", bandwidth %lld MB in %.4f sec = %.2f MB/s\n", dsize/(1024*1024),
			//((double) usecs)/1000000, ((double) dsize) / (double)usecs);
        if(type==1)
        {
          
           printf(" encode total bandwidth = %.2f MB/s \n", total_encode_bandwidth);  
        }else if(type==2)
        {
          
           printf(" decode total bandwidth = %.2f MB/s \n", total_decode_bandwidth);
        } 
#else
		printf(", bandwidth %lld MB ", dsize/(1024*1024));
		printf("in %.4f sec ",(double)usecs/1000000);
		printf("= %.2f MB/s\n", (double)dsize/usecs);
#endif
	}
	else
		printf("\n");
}


int main(int argc, char *argv[])
{
	int i, j, rtest, m, k, nerrs, r;
	void *buf;
	u8 *temp_buffs[TEST_SOURCES], *buffs[TEST_SOURCES];
	u8 a[MMAX * KMAX], b[MMAX * KMAX], c[MMAX * KMAX], d[MMAX * KMAX];
	u8 g_tbls[KMAX * TEST_SOURCES * 32], src_in_err[TEST_SOURCES];
	u8 src_err_list[TEST_SOURCES], *recov[TEST_SOURCES];
	struct perf start, stop,total_time_start,total_time_stop;
        int index;
        FILE *ptr_myfile;
        FILE *ptr_csvfile;
        ptr_csvfile=fopen("/home/oded/oded_test.csv","ab+");
	if (!ptr_csvfile)
	{
	    printf("Unable to open oded_test.csv file!");
	    return 1;
	}

       	// Pick test parameters
	//m = 14;
	//k = 10;
	//nerrs = 4;
        
        //m = 12;
	//k = 10;
	//nerrs = 2;


        m = 20;
	k = 17;
	nerrs = 3;

	const u8 err_list[] = { 2, 4, 5, 7 };
                 
             
	//printf("erasure_code_perf: %dx%d %d %d\n", m, TEST_LEN(m), nerrs,GT_L3_CACHE);
        printf("erasure_code_perf: %dx%d %d %d\n", m, TEST_LEN(m), nerrs);

	if (m > MMAX || k > KMAX || nerrs > (m - k)) {
		printf(" Input test parameter error\n");
		return -1;
	}
   
	memcpy(src_err_list, err_list, nerrs);
	memset(src_in_err, 0, TEST_SOURCES);
	for (i = 0; i < nerrs; i++)
		src_in_err[src_err_list[i]] = 1;

	// Allocate the arrays
	for (i = 0; i < m; i++) {
		if (posix_memalign(&buf, 64, TEST_LEN(m))) {
			printf("alloc error: Fail\n");
			return -1;
		}
		buffs[i] = buf;
	        
	}
	
	for (i = 0; i < (m - k); i++) {
		if (posix_memalign(&buf, 64, TEST_LEN(m))) {
			printf("alloc error: Fail\n");
			return -1;
		}
		temp_buffs[i] = buf;
	        
	}
 
	// Make random data
        for (i = 0; i < k; i++)
	     for (j = 0; j < TEST_LEN(m); j++)
		  buffs[i][j] = rand();
	
	gf_gen_rs_matrix(a, m, k);
	ec_init_tables(k, m - k, &a[k * k], g_tbls);
	ec_encode_data(TEST_LEN(m), k, m - k, g_tbls, buffs, &buffs[k]);

	// Start encode test
	perf_start(&start);
	for (rtest = 0; rtest < TEST_LOOPS(m); rtest++) {
		// Make parity vects
		ec_init_tables(k, m - k, &a[k * k], g_tbls);
		ec_encode_data(TEST_LEN(m), k, m - k, g_tbls, buffs, &buffs[k]);
	}
	perf_stop(&stop);
	printf("erasure_code_encode" TEST_TYPE_STR ": ");
	test_perf_print(stop, start, (long long)(TEST_LEN(m)) * (m) * rtest,1);


	// Start decode test
	perf_start(&start);
	for (rtest = 0; rtest < TEST_LOOPS(m); rtest++) {
		// Construct b by removing error rows
		for (i = 0, r = 0; i < k; i++, r++) {
			while (src_in_err[r])
				r++;
			recov[i] = buffs[r];
			for (j = 0; j < k; j++)
				b[k * i + j] = a[k * r + j];
		}

		if (gf_invert_matrix(b, d, k) < 0) {
			printf("BAD MATRIX\n");
			return -1;
		}

		for (i = 0; i < nerrs; i++)
			for (j = 0; j < k; j++)
				c[k * i + j] = d[k * src_err_list[i] + j];

		// Recover data
		ec_init_tables(k, nerrs, c, g_tbls);
		ec_encode_data(TEST_LEN(m), k, nerrs, g_tbls, recov, temp_buffs);
	}
	perf_stop(&stop);

	for (i = 0; i < nerrs; i++) {
		if (0 != memcmp(temp_buffs[i], buffs[src_err_list[i]], TEST_LEN(m))) {
			printf("Fail error recovery (%d, %d, %d) - ", m, k, nerrs);
			return -1;
		}
	}

	printf("erasure_code_decode" TEST_TYPE_STR ": ");
	test_perf_print(stop, start, (long long)(TEST_LEN(m)) * (k + nerrs) * rtest,2);

        total_perf_print((long long)(TEST_LEN(m)) * (m) * rtest,1);
        total_perf_print((long long)(TEST_LEN(m)) * (m) * rtest,2);  
        //fprintf(ptr_csvfile, "%s,%lu,%d,%s,%d,%d,%f,%f\n", "17-3", GT_L3_CACHE, 4, "intel core i7-6700 CPU-3.40GHZ", total_encode_time,total_decode_time,total_encode_bandwidth,total_decode_bandwidth);
        
        fprintf(ptr_csvfile, "%s,%lu,%d,%s,%d,%d,%f,%f\n", "17-3 CACHED_TEST", TEST_LEN(m), 1, "intel core i7-6700 CPU-3.40GHZ", 
                total_encode_time,total_decode_time,total_encode_bandwidth,total_decode_bandwidth);
    
        fclose(ptr_csvfile); 
        printf("done all: Pass\n");


	return 0;

}
