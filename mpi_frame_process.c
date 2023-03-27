#include "mpi_frame_process.h"
#include "RgaApi.h"

int frame_copy(MppFrame* src_frame,MppFrame* dst_frame){
	
		MPP_RET ret;		
		RK_U32 w = mpp_frame_get_width(*src_frame);
		RK_U32 h = mpp_frame_get_height(*src_frame);
		RK_U32 hor_stride = mpp_frame_get_hor_stride(*src_frame);
		RK_U32 ver_stride = mpp_frame_get_ver_stride(*src_frame);
		RK_U32 fmt = mpp_frame_get_fmt(*src_frame);

		ret = mpp_frame_init(dst_frame);
        if (ret) {
            mpp_err_f("mpp_frame_init failed\n");
            return -1;
        }
			
		mpp_frame_set_width(*dst_frame, w);
		mpp_frame_set_height(*dst_frame, h);
		mpp_frame_set_hor_stride(*dst_frame, hor_stride);
		mpp_frame_set_ver_stride(*dst_frame, ver_stride);
		mpp_frame_set_fmt(*dst_frame, fmt);
		mpp_frame_set_pts(*dst_frame,mpp_frame_get_pts(*src_frame));
		mpp_frame_set_eos(*dst_frame,mpp_frame_get_eos(*src_frame));
		size_t frame_size=hor_stride * ver_stride * 3 / 2;
		
		MppBuffer buf=NULL;
		buf =mpp_frame_get_buffer(*src_frame);	
		mpp_frame_set_buffer(*dst_frame,buf);
	
	return 1;
}

int frame_init(MppFrame* src_frame,MppFrame* dst_frame,RK_U32 w,RK_U32 h){
	
		MPP_RET ret;		
		RK_U32 hor_stride = MPP_ALIGN(w, 8);
		RK_U32 ver_stride = MPP_ALIGN(h, 8);
		RK_U32 fmt = mpp_frame_get_fmt(*src_frame);

		ret = mpp_frame_init(dst_frame);
        if (ret) {
            mpp_err_f("mpp_frame_init failed\n");
            return -1;
        }
			
		mpp_frame_set_width(*dst_frame, w);
		mpp_frame_set_height(*dst_frame, h);
		mpp_frame_set_hor_stride(*dst_frame, hor_stride);
		mpp_frame_set_ver_stride(*dst_frame, ver_stride);
		mpp_frame_set_fmt(*dst_frame, fmt);
		mpp_frame_set_eos(*dst_frame,mpp_frame_get_eos(*src_frame));
		size_t frame_size=hor_stride * ver_stride * 3 / 2;
		
		MppBuffer buf=NULL;
		ret = mpp_buffer_get(NULL, &buf,frame_size);	
		if (ret) {
			mpp_err("failed to get buffer for input frame ret %d\n", ret);
			return -1;
		}
			
		mpp_frame_set_buffer(*dst_frame,buf);
	
	return 1;
}

int frame_resize(MppFrame* src_frame,MppFrame* dst_frame)
{
	MppBuffer src_buf=NULL;
	void *src_ptr=NULL;
	MppBuffer dst_buf=NULL;
	void *dst_ptr=NULL;
	
	RK_U32 src_width = mpp_frame_get_width(*src_frame);
	RK_U32 src_height = mpp_frame_get_height(*src_frame);
	RK_U32 src_hor_stride = mpp_frame_get_hor_stride(*src_frame);
	RK_U32 src_ver_stride = mpp_frame_get_ver_stride(*src_frame);
	
	RK_U32 dst_width = mpp_frame_get_width(*dst_frame);
	RK_U32 dst_height = mpp_frame_get_height(*dst_frame);
	RK_U32 dst_hor_stride = mpp_frame_get_hor_stride(*dst_frame);
	RK_U32 dst_ver_stride = mpp_frame_get_ver_stride(*dst_frame);
	FILE* fp_output;
	//fp_output = fopen("/mnt/sdcard/out.yuv", "w+b");
	//printf("src_width %d src_height %d\n",src_width,src_height);
	mpp_frame_get_fmt(*src_frame);
		
	src_buf=mpp_frame_get_buffer(*src_frame);
	src_ptr = mpp_buffer_get_ptr(src_buf);
	size_t len  = mpp_buffer_get_size(src_buf);
			
	dst_buf=mpp_frame_get_buffer(*dst_frame);
	dst_ptr = mpp_buffer_get_ptr(dst_buf);
	size_t len2  = mpp_buffer_get_size(dst_buf);
	
	rga_info_t src;
	rga_info_t dst;
					
	memset(&src, 0, sizeof(rga_info_t));
	src.fd = -1;
	src.mmuFlag = 1;
	src.virAddr = src_ptr;
					
	memset(&dst, 0, sizeof(rga_info_t));
	dst.fd = -1;
	dst.mmuFlag = 1;
	dst.virAddr = dst_ptr;
		
	rga_set_rect(&src.rect, 0,0,src_width,src_height,src_hor_stride,src_ver_stride,RK_FORMAT_YCbCr_420_SP);
	rga_set_rect(&dst.rect, 0,0,dst_width,dst_height,dst_hor_stride,dst_ver_stride,RK_FORMAT_YCbCr_420_SP);
					
	MPP_RET ret = c_RkRgaBlit(&src, &dst, NULL);
	mpp_frame_set_pts(*dst_frame,mpp_frame_get_pts(*src_frame));
	mpp_frame_set_eos(*dst_frame, mpp_frame_get_eos(*src_frame));
	//dump_mpp_frame_to_file(*dst_frame, fp_output);
}		