/*
 * Copyright (c) 2015, University of Kaiserslautern
 * All rights reserved.
 * (License header omitted for brevity)
 * Authors: Matthias Jung, Frederik Lauer
 */

.section INTERRUPT_VECTOR, "x"
.global _Reset
_Reset:
    B Reset_Handler    /* Reset */
    B .                /* Undefined */
    B .                /* SWI */
    B .                /* Prefetch Abort */
    B .                /* Data Abort */
    B .                /* reserved */
    B irq_handler      /* IRQ */
    B .                /* FIQ */

.equ Len_Stack,        0x1000;  // 4kB of stack memory
.equ Len_IRQ_Stack,    0x1000;  // 4kB of stack memory for IRQ Mode

// GIC_Distributor
.equ GIC_Dist_Base,    0x2c001000

// Register offsets
.equ set_enable1,      0x104
.equ set_enable2,      0x108

// Example definitions
.equ timer_irq_id,     131
.equ kmio_irq_id,      44
.equ uart0_irq_id,     37
.equ rtc_irq_id,       36
.equ top_dev_id,       68

// GIC_CPU_INTERFACE
.equ GIC_CPU_BASE,               0x2c002000
.equ GIC_CPU_mask_reg_offset,    0x04
.equ GIC_CPU_Int_Ack_reg_offset, 0x0C
.equ GIC_CPU_End_of_int_offset,  0x10

/* ------------------------------------------------------------------
 * PAGE TABLE ALLOCATION (Must be 16KB Aligned for ARMv7)
 * ------------------------------------------------------------------ */
.align 14
page_table:
    .space 4096 * 4  /* 4096 entries * 4 bytes = 16KB */


/* ------------------------------------------------------------------
 * MMU & CACHE SETUP (Translating the AArch64 baseline to 32-bit)
 * ------------------------------------------------------------------ */
.global setup_mmu
setup_mmu:
    push {r0-r4, lr}

    /* Set VBAR to 0x80000000 where linker placed the interrupt vectors */
    ldr r0, =0x80000000
    mcr p15, 0, r0, c12, c0, 0

    /* --- Enable SMP bit in ACTLR to unlock Data Caches --- */
    mrc p15, 0, r0, c1, c0, 1    @ Read Auxiliary Control Register
    orr r0, r0, #(1 << 6)        @ Set Bit 6 (SMP)
    mcr p15, 0, r0, c1, c0, 1    @ Write back
    isb

    /* 2. Load the page table base address */
    ldr r0, =page_table

    /* 3. Map 0x00000000 to 0x7FFFFFFF (First 2GB) as DEVICE memory */
    mov r1, #0                  
    ldr r2, =0x00000C02         @ Device Memory
    mov r3, #2048               
fill_device:
    str r2, [r0, r1, lsl #2]    
    add r2, r2, #0x00100000     
    add r1, r1, #1
    cmp r1, r3
    bne fill_device

    /* --- NEW: SALAM Accelerator Unaligned Access Override --- */
    /* Override 0x2F000000 to be Normal Non-Cacheable memory */
    ldr r0, =page_table
    ldr r4, =0x2F0              @ USE R4 HERE! Index for 0x2F000000
    ldr r2, =0x2F011C02         @ Descriptor: Normal Non-Cacheable, Shareable
    str r2, [r0, r4, lsl #2]    @ USE R4 HERE! Overwrite just this one 1MB entry

    /* 4. Map 0x80000000 to 0xFFFFFFFF (Next 2GB) as CACHEABLE memory */
    /* --- NEW: Added the Shareable Bit (Bit 16) -> 0x80011C0E --- */
    ldr r2, =0x80011C0E         @ Cacheable, Shareable, Write-Back Memory
    mov r3, #4096
    /* r1 is safely still 2048 here, ready to continue! */
fill_cache:
    str r2, [r0, r1, lsl #2]
    add r2, r2, #0x00100000     
    add r1, r1, #1
    cmp r1, r3
    bne fill_cache

    /* 5. Configure Domain Access Control Register (DACR) */
    /* Set all domains to Manager (0x3) = bypass permission checks */
    ldr r0, =0xFFFFFFFF
    mcr p15, 0, r0, c3, c0, 0

    /* 6. Set Translation Table Base Register 0 (TTBR0) */
    ldr r0, =page_table
    mcr p15, 0, r0, c2, c0, 0

    /* 7. Invalidate TLB and Caches before turning them on */
    mov r0, #0
    mcr p15, 0, r0, c8, c7, 0   @ Invalidate entire unified TLB
    mcr p15, 0, r0, c7, c5, 0   @ Invalidate all instruction caches

    /* 8. Enable MMU and Caches via System Control Register (SCTLR) */
    mrc p15, 0, r0, c1, c0, 0
    orr r0, r0, #0x1            @ Bit 0:  M (MMU enable)
    orr r0, r0, #0x4            @ Bit 2:  C (Data Cache enable)
    orr r0, r0, #0x1000         @ Bit 12: I (Instruction Cache enable)
    mcr p15, 0, r0, c1, c0, 0
    
    dsb
    isb

    pop {r0-r4, pc}
/* ------------------------------------------------------------------
 * MAIN RESET HANDLER
 * ------------------------------------------------------------------ */
.global Reset_Handler
Reset_Handler:
    // Set up stack pointers for IRQ processor mode
    mov R1, #0b11010010 // interrupts masked, MODE=IRQ IRQ|FIQ|0|Mode[4:0]
    msr CPSR, R1    // change to IRQ mode
    ldr SP, =stack_base + Len_Stack + Len_IRQ_Stack // set IRQ stack

    // Change back to SVC (supervisor) mode with interrupts disabled
    mov R1, #0b11010011 // interrupts masked, MODE=SVC IRQ|FIQ|0|Mode[4:0]
    msr CPSR, R1    // change to SVC mode
    ldr SP, =stack_base + Len_Stack // set stack

    // --- NEW: Enable MMU and Caches BEFORE touching GIC ---
    bl setup_mmu

    // Enable individual interrupts, set target
    bl config_gic_dist

    // Enable individual interrupts, set target
    bl config_gic_cpu_interface

    // Enable interrupts in GIC Distributor
    ldr r0, =GIC_Dist_Base
    mov r1, #1
    str r1, [r0]

    // Enable IRQ interrupts in the processor:
    mov R1, #0b01010011 // IRQ not masked (=0), MODE=SVC IRQ|FIQ|0|Mode[4:0]
    msr CPSR, R1

    bl main
    B .


/* ------------------------------------------------------------------
 * GIC & IRQ HANDLERS (Unchanged)
 * ------------------------------------------------------------------ */
.global config_gic_dist
config_gic_dist:
    push {lr}
    ldr r1, =GIC_Dist_Base + set_enable2    // r1 = Set-enable1 Reg Address
    mov r2, #1
    lsl r2, r2, #4
    ldr r3, [r1]    // read current register value
    orr r3, r3, r2  // set the enable bit
    str r3, [r1]    // store the new register value
    pop {pc}


.global config_gic_cpu_interface
config_gic_cpu_interface:
    push {lr}
    ldr r1, =GIC_CPU_BASE + GIC_CPU_mask_reg_offset
    ldr r2, =0xFFFF
    str r2, [r1]
    mov r2, #1
    ldr r1, =GIC_CPU_BASE
    str r2, [r1]
    pop {pc}


.global irq_handler
irq_handler:
    push {r0-r7,lr}
    ldr r1, =GIC_CPU_BASE + GIC_CPU_Int_Ack_reg_offset
    ldr r2, [r1]

irq_top:
    cmp r2, #top_dev_id
    bne irq_end  // if irq is not from top_dev

    BL isr
    ldr r2, = top_dev_id

irq_end:
    ldr r1, =GIC_CPU_BASE + GIC_CPU_End_of_int_offset
    str r2, [r1]

    pop {r0-r7,lr}
    subs pc, lr, #4
