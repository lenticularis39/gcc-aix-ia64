/* Copyright (C) 2000, 2001 Free Software Foundation, Inc.
   This file was adapted from glibc sources.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* See an explanation about .init and .fini in crti.asm.  */

	.section .init
	mov	r14,r15
	lds.l	@r15+,pr
	mov.l	@r15+,r14
	rts
#ifdef __ELF__
	mov.l	@r15+,r12
#else
	nop
#endif

	.section .fini
	mov	r14,r15
	lds.l	@r15+,pr
	mov.l	@r15+,r14
	rts
#ifdef __ELF__
	mov.l	@r15+,r12
#else
	nop
#endif
