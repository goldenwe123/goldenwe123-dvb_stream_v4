
#ifndef TS_PACKET_H_
#define TS_PACKET_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rk_mpi.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_env.h"
#include "mpp_time.h"
#include "mpp_common.h"

#define TS_PACKET_SIZE 188
#define TS_SYNC_BYTE 0x47

typedef struct {
	
	unsigned char sync_byte; //Sync byte
	unsigned char transport_error_indicator; //If 1, it means at least one error
	unsigned char payload_unit_start_indicator;
	unsigned char transport_priority;
	unsigned short pid;
	unsigned char transport_scrambling_control;
	unsigned char adaptation_field_control;
	unsigned char continuity_counter ;

}Ts_header;

typedef struct {
	
	unsigned char flag;
	unsigned char adaptation_field_length;
	unsigned char flag_pcr; //0x50 have pcr 0x40 no pcr 
	unsigned char PCR[5];
	
}Adaptation;

typedef struct {
	
	unsigned char flag;
	unsigned char stream_id;
	unsigned short packet_length;
	unsigned char flag_priority;
	unsigned char flag_pts;
	unsigned char flag_dts;
	unsigned char data_length;
	unsigned char pts[5];
	unsigned char dts[5];

}Pes_header;

typedef struct {
	
	unsigned char flag;
	size_t size;
	int offset;	
}Payload;

typedef struct {
	
	Ts_header	ts_header;
	Adaptation	adapt;
	Pes_header	pes_header;
	Payload	payload;
	unsigned char buffer[TS_PACKET_SIZE];

}Ts_packet;

int ts_read_packet(int  fp ,void (*video_func_ptr)(Ts_packet*) ,void (*audio_func_ptr)(Ts_packet*) ,void (*other_func_ptr)(Ts_packet*) ,void (*es_packet_func_ptr)(MppPacket));

#endif /*TS_PACKET_H_*/
