#include "gn_video_resize.h"
#include <semaphore.h>

#define BUFFER_SIZE 300
#define	FPS	30

MppFrame frame_buffer[BUFFER_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t semaphore; //flag for frame_buffer
uint16_t head;
uint16_t tail;

int resize_width;
int resize_heigh;

pthread_attr_t attr;
pthread_t thd_decoder;
pthread_t thd_encoder;

FILE *yuv_src;
FILE *yuv_out;

void (*on_encoder_get_packet_ptr)(MppPacket * header ,MppPacket * packet); 

int gn_video_resize_init(int width ,int heigh)
{
	int ret;
	ret = decoder_init();
	resize_width = width;
	resize_heigh = heigh;
	
	pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	yuv_src = fopen( "/mnt/sda1/src.yuv","wb" );
	yuv_out = fopen( "/mnt/sda1/yuv.yuv","wb" );
	head = 0;
    tail = 0;
	sem_init(&semaphore, 1, 0);
		
}

void gn_video_resize_put(MppPacket packet)
{
	decoder_put_packet(packet);
	
}	

void on_decoder_frame_changed(MppFrame* frame)
{
	printf("frame change\n");
	for(int i=0;i<BUFFER_SIZE;i++) {
		frame_init(frame,&frame_buffer[i],resize_width,resize_heigh);
	}
}

void on_decoder_get_frame(MppFrame* frame)
{
	MppBuffer src_buf=NULL;
	void *src_ptr=NULL;
	
	pthread_mutex_lock(&mutex); 

	 if( head == ( (tail+1) % BUFFER_SIZE ) ){
		
		 return ;
	}
	
	frame_resize(frame,&frame_buffer[tail]);
	
	// src_buf=mpp_frame_get_buffer(*frame);
	// src_ptr = mpp_buffer_get_ptr(src_buf);
	// size_t len  = mpp_buffer_get_size(src_buf);
	//printf("on_decoder_get_frame %ld\n",len);
	// fwrite(src_ptr, 1, len, yuv_src);
	
	// src_buf=mpp_frame_get_buffer(frame_buffer[tail]);
	// src_ptr = mpp_buffer_get_ptr(src_buf);
	// len  = mpp_buffer_get_size(src_buf);
	//printf("on_decoder_get_frame %ld\n",len);
	// fwrite(src_ptr, 1, len, yuv_out);
	
	
	
	tail++;
    tail = tail % BUFFER_SIZE;
	//printf("decoder:tail:%d\n",tail);
	pthread_mutex_unlock(&mutex);
	sem_post(&semaphore);
}	

void *decode_video(void *arg)
{
	do{
		decoder_run(on_decoder_frame_changed,on_decoder_get_frame);
	}while(1);
}

void *encode_video(void *arg)
{
	
	while(tail==0){}
	encoder_init(&frame_buffer[head],FPS);
	
	do{
		sem_wait(&semaphore);       
		pthread_mutex_lock(&mutex); 
		encoder_put(&frame_buffer[head]);
		encoder_run(on_encoder_get_packet_ptr);
		head++;
		head = head % BUFFER_SIZE;
		pthread_mutex_unlock(&mutex);
	}while(1);

	 
	
}	

int gn_video_resize_run(void (*func_ptr)(MppPacket * header,MppPacket * data))
{
	
	int ret;
	
	on_encoder_get_packet_ptr = func_ptr;
	/*Decode thread*/
	
	ret = pthread_create(&thd_decoder, &attr, decode_video, NULL);
    if (ret) {
        mpp_err("failed to create thread for decode_video ret %d\n", ret);
        return 1;
    
	}
	
	/*Encode thread*/
	
	ret = pthread_create(&thd_encoder, &attr, encode_video, NULL);
     if (ret) {
         mpp_err("failed to create thread for decode_video ret %d\n", ret);
         return 1;
     }
	
	
		
}		