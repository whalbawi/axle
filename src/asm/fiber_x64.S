#include "fiber_reg_off_x64.h"

#include "asm/platform.h"

// rdi: src
// rsi: dst
.text
.global ASM_SYM(axle_fiber_swap)
.align 4
ASM_SYM(axle_fiber_swap):
    // Preserve callee-saved registers
    movq %rbx, AXLE_REG_OFF_X64_RBX(%rdi)
    movq %rbp, AXLE_REG_OFF_X64_RBP(%rdi)
    movq %r12, AXLE_REG_OFF_X64_R12(%rdi)
    movq %r13, AXLE_REG_OFF_X64_R13(%rdi)
    movq %r14, AXLE_REG_OFF_X64_R14(%rdi)
    movq %r15, AXLE_REG_OFF_X64_R15(%rdi)

    // Save return address
    movq (%rsp), %r8
    movq %r8, AXLE_REG_OFF_X64_RIP(%rdi)

    // Save the caller's stack pointer
    leaq 0x08(%rsp), %r8
    movq %r8, AXLE_REG_OFF_X64_RSP(%rdi)

    // Load callee-saved registers
    movq AXLE_REG_OFF_X64_RBX(%rsi), %rbx
    movq AXLE_REG_OFF_X64_RBP(%rsi), %rbp
    movq AXLE_REG_OFF_X64_R12(%rsi), %r12
    movq AXLE_REG_OFF_X64_R13(%rsi), %r13
    movq AXLE_REG_OFF_X64_R14(%rsi), %r14
    movq AXLE_REG_OFF_X64_R15(%rsi), %r15

    // Load stack pointer
    movq AXLE_REG_OFF_X64_RSP(%rsi), %rsp

    // Load return address
    movq AXLE_REG_OFF_X64_RIP(%rsi), %r8

    // Load function parameters
    movq AXLE_REG_OFF_X64_RDI(%rsi), %rdi
    movq AXLE_REG_OFF_X64_RSI(%rsi), %rsi

    jmp *%r8

.section .note.GNU-stack, "", @progbits
