	.text
	.even
	.globl _pistorm_pack_xyzw

/* PiStorm3D MiniGL v12 immediate-mode position packer.
 *
 * The original submitter always exports only bx/by/bz, even after glVertex4f.
 * Export bw as the fourth component so the library's existing vec4 position
 * shader can perform the normal homogeneous transform and perspective divide.
 */
_pistorm_pack_xyzw:
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

	move.l 80(a4),d0
	rol.w #8,d0
	swap d0
	rol.w #8,d0
	move.l d0,(a1)+

	subq.l #1,d1
	bne.s .Lvertex
	jmp 0x12a74
