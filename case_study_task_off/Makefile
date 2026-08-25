# VIPER UPMEM case study — real-hardware version.
# Builds the CPU baseline (Q16.16) and the int8 UPMEM offload driver,
# runs the DPU sweep, and regenerates the figures.
#
#   make            # build everything
#   make baseline   # print T_full and T_stub (CPU baseline)
#   make sweep      # run the UPMEM DPU sweep -> sweep_final.dat
#   make figures    # regenerate the 3 figures from sweep_final.dat
#   make all-repro  # baseline + sweep + figures (needs the UPMEM server)

NR_TASKLETS ?= 16   # int8 buffers are tiny; 16 tasklets fit WRAM easily
DPU_CC      = dpu-upmem-dpurte-clang
HOST_CFLAGS = -O2 -std=c11 -Wall -D_POSIX_C_SOURCE=199309L `dpu-pkg-config --cflags dpu`
HOST_LDLIBS = `dpu-pkg-config --libs dpu` -lm
DPU_CFLAGS  = -O2 -DNR_TASKLETS=$(NR_TASKLETS)
REPS       ?= 5

all: main_cpu main_cpu_stub matmul.kernel host bench_launch

main_cpu: main_cpu.c layer.h common.h
	gcc $(HOST_CFLAGS) -o $@ main_cpu.c -lm

main_cpu_stub: main_cpu.c layer.h common.h
	gcc $(HOST_CFLAGS) -DSTUB -o $@ main_cpu.c -lm

matmul.kernel: matmul_dpu.c common.h
	$(DPU_CC) $(DPU_CFLAGS) -o $@ matmul_dpu.c

host: host.c layer.h common.h
	gcc $(HOST_CFLAGS) -DNR_TASKLETS=$(NR_TASKLETS) -o $@ host.c $(HOST_LDLIBS)

bench_launch: bench_launch.c common.h
	gcc $(HOST_CFLAGS) -o $@ bench_launch.c $(HOST_LDLIBS)

baseline: main_cpu main_cpu_stub
	./main_cpu 300
	./main_cpu_stub 300

sweep: host matmul.kernel
	./host $(REPS) > sweep_final.dat 2> verify.log
	@grep -v 'in fault' verify.log || true
	@cat sweep_final.dat

figures: sweep_final.dat
	python3 plot.py

all-repro: baseline sweep figures

clean:
	rm -f main_cpu main_cpu_stub matmul.kernel host bench_launch verify.log

.PHONY: all baseline sweep figures all-repro clean
