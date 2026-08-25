/*
 * Copyright (c) 2015, University of Kaiserslautern
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Matthias Jung
 			Frederik Lauer
 */

ENTRY(_Reset)
SECTIONS
{
    /* 1. Start everything at 2GB (DRAM start / Cacheable region) */
    . = 0x80000000;

    .text : {
        /* Ensure the vector table from boot.s is at the very beginning */
        KEEP(*(.section.INTERRUPT_VECTOR))
        *(INTERRUPT_VECTOR)
        *(.text)
    }

    /* 2. Read-only data (strings, constants) */
    .rodata : { *(.rodata) }

    . = ALIGN(4096);
    .data : {
        *(.data)
    }

    .bss : {
        __bss_start = .;
        *(.bss COMMON)
        __bss_end = .;
    }

    /* 3. Heap start - _sbrk grows upward from here toward SP */
    . = ALIGN(8);
    PROVIDE (end = .);

    /* 4. Reserve heap space (4MB), then place stacks above */
    . = . + 0x400000; /* 4MB heap room for batched buffers */

    /* 5. Stack Setup - Must be 8-byte aligned for ARM */
    . = ALIGN(8);
    stack_base = .;
    . = . + 0x4000; /* 16kB of stack memory */
    . = . + 0x4000; /* 16kB of stack memory for IRQ mode */
}
