/* ARMv6 has no Thumb-2, replace libgcc's divider helper */

#if defined(__ARM_ARCH_6__)

    .globl __aeabi_uldivmod
    .type __aeabi_uldivmod, %function
__aeabi_uldivmod:
    orrs    ip, r2, r3
    beq     .Larmv6_div_zero
    push    {r4-r11, lr}
    mov     r4, r0
    mov     r5, r1
    mov     r6, r2
    mov     r7, r3
    mov     r8, #0
    mov     r9, #0
    mov     r10, #0
    mov     r11, #0
    mov     lr, #64
.Larmv6_div_loop:
    mov     ip, r5, lsr #31
    adds    r8, r8, r8
    adc     r9, r9, r9
    orr     r8, r8, ip
    adds    r4, r4, r4
    adc     r5, r5, r5
    adds    r10, r10, r10
    adc     r11, r11, r11
    cmp     r9, r7
    cmpeq   r8, r6
    bcc     .Larmv6_div_no_sub
    subs    r8, r8, r6
    sbc     r9, r9, r7
    orr     r10, r10, #1
.Larmv6_div_no_sub:
    subs    lr, lr, #1
    bne     .Larmv6_div_loop
    mov     r0, r10
    mov     r1, r11
    mov     r2, r8
    mov     r3, r9
    pop     {r4-r11, pc}

.Larmv6_div_zero:
#if defined(INIT_FIXTURE)
    b       .Larmv6_div_zero
#else
    mov     r0, #8
    bl      raise
    b       .Larmv6_div_zero
#endif
    .size __aeabi_uldivmod, .-__aeabi_uldivmod

#endif
