	.text
	.even
	.globl _pistorm_pack_xyzwq

/* Binary-patch helper for PiStorm3D MiniGL's immediate triangle submitter.
 *
 * Entry registers/frame (owned by the original submit function):
 *   a5      original stack frame
 *   a3      GL context
 *   d6      vertex count
 *   12(a5)  pointer to ULONG vertex indices
 *  -56(a5)  destination V3D attribute buffer
 *
 * Pack X/Y/Z and MiniGLVertex.q as four little-endian floats, then continue
 * after the original XYZ-only packing loop.
 */
_pistorm_pack_xyzwq:
	movea.l 12(a5),a0
	movea.l -56(a5),a1
	move.l d6,d1
.Lvertex:
	move.l (a0)+,d0
	mulu.l #92,d0
	add.l 3964(a3),d0
	movea.l d0,a4

	move.l (a4),d0
	rol.w #8,d0
	swap d0
	rol.w #8,d0
	move.l d0,(a1)+

	move.l 4(a4),d0
	rol.w #8,d0
	swap d0
	rol.w #8,d0
	move.l d0,(a1)+

	move.l 8(a4),d0
	rol.w #8,d0
	swap d0
	rol.w #8,d0
	move.l d0,(a1)+

	tst.l 4028(a3)
	beq.s .Lq_one
	move.l 84(a4),d0
	bra.s .Lq_ready
.Lq_one:
	move.l #0x3f800000,d0
.Lq_ready:
	rol.w #8,d0
	swap d0
	rol.w #8,d0
	move.l d0,(a1)+

	subq.l #1,d1
	bne.s .Lvertex
	jmp 0x12a74
