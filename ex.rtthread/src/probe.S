/* Exercise preserved registers through actual timer preemption.
 * a0: counter, a1: stop flag. Return zero if all canaries survived.
 */
.section .text,"ax",@progbits
.global c2_spin_probe
c2_spin_probe:
    addi sp, sp, -48
    sw s0, 0(sp)
    sw s1, 4(sp)
    sw s2, 8(sp)
    sw s3, 12(sp)
    sw s4, 16(sp)
    sw s5, 20(sp)
    sw s6, 24(sp)
    sw s7, 28(sp)
    sw s8, 32(sp)
    sw s9, 36(sp)
    sw s10, 40(sp)
    sw s11, 44(sp)
    li s0, 74560
    li s1, 74561
    li s2, 74562
    li s3, 74563
    li s4, 74564
    li s5, 74565
    li s6, 74566
    li s7, 74567
    li s8, 74568
    li s9, 74569
    li s10, 74570
    li s11, 74571
    li t2, 74572
    li a2, 74573
    li a3, 74574
    li a4, 74575
    li a5, 74576
    li a6, 74577
    li a7, 74578
    li t3, 74579
    li t4, 74580
    li t5, 74581
    li t6, 74582
1:
    li t1, 74560
    bne s0, t1, 2f
    li t1, 74561
    bne s1, t1, 2f
    li t1, 74562
    bne s2, t1, 2f
    li t1, 74563
    bne s3, t1, 2f
    li t1, 74564
    bne s4, t1, 2f
    li t1, 74565
    bne s5, t1, 2f
    li t1, 74566
    bne s6, t1, 2f
    li t1, 74567
    bne s7, t1, 2f
    li t1, 74568
    bne s8, t1, 2f
    li t1, 74569
    bne s9, t1, 2f
    li t1, 74570
    bne s10, t1, 2f
    li t1, 74571
    bne s11, t1, 2f
    li t1, 74572
    bne t2, t1, 2f
    li t1, 74573
    bne a2, t1, 2f
    li t1, 74574
    bne a3, t1, 2f
    li t1, 74575
    bne a4, t1, 2f
    li t1, 74576
    bne a5, t1, 2f
    li t1, 74577
    bne a6, t1, 2f
    li t1, 74578
    bne a7, t1, 2f
    li t1, 74579
    bne t3, t1, 2f
    li t1, 74580
    bne t4, t1, 2f
    li t1, 74581
    bne t5, t1, 2f
    li t1, 74582
    bne t6, t1, 2f
    lw t0, 0(a0)
    addi t0, t0, 1
    sw t0, 0(a0)
    lw t0, 0(a1)
    beqz t0, 1b
    li a0, 0
    j 3f
2:  li a0, 1
3:
    lw s0, 0(sp)
    lw s1, 4(sp)
    lw s2, 8(sp)
    lw s3, 12(sp)
    lw s4, 16(sp)
    lw s5, 20(sp)
    lw s6, 24(sp)
    lw s7, 28(sp)
    lw s8, 32(sp)
    lw s9, 36(sp)
    lw s10, 40(sp)
    lw s11, 44(sp)
    addi sp, sp, 48
    ret
.section .note.GNU-stack,"",@progbits
