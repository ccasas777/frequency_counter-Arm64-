#ifndef AXIDMA_TRANSFER_H_
#define AXIDMA_TRANSFER_H_


/*----------------------------------------------------------------------------
* Internal Definitions
*----------------------------------------------------------------------------*/

// A convenient structure to carry information around about the transfer
struct dma_transfer {
    int input_fd;           // The file descriptor for the input file
    int input_channel;      // The channel used to send the data
    int input_size;         // The amount of data to send
    void* input_buf;        // The buffer to hold the input data
    int output_fd;          // The file descriptor for the output file
    int output_channel;     // The channel used to receive the data
    int output_size;        // The amount of data to receive
    int16_t* output_buf; // The buffer to hold the output
};

struct dma_thread_parameters {
    pthread_mutex_t dma_mutex;
    pthread_cond_t dma_cond;
};

extern unsigned char* buf_socket;
int transfer_enable();
int get_fetch_data(float* data, unsigned int pts); 
void debug_data_printf(float* input,unsigned int size);



#endif /* AXIDMA_TRANSFER_H_ */
