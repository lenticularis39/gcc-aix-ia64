/* Program to generate "main" a Java(TM) class containing a main method.
   Copyright (C) 1998, 1999, 2000, 2001 Free Software Foundation, Inc.

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
Boston, MA 02111-1307, USA. 

Java and all Java-based marks are trademarks or registered trademarks
of Sun Microsystems, Inc. in the United States and other countries.
The Free Software Foundation is independent of Sun Microsystems, Inc.  */

/* Written by Per Bothner <bothner@cygnus.com> */

#include "config.h"
#include "system.h"
#include "obstack.h"
#include "gansidecl.h"
#include "jcf.h"
#include "tree.h"
#include "java-tree.h"

static char * do_mangle_classname PARAMS ((const char *string));

struct obstack  name_obstack;
struct obstack *mangle_obstack = &name_obstack;

void
gcc_obstack_init (obstack)
     struct obstack *obstack;
{
  /* Let particular systems override the size of a chunk.  */
#ifndef OBSTACK_CHUNK_SIZE
#define OBSTACK_CHUNK_SIZE 0
#endif
  /* Let them override the alloc and free routines too.  */
#ifndef OBSTACK_CHUNK_ALLOC
#define OBSTACK_CHUNK_ALLOC xmalloc
#endif
#ifndef OBSTACK_CHUNK_FREE
#define OBSTACK_CHUNK_FREE free
#endif
  _obstack_begin (obstack, OBSTACK_CHUNK_SIZE, 0,
		  (void *(*) PARAMS ((long))) OBSTACK_CHUNK_ALLOC,
		  (void (*) PARAMS ((void *))) OBSTACK_CHUNK_FREE);
}

static void usage (const char *) ATTRIBUTE_NORETURN;

static void
usage (const char *name)
{
  fprintf (stderr, "Usage: %s [OPTIONS]... CLASSNAME [OUTFILE]\n", name);
  exit (1);
}

int
main (int argc, const char **argv)
{
  const char *classname;
  FILE *stream;
  const char *mangled_classname;
  int i, last_arg;

  if (argc < 2)
    usage (argv[0]);

  for (i = 1; i < argc; ++i)
    {
      if (! strncmp (argv[i], "-D", 2))
	{
	  /* Handled later.  */
	}
      else
	break;
    }

  if (i < argc - 2 || i == argc)
    usage (argv[0]);
  last_arg = i;

  classname = argv[i];

  gcc_obstack_init (mangle_obstack);
  mangled_classname = do_mangle_classname (classname);

  if (i < argc - 1 && strcmp (argv[i + 1], "-") != 0)
    {
      const char *outfile = argv[i + 1];
      stream = fopen (outfile, "w");
      if (stream == NULL)
	{
	  fprintf (stderr, "%s: Cannot open output file: %s\n",
		   argv[0], outfile);
	  exit (1);
	}
    }
  else
    stream = stdout;

  /* At this point every element of ARGV from 1 to LAST_ARG is a `-D'
     option.  Process them appropriately.  */
  fprintf (stream, "extern const char **_Jv_Compiler_Properties;\n");
  fprintf (stream, "static const char *props[] =\n{\n");
  for (i = 1; i < last_arg; ++i)
    {
      const char *p;
      fprintf (stream, "  \"");
      for (p = &argv[i][2]; *p; ++p)
	{
	  if (! ISPRINT (*p))
	    fprintf (stream, "\\%o", *p);
	  else if (*p == '\\' || *p == '"')
	    fprintf (stream, "\\%c", *p);
	  else
	    putc (*p, stream);
	}
      fprintf (stream, "\",\n");
    }
  fprintf (stream, "  0\n};\n\n");

  fprintf (stream, "extern int %s;\n", mangled_classname);
  fprintf (stream, "int main (int argc, const char **argv)\n");
  fprintf (stream, "{\n");
  fprintf (stream, "   _Jv_Compiler_Properties = props;\n");
  fprintf (stream, "   JvRunMain (&%s, argc, argv);\n", mangled_classname);
  fprintf (stream, "}\n");
  if (stream != stdout && fclose (stream) != 0)
    {
      fprintf (stderr, "%s: Failed to close output file %s\n",
	       argv[0], argv[2]);
      exit (1);
    }
  return 0;
}


static char *
do_mangle_classname (string)
     const char *string;
{
  const char *ptr;
  int count = 0;

  obstack_grow (&name_obstack, "_ZN", 3);

  for (ptr = string; *ptr; ptr++ )
    {
      if (ptr[0] == '.')
	{
	  append_gpp_mangled_name (&ptr [-count], count);
	  count = 0;
	}
      else
	count++;
    }
  append_gpp_mangled_name (&ptr [-count], count);
  obstack_grow (mangle_obstack, "6class$E", 8);
  obstack_1grow (mangle_obstack, '\0');
  return obstack_finish (mangle_obstack);
}
