/*
* Copyright (C) 2013 - 2016  Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge,
* publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in this
* Software without prior written authorization from Xilinx.
*
*/
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
//#include <stdbool.h>
//#include <assert.h>
#include <pthread.h>
//#include <fcntl.h>              // Flags for open()
//#include <sys/stat.h>           // Open() system call
//#include <sys/types.h>          // Types for open()
//#include <unistd.h>             // Close() system call
#include <string.h>             // Memory setting and copying
//#include <getopt.h>             // Option parsing
#include <errno.h>              // Error codes

//#define DEBUG

#include "conversion.h"         // Convert bytes to MiBs
#include "libaxidma.h"          // Interface ot the AXI DMA library
#include "axidma_transfer.h"

#include "parameters.h"

float feq_counter_app(float* input, unsigned const int cycle, const float thld_high, const float thld_low){
    unsigned int cyc_counter = 0;
    unsigned int sample_counter =0;
    unsigned int error_counter =0;
    uint8_t trig_ctrl = 0;
    uint8_t next_trig_ctrl = trig_ctrl;
    uint8_t st_ctrl =0;
    float val;
    float result =0;
    float time_interval = 0.000004;
    unsigned int max_counter = 64*AD_CH_PTS;

   //skip bad initial condition
    while(1){
         val = *(input);
         input++;         
        if(val<thld_low){
            break;            
        }
    }
    //start frequency counter
    while(cyc_counter<cycle){
        val = *(input);
        input++;
        //create ctrl;
        trig_ctrl = next_trig_ctrl;
        if( val>thld_high ){
            next_trig_ctrl = 1;
            st_ctrl = 1;

        }else if(val<thld_low){
            next_trig_ctrl = 0;
        }else{
            next_trig_ctrl = trig_ctrl ;
        }

        if(next_trig_ctrl>trig_ctrl){
             cyc_counter++;
             //printf("cyc=%d, sample=%d val = %.4f\r\n",cyc_counter,sample_counter,val);   
             
        }

        if(st_ctrl){
            sample_counter++;
            if(sample_counter> max_counter){
                printf("[freq_counter app] no frequency\r\n");
                return 0;
            }
        }        
    }
    result = cyc_counter /(sample_counter * time_interval);
    return result;
}


extern int axidma_dataReady;
float *tmp_test;
struct timeval t_start, t_mid,t_end;
double elapsed_time,dt_mid;
float freq =0;
int main(void)
{
    int rc;

    rc = transfer_enable();
    if (rc < 0)
    {
        printf("error: Failed to DMA initialization\r\n");
        return 0;
    };
    //MAX SIZE: MAX_ROUND * DMA_TRANS_SIZE ~ 314.6 MB
    //MIN SIZE: 1         * DMA_TRANS_SIZE ~ 1 MB
    // points = (int *)malloc(MAX_ROUND * DMA_TRANS_SIZE * sizeof(int));
    unsigned int pts = 64*AD_CH_PTS  ;
    tmp_test = (float*) malloc(pts  * sizeof(float));

    while (1)
    {

        gettimeofday(&t_start, NULL);

        get_fetch_data(tmp_test , pts);        

         gettimeofday(&t_mid, NULL);

       
    #ifdef DEBUG
        printf("data fectch finished \r\n");

        debug_data_printf(tmp_test,100);
    #endif
        
        /*
        
        do Frequency counter here
        
        */

        freq = feq_counter_app(tmp_test,1024,0.05,-0.05);

        gettimeofday(&t_end, NULL);
        elapsed_time = (TVAL_TO_SEC(t_end) - TVAL_TO_SEC(t_start)) * 1000;
        dt_mid = (TVAL_TO_SEC(t_mid) - TVAL_TO_SEC(t_start)) * 1000;
        printf("frequency = %.4f kHz  ", freq);
        printf("getdata time: %.2f elapsed time: %.2f ms\r\n",dt_mid, elapsed_time);


    }

    return 0;
}
