/*
 * DDR TPL v1.13 probe, placed in zero padding at 0xe200:
 *   +0x00 ef08:   blink blue power LED (GPIO0_B7) 10x, return -1 (Maskrom)
 *   +0x10 normal: blink 5x, continue normal TPL init at 0x8138
 * Only x0-x8 are used (x8 holds BootROM's lr); callee-saved regs and sp
 * stay untouched.
 */
	.equ	CAVE, 0xe200
	.equ	TPL_NORMAL, 0x8138

	.text
	.global _start
_start:
ef08:
	mov	x8, x30
	mov	w5, #10
	bl	blink
	b	ef08_ret
normal:
	mov	x8, x30
	mov	w5, #5
	bl	blink
	mov	x30, x8
	b	. + (TPL_NORMAL - (CAVE + (. - _start)))

ef08_ret:
	mov	x30, x8
	dsb	sy
	mov	w0, #-1
	ic	ialluis
	isb
	ret

/* w5 = count; clobbers x1-x6 */
blink:
	ldr	w1, =0xfdd00184		/* PMUCRU gate: ungate gpio0 clocks */
	ldr	w2, =0x06000000
	str	w2, [x1]
	ldr	w1, =0xfdc2000c		/* PMU_GRF GPIO0B_IOMUX_H: B7 -> GPIO */
	ldr	w2, =0x70000000
	str	w2, [x1]
	ldr	w1, =0xfdd60000		/* GPIO0 */
	ldr	w4, =0x80008000		/* LED on / DDR bit15 out */
	str	w4, [x1, #8]
	ldr	w3, =0x80000000		/* LED off */
1:
	str	w3, [x1]
	ldr	x6, =0x01000000
2:	subs	x6, x6, #1
	b.ne	2b
	str	w4, [x1]
	ldr	x6, =0x01000000
3:	subs	x6, x6, #1
	b.ne	3b
	subs	w5, w5, #1
	b.ne	1b
	dsb	sy
	ret
	.ltorg
