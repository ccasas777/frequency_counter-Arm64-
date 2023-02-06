/**
 * @file parameters.h
 * @date Oct. 27, 2021 at 01:10:08 AM EST

 *
 * This file contains parameters for out system.
 *
 * @bug No known bugs.
 **/

#ifndef PARAMETERS_H_
#define PARAMETERS_H_

#define DATA_READ 1
#define DATA_WRITE 0
#define HEADER 16
#define DMA_TRANS_PACKS 65536*4
#define DMA_TRANS_SIZE 1048576
#define DMA_TRANS_PTS 262144 //DMA_TRANS_SIZE/32 bytes * 8 (pts per package)
#define AD_CH_PTS 131072
#define MAX_ROUND 300
#define PHASE_FACTOR 0.002746582





#endif /* PARAMETERS_H_ */
