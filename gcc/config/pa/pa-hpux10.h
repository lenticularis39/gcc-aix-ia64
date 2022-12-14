/* Definitions of target machine for GNU compiler, for HP PA-RISC 1.1
   Copyright (C) 1995, 1996, 1997, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Tim Moore (moore@defmacro.cs.utah.edu)

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

/* We can debug dynamically linked executables on hpux9; we also want
   dereferencing of a NULL pointer to cause a SEGV.  */
#undef LINK_SPEC
#if ((TARGET_DEFAULT | TARGET_CPU_DEFAULT) &  MASK_PA_11)
#define LINK_SPEC \
  "%{!mpa-risc-1-0:%{!shared:-L/lib/pa1.1 -L/usr/lib/pa1.1 }} -z %{mlinker-opt:-O} %{!shared:-u main} %{static:-a archive} %{shared:-b}"
#else
#define LINK_SPEC \
  "-z %{mlinker-opt:-O} %{!shared:-u main} %{static:-a archive} %{shared:-b}"
#endif

/* Like the default, except no -lg.  */
#undef LIB_SPEC
#define LIB_SPEC \
  "%{!shared:\
     %{!p:%{!pg:\
       %{!threads:-lc}\
       %{threads:-lcma -lc_r}}}\
     %{p: -L/lib/libp/ -lc}\
     %{pg: -L/lib/libp/ -lc}}"

#undef THREAD_MODEL_SPEC
#define THREAD_MODEL_SPEC "%{!threads:single}%{threads:dce}"

/* Under hpux10, the normal location of the `ld' and `as' programs is the
   /usr/ccs/bin directory.  */

#ifndef CROSS_COMPILE
#undef MD_EXEC_PREFIX
#define MD_EXEC_PREFIX "/usr/ccs/bin/"
#endif

/* Under hpux10, the normal location of the various *crt*.o files is
   the /usr/ccs/lib directory.  However, the profiling files are in
   /opt/langtools/lib.  */

#ifndef CROSS_COMPILE
#undef MD_STARTFILE_PREFIX
#define MD_STARTFILE_PREFIX "/usr/ccs/lib/"
#define MD_STARTFILE_PREFIX_1 "/opt/langtools/lib/"
#endif

/* hpux10 has the new HP assembler.  It's still lousy, but it's a whole lot
   better than the assembler shipped with older versions of hpux.  */
#define NEW_HP_ASSEMBLER
