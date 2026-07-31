/* Entry point for the fixture tasks. Built -static -no-pie, so unlike init
 * itself they have nothing to relocate. */

    .section .note.GNU-stack,"",%progbits
    .text

#ifndef FIXTURE_ENTRY
  #define FIXTURE_ENTRY FixtureMain
#endif

#if defined(__x86_64__)

    .globl _start
_start:
    xorq    %rbp, %rbp
    andq    $-16, %rsp
    call    FIXTURE_ENTRY
    hlt

#elif defined(__i386__)

    .globl _start
_start:
    xorl    %ebp, %ebp
    andl    $-16, %esp
    subl    $12, %esp
    call    FIXTURE_ENTRY
    hlt

#elif defined(__aarch64__)

    .globl _start
_start:
    mov     x29, #0
    mov     x30, #0
    bl      FIXTURE_ENTRY
    brk     #0

#elif defined(__riscv) && __riscv_xlen == 64

    .globl _start
_start:
    .option push
    .option norelax
    lla     gp, __global_pointer$
    .option pop
    mv      s0, zero
    mv      ra, zero
    call    FIXTURE_ENTRY
    ebreak

#elif defined(__loongarch__) && __loongarch_grlen == 64

    .globl _start
_start:
    move    $fp, $zero
    move    $ra, $zero
    bl      FIXTURE_ENTRY
    break   0

#elif defined(__arm__)

    .globl _start
_start:
    mov     fp, #0
    bl      FIXTURE_ENTRY
.Lhang:
    b       .Lhang

#elif defined(__mips__) && _MIPS_SIM == _ABIO32

    .globl _start
_start:
    move    $fp, $zero
    move    $ra, $zero
    addiu   $sp, $sp, -32
    jal     FIXTURE_ENTRY
    break

#else
#error "unsupported architecture"
#endif
