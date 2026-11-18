_Head:
	/* Set _Entry address */
	adr	x1, _Payload

	/* Jump to the real code */
	b	_Start

	/* Fake Linux kernel header */

_StackBase0: //has to be 0 on some devices
	/* Text Offset */
	.quad 0

_StackSize0:
	/* Image Size */
	.quad 0

	/* Flags */
	.quad 0

	/* Reserved */
	.quad 0

	/* Reserved */
	.quad 0

	/* Reserved */
	.quad 0

	/* ARM64 Magic */
	.ascii "ARM\x64"

	/* Reserved */
	.long 0

	.ascii "EDK2-MSM"

_StackBase:
	/* Text Offset */
	.quad UEFI_BASE

_StackSize:
	/* Image Size */
	.quad UEFI_SIZE

_Start:
	/*
	 * mido visual checkpoint #1  (light grey band)
	 *
	 * Fill 0x90001000..0x90009140 of the lk2nd contiguous-splash buffer.
	 * Deliberately small (33 KiB, ~10 scanlines at 24bpp/stride 3240):
	 * this exact range is known to execute and be visible on this panel.
	 * 0x80808080 has all bytes equal so it renders as flat grey at either
	 * 24bpp or 32bpp.
	 */
	movz	x10, #0x9000, lsl #16
	movk	x10, #0x1000
	movz	x11, #0x9000, lsl #16
	movk	x11, #0x9140
	movz	x12, #0x8080
	movk	x12, #0x8080, lsl #16
_HeartbeatStart:
	str	w12, [x10], #4
	cmp	x10, x11
	b.lo	_HeartbeatStart

	mov	x4, x1
	ldr	x5, _StackBase
	cmp	x4, x5
	beq	_Entry
	ldr	x6,	_StackSize

_CopyLoop:
	ldp	x2, x3, [x4], #16
	stp	x2, x3, [x5], #16
	subs	x6, x6, #16
	b.ne	_CopyLoop
	ldr	x5, _StackBase

	/*
	 * mido visual checkpoint #2  (dark grey band, same small range)
	 * The FD has been copied to UEFI_BASE and we are about to jump to PrePi.
	 */
	movz	x10, #0x9000, lsl #16
	movk	x10, #0x1000
	movz	x11, #0x9000, lsl #16
	movk	x11, #0x9140
	movz	x12, #0x4040
	movk	x12, #0x4040, lsl #16
_HeartbeatCopied:
	str	w12, [x10], #4
	cmp	x10, x11
	b.lo	_HeartbeatCopied

_Entry:
	br	x5

_Dead:
	/* We should never get here */
	b	_Dead

.text
.align 4

_Payload:
	/* Your code will get ran right after this binary */
