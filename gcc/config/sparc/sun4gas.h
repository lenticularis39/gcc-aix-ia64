/* Definitions of target machine for GNU compiler, for SunOS 4.x with gas
   Copyright (C) 1997, 2000 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* gas supports unaligned data.  */
#define UNALIGNED_DOUBLE_INT_ASM_OP "\t.uaxword\t"
#define UNALIGNED_INT_ASM_OP	"\t.uaword\t"
#define UNALIGNED_SHORT_ASM_OP	"\t.uahalf\t"

/* defaults.h will define DWARF2_UNWIND_INFO for us.  */
#undef DWARF2_UNWIND_INFO
