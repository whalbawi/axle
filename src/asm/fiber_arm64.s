#if defined(__aarch64__)

#include "fiber_reg_off_arm64.h"

// x0: src
// x1: dst
.text
.global _axle_fiber_swap
.align 4
_axle_fiber_swap:
    // Store special purpose registers
    str x16, [x0, #AXLE_REG_OFF_ARM64_X16]
    str x17, [x0, #AXLE_REG_OFF_ARM64_X17]
    str x18, [x0, #AXLE_REG_OFF_ARM64_X18]

    // Store callee-preserved registerS
    str x19, [x0, #AXLE_REG_OFF_ARM64_X19]
    str x20, [x0, #AXLE_REG_OFF_ARM64_X20]
    str x21, [x0, #AXLE_REG_OFF_ARM64_X21]
    str x22, [x0, #AXLE_REG_OFF_ARM64_X22]
    str x23, [x0, #AXLE_REG_OFF_ARM64_X23]
    str x24, [x0, #AXLE_REG_OFF_ARM64_X24]
    str x25, [x0, #AXLE_REG_OFF_ARM64_X25]
    str x26, [x0, #AXLE_REG_OFF_ARM64_X26]
    str x27, [x0, #AXLE_REG_OFF_ARM64_X27]
    str x28, [x0, #AXLE_REG_OFF_ARM64_X28]
    str x29, [x0, #AXLE_REG_OFF_ARM64_X29]

    str d8,  [x0, #AXLE_REG_OFF_ARM64_D8]
    str d9,  [x0, #AXLE_REG_OFF_ARM64_D9]
    str d10, [x0, #AXLE_REG_OFF_ARM64_D10]
    str d11, [x0, #AXLE_REG_OFF_ARM64_D11]
    str d12, [x0, #AXLE_REG_OFF_ARM64_D12]
    str d13, [x0, #AXLE_REG_OFF_ARM64_D13]
    str d14, [x0, #AXLE_REG_OFF_ARM64_D14]
    str d15, [x0, #AXLE_REG_OFF_ARM64_D15]

    // Store sp and lr
    mov x2, sp
    str x2, [x0, #AXLE_REG_OFF_ARM64_SP]
    str lr, [x0, #AXLE_REG_OFF_ARM64_LR]

    // Load context 'to'
    mov x7, x1

    // Load special purpose registers
    ldr x16, [x7, #AXLE_REG_OFF_ARM64_X16]
    ldr x17, [x7, #AXLE_REG_OFF_ARM64_X17]
    ldr x18, [x7, #AXLE_REG_OFF_ARM64_X18]

    // Load callee-preserved registers
    ldr x19, [x7, #AXLE_REG_OFF_ARM64_X19]
    ldr x20, [x7, #AXLE_REG_OFF_ARM64_X20]
    ldr x21, [x7, #AXLE_REG_OFF_ARM64_X21]
    ldr x22, [x7, #AXLE_REG_OFF_ARM64_X22]
    ldr x23, [x7, #AXLE_REG_OFF_ARM64_X23]
    ldr x24, [x7, #AXLE_REG_OFF_ARM64_X24]
    ldr x25, [x7, #AXLE_REG_OFF_ARM64_X25]
    ldr x26, [x7, #AXLE_REG_OFF_ARM64_X26]
    ldr x27, [x7, #AXLE_REG_OFF_ARM64_X27]
    ldr x28, [x7, #AXLE_REG_OFF_ARM64_X28]
    ldr x29, [x7, #AXLE_REG_OFF_ARM64_X29]

    ldr d8,  [x7, #AXLE_REG_OFF_ARM64_D8]
    ldr d9,  [x7, #AXLE_REG_OFF_ARM64_D9]
    ldr d10, [x7, #AXLE_REG_OFF_ARM64_D10]
    ldr d11, [x7, #AXLE_REG_OFF_ARM64_D11]
    ldr d12, [x7, #AXLE_REG_OFF_ARM64_D12]
    ldr d13, [x7, #AXLE_REG_OFF_ARM64_D13]
    ldr d14, [x7, #AXLE_REG_OFF_ARM64_D14]
    ldr d15, [x7, #AXLE_REG_OFF_ARM64_D15]

    // Load parameter registers
    ldr x0, [x7, #AXLE_REG_OFF_ARM64_X0]
    ldr x1, [x7, #AXLE_REG_OFF_ARM64_X1]

    // Load sp and lr
    ldr lr, [x7, #AXLE_REG_OFF_ARM64_LR]
    ldr x2, [x7, #AXLE_REG_OFF_ARM64_SP]
    mov sp, x2

    blr lr

    // Exit the program if the fiber's entry point ever returns
    mov x0, #0x19
    mov x16, #1
    svc 0x80

#endif // defined(__aarch64__)
