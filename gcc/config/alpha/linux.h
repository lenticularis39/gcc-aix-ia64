/* Definitions of target machine for GNU compiler,
   for Alpha Linux-based GNU systems.
   Copyright (C) 1996, 1997, 1998 Free Software Foundation, Inc.
   Contributed by Richard Henderson.

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

#undef TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_FP | MASK_FPREGS | MASK_GAS)

#undef CPP_PREDEFINES
#define CPP_PREDEFINES \
"-Dlinux -Dunix -Asystem=linux -D_LONGLONG -D__alpha__ " \
SUB_CPP_PREDEFINES

/* The GNU C++ standard library requires that these macros be defined.  */
#undef CPLUSPLUS_CPP_SPEC
#define CPLUSPLUS_CPP_SPEC "-D_GNU_SOURCE %(cpp)"

#undef LIB_SPEC
#define LIB_SPEC \
  "%{shared: -lc} \
   %{!shared: %{pthread:-lpthread} \
              %{profile:-lc_p} %{!profile: -lc}}"

/* Show that we need a GP when profiling.  */
#undef TARGET_PROFILING_NEEDS_GP
#define TARGET_PROFILING_NEEDS_GP 1

/* Don't care about faults in the prologue.  */
#undef TARGET_CAN_FAULT_IN_PROLOGUE
#define TARGET_CAN_FAULT_IN_PROLOGUE 1

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

/* Define this so that all GNU/Linux targets handle the same pragmas.  */
#define HANDLE_PRAGMA_PACK_PUSH_POP

/* Do code reading to identify a signal frame, and set the frame
   state data appropriately.  See unwind-dw2.c for the structs.  */

#ifdef IN_LIBGCC2
#include <signal.h>
#include <sys/ucontext.h>
#endif

#define MD_FALLBACK_FRAME_STATE_FOR(CONTEXT, FS, SUCCESS)		\
  do {									\
    unsigned int *pc_ = (CONTEXT)->ra;					\
    struct sigcontext *sc_;						\
    long new_cfa_, i_;							\
									\
    if (pc_[0] != 0x47fe0410		/* mov $30,$16 */		\
        || pc_[2] != 0x00000083		/* callsys */)			\
      break;								\
    if (pc_[1] == 0x201f0067)		/* lda $0,NR_sigreturn */	\
      sc_ = (CONTEXT)->cfa;						\
    else if (pc_[1] == 0x201f015f)	/* lda $0,NR_rt_sigreturn */	\
      {									\
	struct rt_sigframe {						\
	  struct siginfo info;						\
	  struct ucontext uc;						\
	} *rt_ = (CONTEXT)->cfa;					\
	sc_ = &rt_->uc.uc_mcontext;					\
      }									\
    else								\
      break;								\
    new_cfa_ = sc_->sc_regs[30];					\
    (FS)->cfa_how = CFA_REG_OFFSET;					\
    (FS)->cfa_reg = 30;							\
    (FS)->cfa_offset = new_cfa_ - (long) (CONTEXT)->cfa;		\
    for (i_ = 0; i_ < 30; ++i_)						\
      {									\
	(FS)->regs.reg[i_].how = REG_SAVED_OFFSET;			\
	(FS)->regs.reg[i_].loc.offset					\
	  = (long)&sc_->sc_regs[i_] - new_cfa_;				\
      }									\
    for (i_ = 0; i_ < 31; ++i_)						\
      {									\
	(FS)->regs.reg[i_+32].how = REG_SAVED_OFFSET;			\
	(FS)->regs.reg[i_+32].loc.offset				\
	  = (long)&sc_->sc_fpregs[i_] - new_cfa_;			\
      }									\
    (FS)->regs.reg[31].how = REG_SAVED_OFFSET;				\
    (FS)->regs.reg[31].loc.offset = (long)&sc_->sc_pc - new_cfa_;	\
    (FS)->retaddr_column = 31;						\
    goto SUCCESS;							\
  } while (0)
