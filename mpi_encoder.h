#include "rk_mpi.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_env.h"
#include "mpp_time.h"
#include "mpp_common.h"
#include "mpi_enc_utils.h"

typedef struct {
	
	RK_U32 width;
    RK_U32 height;
	RK_U32 hor_stride;
    RK_U32 ver_stride;
	
	MppPacket packet;
	MppPacket packet_header;
	RK_U32 pkt_eos;
	
    RK_U32 frame_count;
    RK_U64 stream_size;

    // src and dst
    FILE *fp_input;
    FILE *fp_output;
	
	// rate control runtime parameter

    RK_S32 gop_mode;
    RK_S32 gop_len;
    RK_S32 fps;
    RK_S32 bps;
	RK_S32 bps_max;
    RK_S32 bps_min;
	
	RK_S32 vi_len;

    // paramter for resource malloc
    MppFrameFormat fmt;
    MppCodingType type;
	
	// input / output
    MppEncSeiMode sei_mode;
	
	// base flow context
    MppCtx ctx;
    MppApi *mpi;
	MppEncCfg cfg;
    
    // resources
    size_t frame_size;
    /* NOTE: packet buffer may overflow */
    size_t packet_size;
	
	
	//new
	RK_S32 fps_in_flex;
    RK_S32 fps_in_den;
    RK_S32 fps_in_num;
    RK_S32 fps_out_flex;
    RK_S32 fps_out_den;
    RK_S32 fps_out_num;
	
	RK_S32 rc_mode;
	RK_U32 osd_enable;
    RK_U32 osd_mode;
    RK_U32 split_mode;
    RK_U32 split_arg;
	
	MppEncHeaderMode header_mode;
	RK_U32 user_data_enable;
    RK_U32 roi_enable;
	
} EncoderData;

MPP_RET frame_copy(MppFrame* src_frame,MppFrame* dst_frame);

MPP_RET encoder_init(MppFrame* frame,RK_S32 fps);

MPP_RET encoder_deinit();

MPP_RET encoder_get_header();

MPP_RET encoder_write_packet(FILE *fp_output,MppPacket* packet);

MPP_RET encoder_write_header(FILE *fp_output);

MPP_RET encoder_put(MppFrame* frame);

MPP_RET encoder_run(void (*func_ptr)(MppPacket * header,MppPacket * data));

