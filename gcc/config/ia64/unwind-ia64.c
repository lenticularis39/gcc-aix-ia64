/* Subroutines needed for unwinding IA-64 standard format stack frame
   info for exception handling.
   Copyright (C) 1997, 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Andrew MacLeod  <amacleod@cygnus.com>
	          Andrew Haley  <aph@cygnus.com>
		  David Mosberger-Tang <davidm@hpl.hp.com>

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

/* As a special exception, if you link this library with other files,
   some of which are compiled with GCC, to produce an executable,
   this library does not by itself cause the resulting executable
   to be covered by the GNU General Public License.
   This exception does not however invalidate any other reasons why
   the executable file might be covered by the GNU General Public License.  */


#include "tconfig.h"
#include "tsystem.h"
#include "unwind.h"
#include "unwind-ia64.h"

#if !USING_SJLJ_EXCEPTIONS

#define inline

#define UNW_VER(x)		((x) >> 48)
#define UNW_FLAG_MASK		0x0000ffff00000000
#define UNW_FLAG_OSMASK		0x0000f00000000000
#define UNW_FLAG_EHANDLER(x)	((x) & 0x0000000100000000L)
#define UNW_FLAG_UHANDLER(x)	((x) & 0x0000000200000000L)
#define UNW_LENGTH(x)		((x) & 0x00000000ffffffffL)

enum unw_application_register
{
  UNW_AR_BSP,
  UNW_AR_BSPSTORE,
  UNW_AR_PFS,
  UNW_AR_RNAT,
  UNW_AR_UNAT,
  UNW_AR_LC,
  UNW_AR_EC,
  UNW_AR_FPSR,
  UNW_AR_RSC,
  UNW_AR_CCV
};

enum unw_register_index
{
  /* Primary UNAT.  */
  UNW_REG_PRI_UNAT_GR,
  UNW_REG_PRI_UNAT_MEM,

  /* Memory Stack.  */
  UNW_REG_PSP,			/* previous memory stack pointer */

  /* Register Stack.  */
  UNW_REG_BSP,			/* register stack pointer */
  UNW_REG_BSPSTORE,
  UNW_REG_PFS,			/* previous function state */
  UNW_REG_RNAT,
  /* Return Pointer.  */
  UNW_REG_RP,

  /* Special preserved registers.  */
  UNW_REG_UNAT, UNW_REG_PR, UNW_REG_LC, UNW_REG_FPSR,

  /* Non-stacked general registers.  */
  UNW_REG_R2,
  UNW_REG_R4 = UNW_REG_R2 + 2,
  UNW_REG_R7 = UNW_REG_R2 + 5,
  UNW_REG_R31 = UNW_REG_R2 + 29,

  /* Non-stacked floating point registers.  */
  UNW_REG_F2,
  UNW_REG_F5 = UNW_REG_F2 + 3,
  UNW_REG_F16 = UNW_REG_F2 + 14,
  UNW_REG_F31 = UNW_REG_F2 + 29,

  /* Branch registers.  */
  UNW_REG_B1,
  UNW_REG_B5 = UNW_REG_B1 + 4,

  UNW_NUM_REGS
};

enum unw_where
{
  UNW_WHERE_NONE,	/* register isn't saved at all */
  UNW_WHERE_GR,		/* register is saved in a general register */
  UNW_WHERE_FR,		/* register is saved in a floating-point register */
  UNW_WHERE_BR,		/* register is saved in a branch register */
  UNW_WHERE_SPREL,	/* register is saved on memstack (sp-relative) */
  UNW_WHERE_PSPREL,	/* register is saved on memstack (psp-relative) */
 
 /* At the end of each prologue these locations get resolved to
     UNW_WHERE_PSPREL and UNW_WHERE_GR, respectively.  */
  UNW_WHERE_SPILL_HOME,	/* register is saved in its spill home */
  UNW_WHERE_GR_SAVE	/* register is saved in next general register */
};

#define UNW_WHEN_NEVER  0x7fffffff

struct unw_reg_info
{
  unsigned long val;		/* save location: register number or offset */
  enum unw_where where;		/* where the register gets saved */
  int when;			/* when the register gets saved */
};

typedef struct unw_state_record
{
  unsigned int first_region : 1;	/* is this the first region? */
  unsigned int done : 1;		/* are we done scanning descriptors? */
  unsigned int any_spills : 1;		/* got any register spills? */
  unsigned int in_body : 1;	/* are we inside a body? */

  unsigned char *imask;		/* imask of of spill_mask record or NULL */
  unsigned long pr_val;		/* predicate values */
  unsigned long pr_mask;	/* predicate mask */
  long spill_offset;		/* psp-relative offset for spill base */
  int region_start;
  int region_len;
  int epilogue_start;
  int epilogue_count;
  int when_target;

  unsigned char gr_save_loc;	/* next general register to use for saving */
  unsigned char return_link_reg; /* branch register for return link */

  struct unw_reg_state {
    struct unw_reg_state *next;
    unsigned long label;	/* label of this state record */
    struct unw_reg_info reg[UNW_NUM_REGS];
  } curr, *stack, *reg_state_list;

  _Unwind_Personality_Fn personality;
  
} _Unwind_FrameState;

enum unw_nat_type
{
  UNW_NAT_NONE,			/* NaT not represented */
  UNW_NAT_VAL,			/* NaT represented by NaT value (fp reg) */
  UNW_NAT_MEMSTK,		/* NaT value is in unat word at offset OFF  */
  UNW_NAT_REGSTK		/* NaT is in rnat */
};

struct unw_stack
{
  unsigned long limit;
  unsigned long top;
};

struct _Unwind_Context
{
  /* Initial frame info.  */
  unsigned long rnat;		/* rse nat collection */
  unsigned long regstk_top;	/* bsp for first frame */

  /* Current frame info.  */
  unsigned long bsp;		/* backing store pointer value */
  unsigned long sp;		/* stack pointer value */
  unsigned long psp;		/* previous sp value */
  unsigned long rp;		/* return pointer */
  unsigned long pr;		/* predicate collection */

  unsigned long region_start;	/* start of unwind region */
  unsigned long gp;		/* global pointer value */
  void *lsda;			/* language specific data area */

  /* Preserved state.  */
  unsigned long *bsp_loc;	/* previous bsp save location */
  unsigned long *bspstore_loc;
  unsigned long *pfs_loc;
  unsigned long *pri_unat_loc;
  unsigned long *unat_loc;
  unsigned long *lc_loc;
  unsigned long *fpsr_loc;

  unsigned long eh_data[4];

  struct unw_ireg
  {
    unsigned long *loc;
    struct unw_ireg_nat
    {
      enum unw_nat_type type : 3;
      signed long off : 61;		/* NaT word is at loc+nat.off */
    } nat;
  } ireg[32 - 2];

  unsigned long *br_loc[6 - 1];
  void *fr_loc[32 - 2];
};

typedef unsigned long unw_word;

/* Implicit register save order.  See section 11.4.2.3 Rules for Using
   Unwind Descriptors, rule 3.  */

static unsigned char const save_order[] =
{
  UNW_REG_RP, UNW_REG_PFS, UNW_REG_PSP, UNW_REG_PR,
  UNW_REG_UNAT, UNW_REG_LC, UNW_REG_FPSR, UNW_REG_PRI_UNAT_GR
};


#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

/* Unwind decoder routines */

static void
push (struct unw_state_record *sr)
{
  struct unw_reg_state *rs;

  rs = malloc (sizeof (struct unw_reg_state));
  memcpy (rs, &sr->curr, sizeof (*rs));
  rs->next = sr->stack;
  sr->stack = rs;
}

static void
pop (struct unw_state_record *sr)
{
  struct unw_reg_state *rs;

  rs = sr->stack;
  sr->stack = rs->next;
  free (rs);
}

static enum unw_register_index __attribute__((const))
decode_abreg (unsigned char abreg, int memory)
{
  switch (abreg)
    {
    case 0x04 ... 0x07: return UNW_REG_R4 + (abreg - 0x04);
    case 0x22 ... 0x25: return UNW_REG_F2 + (abreg - 0x22);
    case 0x30 ... 0x3f: return UNW_REG_F16 + (abreg - 0x30);
    case 0x41 ... 0x45: return UNW_REG_B1 + (abreg - 0x41);
    case 0x60: return UNW_REG_PR;
    case 0x61: return UNW_REG_PSP;
    case 0x62: return memory ? UNW_REG_PRI_UNAT_MEM : UNW_REG_PRI_UNAT_GR;
    case 0x63: return UNW_REG_RP;
    case 0x64: return UNW_REG_BSP;
    case 0x65: return UNW_REG_BSPSTORE;
    case 0x66: return UNW_REG_RNAT;
    case 0x67: return UNW_REG_UNAT;
    case 0x68: return UNW_REG_FPSR;
    case 0x69: return UNW_REG_PFS;
    case 0x6a: return UNW_REG_LC;
    default:
      abort ();
  }
}

static void
set_reg (struct unw_reg_info *reg, enum unw_where where,
	 int when, unsigned long val)
{
  reg->val = val;
  reg->where = where;
  if (reg->when == UNW_WHEN_NEVER)
    reg->when = when;
}

static void
alloc_spill_area (unsigned long *offp, unsigned long regsize,
		  struct unw_reg_info *lo, struct unw_reg_info *hi)
{
  struct unw_reg_info *reg;

  for (reg = hi; reg >= lo; --reg)
    {
      if (reg->where == UNW_WHERE_SPILL_HOME)
	{
	  reg->where = UNW_WHERE_PSPREL;
	  reg->val = 0x10 - *offp;
	  *offp += regsize;
	}
    }
}

static inline void
spill_next_when (struct unw_reg_info **regp, struct unw_reg_info *lim,
		 unw_word t)
{
  struct unw_reg_info *reg;

  for (reg = *regp; reg <= lim; ++reg)
    {
      if (reg->where == UNW_WHERE_SPILL_HOME)
	{
	  reg->when = t;
	  *regp = reg + 1;
	  return;
	}
    }
  /* Excess spill.  */
  abort ();
}

static void
finish_prologue (struct unw_state_record *sr)
{
  struct unw_reg_info *reg;
  unsigned long off;
  int i;

  /* First, resolve implicit register save locations
     (see Section "11.4.2.3 Rules for Using Unwind Descriptors", rule 3).  */

  for (i = 0; i < (int) sizeof(save_order); ++i)
    {
      reg = sr->curr.reg + save_order[i];
      if (reg->where == UNW_WHERE_GR_SAVE)
	{
	  reg->where = UNW_WHERE_GR;
	  reg->val = sr->gr_save_loc++;
	}
    }

  /* Next, compute when the fp, general, and branch registers get saved.
     This must come before alloc_spill_area() because we need to know
     which registers are spilled to their home locations.  */
  if (sr->imask)
    {
      static unsigned char const limit[3] = {
	UNW_REG_F31, UNW_REG_R7, UNW_REG_B5
      };

      unsigned char kind, mask = 0, *cp = sr->imask;
      int t;
      struct unw_reg_info *(regs[3]);

      regs[0] = sr->curr.reg + UNW_REG_F2;
      regs[1] = sr->curr.reg + UNW_REG_R4;
      regs[2] = sr->curr.reg + UNW_REG_B1;

      for (t = 0; t < sr->region_len; ++t)
	{
	  if ((t & 3) == 0)
	    mask = *cp++;
	  kind = (mask >> 2*(3-(t & 3))) & 3;
	  if (kind > 0)
	    spill_next_when(&regs[kind - 1], sr->curr.reg + limit[kind - 1],
			    sr->region_start + t);
	}
    }

  /* Next, lay out the memory stack spill area.  */
  if (sr->any_spills)
    {
      off = sr->spill_offset;
      alloc_spill_area(&off, 16, sr->curr.reg + UNW_REG_F2,
		       sr->curr.reg + UNW_REG_F31); 
      alloc_spill_area(&off,  8, sr->curr.reg + UNW_REG_B1,
		       sr->curr.reg + UNW_REG_B5);
      alloc_spill_area(&off,  8, sr->curr.reg + UNW_REG_R4,
		       sr->curr.reg + UNW_REG_R7);
    }
}

/*
 * Region header descriptors.
 */

static void
desc_prologue (int body, unw_word rlen, unsigned char mask,
	       unsigned char grsave, struct unw_state_record *sr)
{
  int i;

  if (!(sr->in_body || sr->first_region))
    finish_prologue(sr);
  sr->first_region = 0;

  /* Check if we're done.  */
  if (body && sr->when_target < sr->region_start + sr->region_len)
    {
      sr->done = 1;
      return;
    }

  for (i = 0; i < sr->epilogue_count; ++i)
    pop(sr);
  sr->epilogue_count = 0;
  sr->epilogue_start = UNW_WHEN_NEVER;

  if (!body)
    push(sr);

  sr->region_start += sr->region_len;
  sr->region_len = rlen;
  sr->in_body = body;

  if (!body)
    {
      for (i = 0; i < 4; ++i)
	{
	  if (mask & 0x8)
	    set_reg (sr->curr.reg + save_order[i], UNW_WHERE_GR,
		     sr->region_start + sr->region_len - 1, grsave++);
	  mask <<= 1;
	}
      sr->gr_save_loc = grsave;
      sr->any_spills = 0;
      sr->imask = 0;
      sr->spill_offset = 0x10;	/* default to psp+16 */
    }
}

/*
 * Prologue descriptors.
 */

static inline void
desc_abi (unsigned char abi __attribute__((unused)),
	  unsigned char context __attribute__((unused)),
	  struct unw_state_record *sr __attribute__((unused)))
{
  /* Anything to do?  */
}

static inline void
desc_br_gr (unsigned char brmask, unsigned char gr,
	    struct unw_state_record *sr)
{
  int i;

  for (i = 0; i < 5; ++i)
    {
      if (brmask & 1)
	set_reg (sr->curr.reg + UNW_REG_B1 + i, UNW_WHERE_GR,
		 sr->region_start + sr->region_len - 1, gr++);
      brmask >>= 1;
    }
}

static inline void
desc_br_mem (unsigned char brmask, struct unw_state_record *sr)
{
  int i;

  for (i = 0; i < 5; ++i)
    {
      if (brmask & 1)
	{
	  set_reg (sr->curr.reg + UNW_REG_B1 + i, UNW_WHERE_SPILL_HOME,
		   sr->region_start + sr->region_len - 1, 0);
	  sr->any_spills = 1;
	}
      brmask >>= 1;
    }
}

static inline void
desc_frgr_mem (unsigned char grmask, unw_word frmask,
	       struct unw_state_record *sr)
{
  int i;

  for (i = 0; i < 4; ++i)
    {
      if ((grmask & 1) != 0)
	{
	  set_reg (sr->curr.reg + UNW_REG_R4 + i, UNW_WHERE_SPILL_HOME,
		   sr->region_start + sr->region_len - 1, 0);
	  sr->any_spills = 1;
	}
      grmask >>= 1;
    }
  for (i = 0; i < 20; ++i)
    {
      if ((frmask & 1) != 0)
	{
	  set_reg (sr->curr.reg + UNW_REG_F2 + i, UNW_WHERE_SPILL_HOME,
		   sr->region_start + sr->region_len - 1, 0);
	  sr->any_spills = 1;
	}
      frmask >>= 1;
    }
}

static inline void
desc_fr_mem (unsigned char frmask, struct unw_state_record *sr)
{
  int i;

  for (i = 0; i < 4; ++i)
    {
      if ((frmask & 1) != 0)
	{
	  set_reg (sr->curr.reg + UNW_REG_F2 + i, UNW_WHERE_SPILL_HOME,
		   sr->region_start + sr->region_len - 1, 0);
	  sr->any_spills = 1;
	}
      frmask >>= 1;
    }
}

static inline void
desc_gr_gr (unsigned char grmask, unsigned char gr,
	    struct unw_state_record *sr)
{
  int i;

  for (i = 0; i < 4; ++i)
    {
      if ((grmask & 1) != 0)
	set_reg (sr->curr.reg + UNW_REG_R4 + i, UNW_WHERE_GR,
		 sr->region_start + sr->region_len - 1, gr++);
      grmask >>= 1;
    }
}

static inline void
desc_gr_mem (unsigned char grmask, struct unw_state_record *sr)
{
  int i;

  for (i = 0; i < 4; ++i)
    {
      if ((grmask & 1) != 0)
	{
	  set_reg (sr->curr.reg + UNW_REG_R4 + i, UNW_WHERE_SPILL_HOME,
		   sr->region_start + sr->region_len - 1, 0);
	  sr->any_spills = 1;
	}
      grmask >>= 1;
    }
}

static inline void
desc_mem_stack_f (unw_word t, unw_word size, struct unw_state_record *sr)
{
  set_reg (sr->curr.reg + UNW_REG_PSP, UNW_WHERE_NONE,
	   sr->region_start + MIN ((int)t, sr->region_len - 1), 16*size);
}

static inline void
desc_mem_stack_v (unw_word t, struct unw_state_record *sr)
{
  sr->curr.reg[UNW_REG_PSP].when
    = sr->region_start + MIN ((int)t, sr->region_len - 1);
}

static inline void
desc_reg_gr (unsigned char reg, unsigned char dst, struct unw_state_record *sr)
{
  set_reg (sr->curr.reg + reg, UNW_WHERE_GR,
	   sr->region_start + sr->region_len - 1, dst);
}

static inline void
desc_reg_psprel (unsigned char reg, unw_word pspoff,
		 struct unw_state_record *sr)
{
  set_reg (sr->curr.reg + reg, UNW_WHERE_PSPREL,
	   sr->region_start + sr->region_len - 1,
	   0x10 - 4*pspoff);
}

static inline void
desc_reg_sprel (unsigned char reg, unw_word spoff, struct unw_state_record *sr)
{
  set_reg (sr->curr.reg + reg, UNW_WHERE_SPREL,
	   sr->region_start + sr->region_len - 1,
	   4*spoff);
}

static inline void
desc_rp_br (unsigned char dst, struct unw_state_record *sr)
{
  sr->return_link_reg = dst;
}

static inline void
desc_reg_when (unsigned char regnum, unw_word t, struct unw_state_record *sr)
{
  struct unw_reg_info *reg = sr->curr.reg + regnum;

  if (reg->where == UNW_WHERE_NONE)
    reg->where = UNW_WHERE_GR_SAVE;
  reg->when = sr->region_start + MIN ((int)t, sr->region_len - 1);
}

static inline void
desc_spill_base (unw_word pspoff, struct unw_state_record *sr)
{
  sr->spill_offset = 0x10 - 4*pspoff;
}

static inline unsigned char *
desc_spill_mask (unsigned char *imaskp, struct unw_state_record *sr)
{
  sr->imask = imaskp;
  return imaskp + (2*sr->region_len + 7)/8;
}

/*
 * Body descriptors.
 */
static inline void
desc_epilogue (unw_word t, unw_word ecount, struct unw_state_record *sr)
{
  sr->epilogue_start = sr->region_start + sr->region_len - 1 - t;
  sr->epilogue_count = ecount + 1;
}

static inline void
desc_copy_state (unw_word label, struct unw_state_record *sr)
{
  struct unw_reg_state *rs;

  for (rs = sr->reg_state_list; rs; rs = rs->next)
    {
      if (rs->label == label)
	{
	  memcpy (&sr->curr, rs, sizeof(sr->curr));
	  return;
	}
    }
  abort ();
}

static inline void
desc_label_state (unw_word label, struct unw_state_record *sr)
{
  struct unw_reg_state *rs;

  rs = malloc (sizeof (struct unw_reg_state));
  memcpy (rs, &sr->curr, sizeof (*rs));
  rs->label = label;
  rs->next = sr->reg_state_list;
  sr->reg_state_list = rs;
}

/*
 * General descriptors.
 */

static inline int
desc_is_active (unsigned char qp, unw_word t, struct unw_state_record *sr)
{
  if (sr->when_target <= sr->region_start + MIN ((int)t, sr->region_len - 1))
    return 0;
  if (qp > 0)
    {
      if ((sr->pr_val & (1UL << qp)) == 0) 
	return 0;
      sr->pr_mask |= (1UL << qp);
    }
  return 1;
}

static inline void
desc_restore_p (unsigned char qp, unw_word t, unsigned char abreg,
		struct unw_state_record *sr)
{
  struct unw_reg_info *r;

  if (! desc_is_active (qp, t, sr))
    return;

  r = sr->curr.reg + decode_abreg (abreg, 0);
  r->where = UNW_WHERE_NONE;
  r->when = sr->region_start + MIN ((int)t, sr->region_len - 1);
  r->val = 0;
}

static inline void
desc_spill_reg_p (unsigned char qp, unw_word t, unsigned char abreg,
		  unsigned char x, unsigned char ytreg,
		  struct unw_state_record *sr)
{
  enum unw_where where = UNW_WHERE_GR;
  struct unw_reg_info *r;

  if (! desc_is_active (qp, t, sr))
    return;

  if (x)
    where = UNW_WHERE_BR;
  else if (ytreg & 0x80)
    where = UNW_WHERE_FR;

  r = sr->curr.reg + decode_abreg (abreg, 0);
  r->where = where;
  r->when = sr->region_start + MIN ((int)t, sr->region_len - 1);
  r->val = ytreg & 0x7f;
}

static inline void
desc_spill_psprel_p (unsigned char qp, unw_word t, unsigned char abreg,
		     unw_word pspoff, struct unw_state_record *sr)
{
  struct unw_reg_info *r;

  if (! desc_is_active (qp, t, sr))
    return;

  r = sr->curr.reg + decode_abreg (abreg, 1);
  r->where = UNW_WHERE_PSPREL;
  r->when = sr->region_start + MIN((int)t, sr->region_len - 1);
  r->val = 0x10 - 4*pspoff;
}

static inline void
desc_spill_sprel_p (unsigned char qp, unw_word t, unsigned char abreg,
		    unw_word spoff, struct unw_state_record *sr)
{
  struct unw_reg_info *r;

  if (! desc_is_active (qp, t, sr))
    return;

  r = sr->curr.reg + decode_abreg (abreg, 1);
  r->where = UNW_WHERE_SPREL;
  r->when = sr->region_start + MIN ((int)t, sr->region_len - 1);
  r->val = 4*spoff;
}


#define UNW_DEC_BAD_CODE(code)			abort ();

/* Region headers.  */
#define UNW_DEC_PROLOGUE_GR(fmt,r,m,gr,arg)	desc_prologue(0,r,m,gr,arg)
#define UNW_DEC_PROLOGUE(fmt,b,r,arg)		desc_prologue(b,r,0,32,arg)

/* Prologue descriptors. */
#define UNW_DEC_ABI(fmt,a,c,arg)		desc_abi(a,c,arg)
#define UNW_DEC_BR_GR(fmt,b,g,arg)		desc_br_gr(b,g,arg)
#define UNW_DEC_BR_MEM(fmt,b,arg)		desc_br_mem(b,arg)
#define UNW_DEC_FRGR_MEM(fmt,g,f,arg)		desc_frgr_mem(g,f,arg)
#define UNW_DEC_FR_MEM(fmt,f,arg)		desc_fr_mem(f,arg)
#define UNW_DEC_GR_GR(fmt,m,g,arg)		desc_gr_gr(m,g,arg)
#define UNW_DEC_GR_MEM(fmt,m,arg)		desc_gr_mem(m,arg)
#define UNW_DEC_MEM_STACK_F(fmt,t,s,arg)	desc_mem_stack_f(t,s,arg)
#define UNW_DEC_MEM_STACK_V(fmt,t,arg)		desc_mem_stack_v(t,arg)
#define UNW_DEC_REG_GR(fmt,r,d,arg)		desc_reg_gr(r,d,arg)
#define UNW_DEC_REG_PSPREL(fmt,r,o,arg)		desc_reg_psprel(r,o,arg)
#define UNW_DEC_REG_SPREL(fmt,r,o,arg)		desc_reg_sprel(r,o,arg)
#define UNW_DEC_REG_WHEN(fmt,r,t,arg)		desc_reg_when(r,t,arg)
#define UNW_DEC_PRIUNAT_WHEN_GR(fmt,t,arg)	desc_reg_when(UNW_REG_PRI_UNAT_GR,t,arg)
#define UNW_DEC_PRIUNAT_WHEN_MEM(fmt,t,arg)	desc_reg_when(UNW_REG_PRI_UNAT_MEM,t,arg)
#define UNW_DEC_PRIUNAT_GR(fmt,r,arg)		desc_reg_gr(UNW_REG_PRI_UNAT_GR,r,arg)
#define UNW_DEC_PRIUNAT_PSPREL(fmt,o,arg)	desc_reg_psprel(UNW_REG_PRI_UNAT_MEM,o,arg)
#define UNW_DEC_PRIUNAT_SPREL(fmt,o,arg)	desc_reg_sprel(UNW_REG_PRI_UNAT_MEM,o,arg)
#define UNW_DEC_RP_BR(fmt,d,arg)		desc_rp_br(d,arg)
#define UNW_DEC_SPILL_BASE(fmt,o,arg)		desc_spill_base(o,arg)
#define UNW_DEC_SPILL_MASK(fmt,m,arg)		(m = desc_spill_mask(m,arg))

/* Body descriptors.  */
#define UNW_DEC_EPILOGUE(fmt,t,c,arg)		desc_epilogue(t,c,arg)
#define UNW_DEC_COPY_STATE(fmt,l,arg)		desc_copy_state(l,arg)
#define UNW_DEC_LABEL_STATE(fmt,l,arg)		desc_label_state(l,arg)

/* General unwind descriptors.  */
#define UNW_DEC_SPILL_REG_P(f,p,t,a,x,y,arg)	desc_spill_reg_p(p,t,a,x,y,arg)
#define UNW_DEC_SPILL_REG(f,t,a,x,y,arg)	desc_spill_reg_p(0,t,a,x,y,arg)
#define UNW_DEC_SPILL_PSPREL_P(f,p,t,a,o,arg)	desc_spill_psprel_p(p,t,a,o,arg)
#define UNW_DEC_SPILL_PSPREL(f,t,a,o,arg)	desc_spill_psprel_p(0,t,a,o,arg)
#define UNW_DEC_SPILL_SPREL_P(f,p,t,a,o,arg)	desc_spill_sprel_p(p,t,a,o,arg)
#define UNW_DEC_SPILL_SPREL(f,t,a,o,arg)	desc_spill_sprel_p(0,t,a,o,arg)
#define UNW_DEC_RESTORE_P(f,p,t,a,arg)		desc_restore_p(p,t,a,arg)
#define UNW_DEC_RESTORE(f,t,a,arg)		desc_restore_p(0,t,a,arg)


/*
 * Generic IA-64 unwind info decoder.
 *
 * This file is used both by the Linux kernel and objdump.  Please keep
 * the copies of this file in sync.
 *
 * You need to customize the decoder by defining the following
 * macros/constants before including this file:
 *
 *  Types:
 *	unw_word	Unsigned integer type with at least 64 bits 
 *
 *  Register names:
 *	UNW_REG_BSP
 *	UNW_REG_BSPSTORE
 *	UNW_REG_FPSR
 *	UNW_REG_LC
 *	UNW_REG_PFS
 *	UNW_REG_PR
 *	UNW_REG_RNAT
 *	UNW_REG_PSP
 *	UNW_REG_RP
 *	UNW_REG_UNAT
 *
 *  Decoder action macros:
 *	UNW_DEC_BAD_CODE(code)
 *	UNW_DEC_ABI(fmt,abi,context,arg)
 *	UNW_DEC_BR_GR(fmt,brmask,gr,arg)
 *	UNW_DEC_BR_MEM(fmt,brmask,arg)
 *	UNW_DEC_COPY_STATE(fmt,label,arg)
 *	UNW_DEC_EPILOGUE(fmt,t,ecount,arg)
 *	UNW_DEC_FRGR_MEM(fmt,grmask,frmask,arg)
 *	UNW_DEC_FR_MEM(fmt,frmask,arg)
 *	UNW_DEC_GR_GR(fmt,grmask,gr,arg)
 *	UNW_DEC_GR_MEM(fmt,grmask,arg)
 *	UNW_DEC_LABEL_STATE(fmt,label,arg)
 *	UNW_DEC_MEM_STACK_F(fmt,t,size,arg)
 *	UNW_DEC_MEM_STACK_V(fmt,t,arg)
 *	UNW_DEC_PRIUNAT_GR(fmt,r,arg)
 *	UNW_DEC_PRIUNAT_WHEN_GR(fmt,t,arg)
 *	UNW_DEC_PRIUNAT_WHEN_MEM(fmt,t,arg)
 *	UNW_DEC_PRIUNAT_WHEN_PSPREL(fmt,pspoff,arg)
 *	UNW_DEC_PRIUNAT_WHEN_SPREL(fmt,spoff,arg)
 *	UNW_DEC_PROLOGUE(fmt,body,rlen,arg)
 *	UNW_DEC_PROLOGUE_GR(fmt,rlen,mask,grsave,arg)
 *	UNW_DEC_REG_PSPREL(fmt,reg,pspoff,arg)
 *	UNW_DEC_REG_REG(fmt,src,dst,arg)
 *	UNW_DEC_REG_SPREL(fmt,reg,spoff,arg)
 *	UNW_DEC_REG_WHEN(fmt,reg,t,arg)
 *	UNW_DEC_RESTORE(fmt,t,abreg,arg)
 *	UNW_DEC_RESTORE_P(fmt,qp,t,abreg,arg)
 *	UNW_DEC_SPILL_BASE(fmt,pspoff,arg)
 *	UNW_DEC_SPILL_MASK(fmt,imaskp,arg)
 *	UNW_DEC_SPILL_PSPREL(fmt,t,abreg,pspoff,arg)
 *	UNW_DEC_SPILL_PSPREL_P(fmt,qp,t,abreg,pspoff,arg)
 *	UNW_DEC_SPILL_REG(fmt,t,abreg,x,ytreg,arg)
 *	UNW_DEC_SPILL_REG_P(fmt,qp,t,abreg,x,ytreg,arg)
 *	UNW_DEC_SPILL_SPREL(fmt,t,abreg,spoff,arg)
 *	UNW_DEC_SPILL_SPREL_P(fmt,qp,t,abreg,pspoff,arg)
 */

static unw_word
unw_decode_uleb128 (unsigned char **dpp)
{
  unsigned shift = 0;
  unw_word byte, result = 0;
  unsigned char *bp = *dpp;

  while (1)
    {
      byte = *bp++;
      result |= (byte & 0x7f) << shift;
      if ((byte & 0x80) == 0)
	break;
      shift += 7;
    }
  *dpp = bp;
  return result;
}

static unsigned char *
unw_decode_x1 (unsigned char *dp,
	       unsigned char code __attribute__((unused)),
	       void *arg)
{
  unsigned char byte1, abreg;
  unw_word t, off;

  byte1 = *dp++;
  t = unw_decode_uleb128 (&dp);
  off = unw_decode_uleb128 (&dp);
  abreg = (byte1 & 0x7f);
  if (byte1 & 0x80)
	  UNW_DEC_SPILL_SPREL(X1, t, abreg, off, arg);
  else
	  UNW_DEC_SPILL_PSPREL(X1, t, abreg, off, arg);
  return dp;
}

static unsigned char *
unw_decode_x2 (unsigned char *dp,
	       unsigned char code __attribute__((unused)),
	       void *arg)
{
  unsigned char byte1, byte2, abreg, x, ytreg;
  unw_word t;

  byte1 = *dp++; byte2 = *dp++;
  t = unw_decode_uleb128 (&dp);
  abreg = (byte1 & 0x7f);
  ytreg = byte2;
  x = (byte1 >> 7) & 1;
  if ((byte1 & 0x80) == 0 && ytreg == 0)
    UNW_DEC_RESTORE(X2, t, abreg, arg);
  else
    UNW_DEC_SPILL_REG(X2, t, abreg, x, ytreg, arg);
  return dp;
}

static unsigned char *
unw_decode_x3 (unsigned char *dp,
	       unsigned char code __attribute__((unused)),
	       void *arg)
{
  unsigned char byte1, byte2, abreg, qp;
  unw_word t, off;

  byte1 = *dp++; byte2 = *dp++;
  t = unw_decode_uleb128 (&dp);
  off = unw_decode_uleb128 (&dp);

  qp = (byte1 & 0x3f);
  abreg = (byte2 & 0x7f);

  if (byte1 & 0x80)
    UNW_DEC_SPILL_SPREL_P(X3, qp, t, abreg, off, arg);
  else
    UNW_DEC_SPILL_PSPREL_P(X3, qp, t, abreg, off, arg);
  return dp;
}

static unsigned char *
unw_decode_x4 (unsigned char *dp,
	       unsigned char code __attribute__((unused)),
	       void *arg)
{
  unsigned char byte1, byte2, byte3, qp, abreg, x, ytreg;
  unw_word t;

  byte1 = *dp++; byte2 = *dp++; byte3 = *dp++;
  t = unw_decode_uleb128 (&dp);

  qp = (byte1 & 0x3f);
  abreg = (byte2 & 0x7f);
  x = (byte2 >> 7) & 1;
  ytreg = byte3;

  if ((byte2 & 0x80) == 0 && byte3 == 0)
    UNW_DEC_RESTORE_P(X4, qp, t, abreg, arg);
  else
    UNW_DEC_SPILL_REG_P(X4, qp, t, abreg, x, ytreg, arg);
  return dp;
}

static unsigned char *
unw_decode_r1 (unsigned char *dp, unsigned char code, void *arg)
{
  int body = (code & 0x20) != 0;
  unw_word rlen;

  rlen = (code & 0x1f);
  UNW_DEC_PROLOGUE(R1, body, rlen, arg);
  return dp;
}

static unsigned char *
unw_decode_r2 (unsigned char *dp, unsigned char code, void *arg)
{
  unsigned char byte1, mask, grsave;
  unw_word rlen;

  byte1 = *dp++;

  mask = ((code & 0x7) << 1) | ((byte1 >> 7) & 1);
  grsave = (byte1 & 0x7f);
  rlen = unw_decode_uleb128 (&dp);
  UNW_DEC_PROLOGUE_GR(R2, rlen, mask, grsave, arg);
  return dp;
}

static unsigned char *
unw_decode_r3 (unsigned char *dp, unsigned char code, void *arg)
{
  unw_word rlen;

  rlen = unw_decode_uleb128 (&dp);
  UNW_DEC_PROLOGUE(R3, ((code & 0x3) == 1), rlen, arg);
  return dp;
}

static unsigned char *
unw_decode_p1 (unsigned char *dp, unsigned char code, void *arg)
{
  unsigned char brmask = (code & 0x1f);

  UNW_DEC_BR_MEM(P1, brmask, arg);
  return dp;
}

static unsigned char *
unw_decode_p2_p5 (unsigned char *dp, unsigned char code, void *arg)
{
  if ((code & 0x10) == 0)
    {
      unsigned char byte1 = *dp++;

      UNW_DEC_BR_GR(P2, ((code & 0xf) << 1) | ((byte1 >> 7) & 1),
		    (byte1 & 0x7f), arg);
    }
  else if ((code & 0x08) == 0)
    {
      unsigned char byte1 = *dp++, r, dst;

      r = ((code & 0x7) << 1) | ((byte1 >> 7) & 1);
      dst = (byte1 & 0x7f);
      switch (r)
	{
	case 0: UNW_DEC_REG_GR(P3, UNW_REG_PSP, dst, arg); break;
	case 1: UNW_DEC_REG_GR(P3, UNW_REG_RP, dst, arg); break;
	case 2: UNW_DEC_REG_GR(P3, UNW_REG_PFS, dst, arg); break;
	case 3: UNW_DEC_REG_GR(P3, UNW_REG_PR, dst, arg); break;
	case 4: UNW_DEC_REG_GR(P3, UNW_REG_UNAT, dst, arg); break;
	case 5: UNW_DEC_REG_GR(P3, UNW_REG_LC, dst, arg); break;
	case 6: UNW_DEC_RP_BR(P3, dst, arg); break;
	case 7: UNW_DEC_REG_GR(P3, UNW_REG_RNAT, dst, arg); break;
	case 8: UNW_DEC_REG_GR(P3, UNW_REG_BSP, dst, arg); break;
	case 9: UNW_DEC_REG_GR(P3, UNW_REG_BSPSTORE, dst, arg); break;
	case 10: UNW_DEC_REG_GR(P3, UNW_REG_FPSR, dst, arg); break;
	case 11: UNW_DEC_PRIUNAT_GR(P3, dst, arg); break;
	default: UNW_DEC_BAD_CODE(r); break;
	}
    }
  else if ((code & 0x7) == 0)
    UNW_DEC_SPILL_MASK(P4, dp, arg);
  else if ((code & 0x7) == 1)
    {
      unw_word grmask, frmask, byte1, byte2, byte3;

      byte1 = *dp++; byte2 = *dp++; byte3 = *dp++;
      grmask = ((byte1 >> 4) & 0xf);
      frmask = ((byte1 & 0xf) << 16) | (byte2 << 8) | byte3;
      UNW_DEC_FRGR_MEM(P5, grmask, frmask, arg);
    }
  else
    UNW_DEC_BAD_CODE(code);
  return dp;
}

static unsigned char *
unw_decode_p6 (unsigned char *dp, unsigned char code, void *arg)
{
  int gregs = (code & 0x10) != 0;
  unsigned char mask = (code & 0x0f);

  if (gregs)
    UNW_DEC_GR_MEM(P6, mask, arg);
  else
    UNW_DEC_FR_MEM(P6, mask, arg);
  return dp;
}

static unsigned char *
unw_decode_p7_p10 (unsigned char *dp, unsigned char code, void *arg)
{
  unsigned char r, byte1, byte2;
  unw_word t, size;

  if ((code & 0x10) == 0)
    {
      r = (code & 0xf);
      t = unw_decode_uleb128 (&dp);
      switch (r)
	{
	case 0:
	  size = unw_decode_uleb128 (&dp);
	  UNW_DEC_MEM_STACK_F(P7, t, size, arg);
	  break;

	case 1: UNW_DEC_MEM_STACK_V(P7, t, arg); break;
	case 2: UNW_DEC_SPILL_BASE(P7, t, arg); break;
	case 3: UNW_DEC_REG_SPREL(P7, UNW_REG_PSP, t, arg); break;
	case 4: UNW_DEC_REG_WHEN(P7, UNW_REG_RP, t, arg); break;
	case 5: UNW_DEC_REG_PSPREL(P7, UNW_REG_RP, t, arg); break;
	case 6: UNW_DEC_REG_WHEN(P7, UNW_REG_PFS, t, arg); break;
	case 7: UNW_DEC_REG_PSPREL(P7, UNW_REG_PFS, t, arg); break;
	case 8: UNW_DEC_REG_WHEN(P7, UNW_REG_PR, t, arg); break;
	case 9: UNW_DEC_REG_PSPREL(P7, UNW_REG_PR, t, arg); break;
	case 10: UNW_DEC_REG_WHEN(P7, UNW_REG_LC, t, arg); break;
	case 11: UNW_DEC_REG_PSPREL(P7, UNW_REG_LC, t, arg); break;
	case 12: UNW_DEC_REG_WHEN(P7, UNW_REG_UNAT, t, arg); break;
	case 13: UNW_DEC_REG_PSPREL(P7, UNW_REG_UNAT, t, arg); break;
	case 14: UNW_DEC_REG_WHEN(P7, UNW_REG_FPSR, t, arg); break;
	case 15: UNW_DEC_REG_PSPREL(P7, UNW_REG_FPSR, t, arg); break;
	default: UNW_DEC_BAD_CODE(r); break;
	}
    }
  else
    {
      switch (code & 0xf)
	{
	case 0x0: /* p8 */
	  {
	    r = *dp++;
	    t = unw_decode_uleb128 (&dp);
	    switch (r)
	      {
	      case  1: UNW_DEC_REG_SPREL(P8, UNW_REG_RP, t, arg); break;
	      case  2: UNW_DEC_REG_SPREL(P8, UNW_REG_PFS, t, arg); break;
	      case  3: UNW_DEC_REG_SPREL(P8, UNW_REG_PR, t, arg); break;
	      case  4: UNW_DEC_REG_SPREL(P8, UNW_REG_LC, t, arg); break;
	      case  5: UNW_DEC_REG_SPREL(P8, UNW_REG_UNAT, t, arg); break;
	      case  6: UNW_DEC_REG_SPREL(P8, UNW_REG_FPSR, t, arg); break;
	      case  7: UNW_DEC_REG_WHEN(P8, UNW_REG_BSP, t, arg); break;
	      case  8: UNW_DEC_REG_PSPREL(P8, UNW_REG_BSP, t, arg); break;
	      case  9: UNW_DEC_REG_SPREL(P8, UNW_REG_BSP, t, arg); break;
	      case 10: UNW_DEC_REG_WHEN(P8, UNW_REG_BSPSTORE, t, arg); break;
	      case 11: UNW_DEC_REG_PSPREL(P8, UNW_REG_BSPSTORE, t, arg); break;
	      case 12: UNW_DEC_REG_SPREL(P8, UNW_REG_BSPSTORE, t, arg); break;
	      case 13: UNW_DEC_REG_WHEN(P8, UNW_REG_RNAT, t, arg); break;
	      case 14: UNW_DEC_REG_PSPREL(P8, UNW_REG_RNAT, t, arg); break;
	      case 15: UNW_DEC_REG_SPREL(P8, UNW_REG_RNAT, t, arg); break;
	      case 16: UNW_DEC_PRIUNAT_WHEN_GR(P8, t, arg); break;
	      case 17: UNW_DEC_PRIUNAT_PSPREL(P8, t, arg); break;
	      case 18: UNW_DEC_PRIUNAT_SPREL(P8, t, arg); break;
	      case 19: UNW_DEC_PRIUNAT_WHEN_MEM(P8, t, arg); break;
	      default: UNW_DEC_BAD_CODE(r); break;
	    }
	  }
	  break;

	case 0x1:
	  byte1 = *dp++; byte2 = *dp++;
	  UNW_DEC_GR_GR(P9, (byte1 & 0xf), (byte2 & 0x7f), arg);
	  break;

	case 0xf: /* p10 */
	  byte1 = *dp++; byte2 = *dp++;
	  UNW_DEC_ABI(P10, byte1, byte2, arg);
	  break;

	case 0x9:
	  return unw_decode_x1 (dp, code, arg);

	case 0xa:
	  return unw_decode_x2 (dp, code, arg);

	case 0xb:
	  return unw_decode_x3 (dp, code, arg);

	case 0xc:
	  return unw_decode_x4 (dp, code, arg);

	default:
	  UNW_DEC_BAD_CODE(code);
	  break;
	}
    }
  return dp;
}

static unsigned char *
unw_decode_b1 (unsigned char *dp, unsigned char code, void *arg)
{
  unw_word label = (code & 0x1f);

  if ((code & 0x20) != 0)
    UNW_DEC_COPY_STATE(B1, label, arg);
  else
    UNW_DEC_LABEL_STATE(B1, label, arg);
  return dp;
}

static unsigned char *
unw_decode_b2 (unsigned char *dp, unsigned char code, void *arg)
{
  unw_word t;

  t = unw_decode_uleb128 (&dp);
  UNW_DEC_EPILOGUE(B2, t, (code & 0x1f), arg);
  return dp;
}

static unsigned char *
unw_decode_b3_x4 (unsigned char *dp, unsigned char code, void *arg)
{
  unw_word t, ecount, label;

  if ((code & 0x10) == 0)
    {
      t = unw_decode_uleb128 (&dp);
      ecount = unw_decode_uleb128 (&dp);
      UNW_DEC_EPILOGUE(B3, t, ecount, arg);
    }
  else if ((code & 0x07) == 0)
    {
      label = unw_decode_uleb128 (&dp);
      if ((code & 0x08) != 0)
	UNW_DEC_COPY_STATE(B4, label, arg);
      else
	UNW_DEC_LABEL_STATE(B4, label, arg);
    }
  else
    switch (code & 0x7)
      {
      case 1: return unw_decode_x1 (dp, code, arg);
      case 2: return unw_decode_x2 (dp, code, arg);
      case 3: return unw_decode_x3 (dp, code, arg);
      case 4: return unw_decode_x4 (dp, code, arg);
      default: UNW_DEC_BAD_CODE(code); break;
      }
  return dp;
}

typedef unsigned char *(*unw_decoder) (unsigned char *, unsigned char, void *);

static unw_decoder unw_decode_table[2][8] =
{
  /* prologue table: */
  {
    unw_decode_r1,	/* 0 */
    unw_decode_r1,
    unw_decode_r2,
    unw_decode_r3,
    unw_decode_p1,	/* 4 */
    unw_decode_p2_p5,
    unw_decode_p6,
    unw_decode_p7_p10
  },
  {
    unw_decode_r1,	/* 0 */
    unw_decode_r1,
    unw_decode_r2,
    unw_decode_r3,
    unw_decode_b1,	/* 4 */
    unw_decode_b1,
    unw_decode_b2,
    unw_decode_b3_x4
  }
};

/*
 * Decode one descriptor and return address of next descriptor.
 */
static inline unsigned char *
unw_decode (unsigned char *dp, int inside_body, void *arg)
{
  unw_decoder decoder;
  unsigned char code;

  code = *dp++;
  decoder = unw_decode_table[inside_body][code >> 5];
  dp = (*decoder) (dp, code, arg);
  return dp;
}


/* RSE helper functions.  */

static inline unsigned long
ia64_rse_slot_num (unsigned long *addr)
{
  return (((unsigned long) addr) >> 3) & 0x3f;
}

/* Return TRUE if ADDR is the address of an RNAT slot.  */
static inline unsigned long
ia64_rse_is_rnat_slot (unsigned long *addr)
{
  return ia64_rse_slot_num (addr) == 0x3f;
}

/* Returns the address of the RNAT slot that covers the slot at
   address SLOT_ADDR.  */
static inline unsigned long *
ia64_rse_rnat_addr (unsigned long *slot_addr)
{
  return (unsigned long *) ((unsigned long) slot_addr | (0x3f << 3));
}

/* Calcuate the number of registers in the dirty partition starting at
   BSPSTORE with a size of DIRTY bytes.  This isn't simply DIRTY
   divided by eight because the 64th slot is used to store ar.rnat.  */
static inline unsigned long
ia64_rse_num_regs (unsigned long *bspstore, unsigned long *bsp)
{
  unsigned long slots = (bsp - bspstore);

  return slots - (ia64_rse_slot_num (bspstore) + slots)/0x40;
}

/* The inverse of the above: given bspstore and the number of
   registers, calculate ar.bsp.  */
static inline unsigned long *
ia64_rse_skip_regs (unsigned long *addr, long num_regs)
{
  long delta = ia64_rse_slot_num (addr) + num_regs;

  if (num_regs < 0)
    delta -= 0x3e;
  return addr + num_regs + delta/0x3f;
}


/* Unwind accessors.  */

static void
unw_access_gr (struct _Unwind_Context *info, int regnum,
	       unsigned long *val, char *nat, int write)
{
  unsigned long *addr, *nat_addr = 0, nat_mask = 0, dummy_nat;
  struct unw_ireg *ireg;

  if ((unsigned) regnum - 1 >= 127)
    abort ();

  if (regnum < 1)
    {
      nat_addr = addr = &dummy_nat;
      dummy_nat = 0;
    }
  else if (regnum < 32)
    {
      /* Access a non-stacked register.  */
      ireg = &info->ireg[regnum - 1];
      addr = ireg->loc;
      if (addr)
	{
	  nat_addr = addr + ireg->nat.off;
	  switch (ireg->nat.type)
	    {
	    case UNW_NAT_VAL:
	      /* Simulate getf.sig/setf.sig.  */
	      if (write)
		{
		  if (*nat)
		    {
		      /* Write NaTVal and be done with it.  */
		      addr[0] = 0;
		      addr[1] = 0x1fffe;
		      return;
		    }
		  addr[1] = 0x1003e;
		}
	      else if (addr[0] == 0 && addr[1] == 0x1ffe)
		{
		  /* Return NaT and be done with it.  */
		  *val = 0;
		  *nat = 1;
		  return;
		}
	      /* FALLTHRU */

	    case UNW_NAT_NONE:
	      dummy_nat = 0;
	      nat_addr = &dummy_nat;
	      break;

	    case UNW_NAT_MEMSTK:
	      nat_mask = 1UL << ((long) addr & 0x1f8)/8;
	      break;

	    case UNW_NAT_REGSTK:
	      nat_addr = ia64_rse_rnat_addr (addr);
	      if ((unsigned long) nat_addr >= info->regstk_top)
		nat_addr = &info->rnat;
	      nat_mask = 1UL << ia64_rse_slot_num (addr);
	      break;
	    }
	}
    }
  else
    {
      /* Access a stacked register.  */
      addr = ia64_rse_skip_regs ((unsigned long *) info->bsp, regnum - 32);
      nat_addr = ia64_rse_rnat_addr (addr);
      if ((unsigned long) nat_addr >= info->regstk_top)
	nat_addr = &info->rnat;
      nat_mask = 1UL << ia64_rse_slot_num (addr);
    }

  if (write)
    {
      *addr = *val;
      if (*nat)
	*nat_addr |= nat_mask;
      else
	*nat_addr &= ~nat_mask;
    }
  else
    {
      *val = *addr;
      *nat = (*nat_addr & nat_mask) != 0;
    }
}

/* Get the value of register REG as saved in CONTEXT.  */

_Unwind_Word
_Unwind_GetGR (struct _Unwind_Context *context, int index)
{
  _Unwind_Word ret;
  char nat;

  if (index == 1)
    return context->gp;
  else if (index >= 15 && index <= 18)
    return context->eh_data[index - 15];
  else
    unw_access_gr (context, index, &ret, &nat, 0);

  return ret;
}

/* Overwrite the saved value for register REG in CONTEXT with VAL.  */

void
_Unwind_SetGR (struct _Unwind_Context *context, int index, _Unwind_Word val)
{
  char nat = 0;

  if (index == 1)
    context->gp = val;
  else if (index >= 15 && index <= 18)
    context->eh_data[index - 15] = val;
  else
    unw_access_gr (context, index, &val, &nat, 1);
}

/* Retrieve the return address for CONTEXT.  */

inline _Unwind_Ptr
_Unwind_GetIP (struct _Unwind_Context *context)
{
  return context->rp;
}

/* Overwrite the return address for CONTEXT with VAL.  */

inline void
_Unwind_SetIP (struct _Unwind_Context *context, _Unwind_Ptr val)
{
  context->rp = val;
}

void *
_Unwind_GetLanguageSpecificData (struct _Unwind_Context *context)
{
  return context->lsda;
}

_Unwind_Ptr
_Unwind_GetRegionStart (struct _Unwind_Context *context)
{
  return context->region_start;
}


static _Unwind_Reason_Code
uw_frame_state_for (struct _Unwind_Context *context, _Unwind_FrameState *fs)
{
  struct unw_table_entry *ent;
  unsigned long *unw, header, length;
  unsigned char *insn, *insn_end;
  unsigned long segment_base;

  memset (fs, 0, sizeof (*fs));
  context->lsda = 0;

  ent = _Unwind_FindTableEntry ((void *) context->rp,
				&segment_base, &context->gp);
  if (ent == NULL)
    {
      /* Couldn't find unwind info for this function.  Try an
	 os-specific fallback mechanism.  This will necessarily
	 not profide a personality routine or LSDA.  */
#ifdef MD_FALLBACK_FRAME_STATE_FOR
      MD_FALLBACK_FRAME_STATE_FOR (context, fs, success);
      return _URC_END_OF_STACK;
    success:
      return _URC_NO_REASON;
#else
      return _URC_END_OF_STACK;
#endif
    }

  context->region_start = ent->start_offset + segment_base;
  fs->when_target = (context->rp - context->region_start) / 16 * 3;

  unw = (unsigned long *) (ent->info_offset + segment_base);
  header = *unw;
  length = UNW_LENGTH (header);

  /* ??? Perhaps check UNW_VER / UNW_FLAG_OSMASK.  */

  if (UNW_FLAG_EHANDLER (header) | UNW_FLAG_UHANDLER (header))
    {
      fs->personality =
	*(_Unwind_Personality_Fn *) (unw[length + 1] + context->gp);
      context->lsda = unw + length + 2;
    }

  insn = (unsigned char *) (unw + 1);
  insn_end = (unsigned char *) (unw + 1 + length);
  while (!fs->done && insn < insn_end)
    insn = unw_decode (insn, fs->in_body, fs);

  /* If we're in the epilogue, sp has been restored and all values
     on the memory stack below psp also have been restored.  */
  if (fs->when_target > fs->epilogue_start)
    {
      struct unw_reg_info *r;

      fs->curr.reg[UNW_REG_PSP].where = UNW_WHERE_NONE;
      fs->curr.reg[UNW_REG_PSP].val = 0;
      for (r = fs->curr.reg; r < fs->curr.reg + UNW_NUM_REGS; ++r)
	if ((r->where == UNW_WHERE_PSPREL && r->val <= 0x10)
	    || r->where == UNW_WHERE_SPREL)
	  r->where = UNW_WHERE_NONE;
    }

  /* If RP did't get saved, generate entry for the return link register.  */
  if (fs->curr.reg[UNW_REG_RP].when >= fs->when_target)
    {
      fs->curr.reg[UNW_REG_RP].where = UNW_WHERE_BR;
      fs->curr.reg[UNW_REG_RP].when = -1;
      fs->curr.reg[UNW_REG_RP].val = fs->return_link_reg;
    }

  return _URC_NO_REASON;
}

static void
uw_update_reg_address (struct _Unwind_Context *context,
		       _Unwind_FrameState *fs,
		       enum unw_register_index regno)
{
  struct unw_reg_info *r = fs->curr.reg + regno;
  void *addr;
  unsigned long rval;

  if (r->where == UNW_WHERE_NONE || r->when >= fs->when_target)
    return;

  rval = r->val;
  switch (r->where)
    {
    case UNW_WHERE_GR:
      if (rval >= 32)
	addr = ia64_rse_skip_regs ((unsigned long *) context->bsp, rval - 32);
      else if (rval >= 2)
	addr = context->ireg[rval - 2].loc;
      else
	abort ();
      break;

    case UNW_WHERE_FR:
      if (rval >= 2 && rval < 32)
	addr = context->fr_loc[rval - 2];
      else
	abort ();
      break;

    case UNW_WHERE_BR:
      if (rval >= 1 && rval <= 5)
	addr = context->br_loc[rval - 1];
      else
	abort ();
      break;

    case UNW_WHERE_SPREL:
      addr = (void *)(context->sp + rval);
      break;

    case UNW_WHERE_PSPREL:
      addr = (void *)(context->psp + rval);
      break;

    default:
      abort ();
    }

  switch (regno)
    {
    case UNW_REG_R2 ... UNW_REG_R31:
      context->ireg[regno - UNW_REG_R2].loc = addr;
      switch (r->where)
      {
      case UNW_WHERE_GR:
	if (rval >= 32)
	  {
	    context->ireg[regno - UNW_REG_R2].nat.type = UNW_NAT_MEMSTK;
	    context->ireg[regno - UNW_REG_R2].nat.off
	      = context->pri_unat_loc - (unsigned long *) addr;
	  }
	else if (rval >= 2)
	  {
	    context->ireg[regno - UNW_REG_R2].nat
	      = context->ireg[rval - 2].nat;
	  }
	else
	  abort ();
	break;

      case UNW_WHERE_FR:
	context->ireg[regno - UNW_REG_R2].nat.type = UNW_NAT_VAL;
	context->ireg[regno - UNW_REG_R2].nat.off = 0;
	break;

      case UNW_WHERE_BR:
	context->ireg[regno - UNW_REG_R2].nat.type = UNW_NAT_NONE;
	context->ireg[regno - UNW_REG_R2].nat.off = 0;
	break;

      case UNW_WHERE_PSPREL:
      case UNW_WHERE_SPREL:
	context->ireg[regno - UNW_REG_R2].nat.type = UNW_NAT_MEMSTK;
	context->ireg[regno - UNW_REG_R2].nat.off
	  = context->pri_unat_loc - (unsigned long *) addr;
	break;

      default:
	abort ();
      }
      break;

    case UNW_REG_F2 ... UNW_REG_F31:
      context->fr_loc[regno - UNW_REG_F2] = addr;
      break;

    case UNW_REG_B1 ... UNW_REG_B5:
      context->br_loc[regno - UNW_REG_B1] = addr;
      break;

    case UNW_REG_BSP:
      context->bsp_loc = addr;
      break;
    case UNW_REG_BSPSTORE:
      context->bspstore_loc = addr;
      break;
    case UNW_REG_PFS:
      context->pfs_loc = addr;
      break;
    case UNW_REG_RP:
      context->rp = *(unsigned long *)addr;
      break;
    case UNW_REG_UNAT:
      context->unat_loc = addr;
      break;
    case UNW_REG_PR:
      context->pr = *(unsigned long *) addr;
      break;
    case UNW_REG_LC:
      context->lc_loc = addr;
      break;
    case UNW_REG_FPSR:
      context->fpsr_loc = addr;
      break;

    case UNW_REG_PSP:
      context->psp = *(unsigned long *)addr;
      break;

    case UNW_REG_RNAT:
    case UNW_NUM_REGS:
      abort ();
    }
}

static void
uw_update_context (struct _Unwind_Context *context, _Unwind_FrameState *fs)
{
  long i;

  context->sp = context->psp;

  /* First, set PSP.  Subsequent instructions may depend on this value.  */
  if (fs->when_target > fs->curr.reg[UNW_REG_PSP].when)
    {
      if (fs->curr.reg[UNW_REG_PSP].where == UNW_WHERE_NONE)
	context->psp = context->psp + fs->curr.reg[UNW_REG_PSP].val;
      else
	uw_update_reg_address (context, fs, UNW_REG_PSP);
    }

  /* Determine the location of the primary UNaT.  */
  {
    int i;
    if (fs->when_target < fs->curr.reg[UNW_REG_PRI_UNAT_GR].when)
      i = UNW_REG_PRI_UNAT_MEM;
    else if (fs->when_target < fs->curr.reg[UNW_REG_PRI_UNAT_MEM].when)
      i = UNW_REG_PRI_UNAT_GR;
    else if (fs->curr.reg[UNW_REG_PRI_UNAT_MEM].when
	     > fs->curr.reg[UNW_REG_PRI_UNAT_GR].when)
      i = UNW_REG_PRI_UNAT_MEM;
    else
      i = UNW_REG_PRI_UNAT_GR;
    uw_update_reg_address (context, fs, i);
  }

  /* Compute the addresses of all registers saved in this frame.  */
  for (i = UNW_REG_BSP; i < UNW_NUM_REGS; ++i)
    uw_update_reg_address (context, fs, i);

  /* Unwind BSP for the local registers allocated this frame.  */
  /* ??? What to do with stored BSP or BSPSTORE registers.  */
  if (fs->when_target > fs->curr.reg[UNW_REG_PFS].when)
    {
      unsigned long pfs = *context->pfs_loc;
      unsigned long sol = (pfs >> 7) & 0x7f;
      context->bsp = (unsigned long)
	ia64_rse_skip_regs ((unsigned long *) context->bsp, -sol);
    }
}

/* Fill in CONTEXT for top-of-stack.  The only valid registers at this
   level will be the return address and the CFA.  */
   
#define uw_init_context(CONTEXT) \
  uw_init_context_1 (CONTEXT, __builtin_dwarf_cfa (), __builtin_ia64_bsp ())

static void
uw_init_context_1 (struct _Unwind_Context *context, void *psp, void *bsp)
{
  void *rp = __builtin_extract_return_addr (__builtin_return_address (0));
  void *sp = __builtin_dwarf_cfa ();
  _Unwind_FrameState fs;

  /* Flush the register stack to memory so that we can access it.  */
  __builtin_ia64_flushrs ();

  memset (context, 0, sizeof (struct _Unwind_Context));
  context->bsp = (unsigned long) bsp;
  context->sp = (unsigned long) sp;
  context->psp = (unsigned long) psp;
  context->rp = (unsigned long) rp;

  asm ("mov %0 = pr" : "=r" (context->pr));
  /* ??? Get rnat.  Don't we have to turn off the rse for that?  */

  if (uw_frame_state_for (context, &fs) != _URC_NO_REASON)
    abort ();

  /* Force the frame state to use the known cfa value.  */
  fs.curr.reg[UNW_REG_PSP].when = -1;
  fs.curr.reg[UNW_REG_PSP].where = UNW_WHERE_NONE;
  fs.curr.reg[UNW_REG_PSP].val = sp - psp;

  uw_update_context (context, &fs);
}

/* Install (ie longjmp to) the contents of TARGET.  */

static void __attribute__((noreturn))
uw_install_context (struct _Unwind_Context *current __attribute__((unused)),
		    struct _Unwind_Context *target)
{
  unsigned long ireg_buf[4], ireg_nat = 0, ireg_pr = 0;
  long i;

  /* Copy integer register data from the target context to a
     temporary buffer.  Do this so that we can frob AR.UNAT
     to get the NaT bits for these registers set properly.  */
  for (i = 4; i <= 7; ++i)
    {
      char nat;
      void *t = target->ireg[i - 2].loc;
      if (t)
	{
	  unw_access_gr (target, i, &ireg_buf[i - 4], &nat, 0);
          ireg_nat |= (long)nat << (((size_t)&ireg_buf[i - 4] >> 3) & 0x3f);
	  /* Set p6 - p9.  */
	  ireg_pr |= 4L << i;
	}
    }

  /* The value in uc_bsp that we've computed is that for the 
     target function.  The value that we install below will be
     adjusted by the BR.RET instruction based on the contents
     of AR.PFS.  So we must unadjust that here.  */
  target->bsp
    = ia64_rse_skip_regs (target->bsp, (*target->pfs_loc >> 7) & 0x7f);

  /* Provide assembly with the offsets into the _Unwind_Context.  */
  asm volatile ("uc_rnat = %0"
		: : "i"(offsetof (struct _Unwind_Context, rnat)));
  asm volatile ("uc_bsp = %0"
		: : "i"(offsetof (struct _Unwind_Context, bsp)));
  asm volatile ("uc_psp = %0"
		: : "i"(offsetof (struct _Unwind_Context, psp)));
  asm volatile ("uc_rp = %0"
		: : "i"(offsetof (struct _Unwind_Context, rp)));
  asm volatile ("uc_pr = %0"
		: : "i"(offsetof (struct _Unwind_Context, pr)));
  asm volatile ("uc_gp = %0"
		: : "i"(offsetof (struct _Unwind_Context, gp)));
  asm volatile ("uc_pfs_loc = %0"
		: : "i"(offsetof (struct _Unwind_Context, pfs_loc)));
  asm volatile ("uc_unat_loc = %0"
		: : "i"(offsetof (struct _Unwind_Context, unat_loc)));
  asm volatile ("uc_lc_loc = %0"
		: : "i"(offsetof (struct _Unwind_Context, lc_loc)));
  asm volatile ("uc_fpsr_loc = %0"
		: : "i"(offsetof (struct _Unwind_Context, fpsr_loc)));
  asm volatile ("uc_eh_data = %0"
		: : "i"(offsetof (struct _Unwind_Context, eh_data)));
  asm volatile ("uc_br_loc = %0"
		: : "i"(offsetof (struct _Unwind_Context, br_loc)));
  asm volatile ("uc_fr_loc = %0"
		: : "i"(offsetof (struct _Unwind_Context, fr_loc)));

  asm volatile (
	/* Load up call-saved non-window integer registers from ireg_buf.  */
	"add r20 = 8, %1			\n\t"
	"mov ar.unat = %2			\n\t"
	"mov pr = %3, 0x3c0			\n\t"
	";;					\n\t"
	"(p6) ld8.fill r4 = [%1]		\n\t"
	"(p7) ld8.fill r5 = [r20]		\n\t"
	"add r21 = uc_br_loc + 8, %0		\n\t"
	"adds %1 = 16, %1			\n\t"
	"adds r20 = 16, r20			\n\t"
	";;					\n\t"
	"(p8) ld8.fill r6 = [%1]		\n\t"
	"(p9) ld8.fill r7 = [r20]		\n\t"
	"add r20 = uc_br_loc, %0		\n\t"
	";;					\n\t"
	/* Load up call-saved branch registers.  */
	"ld8 r22 = [r20], 16			\n\t"
	"ld8 r23 = [r21], 16			\n\t"
	";;					\n\t"
	"ld8 r24 = [r20], 16			\n\t"
	"ld8 r25 = [r21], uc_fr_loc - (uc_br_loc + 24)\n\t"
	";;					\n\t"
	"ld8 r26 = [r20], uc_fr_loc + 8 - (uc_br_loc + 32)\n\t"
	"ld8 r27 = [r21], 24			\n\t"
	"cmp.ne p6, p0 = r0, r22		\n\t"
	";;					\n\t"
	"ld8 r28 = [r20], 8			\n\t"
	"(p6) ld8 r22 = [r22]			\n\t"
	"cmp.ne p7, p0 = r0, r23		\n\t"
	";;					\n\t"
	"(p7) ld8 r23 = [r23]			\n\t"
	"cmp.ne p8, p0 = r0, r24		\n\t"
	";;					\n\t"
	"(p8) ld8 r24 = [r24]			\n\t"
	"(p6) mov b1 = r22			\n\t"
	"cmp.ne p9, p0 = r0, r25		\n\t"
	";;					\n\t"
	"(p9) ld8 r25 = [r25]			\n\t"
	"(p7) mov b2 = r23			\n\t"
	"cmp.ne p6, p0 = r0, r26		\n\t"
	";;					\n\t"
	"(p6) ld8 r26 = [r26]			\n\t"
	"(p8) mov b3 = r24			\n\t"
	"cmp.ne p7, p0 = r0, r27		\n\t"
	";;					\n\t"
	/* Load up call-saved fp registers.  */
	"(p7) ldf.fill f2 = [r27]		\n\t"
	"(p9) mov b4 = r25			\n\t"
	"cmp.ne p8, p0 = r0, r28		\n\t"
	";;					\n\t"
	"(p8) ldf.fill f3 = [r28]		\n\t"
	"(p6) mov b5 = r26			\n\t"
	";;					\n\t"
	"ld8 r29 = [r20], 16*8 - 4*8		\n\t"
	"ld8 r30 = [r21], 17*8 - 5*8		\n\t"
	";;					\n\t"
	"ld8 r22 = [r20], 16			\n\t"
	"ld8 r23 = [r21], 16			\n\t"
	";;					\n\t"
	"ld8 r24 = [r20], 16			\n\t"
	"ld8 r25 = [r21]			\n\t"
	"cmp.ne p6, p0 = r0, r29		\n\t"
	";;					\n\t"
	"ld8 r26 = [r20], 8			\n\t"
	"(p6) ldf.fill f4 = [r29]		\n\t"
	"cmp.ne p7, p0 = r0, r30		\n\t"
	";;					\n\t"
	"ld8 r27 = [r20], 8			\n\t"
	"(p7) ldf.fill f5 = [r30]		\n\t"
	"cmp.ne p6, p0 = r0, r22		\n\t"
	";;					\n\t"
	"ld8 r28 = [r20], 8			\n\t"
	"(p6) ldf.fill f16 = [r22]		\n\t"
	"cmp.ne p7, p0 = r0, r23		\n\t"
	";;					\n\t"
	"ld8 r29 = [r20], 8			\n\t"
	"(p7) ldf.fill f17 = [r23]		\n\t"
	"cmp.ne p6, p0 = r0, r24		\n\t"
	";;					\n\t"
	"ld8 r22 = [r20], 8			\n\t"
	"(p6) ldf.fill f18 = [r24]		\n\t"
	"cmp.ne p7, p0 = r0, r25		\n\t"
	";;					\n\t"
	"ld8 r23 = [r20], 8			\n\t"
	"(p7) ldf.fill f19 = [r25]		\n\t"
	"cmp.ne p6, p0 = r0, r26		\n\t"
	";;					\n\t"
	"ld8 r24 = [r20], 8			\n\t"
	"(p6) ldf.fill f20 = [r26]		\n\t"
	"cmp.ne p7, p0 = r0, r27		\n\t"
	";;					\n\t"
	"ld8 r25 = [r20], 8			\n\t"
	"(p7) ldf.fill f21 = [r27]		\n\t"
	"cmp.ne p6, p0 = r0, r28		\n\t"
	";;					\n\t"
	"ld8 r26 = [r20], 8			\n\t"
	"(p6) ldf.fill f22 = [r28]		\n\t"
	"cmp.ne p7, p0 = r0, r29		\n\t"
	";;					\n\t"
	"ld8 r28 = [r20], 8			\n\t"
	"(p7) ldf.fill f23 = [r29]		\n\t"
	"cmp.ne p6, p0 = r0, r22		\n\t"
	";;					\n\t"
	"ld8 r29 = [r20], 8			\n\t"
	"(p6) ldf.fill f24 = [r22]		\n\t"
	"cmp.ne p7, p0 = r0, r23		\n\t"
	";;					\n\t"
	"(p7) ldf.fill f25 = [r23]		\n\t"
	"cmp.ne p6, p0 = r0, r24		\n\t"
	"cmp.ne p7, p0 = r0, r25		\n\t"
	";;					\n\t"
	"(p6) ldf.fill f26 = [r24]		\n\t"
	"(p7) ldf.fill f27 = [r25]		\n\t"
	"cmp.ne p6, p0 = r0, r26		\n\t"
	";;					\n\t"
	"(p6) ldf.fill f28 = [r26]		\n\t"
	"cmp.ne p7, p0 = r0, r27		\n\t"
	"cmp.ne p6, p0 = r0, r28		\n\t"
	";;					\n\t"
	"(p7) ldf.fill f29 = [r27]		\n\t"
	"(p6) ldf.fill f30 = [r28]		\n\t"
	"cmp.ne p7, p0 = r0, r29		\n\t"
	";;					\n\t"
	"(p7) ldf.fill f31 = [r29]		\n\t"
	"add r20 = uc_rnat, %0			\n\t"
	"add r21 = uc_bsp, %0			\n\t"
	";;					\n\t"
	/* Load the balance of the thread state from the context.  */
	"ld8 r22 = [r20], uc_psp - uc_rnat	\n\t"
	"ld8 r23 = [r21], uc_gp - uc_bsp	\n\t"
	";;					\n\t"
	"ld8 r24 = [r20], uc_pfs_loc - uc_psp	\n\t"
	"ld8 r1 = [r21], uc_rp - uc_gp		\n\t"
	";;					\n\t"
	"ld8 r25 = [r20], uc_unat_loc - uc_pfs_loc\n\t"
	"ld8 r26 = [r21], uc_pr - uc_rp		\n\t"
	";;					\n\t"
	"ld8 r27 = [r20], uc_lc_loc - uc_unat_loc\n\t"
	"ld8 r28 = [r21], uc_fpsr_loc - uc_pr	\n\t"
	";;					\n\t"
	"ld8 r29 = [r20], uc_eh_data - uc_lc_loc\n\t"
	"ld8 r30 = [r21], uc_eh_data + 8 - uc_fpsr_loc\n\t"
	";;					\n\t"
	/* Load data for the exception handler.  */
	"ld8 r15 = [r20], 16			\n\t"
	"ld8 r16 = [r21], 16			\n\t"
	";;					\n\t"
	"ld8 r17 = [r20]			\n\t"
	"ld8 r18 = [r21]			\n\t"
	";;					\n\t"
	/* Install the balance of the thread state loaded above.  */
	"cmp.ne p6, p0 = r0, r25		\n\t"
	"cmp.ne p7, p0 = r0, r27		\n\t"
	";;					\n\t"
	"(p6) ld8 r25 = [r25]			\n\t"
	"(p7) ld8 r27 = [r27]			\n\t"
	";;					\n\t"
	"(p7) mov.m ar.unat = r27		\n\t"
	"(p6) mov.i ar.pfs = r25		\n\t"
	"cmp.ne p9, p0 = r0, r29		\n\t"
	";;					\n\t"
	"(p9) ld8 r29 = [r29]			\n\t"
	"cmp.ne p6, p0 = r0, r30		\n\t"
	";;					\n\t"
	"(p6) ld8 r30 = [r30]			\n\t"
	/* Don't clobber p6-p9, which are in use at present.  */
	"mov pr = r28, ~0x3c0			\n\t"
	"(p9) mov.i ar.lc = r29			\n\t"
	";;					\n\t"
	"mov.m r25 = ar.rsc			\n\t"
	"(p6) mov.i ar.fpsr = r30		\n\t"
	";;					\n\t"
	"and r25 = 0x1c, r25			\n\t"
	"mov b0 = r26				\n\t"
	";;					\n\t"
	"mov.m ar.rsc = r25			\n\t"
	";;					\n\t"
	/* This must be done before setting AR.BSPSTORE, otherwise 
	   AR.BSP will be initialized with a random displacement
	   below the value we want, based on the current number of
	   dirty stacked registers.  */
	"loadrs					\n\t"
	"invala					\n\t"
	";;					\n\t"
	"mov.m ar.bspstore = r23		\n\t"
	";;					\n\t"
	"or r25 = 0x3, r25			\n\t"
	"mov.m ar.rnat = r22			\n\t"
	";;					\n\t"
	"mov.m ar.rsc = r25			\n\t"
	"mov sp = r24				\n\t"
	"br.ret.sptk.few b0"
	: : "r"(target), "r"(ireg_buf), "r"(ireg_nat), "r"(ireg_pr)
	: "r15", "r16", "r17", "r18", "r20", "r21", "r22",
	  "r23", "r24", "r25", "r26", "r27", "r28", "r29",
	  "r30", "r31");
  /* NOTREACHED */
  while (1);
}

static inline _Unwind_Ptr
uw_identify_context (struct _Unwind_Context *context)
{
  return _Unwind_GetIP (context);
}

#include "unwind.inc"
#endif
