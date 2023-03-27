#include "mpi_decoder.h"
#include "mpi_encoder.h"
#include "mpi_frame_process.h"
#include "RgaApi.h"
#include <pthread.h>
#include <stdio.h>


int gn_video_resize_init();

void gn_video_resize_put(MppPacket packet);

int gn_video_resize_run(void (*func_ptr)(MppPacket * header,MppPacket * data));