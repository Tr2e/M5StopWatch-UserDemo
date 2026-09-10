Disassembly of section .text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh:

00000000 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)>:
   0:	020136        	entry	a1, 0x100
   3:	078d      	mov.n	a8, a7
   5:	186182        	s32i	a8, a1, 96
   8:	2ca182        	movi	a8, 0x12c
   b:	881a      	add.n	a8, a8, a1
   d:	1298      	l32i.n	a9, a2, 4
   f:	000882        	l8ui	a8, a8, 0
  12:	b2cb      	addi.n	a11, a2, 12
  14:	22a8      	l32i.n	a10, a2, 8
  16:	1a6182        	s32i	a8, a1, 104
  19:	839b90        	moveqz	a9, a11, a9
  1c:	30a182        	movi	a8, 0x130
  1f:	146132        	s32i	a3, a1, 80
  22:	156142        	s32i	a4, a1, 84
  25:	881a      	add.n	a8, a8, a1
  27:	166152        	s32i	a5, a1, 88
  2a:	176162        	s32i	a6, a1, 92
  2d:	1b6192        	s32i	a9, a1, 108
  30:	027d      	mov.n	a7, a2
  32:	0008e2        	l8ui	a14, a8, 0
  35:	941122        	l16ui	a2, a1, 0x128
  38:	140163        	lsi	f6, a1, 80
  3b:	1501d3        	lsi	f13, a1, 84
  3e:	400143        	lsi	f4, a1, 0x100
  41:	410173        	lsi	f7, a1, 0x104
  44:	450133        	lsi	f3, a1, 0x114
  47:	460123        	lsi	f2, a1, 0x118
  4a:	6a8c      	beqz.n	a10, 54 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x54>
			4a: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x54
  4c:	1c61a2        	s32i	a10, a1, 112
  4f:	000246        	j	5c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5c>
			4f: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5c
  52:	00          	.byte	00
  53:	00          	.byte	00
  54:	000081        	l32r	a8, fffc0054 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0054>
			54: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh
  57:	878a      	add.n	a8, a7, a8
  59:	1c6182        	s32i	a8, a1, 112
  5c:	1a1460        	sub.s	f1, f4, f6
  5f:	1ac2d0        	sub.s	f12, f2, f13
  62:	1af7d0        	sub.s	f15, f7, f13
  65:	1a0360        	sub.s	f0, f3, f6
  68:	000081        	l32r	a8, fffc0068 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0068>
			68: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x3c
  6b:	2a51c0        	mul.s	f5, f1, f12
  6e:	fa9850        	wfr	f9, a8
  71:	5a5f00        	msub.s	f5, f15, f0
  74:	fa8510        	abs.s	f8, f5
  77:	5b0980        	ult.s	b0, f9, f8
  7a:	020076        	bf	b0, 80 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x80>
			7a: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x80
  7d:	024206        	j	989 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x989>
			7d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x989
  80:	000081        	l32r	a8, fffc0080 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0080>
			80: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x54
  83:	fa9850        	wfr	f9, a8
  86:	4b0890        	olt.s	b0, f8, f9
  89:	020076        	bf	b0, 8f <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x8f>
			89: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x8f
  8c:	023e46        	j	989 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x989>
			8c: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x989
  8f:	000081        	l32r	a8, fffc0090 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0090>
			8f: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4
  92:	1701a3        	lsi	f10, a1, 92
  95:	878a      	add.n	a8, a7, a8
  97:	0888      	l32i.n	a8, a8, 0
  99:	2841a3        	ssi	f10, a1, 160
  9c:	1801a3        	lsi	f10, a1, 96
  9f:	4b0460        	olt.s	b0, f4, f6
  a2:	fa8600        	mov.s	f8, f6
  a5:	db8400        	movt.s	f8, f4, b0
  a8:	1d6182        	s32i	a8, a1, 116
  ab:	2941a3        	ssi	f10, a1, 164
  ae:	caa800        	float.s	f10, a8, 0
  b1:	000081        	l32r	a8, fffc00b4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc00b4>
			b1: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4c
  b4:	4b0380        	olt.s	b0, f3, f8
  b7:	db8300        	movt.s	f8, f3, b0
  ba:	fab850        	wfr	f11, a8
  bd:	4b0b80        	olt.s	b0, f11, f8
  c0:	160193        	lsi	f9, a1, 88
  c3:	210076        	bf	b0, e8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xe8>
			c3: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0xe8
  c6:	000081        	l32r	a8, fffc00c8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc00c8>
			c6: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x44
  c9:	fab850        	wfr	f11, a8
  cc:	4b08b0        	olt.s	b0, f8, f11
  cf:	150076        	bf	b0, e8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xe8>
			cf: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0xe8
  d2:	9a8800        	trunc.s	a8, f8, 0
  d5:	190c      	movi.n	a9, 1
  d7:	cab800        	float.s	f11, a8, 0
  da:	0a0c      	movi.n	a10, 0
  dc:	4b08b0        	olt.s	b0, f8, f11
  df:	c39a00        	movf	a9, a10, b0
  e2:	c08890        	sub	a8, a8, a9
  e5:	ca8800        	float.s	f8, a8, 0
  e8:	000081        	l32r	a8, fffc00e8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc00e8>
			e8: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x8
  eb:	4b0a80        	olt.s	b0, f10, f8
  ee:	878a      	add.n	a8, a7, a8
  f0:	0888      	l32i.n	a8, a8, 0
  f2:	1d2192        	l32i	a9, a1, 116
  f5:	dba800        	movt.s	f10, f8, b0
  f8:	4b0640        	olt.s	b0, f6, f4
  fb:	cb4600        	movf.s	f4, f6, b0
  fe:	1e6182        	s32i	a8, a1, 120
 101:	898a      	add.n	a8, a9, a8
 103:	4b0430        	olt.s	b0, f4, f3
 106:	880b      	addi.n	a8, a8, -1
 108:	cb3400        	movf.s	f3, f4, b0
 10b:	ca4800        	float.s	f4, a8, 0
 10e:	000081        	l32r	a8, fffc0110 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0110>
			10e: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4c
 111:	2641a3        	ssi	f10, a1, 152
 114:	fa8850        	wfr	f8, a8
 117:	4b0830        	olt.s	b0, f8, f3
 11a:	220076        	bf	b0, 140 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x140>
			11a: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x140
 11d:	000081        	l32r	a8, fffc0120 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0120>
			11d: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x44
 120:	fa8850        	wfr	f8, a8
 123:	4b0380        	olt.s	b0, f3, f8
 126:	160076        	bf	b0, 140 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x140>
			126: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x140
 129:	9a9300        	trunc.s	a9, f3, 0
 12c:	180c      	movi.n	a8, 1
 12e:	ca8900        	float.s	f8, a9, 0
 131:	00a0a2        	movi	a10, 0
 134:	4b0830        	olt.s	b0, f8, f3
 137:	c38a00        	movf	a8, a10, b0
 13a:	808890        	add	a8, a8, a9
 13d:	ca3800        	float.s	f3, a8, 0
 140:	4b0340        	olt.s	b0, f3, f4
 143:	000081        	l32r	a8, fffc0144 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0144>
			143: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0xc
 146:	db4300        	movt.s	f4, f3, b0
 149:	878a      	add.n	a8, a7, a8
 14b:	274143        	ssi	f4, a1, 156
 14e:	4b07d0        	olt.s	b0, f7, f13
 151:	0868      	l32i.n	a6, a8, 0
 153:	490076        	bf	b0, 1a0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1a0>
			153: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1a0
 156:	000081        	l32r	a8, fffc0158 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0158>
			156: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4c
 159:	4b0270        	olt.s	b0, f2, f7
 15c:	fa3850        	wfr	f3, a8
 15f:	191076        	bt	b0, 17c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x17c>
			15f: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x17c
 162:	4b0370        	olt.s	b0, f3, f7
 165:	fa3700        	mov.s	f3, f7
 168:	280076        	bf	b0, 194 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x194>
			168: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x194
 16b:	000081        	l32r	a8, fffc016c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc016c>
			16b: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x44
 16e:	fa4850        	wfr	f4, a8
 171:	4b0740        	olt.s	b0, f7, f4
 174:	491076        	bt	b0, 1c1 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1c1>
			174: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1c1
 177:	000646        	j	194 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x194>
			177: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x194
 17a:	00          	.byte	00
 17b:	00          	.byte	00
 17c:	4b0320        	olt.s	b0, f3, f2
 17f:	fa3200        	mov.s	f3, f2
 182:	021076        	bt	b0, 188 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x188>
			182: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x188
 185:	0002c6        	j	194 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x194>
			185: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x194
 188:	000081        	l32r	a8, fffc0188 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0188>
			188: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x44
 18b:	fa4850        	wfr	f4, a8
 18e:	4b0240        	olt.s	b0, f2, f4
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
			1a0: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4c
 1a3:	4b02d0        	olt.s	b0, f2, f13
 1a6:	fa3d00        	mov.s	f3, f13
 1a9:	db3200        	movt.s	f3, f2, b0
 1ac:	fa4850        	wfr	f4, a8
 1af:	4b0430        	olt.s	b0, f4, f3
 1b2:	230076        	bf	b0, 1d9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1d9>
			1b2: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1d9
 1b5:	000081        	l32r	a8, fffc01b8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc01b8>
			1b5: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x44
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
 1dc:	4b0d70        	olt.s	b0, f13, f7
 1df:	878a      	add.n	a8, a7, a8
 1e1:	0888      	l32i.n	a8, a8, 0
 1e3:	021076        	bt	b0, 1e9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x1e9>
			1e3: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1e9
 1e6:	fa7d00        	mov.s	f7, f13
 1e9:	000091        	l32r	a9, fffc01ec <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc01ec>
			1e9: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4c
 1ec:	4b0720        	olt.s	b0, f7, f2
 1ef:	cb2700        	movf.s	f2, f7, b0
 1f2:	fa4950        	wfr	f4, a9
 1f5:	4b0420        	olt.s	b0, f4, f2
 1f8:	210076        	bf	b0, 21d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x21d>
			1f8: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x21d
 1fb:	000091        	l32r	a9, fffc01fc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc01fc>
			1fb: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x44
 1fe:	fa4950        	wfr	f4, a9
 201:	4b0240        	olt.s	b0, f2, f4
 204:	150076        	bf	b0, 21d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x21d>
			204: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x21d
 207:	9aa200        	trunc.s	a10, f2, 0
 20a:	190c      	movi.n	a9, 1
 20c:	ca4a00        	float.s	f4, a10, 0
 20f:	0b0c      	movi.n	a11, 0
 211:	4b0420        	olt.s	b0, f4, f2
 214:	c39b00        	movf	a9, a11, b0
 217:	8099a0        	add	a9, a9, a10
 21a:	ca2900        	float.s	f2, a9, 0
 21d:	234103        	ssi	f0, a1, 140
 220:	194113        	ssi	f1, a1, 100
 223:	270103        	lsi	f0, a1, 156
 226:	260113        	lsi	f1, a1, 152
 229:	2b61e2        	s32i	a14, a1, 172
 22c:	2a4193        	ssi	f9, a1, 168
 22f:	2541d3        	ssi	f13, a1, 148
 232:	244163        	ssi	f6, a1, 144
 235:	2141f3        	ssi	f15, a1, 132
 238:	2041c3        	ssi	f12, a1, 128
 23b:	4b0010        	olt.s	b0, f0, f1
 23e:	020076        	bf	b0, 244 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x244>
			23e: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x244
 241:	01d106        	j	989 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x989>
			241: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x989
 244:	868a      	add.n	a8, a6, a8
 246:	ca4600        	float.s	f4, a6, 0
 249:	880b      	addi.n	a8, a8, -1
 24b:	ca7800        	float.s	f7, a8, 0
 24e:	4b0430        	olt.s	b0, f4, f3
 251:	cb3400        	movf.s	f3, f4, b0
 254:	4b0270        	olt.s	b0, f2, f7
 257:	cb2700        	movf.s	f2, f7, b0
 25a:	4b0230        	olt.s	b0, f2, f3
 25d:	020076        	bf	b0, 263 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x263>
			25d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x263
 260:	01c946        	j	989 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x989>
			260: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x989
 263:	9a8100        	trunc.s	a8, f1, 0
 266:	0000a1        	l32r	a10, fffc0268 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0268>
			266: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x28
 269:	356182        	s32i	a8, a1, 212
 26c:	9a8000        	trunc.s	a8, f0, 0
 26f:	fab540        	rfr	a11, f5
 272:	366182        	s32i	a8, a1, 216
 275:	9a8200        	trunc.s	a8, f2, 0
 278:	9a3300        	trunc.s	a3, f3, 0
 27b:	1f6182        	s32i	a8, a1, 124
 27e:	000081        	l32r	a8, fffc0280 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0280>
			27e: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x58
			27e: R_XTENSA_ASM_EXPAND	__divsf3
 281:	0008e0        	callx8	a8
 284:	2b21e2        	l32i	a14, a1, 172
 287:	ffa082        	movi	a8, 255
 28a:	226122        	s32i	a2, a1, 136
 28d:	190113        	lsi	f1, a1, 100
 290:	2001c3        	lsi	f12, a1, 128
 293:	2101f3        	lsi	f15, a1, 132
 296:	230103        	lsi	f0, a1, 140
 299:	240163        	lsi	f6, a1, 144
 29c:	2501d3        	lsi	f13, a1, 148
 29f:	2a0193        	lsi	f9, a1, 168
 2a2:	fa4a50        	wfr	f4, a10
 2a5:	029e87        	bne	a14, a8, 2ab <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x2ab>
			2a5: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x2ab
 2a8:	002586        	j	342 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x342>
			2a8: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x342
 2ab:	244103        	ssi	f0, a1, 144
 2ae:	ca0e00        	float.s	f0, a14, 0
 2b1:	0000b1        	l32r	a11, fffc02b4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc02b4>
			2b1: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1c
 2b4:	2c4193        	ssi	f9, a1, 176
 2b7:	2b41d3        	ssi	f13, a1, 172
 2ba:	2a61a2        	s32i	a10, a1, 168
 2bd:	254163        	ssi	f6, a1, 148
 2c0:	2341f3        	ssi	f15, a1, 140
 2c3:	2141c3        	ssi	f12, a1, 132
 2c6:	204113        	ssi	f1, a1, 128
 2c9:	1961e2        	s32i	a14, a1, 100
 2cc:	faa040        	rfr	a10, f0
 2cf:	000081        	l32r	a8, fffc02d0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc02d0>
			2cf: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5c
			2cf: R_XTENSA_ASM_EXPAND	__divsf3
 2d2:	0008e0        	callx8	a8
 2d5:	549520        	extui	a9, a2, 5, 6
 2d8:	418b20        	srli	a8, a2, 11
 2db:	ca7900        	float.s	f7, a9, 0
 2de:	ca5800        	float.s	f5, a8, 0
 2e1:	fa2a50        	wfr	f2, a10
 2e4:	448020        	extui	a8, a2, 0, 5
 2e7:	2a7720        	mul.s	f7, f7, f2
 2ea:	2a5520        	mul.s	f5, f5, f2
 2ed:	ca3800        	float.s	f3, a8, 0
 2f0:	0b0c      	movi.n	a11, 0
 2f2:	2a2320        	mul.s	f2, f3, f2
 2f5:	9a8700        	trunc.s	a8, f7, 0
 2f8:	9a9500        	trunc.s	a9, f5, 0
 2fb:	fc1c      	movi.n	a12, 31
 2fd:	9aa200        	trunc.s	a10, f2, 0
 300:	5388b0        	max	a8, a8, a11
 303:	5399b0        	max	a9, a9, a11
 306:	3fa0d2        	movi	a13, 63
 309:	4388d0        	min	a8, a8, a13
 30c:	4399c0        	min	a9, a9, a12
 30f:	1188b0        	slli	a8, a8, 5
 312:	119950        	slli	a9, a9, 11
 315:	53aab0        	max	a10, a10, a11
 318:	208890        	or	a8, a8, a9
 31b:	43aac0        	min	a10, a10, a12
 31e:	2088a0        	or	a8, a8, a10
 321:	f48080        	extui	a8, a8, 0, 16
 324:	2c0193        	lsi	f9, a1, 176
 327:	2b01d3        	lsi	f13, a1, 172
 32a:	2a0143        	lsi	f4, a1, 168
 32d:	250163        	lsi	f6, a1, 148
 330:	240103        	lsi	f0, a1, 144
 333:	2301f3        	lsi	f15, a1, 140
 336:	2101c3        	lsi	f12, a1, 132
 339:	200113        	lsi	f1, a1, 128
 33c:	1921e2        	l32i	a14, a1, 100
 33f:	226182        	s32i	a8, a1, 136
 342:	07a8      	l32i.n	a10, a7, 0
 344:	40fa80        	nsau	a8, a10
 347:	418580        	srli	a8, a8, 5
 34a:	054a16        	beqz	a10, 3a2 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x3a2>
			34a: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x3a2
 34d:	1a21b2        	l32i	a11, a1, 104
 350:	190c      	movi.n	a9, 1
 352:	8389b0        	moveqz	a8, a9, a11
 355:	049856        	bnez	a8, 3a2 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x3a2>
			355: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x3a2
 358:	11d280        	slli	a13, a2, 8
 35b:	018e80        	slli	a8, a14, 24
 35e:	000091        	l32r	a9, fffc0360 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0360>
			35e: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x14
 361:	20ddb0        	or	a13, a13, a11
 364:	20dd80        	or	a13, a13, a8
 367:	829d90        	mull	a9, a13, a9
 36a:	659990        	extui	a9, a9, 25, 7
 36d:	8a9a      	add.n	a8, a10, a9
 36f:	000882        	l8ui	a8, a8, 0
 372:	e8ac      	beqz.n	a8, 3a4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x3a4>
			372: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x3a4
 374:	880b      	addi.n	a8, a8, -1
 376:	a0b8a0        	addx4	a11, a8, a10
 379:	991b      	addi.n	a9, a9, 1
 37b:	202bb2        	l32i	a11, a11, 128
 37e:	649090        	extui	a9, a9, 0, 7
 381:	ca9a      	add.n	a12, a10, a9
 383:	119db7        	bne	a13, a11, 398 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x398>
			383: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x398
 386:	f48080        	extui	a8, a8, 0, 16
 389:	118850        	slli	a8, a8, 11
 38c:	80a192        	movi	a9, 0x180
 38f:	889a      	add.n	a8, a8, a9
 391:	8a8a      	add.n	a8, a10, a8
 393:	000346        	j	3a4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x3a4>
			393: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x3a4
 396:	00          	.byte	00
 397:	00          	.byte	00
 398:	000c82        	l8ui	a8, a12, 0
 39b:	fd5856        	bnez	a8, 374 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x374>
			39b: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x374
 39e:	000086        	j	3a4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x3a4>
			39e: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x3a4
 3a1:	00          	.byte	00
 3a2:	080c      	movi.n	a8, 0
 3a4:	352192        	l32i	a9, a1, 212
 3a7:	2c6182        	s32i	a8, a1, 176
 3aa:	362182        	l32i	a8, a1, 216
 3ad:	2b4103        	ssi	f0, a1, 172
 3b0:	c0d890        	sub	a13, a8, a9
 3b3:	1f2182        	l32i	a8, a1, 124
 3b6:	280103        	lsi	f0, a1, 160
 3b9:	c09830        	sub	a9, a8, a3
 3bc:	825d90        	mull	a5, a13, a9
 3bf:	14a0c2        	movi	a12, 20
 3c2:	70c182        	addi	a8, a1, 112
 3c5:	034103        	ssi	f0, a1, 12
 3c8:	90a0b2        	movi	a11, 144
 3cb:	290103        	lsi	f0, a1, 164
 3ce:	80bb80        	add	a11, a11, a8
 3d1:	80a1c0        	add	a10, a1, a12
 3d4:	2e61e2        	s32i	a14, a1, 184
 3d7:	2d4143        	ssi	f4, a1, 180
 3da:	2a41f3        	ssi	f15, a1, 168
 3dd:	2541c3        	ssi	f12, a1, 148
 3e0:	244113        	ssi	f1, a1, 144
 3e3:	004163        	ssi	f6, a1, 0
 3e6:	234163        	ssi	f6, a1, 140
 3e9:	0141d3        	ssi	f13, a1, 4
 3ec:	2141d3        	ssi	f13, a1, 132
 3ef:	024193        	ssi	f9, a1, 8
 3f2:	194193        	ssi	f9, a1, 100
 3f5:	044103        	ssi	f0, a1, 16
 3f8:	206152        	s32i	a5, a1, 128
 3fb:	000081        	l32r	a8, fffc03fc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc03fc>
			3fb: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x60
			3fb: R_XTENSA_ASM_EXPAND	memcpy
 3fe:	0008e0        	callx8	a8
 401:	a4a082        	movi	a8, 164
 404:	70c882        	addi	a8, a8, 112
 407:	b81a      	add.n	a11, a8, a1
 409:	14a0c2        	movi	a12, 20
 40c:	28c1a2        	addi	a10, a1, 40
 40f:	000081        	l32r	a8, fffc0410 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0410>
			40f: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x64
			40f: R_XTENSA_ASM_EXPAND	memcpy
 412:	0008e0        	callx8	a8
 415:	070c      	movi.n	a7, 0
 417:	f179      	s32i.n	a7, a1, 60
 419:	00a192        	movi	a9, 0x100
 41c:	106172        	s32i	a7, a1, 64
 41f:	116172        	s32i	a7, a1, 68
 422:	190193        	lsi	f9, a1, 100
 425:	2101d3        	lsi	f13, a1, 132
 428:	230163        	lsi	f6, a1, 140
 42b:	240113        	lsi	f1, a1, 144
 42e:	2501c3        	lsi	f12, a1, 148
 431:	2a01f3        	lsi	f15, a1, 168
 434:	2b0103        	lsi	f0, a1, 172
 437:	2c2182        	l32i	a8, a1, 176
 43a:	2d0143        	lsi	f4, a1, 180
 43d:	2e21e2        	l32i	a14, a1, 184
 440:	022957        	blt	a9, a5, 446 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x446>
			440: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x446
 443:	014e46        	j	980 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x980>
			443: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x980
 446:	3cc142        	addi	a4, a1, 60
 449:	000091        	l32r	a9, fffc044c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc044c>
			449: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x18
 44c:	256162        	s32i	a6, a1, 148
 44f:	194113        	ssi	f1, a1, 100
 452:	046d      	mov.n	a6, a4
 454:	2141c3        	ssi	f12, a1, 132
 457:	074d      	mov.n	a4, a7
 459:	2341f3        	ssi	f15, a1, 140
 45c:	244103        	ssi	f0, a1, 144
 45f:	2a4163        	ssi	f6, a1, 168
 462:	2b6182        	s32i	a8, a1, 172
 465:	2c4143        	ssi	f4, a1, 176
 468:	2d41d3        	ssi	f13, a1, 180
 46b:	2e4193        	ssi	f9, a1, 184
 46e:	017d      	mov.n	a7, a1
 470:	0e5d      	mov.n	a5, a14
 472:	441b      	addi.n	a4, a4, 1
 474:	a28490        	muluh	a8, a4, a9
 477:	010713        	lsi	f1, a7, 4
 47a:	418180        	srli	a8, a8, 1
 47d:	908880        	addx2	a8, a8, a8
 480:	c08480        	sub	a8, a4, a8
 483:	a08880        	addx4	a8, a8, a8
 486:	a08810        	addx4	a8, a8, a1
 489:	010803        	lsi	f0, a8, 4
 48c:	0000a1        	l32r	a10, fffc048c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc048c>
			48c: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x50
 48f:	1a0010        	sub.s	f0, f0, f1
 492:	fa1010        	abs.s	f1, f0
 495:	fab040        	rfr	a11, f0
 498:	fa0a50        	wfr	f0, a10
 49b:	6b0010        	ole.s	b0, f0, f1
 49e:	160076        	bf	b0, 4b8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x4b8>
			49e: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4b8
 4a1:	000803        	lsi	f0, a8, 0
 4a4:	000713        	lsi	f1, a7, 0
 4a7:	1a0010        	sub.s	f0, f0, f1
 4aa:	faa040        	rfr	a10, f0
 4ad:	000081        	l32r	a8, fffc04b0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc04b0>
			4ad: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x68
			4ad: R_XTENSA_ASM_EXPAND	__divsf3
 4b0:	0008e0        	callx8	a8
 4b3:	000091        	l32r	a9, fffc04b4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc04b4>
			4b3: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x18
 4b6:	06a9      	s32i.n	a10, a6, 0
 4b8:	14c772        	addi	a7, a7, 20
 4bb:	664b      	addi.n	a6, a6, 4
 4bd:	b13466        	bnei	a4, 3, 472 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x472>
			4bd: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x472
 4c0:	190113        	lsi	f1, a1, 100
 4c3:	2101c3        	lsi	f12, a1, 132
 4c6:	2301f3        	lsi	f15, a1, 140
 4c9:	240103        	lsi	f0, a1, 144
 4cc:	252162        	l32i	a6, a1, 148
 4cf:	2a0163        	lsi	f6, a1, 168
 4d2:	2b2182        	l32i	a8, a1, 172
 4d5:	2c0143        	lsi	f4, a1, 176
 4d8:	2d01d3        	lsi	f13, a1, 180
 4db:	2e0193        	lsi	f9, a1, 184
 4de:	05ed      	mov.n	a14, a5
 4e0:	012706        	j	980 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x980>
			4e0: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x980
 4e3:	1e21a2        	l32i	a10, a1, 120
 4e6:	c09360        	sub	a9, a3, a6
 4e9:	8299a0        	mull	a9, a9, a10
 4ec:	0000a1        	l32r	a10, fffc04ec <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc04ec>
			4ec: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x24
 4ef:	216122        	s32i	a2, a1, 132
 4f2:	fa7a50        	wfr	f7, a10
 4f5:	234113        	ssi	f1, a1, 140
 4f8:	244103        	ssi	f0, a1, 144
 4fb:	2541d3        	ssi	f13, a1, 148
 4fe:	202880        	or	a2, a8, a8
 501:	3761e2        	s32i	a14, a1, 220
 504:	ca3300        	float.s	f3, a3, 0
 507:	2021a2        	l32i	a10, a1, 128
 50a:	00a182        	movi	a8, 0x100
 50d:	0a3370        	add.s	f3, f3, f7
 510:	0928a7        	blt	a8, a10, 51d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x51d>
			510: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x51d
 513:	362142        	l32i	a4, a1, 216
 516:	352172        	l32i	a7, a1, 212
 519:	005406        	j	66d <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x66d>
			519: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x66d
 51c:	00          	.byte	00
 51d:	0b0c      	movi.n	a11, 0
 51f:	270183        	lsi	f8, a1, 156
 522:	260153        	lsi	f5, a1, 152
 525:	01cd      	mov.n	a12, a1
 527:	3cc1e2        	addi	a14, a1, 60
 52a:	0b6d      	mov.n	a6, a11
 52c:	000081        	l32r	a8, fffc052c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc052c>
			52c: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x18
 52f:	bb1b      	addi.n	a11, a11, 1
 531:	a28b80        	muluh	a8, a11, a8
 534:	010c13        	lsi	f1, a12, 4
 537:	418180        	srli	a8, a8, 1
 53a:	908880        	addx2	a8, a8, a8
 53d:	c08b80        	sub	a8, a11, a8
 540:	a0a880        	addx4	a10, a8, a8
 543:	a0aa10        	addx4	a10, a10, a1
 546:	010a03        	lsi	f0, a10, 4
 549:	0000f1        	l32r	a15, fffc054c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc054c>
			549: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x54
 54c:	4b0010        	olt.s	b0, f0, f1
 54f:	fa2100        	mov.s	f2, f1
 552:	db2000        	movt.s	f2, f0, b0
 555:	faaf50        	wfr	f10, a15
 558:	1a22a0        	sub.s	f2, f2, f10
 55b:	0cdd      	mov.n	a13, a12
 55d:	4b0320        	olt.s	b0, f3, f2
 560:	751076        	bt	b0, 5d9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5d9>
			560: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5d9
 563:	4b0100        	olt.s	b0, f1, f0
 566:	fa2100        	mov.s	f2, f1
 569:	db2000        	movt.s	f2, f0, b0
 56c:	0a22a0        	add.s	f2, f2, f10
 56f:	4b0230        	olt.s	b0, f2, f3
 572:	631076        	bt	b0, 5d9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5d9>
			572: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5d9
 575:	1a0010        	sub.s	f0, f0, f1
 578:	0000f1        	l32r	a15, fffc0578 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0578>
			578: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x50
 57b:	000c23        	lsi	f2, a12, 0
 57e:	faaf50        	wfr	f10, a15
 581:	fa0010        	abs.s	f0, f0
 584:	4b00a0        	olt.s	b0, f0, f10
 587:	360076        	bf	b0, 5c1 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5c1>
			587: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5c1
 58a:	000a03        	lsi	f0, a10, 0
 58d:	4b0020        	olt.s	b0, f0, f2
 590:	100076        	bf	b0, 5a4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5a4>
			590: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5a4
 593:	f48080        	extui	a8, a8, 0, 16
 596:	a08880        	addx4	a8, a8, a8
 599:	a0d810        	addx4	a13, a8, a1
 59c:	fa1000        	mov.s	f1, f0
 59f:	000106        	j	5a7 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5a7>
			59f: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5a7
 5a2:	00          	.byte	00
 5a3:	00          	.byte	00
 5a4:	fa1200        	mov.s	f1, f2
 5a7:	4b0180        	olt.s	b0, f1, f8
 5aa:	020076        	bf	b0, 5b0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5b0>
			5aa: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5b0
 5ad:	000d83        	lsi	f8, a13, 0
 5b0:	4b0200        	olt.s	b0, f2, f0
 5b3:	cb0200        	movf.s	f0, f2, b0
 5b6:	4b0500        	olt.s	b0, f5, f0
 5b9:	db5000        	movt.s	f5, f0, b0
 5bc:	160c      	movi.n	a6, 1
 5be:	0005c6        	j	5d9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5d9>
			5be: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5d9
 5c1:	1a1310        	sub.s	f1, f3, f1
 5c4:	000e03        	lsi	f0, a14, 0
 5c7:	01a062        	movi	a6, 1
 5ca:	4a2010        	madd.s	f2, f0, f1
 5cd:	4b0280        	olt.s	b0, f2, f8
 5d0:	db8200        	movt.s	f8, f2, b0
 5d3:	4b0520        	olt.s	b0, f5, f2
 5d6:	db5200        	movt.s	f5, f2, b0
 5d9:	14ccc2        	addi	a12, a12, 20
 5dc:	ee4b      	addi.n	a14, a14, 4
 5de:	023b26        	beqi	a11, 3, 5e4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x5e4>
			5de: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x5e4
 5e1:	ffd1c6        	j	52c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x52c>
			5e1: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x52c
 5e4:	f2b616        	beqz	a6, 513 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x513>
			5e4: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x513
 5e7:	352182        	l32i	a8, a1, 212
 5ea:	ca2800        	float.s	f2, a8, 0
 5ed:	000081        	l32r	a8, fffc05f0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc05f0>
			5ed: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4c
 5f0:	fa0850        	wfr	f0, a8
 5f3:	4b0080        	olt.s	b0, f0, f8
 5f6:	220076        	bf	b0, 61c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x61c>
			5f6: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x61c
 5f9:	000081        	l32r	a8, fffc05fc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc05fc>
			5f9: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x44
 5fc:	fa0850        	wfr	f0, a8
 5ff:	4b0800        	olt.s	b0, f8, f0
 602:	160076        	bf	b0, 61c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x61c>
			602: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x61c
 605:	9aa800        	trunc.s	a10, f8, 0
 608:	1b0c      	movi.n	a11, 1
 60a:	ca0a00        	float.s	f0, a10, 0
 60d:	00a0c2        	movi	a12, 0
 610:	4b0800        	olt.s	b0, f8, f0
 613:	c3bc00        	movf	a11, a12, b0
 616:	c0aab0        	sub	a10, a10, a11
 619:	ca8a00        	float.s	f8, a10, 0
 61c:	000081        	l32r	a8, fffc061c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc061c>
			61c: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x48
 61f:	fa1850        	wfr	f1, a8
 622:	1a8810        	sub.s	f8, f8, f1
 625:	362182        	l32i	a8, a1, 216
 628:	ca0800        	float.s	f0, a8, 0
 62b:	000081        	l32r	a8, fffc062c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc062c>
			62b: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4c
 62e:	4b0280        	olt.s	b0, f2, f8
 631:	cb8200        	movf.s	f8, f2, b0
 634:	fa2850        	wfr	f2, a8
 637:	4b0250        	olt.s	b0, f2, f5
 63a:	9a7800        	trunc.s	a7, f8, 0
 63d:	200076        	bf	b0, 661 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x661>
			63d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x661
 640:	000081        	l32r	a8, fffc0640 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0640>
			640: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x44
 643:	fa2850        	wfr	f2, a8
 646:	4b0520        	olt.s	b0, f5, f2
 649:	140076        	bf	b0, 661 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x661>
			649: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x661
 64c:	9ab500        	trunc.s	a11, f5, 0
 64f:	1a0c      	movi.n	a10, 1
 651:	ca2b00        	float.s	f2, a11, 0
 654:	0c0c      	movi.n	a12, 0
 656:	4b0250        	olt.s	b0, f2, f5
 659:	c3ac00        	movf	a10, a12, b0
 65c:	aaba      	add.n	a10, a10, a11
 65e:	ca5a00        	float.s	f5, a10, 0
 661:	0a5510        	add.s	f5, f5, f1
 664:	4b0500        	olt.s	b0, f5, f0
 667:	cb5000        	movf.s	f5, f0, b0
 66a:	9a4500        	trunc.s	a4, f5, 0
 66d:	14a477        	bge	a4, a7, 685 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x685>
			66d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x685
 670:	1e2182        	l32i	a8, a1, 120
 673:	331b      	addi.n	a3, a3, 1
 675:	998a      	add.n	a9, a9, a8
 677:	1f2182        	l32i	a8, a1, 124
 67a:	022837        	blt	a8, a3, 680 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x680>
			67a: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x680
 67d:	ffa0c6        	j	504 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x504>
			67d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x504
 680:	00c146        	j	989 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x989>
			680: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x989
 683:	00          	.byte	00
 684:	00          	.byte	00
 685:	250103        	lsi	f0, a1, 148
 688:	1d2182        	l32i	a8, a1, 116
 68b:	1a3300        	sub.s	f3, f3, f0
 68e:	c06980        	sub	a6, a9, a8
 691:	240103        	lsi	f0, a1, 144
 694:	000081        	l32r	a8, fffc0694 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0694>
			694: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x20
 697:	2a8030        	mul.s	f8, f0, f3
 69a:	fa2850        	wfr	f2, a8
 69d:	230103        	lsi	f0, a1, 140
 6a0:	080c      	movi.n	a8, 0
 6a2:	faa850        	wfr	f10, a8
 6a5:	1b2182        	l32i	a8, a1, 108
 6a8:	667a      	add.n	a6, a6, a7
 6aa:	2a3030        	mul.s	f3, f0, f3
 6ad:	905680        	addx2	a5, a6, a8
 6b0:	1c2182        	l32i	a8, a1, 112
 6b3:	196132        	s32i	a3, a1, 100
 6b6:	906680        	addx2	a6, a6, a8
 6b9:	ca1700        	float.s	f1, a7, 0
 6bc:	0a1170        	add.s	f1, f1, f7
 6bf:	1a1160        	sub.s	f1, f1, f6
 6c2:	2a0c10        	mul.s	f0, f12, f1
 6c5:	1a0080        	sub.s	f0, f0, f8
 6c8:	2a0040        	mul.s	f0, f0, f4
 6cb:	4b0020        	olt.s	b0, f0, f2
 6ce:	020076        	bf	b0, 6d4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x6d4>
			6ce: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6d4
 6d1:	00a646        	j	96e <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x96e>
			6d1: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x96e
 6d4:	fa5300        	mov.s	f5, f3
 6d7:	5a5f10        	msub.s	f5, f15, f1
 6da:	2a1540        	mul.s	f1, f5, f4
 6dd:	4b0120        	olt.s	b0, f1, f2
 6e0:	020076        	bf	b0, 6e6 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x6e6>
			6e0: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6e6
 6e3:	00a1c6        	j	96e <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x96e>
			6e3: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x96e
 6e6:	0a5010        	add.s	f5, f0, f1
 6e9:	000081        	l32r	a8, fffc06ec <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc06ec>
			6e9: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x40
 6ec:	fab850        	wfr	f11, a8
 6ef:	4b0b50        	olt.s	b0, f11, f5
 6f2:	020076        	bf	b0, 6f8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x6f8>
			6f2: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6f8
 6f5:	009d46        	j	96e <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x96e>
			6f5: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x96e
 6f8:	420153        	lsi	f5, a1, 0x108
 6fb:	1ad590        	sub.s	f13, f5, f9
 6fe:	470153        	lsi	f5, a1, 0x11c
 701:	1ab590        	sub.s	f11, f5, f9
 704:	fa5900        	mov.s	f5, f9
 707:	4a5d00        	madd.s	f5, f13, f0
 70a:	4a5b10        	madd.s	f5, f11, f1
 70d:	4b0a50        	olt.s	b0, f10, f5
 710:	021076        	bt	b0, 716 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x716>
			710: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x716
 713:	0095c6        	j	96e <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x96e>
			713: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x96e
 716:	000081        	l32r	a8, fffc0718 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0718>
			716: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x3c
 719:	fab510        	abs.s	f11, f5
 71c:	fad850        	wfr	f13, a8
 71f:	5b0db0        	ult.s	b0, f13, f11
 722:	020076        	bf	b0, 728 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x728>
			722: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x728
 725:	009146        	j	96e <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x96e>
			725: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x96e
 728:	000081        	l32r	a8, fffc0728 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0728>
			728: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x34
 72b:	0000a1        	l32r	a10, fffc072c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc072c>
			72b: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x28
 72e:	fab850        	wfr	f11, a8
 731:	2ab5b0        	mul.s	f11, f5, f11
 734:	000081        	l32r	a8, fffc0734 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0734>
			734: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x38
 737:	faea50        	wfr	f14, a10
 73a:	fad850        	wfr	f13, a8
 73d:	4b0be0        	olt.s	b0, f11, f14
 740:	dbbe00        	movt.s	f11, f14, b0
 743:	4b0db0        	olt.s	b0, f13, f11
 746:	cbdb00        	movf.s	f13, f11, b0
 749:	ea3d00        	utrunc.s	a3, f13, 0
 74c:	001582        	l16ui	a8, a5, 0
 74f:	f43030        	extui	a3, a3, 0, 16
 752:	02b387        	bgeu	a3, a8, 758 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x758>
			752: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x758
 755:	008546        	j	96e <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x96e>
			755: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x96e
 758:	2221a2        	l32i	a10, a1, 136
 75b:	212182        	l32i	a8, a1, 132
 75e:	838a20        	moveqz	a8, a10, a2
 761:	1a21a2        	l32i	a10, a1, 104
 764:	200a16        	beqz	a10, 968 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x968>
			764: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x968
 767:	280123        	lsi	f2, a1, 160
 76a:	304133        	ssi	f3, a1, 192
 76d:	430133        	lsi	f3, a1, 0x10c
 770:	346192        	s32i	a9, a1, 208
 773:	1ad320        	sub.s	f13, f3, f2
 776:	480133        	lsi	f3, a1, 0x120
 779:	334193        	ssi	f9, a1, 204
 77c:	1ab320        	sub.s	f11, f3, f2
 77f:	4a2d00        	madd.s	f2, f13, f0
 782:	324143        	ssi	f4, a1, 200
 785:	314163        	ssi	f6, a1, 196
 788:	2f4183        	ssi	f8, a1, 188
 78b:	fae200        	mov.s	f14, f2
 78e:	4aeb10        	madd.s	f14, f11, f1
 791:	2e41f3        	ssi	f15, a1, 184
 794:	2d41c3        	ssi	f12, a1, 180
 797:	fab540        	rfr	a11, f5
 79a:	2a4153        	ssi	f5, a1, 168
 79d:	2c4103        	ssi	f0, a1, 176
 7a0:	2b4113        	ssi	f1, a1, 172
 7a3:	faae40        	rfr	a10, f14
 7a6:	000081        	l32r	a8, fffc07a8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc07a8>
			7a6: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6c
			7a6: R_XTENSA_ASM_EXPAND	__divsf3
 7a9:	0008e0        	callx8	a8
 7ac:	290103        	lsi	f0, a1, 164
 7af:	440113        	lsi	f1, a1, 0x110
 7b2:	2a0153        	lsi	f5, a1, 168
 7b5:	1ae100        	sub.s	f14, f1, f0
 7b8:	490113        	lsi	f1, a1, 0x124
 7bb:	fab540        	rfr	a11, f5
 7be:	1ad100        	sub.s	f13, f1, f0
 7c1:	fa5000        	mov.s	f5, f0
 7c4:	2c0103        	lsi	f0, a1, 176
 7c7:	2b0113        	lsi	f1, a1, 172
 7ca:	4a5e00        	madd.s	f5, f14, f0
 7cd:	2a61a2        	s32i	a10, a1, 168
 7d0:	4a5d10        	madd.s	f5, f13, f1
 7d3:	faa540        	rfr	a10, f5
 7d6:	000081        	l32r	a8, fffc07d8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc07d8>
			7d6: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x70
			7d6: R_XTENSA_ASM_EXPAND	__divsf3
 7d9:	0008e0        	callx8	a8
 7dc:	000081        	l32r	a8, fffc07dc <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc07dc>
			7dc: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x24
 7df:	2a01b3        	lsi	f11, a1, 168
 7e2:	fa7850        	wfr	f7, a8
 7e5:	000081        	l32r	a8, fffc07e8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc07e8>
			7e5: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x20
 7e8:	2d01c3        	lsi	f12, a1, 180
 7eb:	fa2850        	wfr	f2, a8
 7ee:	080c      	movi.n	a8, 0
 7f0:	2e01f3        	lsi	f15, a1, 184
 7f3:	2f0183        	lsi	f8, a1, 188
 7f6:	300133        	lsi	f3, a1, 192
 7f9:	310163        	lsi	f6, a1, 196
 7fc:	320143        	lsi	f4, a1, 200
 7ff:	330193        	lsi	f9, a1, 204
 802:	342192        	l32i	a9, a1, 208
 805:	fa0a50        	wfr	f0, a10
 808:	faa850        	wfr	f10, a8
 80b:	041216        	beqz	a2, 850 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x850>
			80b: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x850
 80e:	000081        	l32r	a8, fffc0810 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0810>
			80e: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x2c
 811:	fad850        	wfr	f13, a8
 814:	2a1bd0        	mul.s	f1, f11, f13
 817:	2a00d0        	mul.s	f0, f0, f13
 81a:	000081        	l32r	a8, fffc081c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc081c>
			81a: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x30
 81d:	4b01a0        	olt.s	b0, f1, f10
 820:	db1a00        	movt.s	f1, f10, b0
 823:	4b00a0        	olt.s	b0, f0, f10
 826:	fa5850        	wfr	f5, a8
 829:	db0a00        	movt.s	f0, f10, b0
 82c:	4b0500        	olt.s	b0, f5, f0
 82f:	db0500        	movt.s	f0, f5, b0
 832:	ea8000        	utrunc.s	a8, f0, 0
 835:	4b0510        	olt.s	b0, f5, f1
 838:	fa0100        	mov.s	f0, f1
 83b:	db0500        	movt.s	f0, f5, b0
 83e:	eaa000        	utrunc.s	a10, f0, 0
 841:	1188b0        	slli	a8, a8, 5
 844:	88aa      	add.n	a8, a8, a10
 846:	908820        	addx2	a8, a8, a2
 849:	001882        	l16ui	a8, a8, 0
 84c:	004606        	j	968 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x968>
			84c: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x968
 84f:	00          	.byte	00
 850:	4b0ba0        	olt.s	b0, f11, f10
 853:	000081        	l32r	a8, fffc0854 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0854>
			853: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x28
 856:	fa1b00        	mov.s	f1, f11
 859:	db1a00        	movt.s	f1, f10, b0
 85c:	4b00a0        	olt.s	b0, f0, f10
 85f:	fa2850        	wfr	f2, a8
 862:	db0a00        	movt.s	f0, f10, b0
 865:	4b0200        	olt.s	b0, f2, f0
 868:	db0200        	movt.s	f0, f2, b0
 86b:	080c      	movi.n	a8, 0
 86d:	2a4103        	ssi	f0, a1, 168
 870:	881a      	add.n	a8, a8, a1
 872:	4b0210        	olt.s	b0, f2, f1
 875:	2a28d2        	l32i	a13, a8, 168
 878:	2121b2        	l32i	a11, a1, 132
 87b:	fa8240        	rfr	a8, f2
 87e:	fac140        	rfr	a12, f1
 881:	1a21a2        	l32i	a10, a1, 104
 884:	d3c800        	movt	a12, a8, b0
 887:	326192        	s32i	a9, a1, 200
 88a:	314193        	ssi	f9, a1, 196
 88d:	304143        	ssi	f4, a1, 192
 890:	2f4163        	ssi	f6, a1, 188
 893:	2e4133        	ssi	f3, a1, 184
 896:	2d4183        	ssi	f8, a1, 180
 899:	2c41f3        	ssi	f15, a1, 176
 89c:	2b41c3        	ssi	f12, a1, 172
 89f:	000081        	l32r	a8, fffc08a0 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc08a0>
			89f: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x74
			89f: R_XTENSA_ASM_EXPAND	lets_and_go::carPaintColor(lets_and_go::CarPaint, unsigned short, float, float)
 8a2:	0008e0        	callx8	a8
 8a5:	00a092        	movi	a9, 0
 8a8:	faa950        	wfr	f10, a9
 8ab:	000091        	l32r	a9, fffc08ac <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc08ac>
			8ab: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x24
 8ae:	208aa0        	or	a8, a10, a10
 8b1:	fa7950        	wfr	f7, a9
 8b4:	3721a2        	l32i	a10, a1, 220
 8b7:	000091        	l32r	a9, fffc08b8 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc08b8>
			8b7: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x20
 8ba:	ffa0b2        	movi	a11, 255
 8bd:	fa2950        	wfr	f2, a9
 8c0:	2b01c3        	lsi	f12, a1, 172
 8c3:	2c01f3        	lsi	f15, a1, 176
 8c6:	2d0183        	lsi	f8, a1, 180
 8c9:	2e0133        	lsi	f3, a1, 184
 8cc:	2f0163        	lsi	f6, a1, 188
 8cf:	300143        	lsi	f4, a1, 192
 8d2:	310193        	lsi	f9, a1, 196
 8d5:	322192        	l32i	a9, a1, 200
 8d8:	029ab7        	bne	a10, a11, 8de <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x8de>
			8d8: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x8de
 8db:	002246        	j	968 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x968>
			8db: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x968
 8de:	ca0a00        	float.s	f0, a10, 0
 8e1:	0000b1        	l32r	a11, fffc08e4 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc08e4>
			8e1: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x1c
 8e4:	2a6182        	s32i	a8, a1, 168
 8e7:	faa040        	rfr	a10, f0
 8ea:	000081        	l32r	a8, fffc08ec <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc08ec>
			8ea: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x78
			8ea: R_XTENSA_ASM_EXPAND	__divsf3
 8ed:	0008e0        	callx8	a8
 8f0:	2a2182        	l32i	a8, a1, 168
 8f3:	fa0a50        	wfr	f0, a10
 8f6:	f4cb80        	extui	a12, a8, 11, 16
 8f9:	44b080        	extui	a11, a8, 0, 5
 8fc:	cabc00        	float.s	f11, a12, 0
 8ff:	ca5b00        	float.s	f5, a11, 0
 902:	548580        	extui	a8, a8, 5, 6
 905:	ca1800        	float.s	f1, a8, 0
 908:	2abb00        	mul.s	f11, f11, f0
 90b:	2a5500        	mul.s	f5, f5, f0
 90e:	2a0100        	mul.s	f0, f1, f0
 911:	fc1c      	movi.n	a12, 31
 913:	9a8b00        	trunc.s	a8, f11, 0
 916:	9ab500        	trunc.s	a11, f5, 0
 919:	9aa000        	trunc.s	a10, f0, 0
 91c:	538820        	max	a8, a8, a2
 91f:	53bb20        	max	a11, a11, a2
 922:	4388c0        	min	a8, a8, a12
 925:	43bbc0        	min	a11, a11, a12
 928:	53aa20        	max	a10, a10, a2
 92b:	fc3c      	movi.n	a12, 63
 92d:	118850        	slli	a8, a8, 11
 930:	43aac0        	min	a10, a10, a12
 933:	11aab0        	slli	a10, a10, 5
 936:	2088b0        	or	a8, a8, a11
 939:	2088a0        	or	a8, a8, a10
 93c:	0a0c      	movi.n	a10, 0
 93e:	faaa50        	wfr	f10, a10
 941:	0000a1        	l32r	a10, fffc0944 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc0944>
			941: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x20
 944:	322192        	l32i	a9, a1, 200
 947:	fa2a50        	wfr	f2, a10
 94a:	0000a1        	l32r	a10, fffc094c <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0xfffc094c>
			94a: R_XTENSA_SLOT0_OP	.literal._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x24
 94d:	310193        	lsi	f9, a1, 196
 950:	300143        	lsi	f4, a1, 192
 953:	2f0163        	lsi	f6, a1, 188
 956:	2e0133        	lsi	f3, a1, 184
 959:	2d0183        	lsi	f8, a1, 180
 95c:	2c01f3        	lsi	f15, a1, 176
 95f:	2b01c3        	lsi	f12, a1, 172
 962:	f48080        	extui	a8, a8, 0, 16
 965:	fa7a50        	wfr	f7, a10
 968:	005532        	s16i	a3, a5, 0
 96b:	005682        	s16i	a8, a6, 0
 96e:	771b      	addi.n	a7, a7, 1
 970:	552b      	addi.n	a5, a5, 2
 972:	662b      	addi.n	a6, a6, 2
 974:	022477        	blt	a4, a7, 97a <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x97a>
			974: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x97a
 977:	ff4f86        	j	6b9 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x6b9>
			977: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x6b9
 97a:	192132        	l32i	a3, a1, 100
 97d:	ff3bc6        	j	670 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x670>
			97d: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x670
 980:	1f2192        	l32i	a9, a1, 124
 983:	022937        	blt	a9, a3, 989 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x989>
			983: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x989
 986:	fed646        	j	4e3 <lets_and_go::CarSurfaceRaster<352, 288>::triangle(lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, lets_and_go::CarScreenVertex, unsigned short, lets_and_go::CarPaint, unsigned char)+0x4e3>
			986: R_XTENSA_SLOT0_OP	.text._ZN11lets_and_go16CarSurfaceRasterILi352ELi288EE8triangleENS_15CarScreenVertexES2_S2_tNS_8CarPaintEh+0x4e3
 989:	f01d      	retw.n

