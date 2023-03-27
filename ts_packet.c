#include"ts_packet.h"

int is_continuous_zero(unsigned char *buf,int p){

	if(buf[p] == 0x00 && buf[p+1] == 0x00 ){

		if(buf[p+2] == 0x01 )
			return p+3;
		else if(buf[p+2] == 0x00 && buf[p+3] == 0x01)
			return p+4;
	
	}
	
	return -1;
}

uint64_t conver_pts(char* pts)
{
	uint64_t pts_0 = (pts[0] >> 1) & 0x07;
	uint64_t pts_1 = pts[1];
	uint64_t pts_2 = pts[2] >> 1;
	uint64_t pts_3 = pts[3];
	uint64_t pts_4 = pts[4] >> 1;
	
	return (pts_0 << 30) + (pts_1 << 22) + (pts_2 << 15) + (pts_3 << 7) + (pts_4 );
	
}

int ts_packet_update(Ts_packet *packet)
{
	packet->ts_header.transport_error_indicator=(packet->buffer[1]&0x80) >> 7;	
	packet->ts_header.payload_unit_start_indicator=(packet->buffer[1]&0x40) >> 6;
	packet->ts_header.transport_priority=(packet->buffer[1]&0x20) >> 5;
	packet->ts_header.pid=(packet->buffer[1]&0x1f);
	packet->ts_header.pid=packet->ts_header.pid<<8;
	packet->ts_header.pid=packet->ts_header.pid^packet->buffer[2];
	packet->ts_header.transport_scrambling_control=(packet->buffer[3]&0xC0) >> 6;
	packet->ts_header.adaptation_field_control=(packet->buffer[3]&0x30) >> 4;
	packet->ts_header.continuity_counter=packet->buffer[3]&0x0f;
	packet->adapt.flag = (packet->ts_header.adaptation_field_control&0x02) >> 1;
	packet->pes_header.flag = 0;
	packet->payload.flag= 0;
	
	int flag_adapt = packet->adapt.flag;
	int flag_payload = packet->ts_header.adaptation_field_control&0x01;
	
	if(packet->ts_header.transport_error_indicator==1)
		return -1;
	
	if(flag_adapt){
		
		packet->adapt.flag=1;
		packet->adapt.adaptation_field_length=packet->buffer[4];
		packet->adapt.flag_pcr=packet->buffer[5];
		
		if(packet->adapt.flag_pcr == 0x50){
			
			packet->adapt.PCR[0]=packet->buffer[6];
			packet->adapt.PCR[1]=packet->buffer[7];
			packet->adapt.PCR[2]=packet->buffer[8];
			packet->adapt.PCR[3]=packet->buffer[9];
			packet->adapt.PCR[4]=packet->buffer[10];
		}
			
	}
	
	if(flag_payload){
		
		int payload_offset=4;
		
		if(flag_adapt)
			payload_offset=payload_offset+packet->adapt.adaptation_field_length+1;
		
		int zero_p=is_continuous_zero(packet->buffer,payload_offset);
		int stream_id=packet->buffer[zero_p];
		//printf("zero_p= %d\n",zero_p);	
		
		if(zero_p>0 && ( (stream_id >= 0xe0 && stream_id <= 0xef) || (stream_id >= 0xc0 && stream_id <= 0xdf) ) ){
			packet->pes_header.flag=1;
			
			
			packet->pes_header.stream_id=stream_id;
			packet->pes_header.packet_length=packet->buffer[zero_p+1];
			packet->pes_header.packet_length=packet->pes_header.packet_length<<8;
			packet->pes_header.packet_length=packet->pes_header.packet_length^packet->buffer[zero_p+2];
			packet->pes_header.flag_priority=packet->buffer[zero_p+3];
		    int flag_pts_dts=packet->buffer[zero_p+4];
			packet->pes_header.data_length=packet->buffer[zero_p+5];
			payload_offset=zero_p+5+packet->pes_header.data_length+1;
			
			if(flag_pts_dts ==0x80 || flag_pts_dts ==0xc0){
				packet->pes_header.flag_pts = 1;
				packet->pes_header.pts[0]=packet->buffer[zero_p+6];
				packet->pes_header.pts[1]=packet->buffer[zero_p+7];
				packet->pes_header.pts[2]=packet->buffer[zero_p+8];
				packet->pes_header.pts[3]=packet->buffer[zero_p+9];
				packet->pes_header.pts[4]=packet->buffer[zero_p+10];
					
				if(flag_pts_dts ==0xc0) {
					packet->pes_header.flag_dts = 1;	
					packet->pes_header.dts[0]=packet->buffer[zero_p+11];
					packet->pes_header.dts[1]=packet->buffer[zero_p+12];
					packet->pes_header.dts[2]=packet->buffer[zero_p+13];
					packet->pes_header.dts[3]=packet->buffer[zero_p+14];
					packet->pes_header.dts[4]=packet->buffer[zero_p+15];
						
				}			
			}			
		}	
		
		
		/////////////
		
		packet->payload.flag=1;
		packet->payload.offset=payload_offset;
		packet->payload.size=TS_PACKET_SIZE-payload_offset;
	}
	
	
}

	

int ts_read_packet(int  fp ,void (*video_func_ptr)(Ts_packet*) ,void (*audio_func_ptr)(Ts_packet*) ,void (*other_func_ptr)(Ts_packet*) ,void (*es_packet_func_ptr)(MppPacket))
{
	
	int video_pid = -1;
	int audio_pid = -1;
	uint64_t video_pts = 0;
	int audid_pts = -1;
	int read_size;
	MppPacket es_packet;
	char es_buffer[8000000] = {0};
	uint64_t es_len = 0;
	mpp_packet_init(&es_packet,es_buffer,2000000);
	static FILE *pFile_stream = NULL;
	static FILE *pFile_save = NULL;
   
   if(pFile_stream == NULL)
		pFile_stream = fopen( "stream.ts","wb" );
	
	if(pFile_save == NULL)
		pFile_save = fopen( "bug.ts","wb" );
	
	int max = 0;
	do{
		
		Ts_packet ts_packet;
		//read_size = fread(ts_packet.buffer, 1, TS_PACKET_SIZE, pFile_read);
		//printf("########## dvb fp:%d\n",fp);
		read_size = read( fp ,ts_packet.buffer ,TS_PACKET_SIZE);
		fwrite(ts_packet.buffer, 1, read_size, pFile_stream);
				
		if(read_size > 0) {
		
		
			if(read_size != TS_PACKET_SIZE) {
				
				printf("read_size:%d\n",read_size);
				continue;
			}
			/*
			int fileSize = ftell(pFile_stream);
		
			//if(fileSize > 20000000)
			//	rewind(pFile_stream);
		
			//fwrite(ts_packet.buffer, 1, read_size, pFile_stream);
			*/
			//printf("read_size %d\n",read_size);

			ts_packet_update(&ts_packet);
		
			if(ts_packet.ts_header.transport_error_indicator) {
				//printf("transport_error\n");
				continue;
			}	
				
			if(ts_packet.pes_header.flag) { 	
				if((ts_packet.pes_header.stream_id >= 0xe0 && ts_packet.pes_header.stream_id <= 0xef))
					video_pid = ts_packet.ts_header.pid;
				else if((ts_packet.pes_header.stream_id >= 0xc0 && ts_packet.pes_header.stream_id <= 0xdf))
					audio_pid = ts_packet.ts_header.pid;
			}
			//printf("%d %ld\n",ts_packet.payload.offset,ts_packet.payload.size);
			//printf("\n",packet->payload.size,packet->payload.offset,);
            //printf("video_pid %d audio_pid %d\n",video_pid,audio_pid);			
			
			if(video_pid != -1 && audio_pid != -1) {
			
				if(ts_packet.ts_header.pid == video_pid) {
						
					if(video_pts) {
						//printf("%d %d %ld\n",max,ts_packet.payload.offset,ts_packet.payload.size);
						// if(max < es_len) {
								// max = es_len;
								// printf("%d\n",max);
						// }	
						
						//printf("copy finish \n");
						if( (ts_packet.payload.offset <= 0) || (ts_packet.payload.offset >= 188) 
								 || (ts_packet.payload.size <= 0) || (ts_packet.payload.size >= 188)) {
								static uint64_t count_error =0;
								ts_packet.buffer[0] = count_error++;
								printf("%ld %d %ld %d %ld\n" ,count_error ,read_size ,es_len,ts_packet.payload.offset,ts_packet.payload.size);
								//fwrite(ts_packet.buffer, 1, read_size, pFile_save);
								fflush(pFile_save);
								
								continue;
					    }			
						//fwrite(ts_packet.buffer, 1, read_size, pFile_save);
						
						
						memcpy(&es_buffer[es_len], &ts_packet.buffer[ts_packet.payload.offset], ts_packet.payload.size);
						es_len += ts_packet.payload.size;
					}
					
					if(ts_packet.pes_header.flag_pts)
						video_pts = conver_pts(ts_packet.pes_header.pts);
					
					if(ts_packet.ts_header.payload_unit_start_indicator) {
						//printf("%ld %d %ld\n" ,es_len,ts_packet.payload.offset,ts_packet.payload.size);	
						//printf("%x %x %x %x %x\n" ,es_buffer[0] ,es_buffer[1] ,es_buffer[2] ,es_buffer[3] ,es_buffer[4]);	
						mpp_packet_set_pos(es_packet, es_buffer);
						mpp_packet_set_length(es_packet, es_len);
						mpp_packet_set_pts(es_packet, video_pts);
						//fwrite(es_buffer, 1, es_len, pFile_save);
						(*es_packet_func_ptr)(es_packet);
						es_len = 0;
					}	
					
					(*video_func_ptr)(&ts_packet);	
				}else if(ts_packet.ts_header.pid == audio_pid) {
					(*audio_func_ptr)(&ts_packet);
				}else { 
				    (*other_func_ptr)(&ts_packet);
				}
			}
			
		}
		
		
	}while(read_size > 0);
	
	return read_size;
}
