.section .startup_code,"ax",@progbits
.global _start
_start:
    .option push
    .option norelax
    la sp, _estack
    .option pop
    la a0, _sidata
    la a1, _sdata
    la a2, _edata
1:  bgeu a1, a2, 2f
    lw a3, 0(a0)
    sw a3, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    j 1b
2:  la a0, _sbss
    la a1, _ebss
3:  bgeu a0, a1, 4f
    sw zero, 0(a0)
    addi a0, a0, 4
    j 3b
4:  call main
5:  j 5b
.section .note.GNU-stack,"",@progbits
