Disassembly of section .text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh:

00000000 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)>:
   0:	022136        	entry	a1, 0x110
   3:	078d      	mov.n	a8, a7
   5:	186182        	s32i	a8, a1, 96
   8:	3ca182        	movi	a8, 0x13c
   b:	881a      	add.n	a8, a8, a1
   d:	000882        	l8ui	a8, a8, 0
  10:	1298      	l32i.n	a9, a2, 4
  12:	1a6182        	s32i	a8, a1, 104
  15:	40a182        	movi	a8, 0x140
  18:	881a      	add.n	a8, a8, a1
  1a:	b2cb      	addi.n	a11, a2, 12
  1c:	000882        	l8ui	a8, a8, 0
  1f:	22a8      	l32i.n	a10, a2, 8
  21:	839b90        	moveqz	a9, a11, a9
  24:	146132        	s32i	a3, a1, 80
  27:	156142        	s32i	a4, a1, 84
  2a:	166152        	s32i	a5, a1, 88
  2d:	176162        	s32i	a6, a1, 92
  30:	256182        	s32i	a8, a1, 148
  33:	1b6192        	s32i	a9, a1, 108
  36:	027d      	mov.n	a7, a2
  38:	140163        	lsi	f6, a1, 80
  3b:	9c1122        	l16ui	a2, a1, 0x138
  3e:	1501d3        	lsi	f13, a1, 84
  41:	440143        	lsi	f4, a1, 0x110
  44:	450123        	lsi	f2, a1, 0x114
  47:	490133        	lsi	f3, a1, 0x124
  4a:	4a0183        	lsi	f8, a1, 0x128
  4d:	4a8c      	beqz.n	a10, 55 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x55>
			4d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x55
  4f:	1c61a2        	s32i	a10, a1, 112
  52:	0001c6        	j	5d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5d>
			52: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5d
  55:	000081        	l32r	a8, fffc0058 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0058>
			55: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh
  58:	878a      	add.n	a8, a7, a8
  5a:	1c6182        	s32i	a8, a1, 112
  5d:	1a1460        	sub.s	f1, f4, f6
  60:	1ac8d0        	sub.s	f12, f8, f13
  63:	1af2d0        	sub.s	f15, f2, f13
  66:	1a0360        	sub.s	f0, f3, f6
  69:	000081        	l32r	a8, fffc006c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc006c>
			69: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x38
  6c:	2a51c0        	mul.s	f5, f1, f12
  6f:	fa9850        	wfr	f9, a8
  72:	5a5f00        	msub.s	f5, f15, f0
  75:	fa7510        	abs.s	f7, f5
  78:	5b0970        	ult.s	b0, f9, f7
  7b:	020076        	bf	b0, 81 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x81>
			7b: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x81
  7e:	022ac6        	j	92d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x92d>
			7e: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x92d
  81:	000081        	l32r	a8, fffc0084 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0084>
			81: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x50
  84:	fa9850        	wfr	f9, a8
  87:	4b0790        	olt.s	b0, f7, f9
  8a:	020076        	bf	b0, 90 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x90>
			8a: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x90
  8d:	022706        	j	92d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x92d>
			8d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x92d
  90:	000081        	l32r	a8, fffc0090 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0090>
			90: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4
  93:	1701a3        	lsi	f10, a1, 92
  96:	878a      	add.n	a8, a7, a8
  98:	0888      	l32i.n	a8, a8, 0
  9a:	2841a3        	ssi	f10, a1, 160
  9d:	1801a3        	lsi	f10, a1, 96
  a0:	4b0460        	olt.s	b0, f4, f6
  a3:	fa7600        	mov.s	f7, f6
  a6:	db7400        	movt.s	f7, f4, b0
  a9:	1d6182        	s32i	a8, a1, 116
  ac:	2941a3        	ssi	f10, a1, 164
  af:	caa800        	float.s	f10, a8, 0
  b2:	000081        	l32r	a8, fffc00b4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc00b4>
			b2: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x48
  b5:	4b0370        	olt.s	b0, f3, f7
  b8:	db7300        	movt.s	f7, f3, b0
  bb:	fab850        	wfr	f11, a8
  be:	4b0b70        	olt.s	b0, f11, f7
  c1:	160193        	lsi	f9, a1, 88
  c4:	210076        	bf	b0, e9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xe9>
			c4: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0xe9
  c7:	000081        	l32r	a8, fffc00c8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc00c8>
			c7: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x40
  ca:	fab850        	wfr	f11, a8
  cd:	4b07b0        	olt.s	b0, f7, f11
  d0:	150076        	bf	b0, e9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xe9>
			d0: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0xe9
  d3:	9a8700        	trunc.s	a8, f7, 0
  d6:	190c      	movi.n	a9, 1
  d8:	cab800        	float.s	f11, a8, 0
  db:	0a0c      	movi.n	a10, 0
  dd:	4b07b0        	olt.s	b0, f7, f11
  e0:	c39a00        	movf	a9, a10, b0
  e3:	c08890        	sub	a8, a8, a9
  e6:	ca7800        	float.s	f7, a8, 0
  e9:	000081        	l32r	a8, fffc00ec <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc00ec>
			e9: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x8
  ec:	4b0a70        	olt.s	b0, f10, f7
  ef:	878a      	add.n	a8, a7, a8
  f1:	0888      	l32i.n	a8, a8, 0
  f3:	1d2192        	l32i	a9, a1, 116
  f6:	dba700        	movt.s	f10, f7, b0
  f9:	4b0640        	olt.s	b0, f6, f4
  fc:	cb4600        	movf.s	f4, f6, b0
  ff:	1e6182        	s32i	a8, a1, 120
 102:	898a      	add.n	a8, a9, a8
 104:	4b0430        	olt.s	b0, f4, f3
 107:	880b      	addi.n	a8, a8, -1
 109:	cb3400        	movf.s	f3, f4, b0
 10c:	ca4800        	float.s	f4, a8, 0
 10f:	000081        	l32r	a8, fffc0110 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0110>
			10f: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x48
 112:	2641a3        	ssi	f10, a1, 152
 115:	fa7850        	wfr	f7, a8
 118:	4b0730        	olt.s	b0, f7, f3
 11b:	210076        	bf	b0, 140 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x140>
			11b: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x140
 11e:	000081        	l32r	a8, fffc0120 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0120>
			11e: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x40
 121:	fa7850        	wfr	f7, a8
 124:	4b0370        	olt.s	b0, f3, f7
 127:	150076        	bf	b0, 140 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x140>
			127: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x140
 12a:	9a9300        	trunc.s	a9, f3, 0
 12d:	180c      	movi.n	a8, 1
 12f:	ca7900        	float.s	f7, a9, 0
 132:	0a0c      	movi.n	a10, 0
 134:	4b0730        	olt.s	b0, f7, f3
 137:	c38a00        	movf	a8, a10, b0
 13a:	808890        	add	a8, a8, a9
 13d:	ca3800        	float.s	f3, a8, 0
 140:	4b0340        	olt.s	b0, f3, f4
 143:	000081        	l32r	a8, fffc0144 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0144>
			143: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0xc
 146:	db4300        	movt.s	f4, f3, b0
 149:	878a      	add.n	a8, a7, a8
 14b:	274143        	ssi	f4, a1, 156
 14e:	4b02d0        	olt.s	b0, f2, f13
 151:	0868      	l32i.n	a6, a8, 0
 153:	490076        	bf	b0, 1a0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1a0>
			153: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1a0
 156:	000081        	l32r	a8, fffc0158 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0158>
			156: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x48
 159:	4b0820        	olt.s	b0, f8, f2
 15c:	fa3850        	wfr	f3, a8
 15f:	191076        	bt	b0, 17c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x17c>
			15f: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x17c
 162:	4b0320        	olt.s	b0, f3, f2
 165:	fa3200        	mov.s	f3, f2
 168:	280076        	bf	b0, 194 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x194>
			168: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x194
 16b:	000081        	l32r	a8, fffc016c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc016c>
			16b: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x40
 16e:	fa4850        	wfr	f4, a8
 171:	4b0240        	olt.s	b0, f2, f4
 174:	491076        	bt	b0, 1c1 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1c1>
			174: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1c1
 177:	000646        	j	194 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x194>
			177: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x194
 17a:	00          	.byte	00
 17b:	00          	.byte	00
 17c:	4b0380        	olt.s	b0, f3, f8
 17f:	fa3800        	mov.s	f3, f8
 182:	021076        	bt	b0, 188 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x188>
			182: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x188
 185:	0002c6        	j	194 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x194>
			185: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x194
 188:	000081        	l32r	a8, fffc0188 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0188>
			188: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x40
 18b:	fa4850        	wfr	f4, a8
 18e:	4b0840        	olt.s	b0, f8, f4
 191:	2c1076        	bt	b0, 1c1 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1c1>
			191: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1c1
 194:	000081        	l32r	a8, fffc0194 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0194>
			194: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x10
 197:	878a      	add.n	a8, a7, a8
 199:	0888      	l32i.n	a8, a8, 0
 19b:	0011c6        	j	1e6 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1e6>
			19b: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1e6
 19e:	00          	.byte	00
 19f:	00          	.byte	00
 1a0:	000081        	l32r	a8, fffc01a0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc01a0>
			1a0: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x48
 1a3:	4b08d0        	olt.s	b0, f8, f13
 1a6:	fa3d00        	mov.s	f3, f13
 1a9:	db3800        	movt.s	f3, f8, b0
 1ac:	fa4850        	wfr	f4, a8
 1af:	4b0430        	olt.s	b0, f4, f3
 1b2:	230076        	bf	b0, 1d9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1d9>
			1b2: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1d9
 1b5:	000081        	l32r	a8, fffc01b8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc01b8>
			1b5: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x40
 1b8:	fa4850        	wfr	f4, a8
 1bb:	4b0340        	olt.s	b0, f3, f4
 1be:	170076        	bf	b0, 1d9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1d9>
			1be: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1d9
 1c1:	9a8300        	trunc.s	a8, f3, 0
 1c4:	01a092        	movi	a9, 1
 1c7:	ca4800        	float.s	f4, a8, 0
 1ca:	00a0a2        	movi	a10, 0
 1cd:	4b0340        	olt.s	b0, f3, f4
 1d0:	c39a00        	movf	a9, a10, b0
 1d3:	c08890        	sub	a8, a8, a9
 1d6:	ca3800        	float.s	f3, a8, 0
 1d9:	000081        	l32r	a8, fffc01dc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc01dc>
			1d9: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x10
 1dc:	4b0d20        	olt.s	b0, f13, f2
 1df:	878a      	add.n	a8, a7, a8
 1e1:	0888      	l32i.n	a8, a8, 0
 1e3:	021076        	bt	b0, 1e9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1e9>
			1e3: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1e9
 1e6:	fa2d00        	mov.s	f2, f13
 1e9:	000091        	l32r	a9, fffc01ec <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc01ec>
			1e9: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x48
 1ec:	4b0280        	olt.s	b0, f2, f8
 1ef:	db2800        	movt.s	f2, f8, b0
 1f2:	fa4950        	wfr	f4, a9
 1f5:	4b0420        	olt.s	b0, f4, f2
 1f8:	200076        	bf	b0, 21c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x21c>
			1f8: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x21c
 1fb:	000091        	l32r	a9, fffc01fc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc01fc>
			1fb: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x40
 1fe:	fa4950        	wfr	f4, a9
 201:	4b0240        	olt.s	b0, f2, f4
 204:	140076        	bf	b0, 21c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x21c>
			204: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x21c
 207:	9aa200        	trunc.s	a10, f2, 0
 20a:	190c      	movi.n	a9, 1
 20c:	ca4a00        	float.s	f4, a10, 0
 20f:	0b0c      	movi.n	a11, 0
 211:	4b0420        	olt.s	b0, f4, f2
 214:	c39b00        	movf	a9, a11, b0
 217:	99aa      	add.n	a9, a9, a10
 219:	ca2900        	float.s	f2, a9, 0
 21c:	224103        	ssi	f0, a1, 136
 21f:	194113        	ssi	f1, a1, 100
 222:	270103        	lsi	f0, a1, 156
 225:	260113        	lsi	f1, a1, 152
 228:	2a4193        	ssi	f9, a1, 168
 22b:	2441d3        	ssi	f13, a1, 144
 22e:	234163        	ssi	f6, a1, 140
 231:	2141f3        	ssi	f15, a1, 132
 234:	2041c3        	ssi	f12, a1, 128
 237:	4b0010        	olt.s	b0, f0, f1
 23a:	020076        	bf	b0, 240 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x240>
			23a: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x240
 23d:	01bb06        	j	92d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x92d>
			23d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x92d
 240:	868a      	add.n	a8, a6, a8
 242:	ca4600        	float.s	f4, a6, 0
 245:	880b      	addi.n	a8, a8, -1
 247:	ca7800        	float.s	f7, a8, 0
 24a:	4b0430        	olt.s	b0, f4, f3
 24d:	cb3400        	movf.s	f3, f4, b0
 250:	4b0270        	olt.s	b0, f2, f7
 253:	cb2700        	movf.s	f2, f7, b0
 256:	4b0230        	olt.s	b0, f2, f3
 259:	020076        	bf	b0, 25f <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x25f>
			259: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x25f
 25c:	01b346        	j	92d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x92d>
			25c: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x92d
 25f:	9a8100        	trunc.s	a8, f1, 0
 262:	0000a1        	l32r	a10, fffc0264 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0264>
			262: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1c
 265:	366182        	s32i	a8, a1, 216
 268:	9a8000        	trunc.s	a8, f0, 0
 26b:	fab540        	rfr	a11, f5
 26e:	376182        	s32i	a8, a1, 220
 271:	9a8200        	trunc.s	a8, f2, 0
 274:	9a3300        	trunc.s	a3, f3, 0
 277:	1f6182        	s32i	a8, a1, 124
 27a:	000081        	l32r	a8, fffc027c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc027c>
			27a: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x58
			27a: R_XTENSA_ASM_EXPAND	__divsf3
 27d:	0008e0        	callx8	a8
 280:	252192        	l32i	a9, a1, 148
 283:	ffa082        	movi	a8, 255
 286:	190113        	lsi	f1, a1, 100
 289:	2001c3        	lsi	f12, a1, 128
 28c:	2101f3        	lsi	f15, a1, 132
 28f:	220103        	lsi	f0, a1, 136
 292:	230163        	lsi	f6, a1, 140
 295:	2401d3        	lsi	f13, a1, 144
 298:	2a0193        	lsi	f9, a1, 168
 29b:	fa4a50        	wfr	f4, a10
 29e:	029987        	bne	a9, a8, 2a4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x2a4>
			29e: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x2a4
 2a1:	0021c6        	j	32c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x32c>
			2a1: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x32c
 2a4:	ca0900        	float.s	f0, a9, 0
 2a7:	0000b1        	l32r	a11, fffc02a8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc02a8>
			2a7: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x54
 2aa:	2b4193        	ssi	f9, a1, 172
 2ad:	2a41d3        	ssi	f13, a1, 168
 2b0:	2461a2        	s32i	a10, a1, 144
 2b3:	faa040        	rfr	a10, f0
 2b6:	000081        	l32r	a8, fffc02b8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc02b8>
			2b6: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5c
			2b6: R_XTENSA_ASM_EXPAND	__divsf3
 2b9:	0008e0        	callx8	a8
 2bc:	54b520        	extui	a11, a2, 5, 6
 2bf:	418b20        	srli	a8, a2, 11
 2c2:	ca5b00        	float.s	f5, a11, 0
 2c5:	ca3800        	float.s	f3, a8, 0
 2c8:	fa0a50        	wfr	f0, a10
 2cb:	448020        	extui	a8, a2, 0, 5
 2ce:	2a5500        	mul.s	f5, f5, f0
 2d1:	2a3300        	mul.s	f3, f3, f0
 2d4:	ca2800        	float.s	f2, a8, 0
 2d7:	3861a2        	s32i	a10, a1, 224
 2da:	2a2200        	mul.s	f2, f2, f0
 2dd:	9a8500        	trunc.s	a8, f5, 0
 2e0:	9aa300        	trunc.s	a10, f3, 0
 2e3:	0c0c      	movi.n	a12, 0
 2e5:	fd1c      	movi.n	a13, 31
 2e7:	9ab200        	trunc.s	a11, f2, 0
 2ea:	5388c0        	max	a8, a8, a12
 2ed:	53aac0        	max	a10, a10, a12
 2f0:	fe3c      	movi.n	a14, 63
 2f2:	4388e0        	min	a8, a8, a14
 2f5:	43aad0        	min	a10, a10, a13
 2f8:	1188b0        	slli	a8, a8, 5
 2fb:	11aa50        	slli	a10, a10, 11
 2fe:	53bbc0        	max	a11, a11, a12
 301:	2088a0        	or	a8, a8, a10
 304:	43bbd0        	min	a11, a11, a13
 307:	2088b0        	or	a8, a8, a11
 30a:	f48080        	extui	a8, a8, 0, 16
 30d:	190113        	lsi	f1, a1, 100
 310:	2001c3        	lsi	f12, a1, 128
 313:	2101f3        	lsi	f15, a1, 132
 316:	220103        	lsi	f0, a1, 136
 319:	230163        	lsi	f6, a1, 140
 31c:	240143        	lsi	f4, a1, 144
 31f:	2a01d3        	lsi	f13, a1, 168
 322:	2b0193        	lsi	f9, a1, 172
 325:	2e6182        	s32i	a8, a1, 184
 328:	000346        	j	339 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x339>
			328: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x339
 32b:	00          	.byte	00
 32c:	000081        	l32r	a8, fffc032c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc032c>
			32c: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1c
 32f:	090c      	movi.n	a9, 0
 331:	991a      	add.n	a9, a9, a1
 333:	386982        	s32i	a8, a9, 224
 336:	2e6122        	s32i	a2, a1, 184
 339:	07b8      	l32i.n	a11, a7, 0
 33b:	40fb80        	nsau	a8, a11
 33e:	418580        	srli	a8, a8, 5
 341:	054b16        	beqz	a11, 399 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x399>
			341: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x399
 344:	1a2192        	l32i	a9, a1, 104
 347:	1a0c      	movi.n	a10, 1
 349:	838a90        	moveqz	a8, a10, a9
 34c:	049856        	bnez	a8, 399 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x399>
			34c: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x399
 34f:	252182        	l32i	a8, a1, 148
 352:	11e280        	slli	a14, a2, 8
 355:	018880        	slli	a8, a8, 24
 358:	0000a1        	l32r	a10, fffc0358 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0358>
			358: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x14
 35b:	20ee90        	or	a14, a14, a9
 35e:	20ee80        	or	a14, a14, a8
 361:	82aea0        	mull	a10, a14, a10
 364:	65a9a0        	extui	a10, a10, 25, 7
 367:	8baa      	add.n	a8, a11, a10
 369:	000882        	l8ui	a8, a8, 0
 36c:	b8ac      	beqz.n	a8, 39b <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x39b>
			36c: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x39b
 36e:	880b      	addi.n	a8, a8, -1
 370:	a098b0        	addx4	a9, a8, a11
 373:	aa1b      	addi.n	a10, a10, 1
 375:	202992        	l32i	a9, a9, 128
 378:	64a0a0        	extui	a10, a10, 0, 7
 37b:	cbaa      	add.n	a12, a11, a10
 37d:	0f9e97        	bne	a14, a9, 390 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x390>
			37d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x390
 380:	f48080        	extui	a8, a8, 0, 16
 383:	118850        	slli	a8, a8, 11
 386:	80a1a2        	movi	a10, 0x180
 389:	88aa      	add.n	a8, a8, a10
 38b:	8b8a      	add.n	a8, a11, a8
 38d:	000286        	j	39b <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x39b>
			38d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x39b
 390:	000c82        	l8ui	a8, a12, 0
 393:	fd7856        	bnez	a8, 36e <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x36e>
			393: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x36e
 396:	000046        	j	39b <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x39b>
			396: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x39b
 399:	080c      	movi.n	a8, 0
 39b:	362192        	l32i	a9, a1, 216
 39e:	2c6182        	s32i	a8, a1, 176
 3a1:	372182        	l32i	a8, a1, 220
 3a4:	2b4103        	ssi	f0, a1, 172
 3a7:	c0e890        	sub	a14, a8, a9
 3aa:	1f2182        	l32i	a8, a1, 124
 3ad:	280103        	lsi	f0, a1, 160
 3b0:	c0d830        	sub	a13, a8, a3
 3b3:	80a082        	movi	a8, 128
 3b6:	825ed0        	mull	a5, a14, a13
 3b9:	4c1c      	movi.n	a12, 20
 3bb:	881a      	add.n	a8, a8, a1
 3bd:	034103        	ssi	f0, a1, 12
 3c0:	90a0b2        	movi	a11, 144
 3c3:	290103        	lsi	f0, a1, 164
 3c6:	bb8a      	add.n	a11, a11, a8
 3c8:	a1ca      	add.n	a10, a1, a12
 3ca:	2d4143        	ssi	f4, a1, 180
 3cd:	2a41f3        	ssi	f15, a1, 168
 3d0:	2441c3        	ssi	f12, a1, 144
 3d3:	234113        	ssi	f1, a1, 140
 3d6:	004163        	ssi	f6, a1, 0
 3d9:	224163        	ssi	f6, a1, 136
 3dc:	0141d3        	ssi	f13, a1, 4
 3df:	2141d3        	ssi	f13, a1, 132
 3e2:	024193        	ssi	f9, a1, 8
 3e5:	194193        	ssi	f9, a1, 100
 3e8:	044103        	ssi	f0, a1, 16
 3eb:	206152        	s32i	a5, a1, 128
 3ee:	000081        	l32r	a8, fffc03f0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc03f0>
			3ee: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x60
			3ee: R_XTENSA_ASM_EXPAND	memcpy
 3f1:	0008e0        	callx8	a8
 3f4:	a4a082        	movi	a8, 164
 3f7:	80a092        	movi	a9, 128
 3fa:	998a      	add.n	a9, a9, a8
 3fc:	4c1c      	movi.n	a12, 20
 3fe:	b91a      	add.n	a11, a9, a1
 400:	28c1a2        	addi	a10, a1, 40
 403:	000081        	l32r	a8, fffc0404 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0404>
			403: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x64
			403: R_XTENSA_ASM_EXPAND	memcpy
 406:	0008e0        	callx8	a8
 409:	070c      	movi.n	a7, 0
 40b:	f179      	s32i.n	a7, a1, 60
 40d:	00a1a2        	movi	a10, 0x100
 410:	106172        	s32i	a7, a1, 64
 413:	116172        	s32i	a7, a1, 68
 416:	190193        	lsi	f9, a1, 100
 419:	2101d3        	lsi	f13, a1, 132
 41c:	220163        	lsi	f6, a1, 136
 41f:	230113        	lsi	f1, a1, 140
 422:	2401c3        	lsi	f12, a1, 144
 425:	2a01f3        	lsi	f15, a1, 168
 428:	2b0103        	lsi	f0, a1, 172
 42b:	2c2182        	l32i	a8, a1, 176
 42e:	2d0143        	lsi	f4, a1, 180
 431:	022a57        	blt	a10, a5, 437 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x437>
			431: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x437
 434:	013b06        	j	924 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x924>
			434: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x924
 437:	3cc142        	addi	a4, a1, 60
 43a:	2a6182        	s32i	a8, a1, 168
 43d:	0000c1        	l32r	a12, fffc0440 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0440>
			43d: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x18
 440:	048d      	mov.n	a8, a4
 442:	075d      	mov.n	a5, a7
 444:	194113        	ssi	f1, a1, 100
 447:	2141c3        	ssi	f12, a1, 132
 44a:	2241f3        	ssi	f15, a1, 136
 44d:	234103        	ssi	f0, a1, 140
 450:	244163        	ssi	f6, a1, 144
 453:	2b4143        	ssi	f4, a1, 172
 456:	2c41d3        	ssi	f13, a1, 176
 459:	2d4193        	ssi	f9, a1, 180
 45c:	014d      	mov.n	a4, a1
 45e:	087d      	mov.n	a7, a8
 460:	551b      	addi.n	a5, a5, 1
 462:	a285c0        	muluh	a8, a5, a12
 465:	010413        	lsi	f1, a4, 4
 468:	418180        	srli	a8, a8, 1
 46b:	908880        	addx2	a8, a8, a8
 46e:	c08580        	sub	a8, a5, a8
 471:	a08880        	addx4	a8, a8, a8
 474:	a08810        	addx4	a8, a8, a1
 477:	010803        	lsi	f0, a8, 4
 47a:	000091        	l32r	a9, fffc047c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc047c>
			47a: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4c
 47d:	1a0010        	sub.s	f0, f0, f1
 480:	fa1010        	abs.s	f1, f0
 483:	fab040        	rfr	a11, f0
 486:	fa0950        	wfr	f0, a9
 489:	6b0010        	ole.s	b0, f0, f1
 48c:	160076        	bf	b0, 4a6 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x4a6>
			48c: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4a6
 48f:	000803        	lsi	f0, a8, 0
 492:	000413        	lsi	f1, a4, 0
 495:	1a0010        	sub.s	f0, f0, f1
 498:	faa040        	rfr	a10, f0
 49b:	000081        	l32r	a8, fffc049c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc049c>
			49b: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x68
			49b: R_XTENSA_ASM_EXPAND	__divsf3
 49e:	0008e0        	callx8	a8
 4a1:	0000c1        	l32r	a12, fffc04a4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc04a4>
			4a1: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x18
 4a4:	07a9      	s32i.n	a10, a7, 0
 4a6:	14c442        	addi	a4, a4, 20
 4a9:	774b      	addi.n	a7, a7, 4
 4ab:	b13566        	bnei	a5, 3, 460 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x460>
			4ab: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x460
 4ae:	190113        	lsi	f1, a1, 100
 4b1:	2101c3        	lsi	f12, a1, 132
 4b4:	2201f3        	lsi	f15, a1, 136
 4b7:	230103        	lsi	f0, a1, 140
 4ba:	240163        	lsi	f6, a1, 144
 4bd:	2a2182        	l32i	a8, a1, 168
 4c0:	2b0143        	lsi	f4, a1, 172
 4c3:	2c01d3        	lsi	f13, a1, 176
 4c6:	2d0193        	lsi	f9, a1, 180
 4c9:	0115c6        	j	924 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x924>
			4c9: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x924
 4cc:	1e2192        	l32i	a9, a1, 120
 4cf:	c0e360        	sub	a14, a3, a6
 4d2:	82ee90        	mull	a14, a14, a9
 4d5:	000091        	l32r	a9, fffc04d8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc04d8>
			4d5: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x20
 4d8:	216122        	s32i	a2, a1, 132
 4db:	2e2122        	l32i	a2, a1, 184
 4de:	fa7950        	wfr	f7, a9
 4e1:	224113        	ssi	f1, a1, 136
 4e4:	00a192        	movi	a9, 0x100
 4e7:	234103        	ssi	f0, a1, 140
 4ea:	2441d3        	ssi	f13, a1, 144
 4ed:	ca3300        	float.s	f3, a3, 0
 4f0:	2021a2        	l32i	a10, a1, 128
 4f3:	0a3370        	add.s	f3, f3, f7
 4f6:	0829a7        	blt	a9, a10, 502 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x502>
			4f6: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x502
 4f9:	372142        	l32i	a4, a1, 220
 4fc:	362172        	l32i	a7, a1, 216
 4ff:	005386        	j	651 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x651>
			4ff: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x651
 502:	0b0c      	movi.n	a11, 0
 504:	270153        	lsi	f5, a1, 156
 507:	260123        	lsi	f2, a1, 152
 50a:	01cd      	mov.n	a12, a1
 50c:	3cc172        	addi	a7, a1, 60
 50f:	0b5d      	mov.n	a5, a11
 511:	0000a1        	l32r	a10, fffc0514 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0514>
			511: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x18
 514:	bb1b      	addi.n	a11, a11, 1
 516:	a2aba0        	muluh	a10, a11, a10
 519:	010c13        	lsi	f1, a12, 4
 51c:	41a1a0        	srli	a10, a10, 1
 51f:	90aaa0        	addx2	a10, a10, a10
 522:	c0aba0        	sub	a10, a11, a10
 525:	a0daa0        	addx4	a13, a10, a10
 528:	a0dd10        	addx4	a13, a13, a1
 52b:	010d03        	lsi	f0, a13, 4
 52e:	000061        	l32r	a6, fffc0530 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0530>
			52e: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x50
 531:	4b0010        	olt.s	b0, f0, f1
 534:	fa8100        	mov.s	f8, f1
 537:	db8000        	movt.s	f8, f0, b0
 53a:	faa650        	wfr	f10, a6
 53d:	1a88a0        	sub.s	f8, f8, f10
 540:	0cfd      	mov.n	a15, a12
 542:	4b0380        	olt.s	b0, f3, f8
 545:	741076        	bt	b0, 5bd <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5bd>
			545: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5bd
 548:	4b0100        	olt.s	b0, f1, f0
 54b:	fa8100        	mov.s	f8, f1
 54e:	db8000        	movt.s	f8, f0, b0
 551:	0a88a0        	add.s	f8, f8, f10
 554:	4b0830        	olt.s	b0, f8, f3
 557:	621076        	bt	b0, 5bd <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5bd>
			557: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5bd
 55a:	1a0010        	sub.s	f0, f0, f1
 55d:	000061        	l32r	a6, fffc0560 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0560>
			55d: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4c
 560:	000c83        	lsi	f8, a12, 0
 563:	faa650        	wfr	f10, a6
 566:	fa0010        	abs.s	f0, f0
 569:	4b00a0        	olt.s	b0, f0, f10
 56c:	350076        	bf	b0, 5a5 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5a5>
			56c: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5a5
 56f:	000d03        	lsi	f0, a13, 0
 572:	4b0080        	olt.s	b0, f0, f8
 575:	0f0076        	bf	b0, 588 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x588>
			575: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x588
 578:	f4a0a0        	extui	a10, a10, 0, 16
 57b:	a0aaa0        	addx4	a10, a10, a10
 57e:	a0fa10        	addx4	a15, a10, a1
 581:	fa1000        	mov.s	f1, f0
 584:	0000c6        	j	58b <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x58b>
			584: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x58b
 587:	00          	.byte	00
 588:	fa1800        	mov.s	f1, f8
 58b:	4b0150        	olt.s	b0, f1, f5
 58e:	020076        	bf	b0, 594 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x594>
			58e: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x594
 591:	000f53        	lsi	f5, a15, 0
 594:	4b0800        	olt.s	b0, f8, f0
 597:	cb0800        	movf.s	f0, f8, b0
 59a:	4b0200        	olt.s	b0, f2, f0
 59d:	db2000        	movt.s	f2, f0, b0
 5a0:	150c      	movi.n	a5, 1
 5a2:	0005c6        	j	5bd <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5bd>
			5a2: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5bd
 5a5:	1a1310        	sub.s	f1, f3, f1
 5a8:	000703        	lsi	f0, a7, 0
 5ab:	01a052        	movi	a5, 1
 5ae:	4a8010        	madd.s	f8, f0, f1
 5b1:	4b0850        	olt.s	b0, f8, f5
 5b4:	db5800        	movt.s	f5, f8, b0
 5b7:	4b0280        	olt.s	b0, f2, f8
 5ba:	db2800        	movt.s	f2, f8, b0
 5bd:	14ccc2        	addi	a12, a12, 20
 5c0:	774b      	addi.n	a7, a7, 4
 5c2:	023b26        	beqi	a11, 3, 5c8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5c8>
			5c2: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5c8
 5c5:	ffd206        	j	511 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x511>
			5c5: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x511
 5c8:	f2d516        	beqz	a5, 4f9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x4f9>
			5c8: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4f9
 5cb:	3621a2        	l32i	a10, a1, 216
 5ce:	ca8a00        	float.s	f8, a10, 0
 5d1:	0000a1        	l32r	a10, fffc05d4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc05d4>
			5d1: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x48
 5d4:	fa0a50        	wfr	f0, a10
 5d7:	4b0050        	olt.s	b0, f0, f5
 5da:	220076        	bf	b0, 600 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x600>
			5da: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x600
 5dd:	0000a1        	l32r	a10, fffc05e0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc05e0>
			5dd: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x40
 5e0:	fa0a50        	wfr	f0, a10
 5e3:	4b0500        	olt.s	b0, f5, f0
 5e6:	160076        	bf	b0, 600 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x600>
			5e6: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x600
 5e9:	9aa500        	trunc.s	a10, f5, 0
 5ec:	1b0c      	movi.n	a11, 1
 5ee:	ca0a00        	float.s	f0, a10, 0
 5f1:	00a0c2        	movi	a12, 0
 5f4:	4b0500        	olt.s	b0, f5, f0
 5f7:	c3bc00        	movf	a11, a12, b0
 5fa:	c0aab0        	sub	a10, a10, a11
 5fd:	ca5a00        	float.s	f5, a10, 0
 600:	0000a1        	l32r	a10, fffc0600 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0600>
			600: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x44
 603:	fa1a50        	wfr	f1, a10
 606:	1a5510        	sub.s	f5, f5, f1
 609:	3721a2        	l32i	a10, a1, 220
 60c:	ca0a00        	float.s	f0, a10, 0
 60f:	4b0850        	olt.s	b0, f8, f5
 612:	0000a1        	l32r	a10, fffc0614 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0614>
			612: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x48
 615:	cb5800        	movf.s	f5, f8, b0
 618:	9a7500        	trunc.s	a7, f5, 0
 61b:	fa5a50        	wfr	f5, a10
 61e:	4b0520        	olt.s	b0, f5, f2
 621:	200076        	bf	b0, 645 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x645>
			621: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x645
 624:	0000a1        	l32r	a10, fffc0624 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0624>
			624: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x40
 627:	fa5a50        	wfr	f5, a10
 62a:	4b0250        	olt.s	b0, f2, f5
 62d:	140076        	bf	b0, 645 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x645>
			62d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x645
 630:	9ab200        	trunc.s	a11, f2, 0
 633:	1a0c      	movi.n	a10, 1
 635:	ca5b00        	float.s	f5, a11, 0
 638:	0c0c      	movi.n	a12, 0
 63a:	4b0520        	olt.s	b0, f5, f2
 63d:	c3ac00        	movf	a10, a12, b0
 640:	aaba      	add.n	a10, a10, a11
 642:	ca2a00        	float.s	f2, a10, 0
 645:	0a2210        	add.s	f2, f2, f1
 648:	4b0200        	olt.s	b0, f2, f0
 64b:	cb2000        	movf.s	f2, f0, b0
 64e:	9a4200        	trunc.s	a4, f2, 0
 651:	14a477        	bge	a4, a7, 669 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x669>
			651: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x669
 654:	1e21a2        	l32i	a10, a1, 120
 657:	331b      	addi.n	a3, a3, 1
 659:	eeaa      	add.n	a14, a14, a10
 65b:	1f21a2        	l32i	a10, a1, 124
 65e:	022a37        	blt	a10, a3, 664 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x664>
			65e: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x664
 661:	ffa206        	j	4ed <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x4ed>
			661: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4ed
 664:	00b146        	j	92d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x92d>
			664: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x92d
 667:	00          	.byte	00
 668:	00          	.byte	00
 669:	240103        	lsi	f0, a1, 144
 66c:	1d21a2        	l32i	a10, a1, 116
 66f:	1a3300        	sub.s	f3, f3, f0
 672:	c06ea0        	sub	a6, a14, a10
 675:	230103        	lsi	f0, a1, 140
 678:	0000a1        	l32r	a10, fffc0678 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0678>
			678: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x24
 67b:	2a8030        	mul.s	f8, f0, f3
 67e:	fa2a50        	wfr	f2, a10
 681:	220103        	lsi	f0, a1, 136
 684:	0a0c      	movi.n	a10, 0
 686:	faaa50        	wfr	f10, a10
 689:	1b21a2        	l32i	a10, a1, 108
 68c:	667a      	add.n	a6, a6, a7
 68e:	2a3030        	mul.s	f3, f0, f3
 691:	9056a0        	addx2	a5, a6, a10
 694:	1c21a2        	l32i	a10, a1, 112
 697:	196132        	s32i	a3, a1, 100
 69a:	9066a0        	addx2	a6, a6, a10
 69d:	ca5700        	float.s	f5, a7, 0
 6a0:	0a5570        	add.s	f5, f5, f7
 6a3:	1a5560        	sub.s	f5, f5, f6
 6a6:	2a0c50        	mul.s	f0, f12, f5
 6a9:	1a0080        	sub.s	f0, f0, f8
 6ac:	2a0040        	mul.s	f0, f0, f4
 6af:	4b0020        	olt.s	b0, f0, f2
 6b2:	020076        	bf	b0, 6b8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x6b8>
			6b2: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6b8
 6b5:	009646        	j	912 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x912>
			6b5: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x912
 6b8:	fa1300        	mov.s	f1, f3
 6bb:	5a1f50        	msub.s	f1, f15, f5
 6be:	2a1140        	mul.s	f1, f1, f4
 6c1:	4b0120        	olt.s	b0, f1, f2
 6c4:	020076        	bf	b0, 6ca <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x6ca>
			6c4: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6ca
 6c7:	0091c6        	j	912 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x912>
			6c7: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x912
 6ca:	0a5010        	add.s	f5, f0, f1
 6cd:	0000a1        	l32r	a10, fffc06d0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc06d0>
			6cd: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x3c
 6d0:	faba50        	wfr	f11, a10
 6d3:	4b0b50        	olt.s	b0, f11, f5
 6d6:	020076        	bf	b0, 6dc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x6dc>
			6d6: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6dc
 6d9:	008d46        	j	912 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x912>
			6d9: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x912
 6dc:	460153        	lsi	f5, a1, 0x118
 6df:	1ad590        	sub.s	f13, f5, f9
 6e2:	4b0153        	lsi	f5, a1, 0x12c
 6e5:	1ab590        	sub.s	f11, f5, f9
 6e8:	fa5900        	mov.s	f5, f9
 6eb:	4a5d00        	madd.s	f5, f13, f0
 6ee:	4a5b10        	madd.s	f5, f11, f1
 6f1:	4b0a50        	olt.s	b0, f10, f5
 6f4:	021076        	bt	b0, 6fa <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x6fa>
			6f4: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6fa
 6f7:	0085c6        	j	912 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x912>
			6f7: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x912
 6fa:	0000a1        	l32r	a10, fffc06fc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc06fc>
			6fa: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x38
 6fd:	fab510        	abs.s	f11, f5
 700:	fada50        	wfr	f13, a10
 703:	5b0db0        	ult.s	b0, f13, f11
 706:	020076        	bf	b0, 70c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x70c>
			706: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x70c
 709:	008146        	j	912 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x912>
			709: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x912
 70c:	0000a1        	l32r	a10, fffc070c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc070c>
			70c: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x30
 70f:	0000b1        	l32r	a11, fffc0710 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0710>
			70f: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1c
 712:	faba50        	wfr	f11, a10
 715:	2ab5b0        	mul.s	f11, f5, f11
 718:	0000a1        	l32r	a10, fffc0718 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0718>
			718: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x34
 71b:	faeb50        	wfr	f14, a11
 71e:	fada50        	wfr	f13, a10
 721:	4b0be0        	olt.s	b0, f11, f14
 724:	dbbe00        	movt.s	f11, f14, b0
 727:	4b0db0        	olt.s	b0, f13, f11
 72a:	cbdb00        	movf.s	f13, f11, b0
 72d:	ea3d00        	utrunc.s	a3, f13, 0
 730:	0015a2        	l16ui	a10, a5, 0
 733:	f43030        	extui	a3, a3, 0, 16
 736:	02b3a7        	bgeu	a3, a10, 73c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x73c>
			736: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x73c
 739:	007546        	j	912 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x912>
			739: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x912
 73c:	2121a2        	l32i	a10, a1, 132
 73f:	1a21b2        	l32i	a11, a1, 104
 742:	83a280        	moveqz	a10, a2, a8
 745:	1c3b16        	beqz	a11, 90c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x90c>
			745: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x90c
 748:	280123        	lsi	f2, a1, 160
 74b:	304133        	ssi	f3, a1, 192
 74e:	470133        	lsi	f3, a1, 0x11c
 751:	3561e2        	s32i	a14, a1, 212
 754:	1ad320        	sub.s	f13, f3, f2
 757:	4c0133        	lsi	f3, a1, 0x130
 75a:	344193        	ssi	f9, a1, 208
 75d:	1ab320        	sub.s	f11, f3, f2
 760:	4a2d00        	madd.s	f2, f13, f0
 763:	334143        	ssi	f4, a1, 204
 766:	326182        	s32i	a8, a1, 200
 769:	314163        	ssi	f6, a1, 196
 76c:	fae200        	mov.s	f14, f2
 76f:	4aeb10        	madd.s	f14, f11, f1
 772:	2f4183        	ssi	f8, a1, 188
 775:	2e41f3        	ssi	f15, a1, 184
 778:	2d41c3        	ssi	f12, a1, 180
 77b:	fab540        	rfr	a11, f5
 77e:	faae40        	rfr	a10, f14
 781:	2a4153        	ssi	f5, a1, 168
 784:	2c4103        	ssi	f0, a1, 176
 787:	2b4113        	ssi	f1, a1, 172
 78a:	000081        	l32r	a8, fffc078c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc078c>
			78a: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6c
			78a: R_XTENSA_ASM_EXPAND	__divsf3
 78d:	0008e0        	callx8	a8
 790:	290103        	lsi	f0, a1, 164
 793:	480113        	lsi	f1, a1, 0x120
 796:	2a0153        	lsi	f5, a1, 168
 799:	1ad100        	sub.s	f13, f1, f0
 79c:	4d0113        	lsi	f1, a1, 0x134
 79f:	fab540        	rfr	a11, f5
 7a2:	1ab100        	sub.s	f11, f1, f0
 7a5:	fa5000        	mov.s	f5, f0
 7a8:	2c0103        	lsi	f0, a1, 176
 7ab:	2b0113        	lsi	f1, a1, 172
 7ae:	4a5d00        	madd.s	f5, f13, f0
 7b1:	2a61a2        	s32i	a10, a1, 168
 7b4:	4a5b10        	madd.s	f5, f11, f1
 7b7:	faa540        	rfr	a10, f5
 7ba:	000081        	l32r	a8, fffc07bc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc07bc>
			7ba: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x70
			7ba: R_XTENSA_ASM_EXPAND	__divsf3
 7bd:	0008e0        	callx8	a8
 7c0:	000081        	l32r	a8, fffc07c0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc07c0>
			7c0: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x20
 7c3:	2a01e3        	lsi	f14, a1, 168
 7c6:	fa7850        	wfr	f7, a8
 7c9:	000081        	l32r	a8, fffc07cc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc07cc>
			7c9: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x24
 7cc:	2d01c3        	lsi	f12, a1, 180
 7cf:	fa2850        	wfr	f2, a8
 7d2:	080c      	movi.n	a8, 0
 7d4:	faa850        	wfr	f10, a8
 7d7:	322182        	l32i	a8, a1, 200
 7da:	2e01f3        	lsi	f15, a1, 184
 7dd:	2f0183        	lsi	f8, a1, 188
 7e0:	300133        	lsi	f3, a1, 192
 7e3:	310163        	lsi	f6, a1, 196
 7e6:	330143        	lsi	f4, a1, 204
 7e9:	340193        	lsi	f9, a1, 208
 7ec:	3521e2        	l32i	a14, a1, 212
 7ef:	fa5a50        	wfr	f5, a10
 7f2:	00a192        	movi	a9, 0x100
 7f5:	f8bc      	beqz.n	a8, 838 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x838>
			7f5: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x838
 7f7:	0000a1        	l32r	a10, fffc07f8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc07f8>
			7f7: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x28
 7fa:	fa0a50        	wfr	f0, a10
 7fd:	2abe00        	mul.s	f11, f14, f0
 800:	2a0500        	mul.s	f0, f5, f0
 803:	0000a1        	l32r	a10, fffc0804 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0804>
			803: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x2c
 806:	4b0ba0        	olt.s	b0, f11, f10
 809:	fa5b00        	mov.s	f5, f11
 80c:	db5a00        	movt.s	f5, f10, b0
 80f:	4b00a0        	olt.s	b0, f0, f10
 812:	fa1a50        	wfr	f1, a10
 815:	db0a00        	movt.s	f0, f10, b0
 818:	4b0100        	olt.s	b0, f1, f0
 81b:	db0100        	movt.s	f0, f1, b0
 81e:	4b0150        	olt.s	b0, f1, f5
 821:	eaa000        	utrunc.s	a10, f0, 0
 824:	cb1500        	movf.s	f1, f5, b0
 827:	eab100        	utrunc.s	a11, f1, 0
 82a:	11aab0        	slli	a10, a10, 5
 82d:	aaba      	add.n	a10, a10, a11
 82f:	90aa80        	addx2	a10, a10, a8
 832:	001aa2        	l16ui	a10, a10, 0
 835:	0034c6        	j	90c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x90c>
			835: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x90c
 838:	4b0ea0        	olt.s	b0, f14, f10
 83b:	306182        	s32i	a8, a1, 192
 83e:	000081        	l32r	a8, fffc0840 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0840>
			83e: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1c
 841:	dbea00        	movt.s	f14, f10, b0
 844:	4b05a0        	olt.s	b0, f5, f10
 847:	fa0850        	wfr	f0, a8
 84a:	db5a00        	movt.s	f5, f10, b0
 84d:	4b0050        	olt.s	b0, f0, f5
 850:	db5000        	movt.s	f5, f0, b0
 853:	2121b2        	l32i	a11, a1, 132
 856:	4b00e0        	olt.s	b0, f0, f14
 859:	face40        	rfr	a12, f14
 85c:	1a21a2        	l32i	a10, a1, 104
 85f:	d3c800        	movt	a12, a8, b0
 862:	fad540        	rfr	a13, f5
 865:	3361e2        	s32i	a14, a1, 204
 868:	324193        	ssi	f9, a1, 200
 86b:	314143        	ssi	f4, a1, 196
 86e:	2f4163        	ssi	f6, a1, 188
 871:	2e4133        	ssi	f3, a1, 184
 874:	2d4183        	ssi	f8, a1, 180
 877:	2c41f3        	ssi	f15, a1, 176
 87a:	2b41c3        	ssi	f12, a1, 172
 87d:	2a4153        	ssi	f5, a1, 168
 880:	000081        	l32r	a8, fffc0880 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0880>
			880: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x74
			880: R_XTENSA_ASM_EXPAND	lets_and_go::carPaintColor(lets_and_go::CarPaint, unsigned short, float, float)
 883:	0008e0        	callx8	a8
 886:	080c      	movi.n	a8, 0
 888:	faa850        	wfr	f10, a8
 88b:	000081        	l32r	a8, fffc088c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc088c>
			88b: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x20
 88e:	2521c2        	l32i	a12, a1, 148
 891:	fa7850        	wfr	f7, a8
 894:	000081        	l32r	a8, fffc0894 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0894>
			894: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x24
 897:	ffa0b2        	movi	a11, 255
 89a:	fa2850        	wfr	f2, a8
 89d:	2b01c3        	lsi	f12, a1, 172
 8a0:	2c01f3        	lsi	f15, a1, 176
 8a3:	2d0183        	lsi	f8, a1, 180
 8a6:	2e0133        	lsi	f3, a1, 184
 8a9:	2f0163        	lsi	f6, a1, 188
 8ac:	302182        	l32i	a8, a1, 192
 8af:	310143        	lsi	f4, a1, 196
 8b2:	320193        	lsi	f9, a1, 200
 8b5:	3321e2        	l32i	a14, a1, 204
 8b8:	00a192        	movi	a9, 0x100
 8bb:	4d1cb7        	beq	a12, a11, 90c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x90c>
			8bb: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x90c
 8be:	f4cba0        	extui	a12, a10, 11, 16
 8c1:	44b0a0        	extui	a11, a10, 0, 5
 8c4:	3801b3        	lsi	f11, a1, 224
 8c7:	ca5c00        	float.s	f5, a12, 0
 8ca:	ca1b00        	float.s	f1, a11, 0
 8cd:	54a5a0        	extui	a10, a10, 5, 6
 8d0:	ca0a00        	float.s	f0, a10, 0
 8d3:	2a55b0        	mul.s	f5, f5, f11
 8d6:	2a11b0        	mul.s	f1, f1, f11
 8d9:	2a00b0        	mul.s	f0, f0, f11
 8dc:	1fa0d2        	movi	a13, 31
 8df:	9aa500        	trunc.s	a10, f5, 0
 8e2:	9ac100        	trunc.s	a12, f1, 0
 8e5:	9ab000        	trunc.s	a11, f0, 0
 8e8:	53aa80        	max	a10, a10, a8
 8eb:	53cc80        	max	a12, a12, a8
 8ee:	43aad0        	min	a10, a10, a13
 8f1:	43ccd0        	min	a12, a12, a13
 8f4:	53bb80        	max	a11, a11, a8
 8f7:	3fa0d2        	movi	a13, 63
 8fa:	11aa50        	slli	a10, a10, 11
 8fd:	43bbd0        	min	a11, a11, a13
 900:	20aac0        	or	a10, a10, a12
 903:	11bbb0        	slli	a11, a11, 5
 906:	20aab0        	or	a10, a10, a11
 909:	f4a0a0        	extui	a10, a10, 0, 16
 90c:	005532        	s16i	a3, a5, 0
 90f:	0056a2        	s16i	a10, a6, 0
 912:	771b      	addi.n	a7, a7, 1
 914:	552b      	addi.n	a5, a5, 2
 916:	662b      	addi.n	a6, a6, 2
 918:	022477        	blt	a4, a7, 91e <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x91e>
			918: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x91e
 91b:	ff5f86        	j	69d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x69d>
			91b: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x69d
 91e:	192132        	l32i	a3, a1, 100
 921:	ff4bc6        	j	654 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x654>
			921: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x654
 924:	1f2192        	l32i	a9, a1, 124
 927:	022937        	blt	a9, a3, 92d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x92d>
			927: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x92d
 92a:	fee786        	j	4cc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x4cc>
			92a: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4cc
 92d:	f01d      	retw.n

