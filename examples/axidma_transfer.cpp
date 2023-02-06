/**
 * @file axidma_transfer.c
 * @date Sunday, November 29, 2015 at 12:23:43 PM EST
 * @author Brandon Perez (bmperez)
 * @author Jared Choi (jaewonch)
 *
 * This program performs a simple AXI DMA transfer. It takes the input file,
 * loads it into memory, and then sends it out over the PL fabric. It then
 * receives the data back, and places it into the given output file.
 *
 * By default it uses the lowest numbered channels for the transmit and receive,
 * unless overriden by the user. The amount of data transfered is automatically
 * determined from the file size. Unless specified, the output file size is
 * made to be 2 times the input size (to account for creating more data).
 *
 * This program also handles any additional channels that the pipeline
 * on the PL fabric might depend on. It starts up DMA transfers for these
 * pipeline stages, and discards their results.
 *
 * @bug No known bug
 **/
#include <iostream>
#include <thread>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <fcntl.h>              // Flags for open()
#include <sys/stat.h>           // Open() system call
#include <sys/types.h>          // Types for open()
#include <unistd.h>             // Close() system call
#include <string.h>             // Memory setting and copying
#include <getopt.h>             // Option parsing
#include <errno.h>              // Error codes
#include <pthread.h>

#include "conversion.h"         // Convert bytes to MiBs
#include "libaxidma.h"          // Interface ot the AXI DMA library
#include "axidma_transfer.h"
#include "parameters.h"
#include <arm_neon.h>
#include <arm_fp16.h>


 //struct stat input_stat;
axidma_dev_t axidma_dev;
pthread_t axidma_thread;
struct dma_transfer trans;
struct dma_thread_parameters dma_utc;
int g_counter, buf_counter;
int n_round = MAX_ROUND;
int single_trans_size;
struct timeval time1, time2, time3,time4;

double dt1,dt2,dt3=0;
double t_curr, t_last;
int* databuffer;
float32_t* usrbuffer;
int16_t* dma_buf[2];
float32_t* dma_buf_chA[2];
float32_t* dma_buf_chB[2];
int axidma_dataReady;
int pts;

/*----------------------------------------------------------------------------
 * User callback function to be invoked upon completion of an
 * asynchronous transfer for the specified DMA channel.
 * As a reminder: typedef void (*axidma_cb_t)(int channel_id, void *data);
 *----------------------------------------------------------------------------*/
static inline void dma_cvt_neon(float32_t* __restrict output_chA,float32_t* __restrict output_chB, int16_t* __restrict input,  uint pts);
//static inline void dma_cvt_neon_v2(float* __restrict output, int16_t* __restrict input,  uint pts);

/*
static inline void dma_cvt_neon_v2(float* __restrict output, int16_t* __restrict input,  uint pts){

	const float32x4_t b_adc = vdupq_n_f32(0.019342);
	const float32x4_t a_adc = vdupq_n_f32(0.001303);
	//const float32x4_t b_adc = vdupq_n_f32(0.00);
	//const float32x4_t a_adc = vdupq_n_f32(1.00);
	int16x8x4_t load0,load1;
	float32x4x4_t store0,store1,store2,store3;;
	//float32x4x4_t v0_mul,v1_mul;
	float32x4_t v0_f32,v1_f32,v2_f32,v3_f32;;
	int16x4_t vec0,vec1,vec2,vec3;
	int32x4_t v0_32, v1_32,v2_32, v3_32;
	unsigned int i = 0;
	while(i < pts) {
		load0 = vld4q_s16(input + i);
		load1 = vld4q_s16(input + i +32);
		for (int j=0;j<4;j++){
			vec0 = vget_low_s16(load0.val[j]);
			vec2 = vget_low_s16(load1.val[j]);
			vec1 = vget_high_s16(load0.val[j]);			
			vec3 = vget_high_s16(load1.val[j]);	

			v0_32 = vmovl_s16(vec0);
			v1_32 = vmovl_s16(vec1);
			v2_32 = vmovl_s16(vec2);
			v3_32 = vmovl_s16(vec3);

			v0_f32 = vcvtq_f32_s32(v0_32);	
			v1_f32 = vcvtq_f32_s32(v1_32);
			v2_f32 = vcvtq_f32_s32(v2_32);	
			v3_f32 = vcvtq_f32_s32(v3_32);


			store0.val[j] = vfmaq_f32(b_adc,a_adc,v0_f32);
			store1.val[j] = vfmaq_f32(b_adc,a_adc,v1_f32);
			store2.val[j] = vfmaq_f32(b_adc,a_adc,v2_f32);
			store3.val[j] = vfmaq_f32(b_adc,a_adc,v3_f32);

			//store0.val[j] = v0_f32;
			//store1.val[j] = v1_f32;
			//store2.val[j] = v2_f32;
			//store3.val[j] = v3_f32;
		}

		vst4q_f32(output+i, store0);
		vst4q_f32(output+i+16, store1);
		vst4q_f32(output+i+32, store2);
		vst4q_f32(output+i+48, store3);

		i+=64;
	}
}
*/
static void dma_calibration_neon(float32_t* __restrict output, float32_t* __restrict input,  uint pts){

	const float32x4_t a = vdupq_n_f32(0.001303);
	const float32x4_t b = vdupq_n_f32(0.019342);
	float32x4x4_t load;
	float32x4x4_t store;
	unsigned int i =0;
	if (!axidma_dataReady){

		while(i < pts) {

			load = vld4q_f32(input + i);

			for (int j=0;j<4;j++){
				store.val[j] = vfmaq_f32(b, a, load.val[j]);
			}
			vst4q_f32(output+i, store);

			i+=16;
		}
		axidma_dataReady =1;
	}

//10 
}



static inline void dma_cvt_neon_ch(float32_t* __restrict output_chA, int16_t* __restrict input,  uint pts, int ch);
static inline void dma_cvt_neon_ch(float32_t* __restrict output_chA, int16_t* __restrict input,  uint pts, int ch){
	const float32_t a_0= 0.001303;
	// const float32x4_t a_0 = vdupq_n_f32(0.001303);
	// const float32x4_t b_0 = vdupq_n_f32(0.019342);
	// const float32x4_t a_1 = vdupq_n_f32(0.001303);
	// const float32x4_t b_1 = vdupq_n_f32(0.019342);
	//const float32x4_t b_adc = vdupq_n_f32(0.00);
	//const float32x4_t a_adc = vdupq_n_f32(1.00);
	
	float32x4_t store0,store1,store2,store3;//8
	int16x4_t v0,v1,v2,v3;
	//float32x4x4_t v0_mul,v1_mul;
	float32x4_t v0_f32,v1_f32,v2_f32,v3_f32; //+4
	int32x4_t v0_32,v1_32,v2_32,v3_32; //+4
	unsigned int i =0;
	if(ch == 1){input+=16;}
	while(i<pts) {

		
		v0 = vld1_s16(input);
		input +=8;
		v1 = vld1_s16(input);
		input +=8;
		v2 = vld1_s16(input);
		input +=8;
		v3 = vld1_s16(input);
		input +=8;

		v0_32 = vmovl_s16(v0);	
		v1_32 = vmovl_s16(v1);
		v2_32 = vmovl_s16(v2);
		v3_32 = vmovl_s16(v3);

		v0_f32 = vcvtq_f32_s32(v0_32);
		v1_f32 = vcvtq_f32_s32(v1_32);	
		v2_f32 = vcvtq_f32_s32(v2_32);	
		v3_f32 = vcvtq_f32_s32(v3_32);	

		store0 = vmulq_n_f32(v0_f32,a_0);
		store1 = vmulq_n_f32(v1_f32,a_0);	
		store2 = vmulq_n_f32(v2_f32,a_0);	
		store3 = vmulq_n_f32(v3_f32,a_0);

		vst1q_f32(output_chA,store0);		
		output_chA +=4;
		vst1q_f32(output_chA,store1);
		output_chA +=4;
		vst1q_f32(output_chA,store2);
		output_chA +=4;
		vst1q_f32(output_chA,store3);
		output_chA +=4;
		

		i+=32;

	}
}



static inline void dma_cvt_neon(float32_t* __restrict output_chA, float32_t* __restrict output_chB, int16_t* __restrict input,  uint pts){
	const float32_t a_0= 0.001303;
	// const float32x4_t a_0 = vdupq_n_f32(0.001303);
	// const float32x4_t b_0 = vdupq_n_f32(0.019342);
	// const float32x4_t a_1 = vdupq_n_f32(0.001303);
	// const float32x4_t b_1 = vdupq_n_f32(0.019342);
	//const float32x4_t b_adc = vdupq_n_f32(0.00);
	//const float32x4_t a_adc = vdupq_n_f32(1.00);
	int16x8x4_t loaded; //+4
	float32x4x4_t storeA, storeB; //+8
	//float32x4x4_t v0_mul,v1_mul;
	float32x4_t v0_f32,v1_f32; //+2
	int16x4_t vec0,vec1; //+2 64bit
	int32x4_t v0_32, v1_32; //+2
	unsigned int i =0;

	while(i<pts) {

		loaded = vld4q_s16(input);
		input+=32;


		for (int j=0;j<4;j++){
			vec0 = vget_low_s16(loaded.val[j]);
			vec1 = vget_high_s16(loaded.val[j]);

			v0_32 = vmovl_s16(vec0);
			v1_32 = vmovl_s16(vec1);

			v0_f32 = vcvtq_f32_s32(v0_32);	
			v1_f32 = vcvtq_f32_s32(v1_32);

			storeA.val[j] = vmulq_n_f32(v0_f32,a_0);
			storeB.val[j] = vmulq_n_f32(v1_f32,a_0);
			//storeA.val[j] = vfmaq_f32(b_0, a_0, v0_f32);
			//storeB.val[j] = vfmaq_f32(b_1, a_1, v1_f32);
			//storeA.val[j] = v0_f32;
			//storeB.val[j] = v1_f32;
		}
i+=32;
		
		// for (int j=0;j<4;j++){
		// 	vec0 = vget_low_s16(loaded.val[j]);

		// 	v0_32 = vmovl_s16(vec0);

		// 	v0_f32 = vcvtq_f32_s32(v0_32);	

		// 	storeA.val[j] = vmulq_n_f32(v0_f32,a_0);
		// }

		vst4q_f32(output_chA, storeA);
		output_chA +=16;
		

		// for (int j=0;j<4;j++){

		// 	vec1 = vget_high_s16(loaded.val[j]);

		// 	v1_32 = vmovl_s16(vec1);

		// 	v1_f32 = vcvtq_f32_s32(v1_32);

		// 	storeB.val[j] = vmulq_n_f32(v1_f32,a_0);
	
		// }

		vst4q_f32(output_chB, storeB);
		output_chB +=16;

		// vst4q_f32(output_chB, storeB);
		// output_chB +=16;
		i+=32;

	}

	
//16 128 bit / 2 64 bit
}

/*----------------------------------------------------------------------------
 * DMA File Transfer Functions
 *----------------------------------------------------------------------------*/


int dma_counter=0;

void dma_callback(int channel_id, void *utc)
{
	struct dma_thread_parameters *dma_utc_local = (struct dma_thread_parameters *)utc;
	int rc1;
	// we're going to manipulate the cond, so we need the mutex
	
		
	// gettimeofday(&time3, NULL);
	pthread_mutex_lock( &dma_utc_local->dma_mutex );
	unsigned int p_buf = buf_counter%2;
	unsigned int pos = dma_counter* AD_CH_PTS;

	// Goal : the real-time solution need < 300 us
	// information:
	// 1. each dma transfer roughly cost 525 us... but the order need ~200 us to start the transfer.
	// 2. the remaining time is ~300 us
	// 3. the total points is DMA_TRANS_PTS ; each channel is AD_CH_PTS = DMA_TRANS_PTS/2

	//case1: 220 us only for buffer saving..
	//memcpy((dma_buf[buf_counter%2] + dma_counter* DMA_TRANS_PTS), trans.output_buf, trans.output_size);

	//case2: 500 us -> 250 us /per ch
	dma_cvt_neon_ch((dma_buf_chA[p_buf] + pos), trans.output_buf, DMA_TRANS_PTS,0);
	//dma_cvt_neon_ch((dma_buf_chB[p_buf] + pos), trans.output_buf, DMA_TRANS_PTS,1);

	//case3: 800 us with 2 chs together
	//dma_cvt_neon((dma_buf_chA[p_buf] + pos), (dma_buf_chB[p_buf] + pos) ,trans.output_buf, DMA_TRANS_PTS);

	//case4: 340/per ch
	//std::thread chA(dma_cvt_neon_ch,(dma_buf_chA[p_buf] + pos), trans.output_buf, DMA_TRANS_PTS,0);	
	//chA.join();
	
	// gettimeofday(&time4, NULL);

	// t_curr = TVAL_TO_SEC(time3)*1000000;
	// dt3 = TVAL_TO_SEC(time4)*1000000 - t_curr;
	// dt2 = t_curr - t_last;
	// t_last = t_curr;
	// printf("no.%d,dt2: %.2f dt3: %.2f us\r\n",dma_counter,dt2, dt3);

	if(dma_counter<(n_round-1)){

		rc1 = axidma_oneway_transfer(axidma_dev,
									trans.output_channel,
									trans.output_buf,
									trans.output_size,
									false);
		if (rc1 < 0) {
			fprintf(stderr, "DMA read transaction failed.\n");			
		}

		dma_counter++;
		
	}else{	
		// wake up the dma thread (AS it is sleeping) to terminate operations
		
		dma_counter =0;	
		buf_counter++;	
		
		pthread_cond_signal( &dma_utc_local->dma_cond );		
	}

	pthread_mutex_unlock( &dma_utc_local->dma_mutex );	
}

 /*----------------------------------------------------------------------------
  * DMA Thread Creation
  *----------------------------------------------------------------------------*/

static void* axidma_platform(void* arg) {
    int rc0 = 1;
    printf("%s\n", (char*)arg);
    while (1) {
		// gettimeofday(&time1, NULL);
        // Receive the file from the AXI DMA   
		pthread_mutex_lock(&dma_utc.dma_mutex);		
		rc0 = pthread_cond_wait(&dma_utc.dma_cond, &dma_utc.dma_mutex);
		pthread_mutex_unlock(&dma_utc.dma_mutex);

		if (rc0 < 0) {
			fprintf(stderr, "pthread_cond_wait failed with rc: %d.\n", rc0);
			break;
		}

		//debug_data_printf(dma_buf[(buf_counter+1)%2],1000);

		rc0 = axidma_oneway_transfer(axidma_dev,
									trans.output_channel,
									trans.output_buf,
									trans.output_size,
									false);
		if (rc0 < 0) {
			fprintf(stderr, "DMA read transaction failed.\n");
			break;
			}

		// gettimeofday(&time2, NULL);
		// unsigned int p_buf = (buf_counter+1)%2;
		// //unsigned int pos = dma_counter* AD_CH_PTS;
		// printf("[info: ]");
		// dma_cvt_neon_ch(dma_buf_chA[p_buf], dma_buf[p_buf], n_round*DMA_TRANS_PTS,0);
		// dma_cvt_neon_ch(dma_buf_chB[p_buf], dma_buf[p_buf], n_round*DMA_TRANS_PTS,1);
		axidma_dataReady =1;

		// gettimeofday(&time3, NULL);
		// dt1 = (TVAL_TO_SEC(time2) - TVAL_TO_SEC(time1))*1000;
		// dt2 = (TVAL_TO_SEC(time3) - TVAL_TO_SEC(time2))*1000;
		
        // printf("DMA_transfer: %.3f ms ; dt2: %.3f ms\r\n",dt1,dt2);
    }
    //close the thread & axidma_dev & axidma_buff
    printf("EZ system: AXI-DMA Thread disable\r\n");
    axidma_free(axidma_dev, trans.output_buf, trans.output_size);
    axidma_destroy(axidma_dev);
	pthread_mutex_destroy(&dma_utc.dma_mutex);
	pthread_cond_destroy(&dma_utc.dma_cond);
    assert(close(trans.output_fd) == 0);
	free(dma_buf_chA[0]);
	free(dma_buf_chA[1]);
	free(dma_buf_chB[0]);
	free(dma_buf_chB[1]);
	free(databuffer);
	free(usrbuffer);
    pthread_exit(0);
}

void debug_data_printf(float* input,unsigned int size){
	float val; 
	char command[10];
	while(1){
		for (unsigned int k =0; k<size;k++){
			val =*(input+k);
			printf("No.%d = %.5f\r\n",k,val);
		}

		printf("enter e to debug again\r\n");
		scanf("%s", command);

		 if(command[0] == 'e'){ 
			break;
			}

	}


	
}
/*----------------------------------------------------------------------------
 * DMA File Transfer Functions
 *----------------------------------------------------------------------------*/

int get_fetch_data(float* data, unsigned int pts) {

	if (data ==NULL) return -1;
	if (pts ==0 ) return -1;

	while(!axidma_dataReady){
		usleep(300);
		}

	memcpy(data, dma_buf_chA[(buf_counter+1)%2], pts*sizeof(float));
	axidma_dataReady = 0;

    return 1;
}



/*----------------------------------------------------------------------------
 * Initialization
 *----------------------------------------------------------------------------*/

int transfer_enable()
{
    axidma_dataReady = 0;
    const array_t *rx_chans;	
	int rc;
    // Init dma_utc_inst
    pthread_mutex_init(&dma_utc.dma_mutex, NULL);
    pthread_cond_init(&dma_utc.dma_cond, NULL);
    
    // Parse the input arguments
    memset(&trans, 0, sizeof(trans));    

    // Initialize the AXIDMA device
    axidma_dev = axidma_init();
    if (axidma_dev == NULL) {
        fprintf(stderr, "Error: Failed to initialize the AXI DMA device.\n");        
        return -1;
    }
    	
    rx_chans = axidma_get_dma_rx(axidma_dev);
    if (rx_chans->len < 1) {
        fprintf(stderr, "Error: No receive channels were found.\n");        
        return -1;
    }

    /* If the user didn't specify the channels, we assume that the transmit and
 * receive channels are the lowest numbered ones. */
    trans.output_channel = rx_chans->data[0];


    // Allocate a buffer for the output file
    single_trans_size = DMA_TRANS_SIZE;
    trans.output_size = DMA_TRANS_PTS * sizeof(int16_t); //total size;
    trans.output_buf = (int16_t*) axidma_malloc(axidma_dev, trans.output_size);

	dma_buf[0] = (int16_t*)malloc(n_round*trans.output_size );
	dma_buf[1] = (int16_t*)malloc(n_round*trans.output_size );	
	memset((int16_t*)dma_buf[0],0,n_round*trans.output_size );
	memset((int16_t*)dma_buf[1],0,n_round*trans.output_size );

    if(trans.output_buf == NULL ){
		fprintf(stderr, "Error: unable to allocate the DMAbuffer.\n");  
		return -1;
	}

    printf("[info] successfully allocate DMA buf address: %p with the size of %d bytes\n\r", trans.output_buf, trans.output_size);
	int buf_size = n_round * AD_CH_PTS * sizeof(float32_t);
	dma_buf_chA[0] = (float32_t*)malloc(buf_size);
	dma_buf_chA[1] = (float32_t*)malloc(buf_size);	
	memset((float32_t*)dma_buf_chA[0],0,buf_size);
	memset((float32_t*)dma_buf_chA[1],0,buf_size);

	dma_buf_chB[0] = (float32_t*)malloc(buf_size);
	dma_buf_chB[1] = (float32_t*)malloc(buf_size);	
	memset((float32_t*)dma_buf_chB[0],0,buf_size);
	memset((float32_t*)dma_buf_chB[1],0,buf_size);

	usrbuffer = (float32_t*)malloc(buf_size);
	memset((float32_t*)usrbuffer ,0,buf_size );

    databuffer = (int*)malloc(n_round * DMA_TRANS_PTS * sizeof(int));
    memset((int*)databuffer,0,n_round * DMA_TRANS_PTS * sizeof(int));
    
    if(databuffer == NULL){
	    fprintf(stderr, "Error: unable to set databuffer.");     
	return -1;
	}	
	
	/* Sets up a callback function to be called whenever the RX transaction completes */
    axidma_set_callback(axidma_dev, trans.output_channel, dma_callback, (void*) &dma_utc);

	/* Initialize dma_cvt_neon 
	int16_t* vec_zero = (int16_t*) malloc(trans.output_size);
	memset(vec_zero, 0, trans.output_size );

	for(int i=0;i<256;i++){
		dma_cvt_neon(dma_buf[0],vec_zero,DMA_TRANS_PACKS);
	}*/
	
	
    /* Start transmitting... */
	rc = axidma_oneway_transfer(axidma_dev,
	trans.output_channel,
	trans.output_buf,
	trans.output_size,
	false);
	if (rc < 0) {
		fprintf(stderr, "DMA read transaction failed.\n");
		return -1;
	}

    /* Create pthread platform to deal with dma transfer */
    pthread_create(&axidma_thread, NULL, axidma_platform, (void*)"Enable DMA transfer module\r\n");
	
	
	
    return 1;

}

