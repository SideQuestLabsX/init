/* Entry point. No crt0, the kernel jumps straight here.
 *
 * Order matters. InitSelfRelocate has to run before any C code that touches a
 * relocated pointer, and the stack guard has to be seeded before the first
 * function carrying a canary. */

    .section .note.GNU-stack,"",%progbits
    .text

#if defined(__x86_64__)

    .globl _start
    .type _start, @function
_start:
    xorq    %rbp, %rbp
    andq    $-16, %rsp
    leaq    __ehdr_start(%rip), %rdi
    call    InitSelfRelocate
    call    InitGuardSeed
    call    InitMain
    hlt
    .size _start, .-_start

    .globl InitSigRestore
    .type InitSigRestore, @function
InitSigRestore:
    movq    $15, %rax           /* SYS_rt_sigreturn */
    syscall
    .size InitSigRestore, .-InitSigRestore

#elif defined(__i386__)

    .globl _start
    .type _start, @function
_start:
    xorl    %ebp, %ebp
    call    1f
1:  popl    %eax
    addl    $(__ehdr_start - 1b), %eax
    andl    $-16, %esp
    subl    $12, %esp
    pushl   %eax
    call    InitSelfRelocate
    addl    $16, %esp
    andl    $-16, %esp
    subl    $12, %esp
    pushl   $0
    call    InitGuardSeed
    call    InitMain
    hlt
    .size _start, .-_start

    .globl InitSigRestore
    .type InitSigRestore, @function
InitSigRestore:
    movl    $173, %eax          /* SYS_rt_sigreturn */
    int     $0x80
    .size InitSigRestore, .-InitSigRestore

#elif defined(__aarch64__)

    .globl _start
    .type _start, %function
_start:
    mov     x29, #0
    mov     x30, #0
    adrp    x0, __ehdr_start
    add     x0, x0, :lo12:__ehdr_start
    bl      InitSelfRelocate
    bl      InitGuardSeed
    bl      InitMain
    brk     #0
    .size _start, .-_start

    .globl InitSigRestore
    .type InitSigRestore, %function
InitSigRestore:
    mov     x8, #139            /* SYS_rt_sigreturn */
    svc     #0
    .size InitSigRestore, .-InitSigRestore

#elif defined(__riscv) && __riscv_xlen == 64

    .globl _start
    .type _start, @function
_start:
    /* gp has to be live before any relaxed gp-relative access, and setting it
       is itself the one access that must not be relaxed */
    .option push
    .option norelax
    lla     gp, __global_pointer$
    .option pop
    mv      s0, zero
    mv      ra, zero
    lla     a0, __ehdr_start
    call    InitSelfRelocate
    call    InitGuardSeed
    call    InitMain
    ebreak
    .size _start, .-_start

/* riscv has no SA_RESTORER, the kernel supplies the return trampoline */

#elif defined(__loongarch__) && __loongarch_grlen == 64

    .globl _start
    .type _start, @function
_start:
    move    $fp, $zero
    move    $ra, $zero
    la.pcrel $a0, __ehdr_start
    bl      InitSelfRelocate
    bl      InitGuardSeed
    bl      InitMain
    break   0
    .size _start, .-_start

/* loongarch has no SA_RESTORER, the kernel supplies the return trampoline */

#elif defined(__arm__)

    .globl _start
    .type _start, %function
_start:
    mov     fp, #0
    ldr     r0, .Lehdr_off
.Lehdr_pc:
    add     r0, pc, r0
    bl      InitSelfRelocate
    bl      InitGuardSeed
    bl      InitMain
.Lhang:
    b       .Lhang
.Lehdr_off:
    .word   __ehdr_start - (.Lehdr_pc + 8)
    .size _start, .-_start

    .globl InitSigRestore
    .type InitSigRestore, %function
InitSigRestore:
    mov     r7, #173            /* SYS_rt_sigreturn */
    svc     #0
    .size InitSigRestore, .-InitSigRestore

#elif defined(__mips__) && _MIPS_SIM == _ABIO32

    .globl _start
    .type _start, @function
_start:
    move    $fp, $zero
    move    $ra, $zero
    /* an o32 caller owns the 16 bytes its callee may spill a0-a3 into, and
       nothing below returns, so this frame is never given back */
    addiu   $sp, $sp, -32
    lui     $a0, %hi(__ehdr_start)
    addiu   $a0, $a0, %lo(__ehdr_start)
    jal     InitSelfRelocate
    jal     InitGuardSeed
    jal     InitMain
    break
    .size _start, .-_start

/* MIPS has no SA_RESTORER, the kernel supplies the return trampoline */

#else
#error "unsupported architecture"
#endif
