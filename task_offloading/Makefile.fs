# Makefile for TaskAccelerator Full System components

# Kernel driver configuration
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
MODULE_NAME = task_accel_driver

# User space programs
TEST_PROG = test_fs_accel
CC_X86 = gcc

.PHONY: all clean driver test-x86

all: driver test-x86

driver:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)/driver modules

test-x86:
	$(CC_X86) -o tests/$(TEST_PROG)_x86 tests/$(TEST_PROG).c

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)/driver clean || true
	rm -f tests/$(TEST_PROG)_x86
