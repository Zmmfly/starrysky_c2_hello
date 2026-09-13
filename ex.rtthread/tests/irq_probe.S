/* Standalone diagnostic: print before executing any custom IRQ instruction.
 * Never unmask IRQs. If the CPU has ENABLE_IRQ=0, execution stops at maskirq.
 * This probes instruction availability, not IRQ routing or q-register mode.
 */
.section .text,"ax",@progbits
.global _start
_start:
    li s0, 0x10001000
    li t0, 625
    sw t0, 0(s0)
    la a0, before
    call puts
    li a0, -1
    .insn r 0x0b, 0, 3, a0, a0, zero
    la a0, after_mask
    call puts
    li a0, 100
    .insn r 0x0b, 0, 5, a0, a0, zero
    .insn r 0x0b, 0, 5, zero, zero, zero
    la a0, after_timer
    call puts
1:  j 1b
puts:
    lbu t0, 0(a0)
    beqz t0, 2f
    sw t0, 4(s0)
    addi a0, a0, 1
    j puts
2:  ret
before: .asciz "C2 IRQ probe: before maskirq\r\n"
after_mask: .asciz "C2 IRQ probe: maskirq supported\r\n"
after_timer: .asciz "C2 IRQ probe: timer supported (IRQs remain masked)\r\n"
.section .note.GNU-stack,"",@progbits
