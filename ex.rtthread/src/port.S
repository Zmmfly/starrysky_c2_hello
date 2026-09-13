/* PicoRV32 custom-0 instructions, ENABLE_IRQ_QREGS=0.
 * gp/tp are hardware return-PC / pending-IRQ registers, reserved globally.
 * .insn r: opcode, funct3, funct7, rd, rs1, rs2.
 */
.option norvc
.section .startup_code,"ax",@progbits
.global _start
_start:
    /* Reset and IRQ share address zero in the reference retroSoC wrapper.
     * Reset mask is all ones; a timer IRQ can only arrive with bit 0 clear.
     * Test the old mask without touching any ordinary application register.
     */
    li tp, -1
    .insn r 0x0b, 0, 3, tp, tp, zero  /* maskirq tp, tp */
    andi tp, tp, 1
    beqz tp, c2_irq_entry
    la sp, _estack
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

.section .text,"ax",@progbits
.balign 4
.global rt_hw_interrupt_disable
rt_hw_interrupt_disable:
    li a0, -1
    .insn r 0x0b, 0, 3, a0, a0, zero
    ret
.global rt_hw_interrupt_enable
rt_hw_interrupt_enable:
    .insn r 0x0b, 0, 3, zero, a0, zero
    ret
.global c2_timer_set
c2_timer_set:
    .insn r 0x0b, 0, 5, zero, a0, zero
    ret
.global c2_cycle
c2_cycle:
    rdcycle a0
    ret

c2_irq_entry:
    addi sp, sp, -128
    sw gp, 0(sp)
    sw x1, 4(sp)
    sw x5, 20(sp)
    sw x6, 24(sp)
    sw x7, 28(sp)
    sw x8, 32(sp)
    sw x9, 36(sp)
    sw x10, 40(sp)
    sw x11, 44(sp)
    sw x12, 48(sp)
    sw x13, 52(sp)
    sw x14, 56(sp)
    sw x15, 60(sp)
    sw x16, 64(sp)
    sw x17, 68(sp)
    sw x18, 72(sp)
    sw x19, 76(sp)
    sw x20, 80(sp)
    sw x21, 84(sp)
    sw x22, 88(sp)
    sw x23, 92(sp)
    sw x24, 96(sp)
    sw x25, 100(sp)
    sw x26, 104(sp)
    sw x27, 108(sp)
    sw x28, 112(sp)
    sw x29, 116(sp)
    sw x30, 120(sp)
    sw x31, 124(sp)
    mv a0, sp
    call c2_irq_dispatch
    mv sp, a0
    lw gp, 0(sp)
    lw x1, 4(sp)
    lw x5, 20(sp)
    lw x6, 24(sp)
    lw x7, 28(sp)
    lw x8, 32(sp)
    lw x9, 36(sp)
    lw x10, 40(sp)
    lw x11, 44(sp)
    lw x12, 48(sp)
    lw x13, 52(sp)
    lw x14, 56(sp)
    lw x15, 60(sp)
    lw x16, 64(sp)
    lw x17, 68(sp)
    lw x18, 72(sp)
    lw x19, 76(sp)
    lw x20, 80(sp)
    lw x21, 84(sp)
    lw x22, 88(sp)
    lw x23, 92(sp)
    lw x24, 96(sp)
    lw x25, 100(sp)
    lw x26, 104(sp)
    lw x27, 108(sp)
    lw x28, 112(sp)
    lw x29, 116(sp)
    lw x30, 120(sp)
    lw x31, 124(sp)
    addi sp, sp, 128
    li tp, -2
    .insn r 0x0b, 0, 3, zero, tp, zero
    .insn r 0x0b, 0, 2, zero, gp, zero  /* retirq */
.section .note.GNU-stack,"",@progbits
