# Makefile for TaskAccelerator testing

# Compiler settings
CC = gcc
ARM_CC = arm-linux-gnueabihf-gcc
AARCH64_CC = aarch64-linux-gnu-gcc

CFLAGS = -static -O2 -Wall -Wextra
ARM_CFLAGS = $(CFLAGS) -march=armv7-a
AARCH64_CFLAGS = $(CFLAGS)

# Targets
TEST_PROG = test_accelerator
ARM_TEST_PROG = test_accelerator_arm
AARCH64_TEST_PROG = test_accelerator_aarch64

# Source files
SRC = test_accelerator.c

.PHONY: help

##@ General
help: ## Show this help message.
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n"} /^[a-zA-Z0-9_-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Container
build-container: ## Build custom apptainer image with transformers
	apptainer build gem5-transformers.sif gem5-transformers.def

enter: ## Enter apptainer container
	apptainer shell --bind $(PWD):/workspace  docker://ghcr.io/gem5/ubuntu-24.04_all-dependencies:v24-0
	#docker://ghcr.io/gem5/ubuntu-24.04_min-dependencies:v24-0

enter-custom: ## Enter custom container with transformers
	apptainer shell --bind $(PWD):/workspace gem5-transformers.sif

##@ Building
native: $(TEST_PROG) ## Compile test program for native architecture

$(TEST_PROG): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

arm: $(ARM_TEST_PROG) ## Compile test program for ARM 32-bit

$(ARM_TEST_PROG): $(SRC)
	$(ARM_CC) $(ARM_CFLAGS) -o $@ $<

aarch64: $(AARCH64_TEST_PROG) ## Compile test program for ARM 64-bit

$(AARCH64_TEST_PROG): $(SRC)
	$(AARCH64_CC) $(AARCH64_CFLAGS) -o $@ $<

##@ Testing
test: $(TEST_PROG) ## Run gem5 simulation with test program
	@echo "Running test simulation..."
	cd /workspace/include/gem5 && ./build/X86/gem5.opt /workspace/test_config.py /workspace/$(TEST_PROG)

test-simple: ## Run simple validation test
	@echo "Compiling simple test..."
	$(CC) -o tests/simple_test tests/simple_test.c
	@echo "Running simple validation test..."
	include/gem5/build/X86/gem5.opt configs/test_config.py tests/simple_test

test-register: ## Run register interface test with debug output
	@echo "Compiling register test..."
	$(CC) -o tests/register_test tests/register_test.c
	@echo "Running register interface test..."
	include/gem5/build/X86/gem5.opt --debug-flags=TaskAccelerator configs/test_config.py tests/register_test

test-dma: ## Run DMA functionality test with debug output
	@echo "Compiling DMA test..."
	$(CC) -o tests/dma_test tests/dma_test.c
	@echo "Running DMA functionality test..."
	include/gem5/build/X86/gem5.opt --debug-flags=TaskAccelerator,DMA configs/dma_test_config.py tests/dma_test

test-accelerator: ## Run full accelerator functionality test
	@echo "Compiling full accelerator test..."
	$(CC) -o tests/test_accelerator tests/test_accelerator.c
	@echo "Running full accelerator test..."
	include/gem5/build/X86/gem5.opt configs/test_config.py tests/test_accelerator

test-syscalls: ## Test accelerator syscalls
	@echo "Compiling accelerator syscall test..."
	$(CC) $(CFLAGS) -o tests/test_syscalls tests/test_syscalls.c
	@echo "Running accelerator syscall test..."
	include/gem5/build/X86/gem5.opt configs/test_config.py tests/test_syscalls

test-all: test-simple test-register test-dma test-syscalls ## Run all accelerator tests
	@echo "All tests completed!"

test-llama: ## Run llama2.c inference inside gem5
	@echo "Running llama2.c inference in gem5..."
	include/gem5/build/X86/gem5.opt --outdir=m5out_llama_30_tokens configs/common_config.py --cmd "./llama2.c/run" --args="./llama2.c/stories15M.bin -n 30"

##@ gem5
clean-gem5: ## Clean gem5 build files (run inside container)
	@echo "Cleaning gem5 build files..."
	rm -rf /workspace/include/gem5/build

setup-accelerator: ## Copy accelerator files to gem5 source tree
	@echo "Copying accelerator files to gem5 source tree..."
	mkdir -p include/gem5/src/task_accelerator
	cp accelerator_files/* include/gem5/src/task_accelerator/
	@echo "Accelerator files copied successfully"

build-gem5: setup-accelerator ## Build gem5 with TaskAccelerator (run inside container)
	@echo "Building gem5 with TaskAccelerator..."
	cd include/gem5 && python3 $(shell which scons) build/X86/gem5.opt -j$(shell nproc)

build-gem5-normal: ## Build standard gem5 without TaskAccelerator (run inside container)
	@echo "Removing accelerator files..."
	rm -rf include/gem5/src/task_accelerator
	@echo "Building standard gem5..."
	cd include/gem5 && python3 $(shell which scons) build/X86/gem5.opt -j$(shell nproc)

##@ Utilities
clean: ## Remove compiled binaries
	rm -f $(TEST_PROG) $(ARM_TEST_PROG) $(AARCH64_TEST_PROG) simple_test register_test dma_test

.PHONY: enter native arm aarch64 test test-simple test-register test-dma test-all test-llama setup-accelerator setup-gem5 build-gem5 build-gem5-normal clean
