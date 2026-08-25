// Microbench: isolate UPMEM fixed per-dispatch overhead vs DPU count.
// Loads the matmul kernel, sets row_cnt=0 (kernel body does nothing),
// and times a synchronous launch. This is the launch/sync floor that
// bounds any multi-dispatch offload.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dpu.h>
#include "common.h"

static double now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e3+t.tv_nsec*1e-6;}

int main(void){
    uint32_t reqs[]={1,64,512,2560};
    for(int p=0;p<4;p++){
        struct dpu_set_t set,dpu; uint32_t idx,nr;
        if(dpu_alloc(reqs[p],NULL,&set)!=DPU_OK){printf("alloc %u fail\n",reqs[p]);continue;}
        dpu_get_nr_dpus(set,&nr);
        dpu_load(set,"./matmul.kernel",NULL);
        dpu_args_t a={.in_dim=DIM,.out_dim=DIM,.w_off=0,.row_cnt=0,.col_start=0,.col_pad=2};
        DPU_FOREACH(set,dpu,idx) dpu_prepare_xfer(dpu,&a);
        dpu_push_xfer(set,DPU_XFER_TO_DPU,"ARGS",0,sizeof a,DPU_XFER_DEFAULT);
        double best=1e30;
        for(int r=0;r<20;r++){
            double t0=now_ms(); dpu_launch(set,DPU_SYNCHRONOUS); double dt=now_ms()-t0;
            if(r>=3&&dt<best)best=dt;
        }
        printf("nr=%5u  empty_launch=%.3f ms\n",nr,best);
        dpu_free(set);
    }
    return 0;
}
