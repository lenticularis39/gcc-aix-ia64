/* Various diagnostic subroutines for the GNU C language.
   Copyright (C) 2000 Free Software Foundation, Inc.
   Contributed by Gabriel Dos Reis <gdr@codesourcery.com>

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

#include "config.h"
#include "system.h"
#include "tree.h"
#include "c-tree.h"
#include "tm_p.h"
#include "flags.h"
#include "diagnostic.h"

/* Issue an ISO C99 pedantic warning MSGID.  */

void
pedwarn_c99 VPARAMS ((const char *msgid, ...))
{
#ifndef ANSI_PROTOTYPES
  const char *msgid;
#endif
  va_list ap;
  diagnostic_context dc;

  VA_START (ap, msgid);

#ifndef ANSI_PROTOTYPES
  msgid = va_arg (ap, const char *);
#endif

  set_diagnostic_context (&dc, msgid, &ap, input_filename, lineno,
                          !flag_isoc99 || !flag_pedantic_errors);
  report_diagnostic (&dc);
  va_end (ap);
}
