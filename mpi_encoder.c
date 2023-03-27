#include "mpi_encoder.h"
#include "mpi_frame_process.h"

EncoderData *encoder = NULL;

MPP_RET encoder_init(MppFrame* frame,RK_S32 fps)
{
	
	MPP_RET ret = MPP_OK;	
	encoder = mpp_calloc(EncoderData, 1);
	
	if (!encoder) {
        mpp_err_f("create MpiEncTestData failed\n");
        ret = MPP_ERR_MALLOC;
        return ret;
    }
	
	encoder->width        = mpp_frame_get_width(*frame);
    encoder->height       = mpp_frame_get_height(*frame);
	encoder->hor_stride   = mpp_frame_get_hor_stride(*frame);
    encoder->ver_stride   = mpp_frame_get_ver_stride(*frame);
	
	encoder->packet	= NULL;
	encoder->pkt_eos	=0;
	
	encoder->frame_count	= 0;
	encoder->stream_size	= 0;
	
	encoder->fp_input	= NULL;
	encoder->fp_output	= NULL;
	

	
    encoder->fmt          = mpp_frame_get_fmt(*frame);
    encoder->type         = MPP_VIDEO_CodingAVC;
	encoder->sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
	
	
	/// frame size //
	
	
	switch (encoder->fmt & MPP_FRAME_FMT_MASK) {
    case MPP_FMT_YUV420SP:
    case MPP_FMT_YUV420P: {
        encoder->frame_size = MPP_ALIGN(encoder->hor_stride, 64) * MPP_ALIGN(encoder->ver_stride, 64) * 3 / 2;
    } break;

    case MPP_FMT_YUV422_YUYV :
    case MPP_FMT_YUV422_YVYU :
    case MPP_FMT_YUV422_UYVY :
    case MPP_FMT_YUV422_VYUY :
    case MPP_FMT_YUV422P :
    case MPP_FMT_YUV422SP :
    case MPP_FMT_RGB444 :
    case MPP_FMT_BGR444 :
    case MPP_FMT_RGB555 :
    case MPP_FMT_BGR555 :
    case MPP_FMT_RGB565 :
    case MPP_FMT_BGR565 : {
        encoder->frame_size = MPP_ALIGN(encoder->hor_stride, 64) * MPP_ALIGN(encoder->ver_stride, 64) * 2;
    } break;
	
	 default: {
        encoder->frame_size = MPP_ALIGN(encoder->hor_stride, 64) * MPP_ALIGN(encoder->ver_stride, 64) * 4;
    } break;
    }
	
	//////
	
	ret = mpp_create(&encoder->ctx, &encoder->mpi);
    if (ret) {
        mpp_err("mpp_create failed ret %d\n", ret);
        return ret;
    }
	
	ret = mpp_init(encoder->ctx, MPP_CTX_ENC, encoder->type);
    if (ret) {
        mpp_err("mpp_init failed ret %d\n", ret);
        return ret;
    }
	
	ret = mpp_enc_cfg_init(&encoder->cfg);
    if (ret) {
        mpp_err_f("mpp_enc_cfg_init failed ret %d\n", ret);
        return ret;
    }
	
	MppApi *mpi;
    MppCtx ctx;
	MppEncCfg cfg;
    
	mpi = encoder->mpi;
	ctx = encoder->ctx;
	cfg = encoder->cfg;
	
	 /* setup default parameter */
    if (encoder->fps_in_den == 0)
        encoder->fps_in_den = 1;
    if (encoder->fps_in_num == 0)
        encoder->fps_in_num = 30;
    if (encoder->fps_out_den == 0)
        encoder->fps_out_den = 1;
    if (encoder->fps_out_num == 0)
        encoder->fps_out_num = 30;
	
	if (!encoder->bps)
        encoder->bps = encoder->width * encoder->height / 8 * (encoder->fps_out_num / encoder->fps_out_den);
	
	mpp_enc_cfg_set_s32(cfg, "prep:width", encoder->width);
    mpp_enc_cfg_set_s32(cfg, "prep:height", encoder->height);
    mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", encoder->hor_stride);
    mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", encoder->ver_stride);
    mpp_enc_cfg_set_s32(cfg, "prep:format", encoder->fmt);
	
	mpp_enc_cfg_set_s32(cfg, "rc:mode", encoder->rc_mode);
	
	 /* fix input / output frame rate */
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_flex", encoder->fps_in_flex);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", encoder->fps_in_num);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_denorm", encoder->fps_in_den);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_flex", encoder->fps_out_flex);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", encoder->fps_out_num);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_denorm", encoder->fps_out_den);
    mpp_enc_cfg_set_s32(cfg, "rc:gop", encoder->gop_len ? encoder->gop_len : encoder->fps_out_num * 2);

    /* drop frame or not when bitrate overflow */
    mpp_enc_cfg_set_u32(cfg, "rc:drop_mode", MPP_ENC_RC_DROP_FRM_DISABLED);
    mpp_enc_cfg_set_u32(cfg, "rc:drop_thd", 20);        /* 20% of max bps */
    mpp_enc_cfg_set_u32(cfg, "rc:drop_gap", 1);         /* Do not continuous drop frame */
	
	/* setup bitrate for different rc_mode */
    mpp_enc_cfg_set_s32(cfg, "rc:bps_target", encoder->bps);
    switch (encoder->rc_mode) {
    case MPP_ENC_RC_MODE_FIXQP : {
        /* do not setup bitrate on FIXQP mode */
    } break;
    case MPP_ENC_RC_MODE_CBR : {
        /* CBR mode has narrow bound */
        mpp_enc_cfg_set_s32(cfg, "rc:bps_max", encoder->bps_max ? encoder->bps_max : encoder->bps * 17 / 16);
        mpp_enc_cfg_set_s32(cfg, "rc:bps_min", encoder->bps_min ? encoder->bps_min : encoder->bps * 15 / 16);
    } break;
    case MPP_ENC_RC_MODE_VBR :
    case MPP_ENC_RC_MODE_AVBR : {
        /* VBR mode has wide bound */
        mpp_enc_cfg_set_s32(cfg, "rc:bps_max", encoder->bps_max ? encoder->bps_max : encoder->bps * 17 / 16);
        mpp_enc_cfg_set_s32(cfg, "rc:bps_min", encoder->bps_min ? encoder->bps_min : encoder->bps * 1 / 16);
    } break;
    default : {
        /* default use CBR mode */
        mpp_enc_cfg_set_s32(cfg, "rc:bps_max", encoder->bps_max ? encoder->bps_max : encoder->bps * 17 / 16);
        mpp_enc_cfg_set_s32(cfg, "rc:bps_min", encoder->bps_min ? encoder->bps_min : encoder->bps * 15 / 16);
    } break;
    }
	
	 /* setup qp for different codec and rc_mode */
    switch (encoder->type) {
    case MPP_VIDEO_CodingAVC :
    case MPP_VIDEO_CodingHEVC : {
        switch (encoder->rc_mode) {
        case MPP_ENC_RC_MODE_FIXQP : {
            mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 20);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_max", 20);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_min", 20);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 20);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 20);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 2);
        } break;
        case MPP_ENC_RC_MODE_CBR :
        case MPP_ENC_RC_MODE_VBR :
        case MPP_ENC_RC_MODE_AVBR : {
            mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 26);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_max", 51);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_min", 10);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 51);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 10);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 2);
        } break;
        default : {
            mpp_err_f("unsupport encoder rc mode %d\n", encoder->rc_mode);
        } break;
        }
    } break;
    case MPP_VIDEO_CodingVP8 : {
        /* vp8 only setup base qp range */
        mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 40);
        mpp_enc_cfg_set_s32(cfg, "rc:qp_max",  127);
        mpp_enc_cfg_set_s32(cfg, "rc:qp_min",  0);
        mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 127);
        mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 0);
        mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 6);
    } break;
    case MPP_VIDEO_CodingMJPEG : {
        /* jpeg use special codec config to control qtable */
        mpp_enc_cfg_set_s32(cfg, "jpeg:q_factor", 80);
        mpp_enc_cfg_set_s32(cfg, "jpeg:qf_max", 99);
        mpp_enc_cfg_set_s32(cfg, "jpeg:qf_min", 1);
    } break;
    default : {
    } break;
    }

    /* setup codec  */
    mpp_enc_cfg_set_s32(cfg, "codec:type", encoder->type);
    switch (encoder->type) {
    case MPP_VIDEO_CodingAVC : {
        /*
         * H.264 profile_idc parameter
         * 66  - Baseline profile
         * 77  - Main profile
         * 100 - High profile
         */
        mpp_enc_cfg_set_s32(cfg, "h264:profile", 100);
        /*
         * H.264 level_idc parameter
         * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
         * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
         * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
         * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
         * 50 / 51 / 52         - 4K@30fps
         */
        mpp_enc_cfg_set_s32(cfg, "h264:level", 40);
        mpp_enc_cfg_set_s32(cfg, "h264:cabac_en", 1);
        mpp_enc_cfg_set_s32(cfg, "h264:cabac_idc", 0);
        mpp_enc_cfg_set_s32(cfg, "h264:trans8x8", 1);
    } break;
    case MPP_VIDEO_CodingHEVC :
    case MPP_VIDEO_CodingMJPEG :
    case MPP_VIDEO_CodingVP8 : {
    } break;
    default : {
        mpp_err_f("unsupport encoder coding type %d\n", encoder->type);
    } break;
    }

    encoder->split_mode = 0;
    encoder->split_arg = 0;

    mpp_env_get_u32("split_mode", &encoder->split_mode, MPP_ENC_SPLIT_NONE);
    mpp_env_get_u32("split_arg", &encoder->split_arg, 0);

    if (encoder->split_mode) {
        mpp_log("%p split_mode %d split_arg %d\n", ctx, encoder->split_mode, encoder->split_arg);
        mpp_enc_cfg_set_s32(cfg, "split:mode", encoder->split_mode);
        mpp_enc_cfg_set_s32(cfg, "split:arg", encoder->split_arg);
    }

    ret = mpi->control(ctx, MPP_ENC_SET_CFG, cfg);
    if (ret) {
        mpp_err("mpi control enc set cfg failed ret %d\n", ret);
        return ret;
    }

    /* optional */
    encoder->sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
    ret = mpi->control(ctx, MPP_ENC_SET_SEI_CFG, &encoder->sei_mode);
    if (ret) {
        mpp_err("mpi control enc set sei cfg failed ret %d\n", ret);
        return ret;
    }

    if (encoder->type == MPP_VIDEO_CodingAVC || encoder->type == MPP_VIDEO_CodingHEVC) {
        encoder->header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
        ret = mpi->control(ctx, MPP_ENC_SET_HEADER_MODE, &encoder->header_mode);
        if (ret) {
            mpp_err("mpi control enc set header mode failed ret %d\n", ret);
            return ret;
        }
    }

    RK_U32 gop_mode = encoder->gop_mode;

    mpp_env_get_u32("gop_mode", &gop_mode, gop_mode);
    if (gop_mode) {
        MppEncRefCfg ref;

        mpp_enc_ref_cfg_init(&ref);

        if (encoder->gop_mode < 4)
            mpi_enc_gen_ref_cfg(ref, gop_mode);
        else
            mpi_enc_gen_smart_gop_ref_cfg(ref, encoder->gop_len, encoder->vi_len);

        ret = mpi->control(ctx, MPP_ENC_SET_REF_CFG, ref);
        if (ret) {
            mpp_err("mpi control enc set ref cfg failed ret %d\n", ret);
            return ret;
        }
        mpp_enc_ref_cfg_deinit(&ref);
    }

    /* setup test mode by env */
    mpp_env_get_u32("osd_enable", &encoder->osd_enable, 0);
    mpp_env_get_u32("osd_mode", &encoder->osd_mode, MPP_ENC_OSD_PLT_TYPE_DEFAULT);
    mpp_env_get_u32("roi_enable", &encoder->roi_enable, 0);
    mpp_env_get_u32("user_data_enable", &encoder->user_data_enable, 0);
	
	
	
	encoder_get_header();
	
	
	
	return ret;
}

MPP_RET encoder_deinit()
{
    
    if (!encoder) {
        mpp_err_f("invalid input data %p\n", encoder);
        return MPP_ERR_NULL_PTR;
    }

    if (encoder) {
        if (encoder->fp_input) {
            fclose(encoder->fp_input);
            encoder->fp_input = NULL;
        }
        if (encoder->fp_output) {
            fclose(encoder->fp_output);
            encoder->fp_output = NULL;
        }
        MPP_FREE(encoder);
        encoder = NULL;
    }

    return MPP_OK;
}	

MPP_RET encoder_write_packet(FILE *fp_output,MppPacket* packet){
	
		if (*packet && fp_output) {
            
			void *ptr   = mpp_packet_get_pos(*packet);
            size_t len  = mpp_packet_get_length(*packet);
		
            fwrite(ptr, 1, len, fp_output);

          return 1;
		}
	
	return -1;
}	

MPP_RET encoder_write_header(FILE *fp_output){
	
		if (encoder->packet_header && fp_output) {
            
			void *ptr   = mpp_packet_get_pos(encoder->packet_header);
            size_t len  = mpp_packet_get_length(encoder->packet_header);
					
            fwrite(ptr, 1, len, fp_output);

          return 1;
		}
	
	return -1;
}	

MPP_RET encoder_get_header()
{
	static MppBuffer pkt_buf=NULL;
	MPP_RET ret = MPP_OK;
	if(pkt_buf==NULL) {
		mpp_buffer_get(NULL, &pkt_buf,100000);	
		
		mpp_log("type: %d\n",encoder->type);
	}
	switch(encoder->type) {
		
		case MPP_VIDEO_CodingAVC:
			
		mpp_packet_init_with_buffer(&encoder->packet_header, pkt_buf);
        /* NOTE: It is important to clear output packet length!! */
        mpp_packet_set_length(encoder->packet_header, 0);
		
			ret = encoder->mpi->control(encoder->ctx, MPP_ENC_GET_HDR_SYNC, encoder->packet_header);
	
			if (ret) {
				mpp_err("mpi control enc get extra info failed\n");
				return ret;
			}
		
		break;
		 
	}

	return ret;
}

MPP_RET encoder_put(MppFrame* frame)
{
	MPP_RET ret = MPP_OK;
	MppFrame frame2 = NULL;
	frame_copy(frame,&frame2);
	ret = encoder->mpi->encode_put_frame(encoder->ctx, frame2 );
	encoder->frame_count++;
	return ret;
}	

MPP_RET encoder_run(void (*func_ptr)(MppPacket * header,MppPacket * data))
{
	MPP_RET ret = MPP_OK;

	do{
		
		ret = encoder->mpi->encode_get_packet(encoder->ctx, &encoder->packet);
		
		if(ret != 0)
			printf("RET %d\n",ret);
	}while(!encoder->packet);
	
	if (encoder->packet) {
		
		if(*func_ptr)
			(*func_ptr)(&encoder->packet_header ,&encoder->packet);
		
		encoder->pkt_eos = mpp_packet_get_eos(encoder->packet);
		
		mpp_packet_deinit(&encoder->packet);
		
		
	}
	
	return ret;
}	