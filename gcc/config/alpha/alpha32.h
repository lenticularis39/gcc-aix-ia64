/* Definitions of target machine for GNU compiler, for DEC Alpha
   running Windows/NT.
   Copyright (C) 1995, 1996, 1998, 1999 Free Software Foundation, Inc.

   Derived from code
      Contributed by Richard Kenner (kenner@vlsi1.ultra.nyu.edu)

   Donn Terry, Softway Systems, Inc.

   This file contains the code-generation stuff common to the 32-bit
   versions of the DEC/Compaq Alpha architecture.  It is shared by
   Interix and NT/Win32 ports.   It should not contain compile-time
   or run-time dependent environment values (such as compiler options
   or anything containing a file or pathname.)

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

#undef TARGET_WINDOWS_NT
#define TARGET_WINDOWS_NT 1

/* WinNT (and thus Interix) use unsigned int */
#define SIZE_TYPE "unsigned int"

/* Pointer is 32 bits but the hardware has 64-bit addresses, sign extended. */
#undef POINTER_SIZE
#define POINTER_SIZE 32
#define POINTERS_EXTEND_UNSIGNED 0

/* We don't change Pmode to the "obvious" SI mode... the above appears
   to affect the in-memory size; we want the registers to stay DImode
   to match the md file */

/* "long" is 32 bits.  */
#undef LONG_TYPE_SIZE
#define LONG_TYPE_SIZE 32


/* Output assembler code for a block containing the constant parts
   of a trampoline, leaving space for the variable parts.

   The trampoline should set the static chain pointer to value placed
   into the trampoline and should branch to the specified routine.  */

#undef TRAMPOLINE_TEMPLATE
#define TRAMPOLINE_TEMPLATE(FILE)			\
{							\
  fprintf (FILE, "\tbr $27,$LTRAMPP\n");		\
  fprintf (FILE, "$LTRAMPP:\n\tldl $1,12($27)\n");	\
  fprintf (FILE, "\tldl $27,16($27)\n");		\
  fprintf (FILE, "\tjmp $31,($27),0\n");		\
  fprintf (FILE, "\t.long 0,0\n");			\
}

/* Length in units of the trampoline for entering a nested function.  */

#undef TRAMPOLINE_SIZE
#define TRAMPOLINE_SIZE    24

/* Emit RTL insns to initialize the variable parts of a trampoline.
   FNADDR is an RTX for the address of the function's pure code.
   CXT is an RTX for the static chain value for the function.   */

#undef INITIALIZE_TRAMPOLINE
#define INITIALIZE_TRAMPOLINE(TRAMP, FNADDR, CXT) \
  alpha_initialize_trampoline (TRAMP, FNADDR, CXT, 20, 16, 12)

/* Output code to add DELTA to the first argument, and then jump to FUNCTION.
   Used for C++ multiple inheritance.  */
/* ??? This is only used with the v2 ABI, and alpha.c makes assumptions
   about current_function_is_thunk that are not valid with the v3 ABI.  */
#if 0
#undef ASM_OUTPUT_MI_THUNK
#define ASM_OUTPUT_MI_THUNK(FILE, THUNK_FNDECL, DELTA, FUNCTION)	\
do {									\
  const char *op, *fn_name = XSTR (XEXP (DECL_RTL (FUNCTION), 0), 0);	\
  int reg;								\
									\
  /* Mark end of prologue.  */						\
  output_end_prologue (FILE);						\
									\
  /* Rely on the assembler to macro expand a large delta.  */		\
  reg = aggregate_value_p (TREE_TYPE (TREE_TYPE (FUNCTION))) ? 17 : 16; \
  fprintf (FILE, "\tlda $%d,%ld($%d)\n", reg, (long)(DELTA), reg);      \
									\
  op = "jsr";								\
  if (current_file_function_operand (XEXP (DECL_RTL (FUNCTION), 0)))	\
    op = "br";								\
  fprintf (FILE, "\t%s $31,", op);					\
  assemble_name (FILE, fn_name);					\
  fputc ('\n', FILE);							\
} while (0)
#endif
