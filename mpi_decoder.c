

#include "mpi_decoder.h"

DecoderData *decoder = NULL;

int decoder_init()
{
	
	MPP_RET ret         = MPP_OK;
	MpiCmd mpi_cmd      = MPP_CMD_BASE;
	MppCodingType type  = MPP_VIDEO_CodingAVC;
    MppParam param      = NULL;
    RK_U32 need_split   = 1;
    MppPollType timeout = -1;
	
	decoder = mpp_calloc(DecoderData, 1);
	
	// base flow context
    decoder->ctx         = NULL;
    decoder->mpi         = NULL;
	
	decoder->width=0;
	decoder->height=0;
	
	decoder->fp_input=NULL;
	decoder->fp_output=NULL;
	
    // input / output
    decoder->packet    = NULL;
	decoder->packet_size=MPI_DEC_STREAM_SIZE;
	decoder->packet_count	=0;
	
	decoder->frm_grp	=NULL;
    decoder->frame     = NULL;
	decoder->frame_count=0;
	
	// resources
	decoder->frm_eos		=0;
  
	// paramter for resource malloc
	decoder->buf = mpp_malloc(char,decoder->packet_size);

    if (NULL == decoder->buf) {
        mpp_err("mpi_dec_test malloc input stream buffer failed\n");
        return -1;
    }
	
	ret = mpp_packet_init(&decoder->packet,decoder->buf,decoder->packet_size);
    if (ret) {
        mpp_err("mpp_packet_init failed\n");
        return -1;
    }
	
	// decoder demo
    ret = mpp_create(&decoder->ctx, &decoder->mpi);
    if (MPP_OK != ret) {
        mpp_err("mpp_create failed\n");
        return -1;
    }
	
	// NOTE: decoder split mode need to be set before init
    mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
    param = &need_split;
    ret = decoder->mpi->control(decoder->ctx, mpi_cmd, param);
    if (MPP_OK != ret) {
        mpp_err("mpi->control failed\n");
        return -1;
    }
	 
	// NOTE: timeout value please refer to MppPollType definition
    //  0   - non-block call (default)
    // -1   - block call
    // +val - timeout value in ms
	
	if (timeout) {
        param = &timeout;
        ret = decoder->mpi->control(decoder->ctx, MPP_SET_OUTPUT_TIMEOUT, param);
        if (MPP_OK != ret) {
            mpp_err("Failed to set output timeout %d ret %d\n", timeout, ret);
            return -1;
        }
    }
	
	ret = mpp_init(decoder->ctx, MPP_CTX_DEC, type);
    if (MPP_OK != ret) {
        mpp_err("mpp_init failed\n");
        return -1;
    }
	return 1;
	
}	

void decoder_deinit(){
	
	if (decoder->packet) {
        mpp_packet_deinit(&decoder->packet);
        decoder->packet = NULL;
    }

    if (decoder->frame) {
        mpp_frame_deinit(&decoder->frame);
        decoder->frame = NULL;
    }

    if (decoder->ctx) {
        mpp_destroy(decoder->ctx);
        decoder->ctx = NULL;
    }
	
	if (decoder->buf) {
        mpp_free(decoder->buf);
        decoder->buf = NULL;
    }
	
	if (decoder->frm_grp) {
        mpp_buffer_group_put(decoder->frm_grp);
        decoder->frm_grp = NULL;
    }
	
	if (decoder->fp_output) {
        fclose(decoder->fp_output);
        decoder->fp_output = NULL;
    }

    if (decoder->fp_input) {
        fclose(decoder->fp_input);
        decoder->fp_input = NULL;
    }
		
	return;
}


void decoder_put_packet(MppPacket packet)
{
	MPP_RET ret = MPP_OK;
	
	do {
		ret =decoder->mpi->decode_put_packet(decoder->ctx,packet);
	}while (ret != MPP_OK);
	return ;
}	

int  decoder_run(void (*func_frame_change)(MppFrame*),void (*func_frame_get)(MppFrame*))
{
	MPP_RET ret=MPP_OK;
	//Get a frame from decoder
	ret = decoder->mpi->decode_get_frame(decoder->ctx, &decoder->frame);
	
	if (decoder->frame) {
	 
		if (mpp_frame_get_info_change(decoder->frame)) {
			
			decoder->width=mpp_frame_get_width(decoder->frame);
			decoder->height=mpp_frame_get_height(decoder->frame);
			RK_U32 buf_size = mpp_frame_get_buf_size(decoder->frame);
			if (NULL == decoder->frm_grp) {
				/* If buffer group is not set create one and limit it */
				ret = mpp_buffer_group_get_internal(&decoder->frm_grp, MPP_BUFFER_TYPE_ION);
				if (ret) {
					mpp_err("get mpp buffer group failed ret %d\n", ret);
					return -1;
				}

				/* Set buffer to mpp decoder */
				ret = decoder->mpi->control(decoder->ctx, MPP_DEC_SET_EXT_BUF_GROUP, decoder->frm_grp);
				if (ret) {
					mpp_err("set buffer group failed ret %d\n", ret);
					return -1;
				}
			} else {
				/* If old buffer group exist clear it */
				ret = mpp_buffer_group_clear(decoder->frm_grp);
				if (ret) {
					mpp_err("clear buffer group failed ret %d\n", ret);
					return -1;
				}
			}
					
			/* Use limit config to limit buffer count to 24 with buf_size */
			ret = mpp_buffer_group_limit_config(decoder->frm_grp, buf_size, 48);
			if (ret) {
				mpp_err("limit buffer group failed ret %d\n", ret);
				return -1;
			}
						
					
			ret = decoder->mpi->control(decoder->ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
			if (ret) {
				mpp_err("info change ready failed ret %d\n", ret);
				return -1;
			}
			
			if(*func_frame_change)
				(*func_frame_change)(&decoder->frame);
		
			
			
		}else {
			 RK_U32 err_info = mpp_frame_get_errinfo(decoder->frame);
             RK_U32 discard = mpp_frame_get_discard(decoder->frame);
		     printf("err_info %x discard %x\n",err_info,discard);
			
			
			
			
			if(err_info==0 && discard==0) {
				if(*func_frame_get)
					(*func_frame_get)(&decoder->frame);
			}
			
			decoder->frm_eos=mpp_frame_get_eos(decoder->frame);
			decoder->frame_count++;
		}	
	
	
	
			mpp_frame_deinit(&decoder->frame);
			decoder->frame = NULL;
	
	}
	
}	
