#include "gn_dvb.h"
#include"gn_tcp.h"
#include "ts_packet.h"
#include "gn_video_resize.h"
#include "conver_format.h"
#include "webserver.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <signal.h>

//new folder
#include <sys/stat.h>
#include <sys/types.h>

#define FRONTEND_DEV "/dev/dvb/adapter0/frontend0"
#define DEMUX_DEV "/dev/dvb/adapter0/demux0"
#define CHANNEL_FILE "/channels.conf"
#define	FRAME_WIDTH 1280
#define	FRAME_HEIGHT 720

void *msg_print(void *arg);

struct gn_program *program;
struct gn_dvb *device;
int channel;
char buffer[188];
FILE *pFile_out;
int ischange=0;

pthread_attr_t attr;
pthread_t thd_tcp;
pthread_t thd_audio;
pthread_t thd_msg;

sem_t semaphore_server; //flag for server
void *tcp_sender(void *arg)
{
		
	int len;
	int ret;
	char buf[10];
	
	while(1) {
		//channel = 11;
		//ischange=1;
		gn_tcp_listener();
		sleep(2);
		printf("Server start....\n");
		sem_post(&semaphore_server);
		
		//pthread_create(&thd_msg, &attr, msg_print, NULL);
		
		static int error_count=0;
		while(1) {
			len = gn_tcp_send();
			
			
			 if(IsSocketClosed())
				 break;
			 
			 if(gn_tcp_read(buf) > 0 ){
				 channel=buf[0];
				 printf("Get Data... %d\n",channel);
				 ischange=1;
			  }	 
			 // printf(".....\n");
		
		}
	}	
}

void on_websocket_recevice(void *in, size_t len )
{
	char* c = in;
	
	if(len > 0 && len < 8) {
		channel=atoi(c);
		printf("%d\n",channel);
		ischange=1;
	}
}

void *msg_print(void *arg)
{
	int16_t snr;
	uint32_t ber;
	int16_t  strength;
	
	web_server_init();
	addwebsocketCallback(on_websocket_recevice);
	
	while(1) {
		web_server_run();
	}	
	
	//while(1) {
		
		// ret=ioctl(device->frontend_fd, FE_READ_BER, &ber);
		// if(ret!=0) {
			// printf("BER GET FAILD \n");
		// }
		
		// ret=ioctl(device->frontend_fd, FE_READ_SNR, &snr);
		// if(ret!=0) {
			// printf("SNR GET FAILD \n");
		// }
		
		// ret=ioctl(device->frontend_fd, FE_READ_SIGNAL_STRENGTH, &strength);
		// if(ret!=0) {
			// printf("STRENGTH GET FAILD \n");
		// }
//		if(strength!=0)
		//printf("SNR: %d BER: %u Strength %d\n",snr,ber,strength);
		
		// sleep(20);
		// channel++;
		// ret = dvb_open_chennel(device,&program[channel]);
		
	//}
	
}
uint8_t schedule_channel[10];
uint32_t schedule_time[10][2];
uint32_t schedule_index = 0;
uint32_t schedule_len = 0;
int isSave=0;

uint32_t getAlarmCount(uint32_t furter_sec)
{
	uint32_t count;
	time_t time_ptr;
    struct tm *tmp_ptr = NULL;
	int year,month,day,hour,minute,sec;
	
	time(&time_ptr);
    tmp_ptr = localtime(&time_ptr);
    hour = tmp_ptr->tm_hour;
    minute = tmp_ptr->tm_min;
    sec = tmp_ptr->tm_sec;
	
	uint32_t current = hour*3600 + minute*60 + sec;
	
	if(current > furter_sec)
		count = 86400 - current + furter_sec;
	else
		count = furter_sec - current;
	
	if(!count)
		count+=1;
	printf("count: %d\n",count);
	return count;
}

void schedule_alarm()
{
	time_t time_ptr;
    struct tm *tmp_ptr = NULL;
	int year,month,day,hour,minute,sec;
	char * path = "/mnt/sda1/record/";
	char file[100];
	char folder[50];
	
	
	time(&time_ptr);
    tmp_ptr = localtime(&time_ptr);
	
	year = tmp_ptr->tm_year + 1900;
    month = tmp_ptr->tm_mon + 1;
    day = tmp_ptr->tm_mday;

    hour = tmp_ptr->tm_hour;
    minute = tmp_ptr->tm_min;
    sec = tmp_ptr->tm_sec;
	
	
	
	if(!isSave) {
		
		uint32_t play_time = schedule_time[schedule_index][1] - schedule_time[schedule_index][0];
		
		if(getAlarmCount(schedule_time[schedule_index][1]) > play_time) {
			printf("record ending...\n");
			return;
		}	
		
		channel = schedule_channel[schedule_index];
		ischange=1;
		sprintf(folder,"%s%02d%02d/",path,hour ,minute);
		mkdir(folder);
		sprintf(file,"%s%04d%02d%02d%02d%02d.ts",folder ,year ,month ,day ,hour ,minute);
		printf("%s\n",file);
		pFile_out = fopen( file,"wb" );
		alarm(getAlarmCount(schedule_time[schedule_index][1]));
		schedule_index++;
		schedule_index%=schedule_len;
		isSave = 1;
		printf("record start...\n");
	}else{
		
		uint32_t next_schedule_start = getAlarmCount(schedule_time[schedule_index][0]);
		uint32_t next_schedule_end = getAlarmCount(schedule_time[schedule_index][1]);
		printf("start %d end %d\n",next_schedule_start ,next_schedule_end);
		isSave = 0;	
		if(next_schedule_start > schedule_time[schedule_index][0] && next_schedule_end < schedule_time[schedule_index][1]){
			printf("Start record after 10 sec\n");
			alarm(10);
		}else{
			printf("record ending...\n");
			alarm(getAlarmCount(schedule_time[schedule_index][0]));
		}
		
	}	
	
	
}

void recoder_init()
{
	FILE *schedule_file = fopen("recoder", "r");
	
	char *line_buf = NULL;
	size_t line_size = 0;
	int line_len = 0;
	
	int i=0;
	while ((line_len = getline(&line_buf, &line_size, schedule_file)) > 0 ) {
		
		char* channel = strtok(line_buf, ",");
		char* start = strtok(NULL, ",");
		char* end = strtok(NULL, ",");
		printf("Channel %s Start %s End %s\n",channel,start ,end);
		char* h = strtok(start, ":");
		char* m = strtok(NULL, ":");
		schedule_channel[i] = atoi(channel);
		schedule_time[i][0] = atoi(h)*3600 + atoi(m)*60;
		
		h = strtok(end, ":");
		m = strtok(NULL, ":");
		schedule_time[i][1] = atoi(h)*3600 + atoi(m)*60;
		i++;
	}
	 schedule_len=i;
	 signal(SIGALRM, schedule_alarm);
	 
	 uint32_t next_schedule_start = getAlarmCount(schedule_time[schedule_index][0]);
	 uint32_t next_schedule_end = getAlarmCount(schedule_time[schedule_index][1]);
	 
    if(next_schedule_start > schedule_time[schedule_index][0] && next_schedule_end < schedule_time[schedule_index][1]){
			printf("Start record after 10 sec\n");
			alarm(10);
	}else{
			alarm(getAlarmCount(schedule_time[schedule_index][0]));
	}
}

#define	PID 4000
#define	PID_VIDEO 4001
#define	PID_AUDIO 4002




char pid_0[188] = {0x47, 0x40, 0x00, 0x13, 0x00, 0x00, 0xB0, 0x1D, 0x00, 0x20,
					 0xDD, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x10, 0x01, 0x90, 0xEF,
					  0xA0, 0x01, 0x91, 0xEF, 0xAA, 0x01, 0x92, 0xEF, 0xB4, 0x01,
					   0x93, 0xEF, 0xBE, 0xE2, 0xE6, 0xC1, 0x15, 0xFF, 0xFF, 0xFF,
						0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
									0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
									 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
									   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
									   
char pid_4000[188] = {0x47, 0x4F, 0xA0, 0x15, 0x00, 0x02, 0xB0, 0x2A, 0x01, 0x90,
				   0xC3, 0x00, 0x00, 0xEF, 0xA1, 0xF0, 0x00, 0x1B, 0xEF, 0xA1,
				    0xF0, 0x00, 0x11, 0xEF, 0xA2, 0xF0, 0x07, 0x1C, 0x01, 0x58,
				     0x7C, 0x02, 0x58, 0x00, 0x11, 0xEF, 0xA3, 0xF0, 0x07, 0x1C,
				      0x01, 0x58, 0x7C, 0x02, 0x58, 0x00, 0x90, 0xB2, 0x3F, 0xD6,
					   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				     	    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};									   
									   
									   
int channel_temp;

int main(int argc, char *argv[])
{	
	int ret;
	FILE *channel_file = fopen(CHANNEL_FILE, "r");
	int channels_nums = dvbcfg_zapchannel_parse(channel_file,&program);
	//pFile_out = fopen( "/mnt/sda1/record/stream.ts","wb" );
	dvb_open_device(FRONTEND_DEV,DEMUX_DEV,&device);
	ret=ioctl(device->demux_fd, DMX_SET_BUFFER_SIZE,20000);
	int fd = open("bug2.ts", O_RDONLY);
	//sleep(20);
	
	
	if(ret!=0) {
		printf("set buffer faild \n");
	}
	
	channel = atoi(argv[1]);
	ret = dvb_open_chennel(device,&program[channel]);
	channel_temp = channel;
		
	if(ret) {
		printf("open channel faild \n");
	}
	
	pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	gn_tcp_init();
	sem_init(&semaphore_server, 1, 0);
	ret = pthread_create(&thd_tcp, &attr, tcp_sender, NULL);
    if (ret) {
        mpp_err("failed to create thread for decode_video ret %d\n", ret);
        return 1;
    }
	
	 ret = pthread_create(&thd_msg, &attr, msg_print, NULL);
     if (ret) {
         mpp_err("failed to create msg_print %d\n", ret);
         return 1;
     }

	recoder_init();
	
	
	 
	while(1) {
	
		while(!ischange) {
			uint8_t buffer[TS_PACKET_SIZE];
			int read_size = read( device->demux_fd ,buffer ,TS_PACKET_SIZE);
			//printf("read_size %d\n",read_size);
			//gn_tcp_put(buffer);
			
			
			 uint16_t pid = buffer[1] << 8 | buffer[2];
		// printf("PID %d %d\n",pid,program[channel_temp].video_pid);
			pid &= 0x1FFF;
			
			
			if(pid!=0 && pid != program[channel_temp].video_pid - 1) {
			
				if(pid == program[channel_temp].video_pid) {
					buffer[1] &= 0xE0;
					buffer[1] |= PID_VIDEO >> 8;
					buffer[2] = PID_VIDEO & 0xff;
				}else if(pid == program[channel_temp].audio_pid) {
					buffer[1] &= 0xE0;
					buffer[1] |= PID_AUDIO >> 8;
					buffer[2] = PID_AUDIO & 0xff;
					
				}else if(pid == program[channel_temp].video_pid - 1) {
					// fwrite(buffer, 1, read_size, pFile_out);
					// buffer[1] &= 0xE0;
					// buffer[1] |= PID >> 8;
					// buffer[2] = PID & 0xff;
					
				}
				gn_tcp_put(buffer);
			}else if(pid == 0){
				//printf("PID %d\n",pid);
				memcpy(buffer, pid_0, TS_PACKET_SIZE);
				gn_tcp_put(buffer);
			}else if(pid == program[channel_temp].video_pid - 1){
				memcpy(buffer, pid_4000, TS_PACKET_SIZE);
				gn_tcp_put(pid_4000);
			}		
			//isSave=1;
			
			if(isSave && read_size>0) {
				//printf("write...\n");
				fwrite(buffer, 1, read_size, pFile_out);
			}	
			
		}
		
		ret = dvb_open_chennel(device,&program[channel]);
		channel_temp = channel;
		ischange=0;
		
	}
	
	return 0;
	
	
}
	
