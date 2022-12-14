// Low-level functions for atomic operations: Sparc64 version  -*- C++ -*-

// Copyright (C) 1999, 2000, 2001 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

#ifndef _BITS_ATOMICITY_H
#define _BITS_ATOMICITY_H	1

typedef long _Atomic_word;

static inline _Atomic_word
__attribute__ ((__unused__))
__exchange_and_add (volatile _Atomic_word *__mem, int __val)
{
  _Atomic_word __tmp1, __tmp2;

  __asm__ __volatile__("1:	ldx	[%2], %0\n\t"
		       "	add	%0, %3, %1\n\t"
		       "	casx	[%2], %0, %1\n\t"
		       "	sub	%0, %1, %0\n\t"
		       "	brnz,pn	%0, 1b\n\t"
		       "	 nop"
		       : "=&r" (__tmp1), "=&r" (__tmp2)
		       : "r" (__mem), "r" (__val)
		       : "memory");
  return __tmp2;
}

static inline void
__attribute__ ((__unused__))
__atomic_add (volatile _Atomic_word* __mem, int __val)
{
  _Atomic_word __tmp1, __tmp2;

  __asm__ __volatile__("1:	ldx	[%2], %0\n\t"
		       "	add	%0, %3, %1\n\t"
		       "	casx	[%2], %0, %1\n\t"
		       "	sub	%0, %1, %0\n\t"
		       "	brnz,pn	%0, 1b\n\t"
		       "	 nop"
		       : "=&r" (__tmp1), "=&r" (__tmp2)
		       : "r" (__mem), "r" (__val)
		       : "memory");
}

static inline int
__attribute__ ((__unused__))
__compare_and_swap (volatile long *__p, long __oldval, long __newval)
{
  register int __tmp;
  register long __tmp2;

  __asm__ __volatile__("1:	ldx	[%4], %0\n\t"
		       "	mov	%2, %1\n\t"
		       "	cmp	%0, %3\n\t"
		       "	bne,a,pn %%xcc, 2f\n\t"
		       "	 mov	0, %0\n\t"
		       "	casx	[%4], %0, %1\n\t"
		       "	sub	%0, %1, %0\n\t"
		       "	brnz,pn	%0, 1b\n\t"
		       "	 mov	1, %0\n\t"
		       "2:"
		       : "=&r" (__tmp), "=&r" (__tmp2)
		       : "r" (__newval), "r" (__oldval), "r" (__p)
		       : "memory");
  return __tmp;
}

#endif /* atomicity.h */
