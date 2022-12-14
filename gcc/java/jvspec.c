 /* Specific flags and argument handling of the front-end of the 
   GNU compiler for the Java(TM) language.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001 Free Software Foundation, Inc.

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

#include "config.h"
#include "system.h"
#include "gcc.h"

/* Name of spec file.  */
#define SPEC_FILE "libgcj.spec"

/* This bit is set if we saw a `-xfoo' language specification.  */
#define LANGSPEC	(1<<1)
/* True if this arg is a parameter to the previous option-taking arg. */
#define PARAM_ARG	(1<<2)
/* True if this arg is a .java input file name. */
#define JAVA_FILE_ARG	(1<<3)
/* True if this arg is a .class input file name. */
#define CLASS_FILE_ARG	(1<<4)
/* True if this arg is a .zip or .jar input file name. */
#define ZIP_FILE_ARG	(1<<5)
/* True if this arg is @FILE - where FILE contains a list of filenames. */
#define INDIRECT_FILE_ARG (1<<6)

static char *find_spec_file	PARAMS ((const char *));

static const char *main_class_name = NULL;
int lang_specific_extra_outfiles = 0;

/* True if we should add -shared-libgcc to the command-line.  */
int shared_libgcc = 1;

const char jvgenmain_spec[] =
  "jvgenmain %{D*} %i %{!pipe:%umain.i} |\n\
   cc1 %{!pipe:%Umain.i} %1 \
		   %{!Q:-quiet} -dumpbase %b.c %{d*} %{m*} %{a*}\
		   %{g*} %{O*} \
		   %{v:-version} %{pg:-p} %{p}\
		   %{<fbounds-check} %{<fno-bounds-check}\
		   %{<fassume-compiled} %{<fno-assume-compiled}\
		   %{<femit-class-file} %{<femit-class-files} %{<fencoding*}\
		   %{<fuse-boehm-gc} %{<fhash-synchronization} %{<fjni}\
		   %{<fclasspath*} %{<fCLASSPATH*} %{<foutput-class-dir}\
		   %{<fuse-divide-subroutine} %{<fno-use-divide-subroutine}\
		   %{<ffilelist-file}\
		   %{f*} -fdollars-in-identifiers\
		   %{aux-info*}\
		   %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
		   %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%Umain.s}} |\n\
              %{!S:as %a %Y -o %d%w%umain%O %{!pipe:%Umain.s} %A\n }";

/* Return full path name of spec file if it is in DIR, or NULL if
   not.  */
static char *
find_spec_file (dir)
     const char *dir;
{
  char *spec;
  int x;
  struct stat sb;

  spec = (char *) xmalloc (strlen (dir) + sizeof (SPEC_FILE)
			   + sizeof ("-specs=") + 4);
  strcpy (spec, "-specs=");
  x = strlen (spec);
  strcat (spec, dir);
  strcat (spec, "/");
  strcat (spec, SPEC_FILE);
  if (! stat (spec + x, &sb))
    return spec;
  free (spec);
  return NULL;
}

void
lang_specific_driver (in_argc, in_argv, in_added_libraries)
     int *in_argc;
     const char *const **in_argv;
     int *in_added_libraries;
{
  int i, j;

  /* If non-zero, the user gave us the `-v' flag.  */ 
  int saw_verbose_flag = 0;

  int saw_save_temps = 0;

  /* This will be 0 if we encounter a situation where we should not
     link in libgcj.  */
  int library = 1;

  /* This will be 1 if multiple input files (.class and/or .java)
     should be passed to a single jc1 invocation. */
  int combine_inputs = 0;

  /* Index of last .java or .class argument. */
  int last_input_index;

  /* Number of .java and .class source file arguments seen. */
  int java_files_count = 0;
  int class_files_count = 0;
  /* Number of .zip or .jar file arguments seen. */
  int zip_files_count = 0;
  /* Number of '@FILES' arguments seen. */
  int indirect_files_count = 0;

  /* Name of file containing list of files to compile. */
  char *filelist_filename = 0;

  FILE *filelist_file = 0;

  /* The number of arguments being added to what's in argv, other than
     libraries.  */
  int added = 2;

  /* Used to track options that take arguments, so we don't go wrapping
     those with -xc++/-xnone.  */
  const char *quote = NULL;

  /* The new argument list will be contained in this.  */
  const char **arglist;

  /* Non-zero if we saw a `-xfoo' language specification on the
     command line.  Used to avoid adding our own -xc++ if the user
     already gave a language for the file.  */
  int saw_speclang = 0;

#if 0
  /* "-lm" or "-lmath" if it appears on the command line.  */
  const char *saw_math ATTRIBUTE_UNUSED = 0;

  /* "-lc" if it appears on the command line.  */
  const char *saw_libc ATTRIBUTE_UNUSED = 0;

  /* "-lgcjgc" if it appears on the command line.  */
  const char *saw_gc ATTRIBUTE_UNUSED = 0;

  /* Saw `-l' option for the thread library.  */
  const char *saw_threadlib ATTRIBUTE_UNUSED = 0;

  /* Saw `-lgcj' on command line.  */
  int saw_libgcj ATTRIBUTE_UNUSED = 0;
#endif

  /* Saw -C or -o option, respectively. */
  int saw_C = 0;
  int saw_o = 0;

  /* Saw some -O* or -g* option, respectively. */
  int saw_O = 0;
  int saw_g = 0;

  /* Saw a `-D' option.  */
  int saw_D = 0;

  /* An array used to flag each argument that needs a bit set for
     LANGSPEC, MATHLIB, WITHLIBC, or GCLIB.  */
  int *args;

  /* The total number of arguments with the new stuff.  */
  int argc;

  /* The argument list.  */
  const char *const *argv;

  /* The number of libraries added in.  */
  int added_libraries;

  /* The total number of arguments with the new stuff.  */
  int num_args = 1;

  /* Non-zero if linking is supposed to happen.  */
  int will_link = 1;

  /* Non-zero if we want to find the spec file.  */
  int want_spec_file = 1;

  /* The argument we use to specify the spec file.  */
  char *spec_file = NULL;

  argc = *in_argc;
  argv = *in_argv;
  added_libraries = *in_added_libraries;

  args = (int *) xcalloc (argc, sizeof (int));

  for (i = 1; i < argc; i++)
    {
      /* If the previous option took an argument, we swallow it here.  */
      if (quote)
	{
	  quote = NULL;
	  args[i] |= PARAM_ARG;
	  continue;
	}

      /* We don't do this anymore, since we don't get them with minus
	 signs on them.  */
      if (argv[i][0] == '\0' || argv[i][1] == '\0')
	continue;

      if (argv[i][0] == '-')
	{
	  if (library != 0 && (strcmp (argv[i], "-nostdlib") == 0
			       || strcmp (argv[i], "-nodefaultlibs") == 0))
	    {
	      library = 0;
	    }
	  else if (strncmp (argv[i], "-fmain=", 7) == 0)
	    {
	      main_class_name = argv[i] + 7;
	      added--;
	    }
	  else if (strcmp (argv[i], "-fhelp") == 0)
	    want_spec_file = 0;
	  else if (strcmp (argv[i], "-v") == 0)
	    {
	      saw_verbose_flag = 1;
	      if (argc == 2)
		{
		  /* If they only gave us `-v', don't try to link
		     in libgcj.  */ 
		  library = 0;
		}
	    }
	  else if (strncmp (argv[i], "-x", 2) == 0)
	    saw_speclang = 1;
	  else if (strcmp (argv[i], "-C") == 0)
	    {
	      saw_C = 1;
	      want_spec_file = 0;
	      if (library != 0)
		added -= 2;
	      library = 0;
	      will_link = 0;
	    }
	  else if (argv[i][1] == 'D')
	    saw_D = 1;
	  else if (argv[i][1] == 'g')
	    saw_g = 1;
	  else if (argv[i][1] == 'O')
	    saw_O = 1;
	  else if ((argv[i][2] == '\0'
		    && (char *)strchr ("bBVDUoeTuIYmLiA", argv[i][1]) != NULL)
		   || strcmp (argv[i], "-Tdata") == 0
		   || strcmp (argv[i], "-MT") == 0
		   || strcmp (argv[i], "-MF") == 0)
	    {
	      if (strcmp (argv[i], "-o") == 0)
		saw_o = 1;
	      quote = argv[i];
	    }
	  else if (strcmp(argv[i], "-classpath") == 0
		   || strcmp(argv[i], "-CLASSPATH") == 0)
	    {
	      quote = argv[i];
	      added -= 1;
	    }
	  else if (library != 0 
		   && ((argv[i][2] == '\0'
			&& (char *) strchr ("cSEM", argv[i][1]) != NULL)
		       || strcmp (argv[i], "-MM") == 0))
	    {
	      /* Don't specify libraries if we won't link, since that would
		 cause a warning.  */
	      library = 0;
	      added -= 2;

	      /* Remember this so we can confirm -fmain option.  */
	      will_link = 0;
	    }
	  else if (strcmp (argv[i], "-d") == 0)
	    {
	      /* `-d' option is for javac compatibility.  */
	      quote = argv[i];
	      added -= 1;
	    }
	  else if (strcmp (argv[i], "-fsyntax-only") == 0
		   || strcmp (argv[i], "--syntax-only") == 0)
	    {
	      want_spec_file = 0;
	      library = 0;
	      will_link = 0;
	      continue;
	    }
          else if (strcmp (argv[i], "-save-temps") == 0)
	    saw_save_temps = 1;
          else if (strcmp (argv[i], "-static-libgcc") == 0
                   || strcmp (argv[i], "-static") == 0)
	    shared_libgcc = 0;
	  else
	    /* Pass other options through.  */
	    continue;
	}
      else
	{
	  int len; 

	  if (saw_speclang)
	    {
	      saw_speclang = 0;
	      continue;
	    }

	  if (argv[i][0] == '@')
	    {
	      args[i] |= INDIRECT_FILE_ARG;
	      indirect_files_count++;
	      added += 2;  /* for -xjava and -xnone */
	    }

	  len = strlen (argv[i]);
	  if (len > 5 && strcmp (argv[i] + len - 5, ".java") == 0)
	    {
	      args[i] |= JAVA_FILE_ARG;
	      java_files_count++;
	      last_input_index = i;
	    }
	  if (len > 6 && strcmp (argv[i] + len - 6, ".class") == 0)
	    {
	      args[i] |= CLASS_FILE_ARG;
	      class_files_count++;
	      last_input_index = i;
	    }
	  if (len > 4
	      && (strcmp (argv[i] + len - 4, ".zip") == 0
		  || strcmp (argv[i] + len - 4, ".jar") == 0))
	    {
	      args[i] |= ZIP_FILE_ARG;
	      zip_files_count++;
	      last_input_index = i;
	    }
	}
    }

  if (quote)
    fatal ("argument to `%s' missing\n", quote);

  if (saw_D && ! main_class_name)
    fatal ("can't specify `-D' without `--main'\n");

  num_args = argc + added;
  if (saw_C)
    {
      num_args += 3;
      if (class_files_count + zip_files_count > 0)
	{
	  error ("Warning: already-compiled .class files ignored with -C"); 
	  num_args -= class_files_count + zip_files_count;
	  class_files_count = 0;
	  zip_files_count = 0;
	}
      num_args += 2;  /* For -o NONE. */
      if (saw_o)
	fatal ("cannot specify both -C and -o");
    }
  if ((saw_o && java_files_count + class_files_count + zip_files_count > 1)
      || (saw_C && java_files_count > 1)
      || (indirect_files_count > 0
	  && java_files_count + class_files_count + zip_files_count > 0))
    combine_inputs = 1;

  if (combine_inputs)
    {
      filelist_filename = make_temp_file ("jx");
      if (filelist_filename == NULL)
	fatal ("cannot create temporary file");
      record_temp_file (filelist_filename, ! saw_save_temps, 0);
      filelist_file = fopen (filelist_filename, "w");
      if (filelist_file == NULL)
	pfatal_with_name (filelist_filename);
      num_args -= java_files_count + class_files_count + zip_files_count;
      num_args += 2;  /* for the combined arg and "-xjava" */
    }
  /* If we know we don't have to do anything, bail now.  */
#if 0
  if (! added && ! library && main_class_name == NULL && ! saw_C)
    {
      free (args);
      return;
    }
#endif

  if (main_class_name)
    {
      lang_specific_extra_outfiles++;
    }
  if (saw_g + saw_O == 0)
    num_args++;
  num_args++;

  if (combine_inputs || indirect_files_count > 0)
    num_args += 1; /* for "-ffilelist-file" */
  if (combine_inputs && indirect_files_count > 0)
    fatal("using both @FILE with multiple files not implemented");

  /* There's no point adding -shared-libgcc if we don't have a shared
     libgcc.  */
#ifndef ENABLE_SHARED_LIBGCC
  shared_libgcc = 0;
#endif  
  
  num_args += shared_libgcc;

  arglist = (const char **) xmalloc ((num_args + 1) * sizeof (char *));
  j = 0;

  for (i = 0; i < argc; i++, j++)
    {
      arglist[j] = argv[i];

      if ((args[i] & PARAM_ARG) || i == 0)
	continue;

      if (strcmp (argv[i], "-classpath") == 0
	  || strcmp (argv[i], "-CLASSPATH") == 0)
	{
	  arglist[j] = concat ("-f", argv[i]+1, "=", argv[i+1], NULL);
	  i++;
	  continue;
	}

      if (strcmp (argv[i], "-d") == 0)
	{
	  arglist[j] = concat ("-foutput-class-dir=", argv[i + 1], NULL);
	  ++i;
	  continue;
	}

      if (spec_file == NULL && strncmp (argv[i], "-L", 2) == 0)
	spec_file = find_spec_file (argv[i] + 2);

      if (strncmp (argv[i], "-fmain=", 7) == 0)
	{
	  if (! will_link)
	    fatal ("cannot specify `main' class when not linking");
	  --j;
	  continue;
	}

      if ((args[i] & INDIRECT_FILE_ARG) != 0)
	{
	  arglist[j++] = "-xjava";
	  arglist[j++] = argv[i]+1;  /* Drop '@'. */
	  arglist[j] = "-xnone";
	}

      if ((args[i] & (CLASS_FILE_ARG|ZIP_FILE_ARG)) && saw_C)
	{
	  --j;
	  continue;
	}

      if (combine_inputs
	  && (args[i] & (CLASS_FILE_ARG|JAVA_FILE_ARG|ZIP_FILE_ARG)) != 0)
	{
	  fputs (argv[i], filelist_file);
	  fputc ('\n', filelist_file);
	  --j;
	  continue;
	}
  }

  if (combine_inputs || indirect_files_count > 0)
    arglist[j++] = "-ffilelist-file";

  if (combine_inputs)
    {
      if (fclose (filelist_file))
	pfatal_with_name (filelist_filename);
      arglist[j++] = "-xjava";
      arglist[j++] = filelist_filename;
    }

  /* If we saw no -O or -g option, default to -g1, for javac compatibility. */
  if (saw_g + saw_O == 0)
    arglist[j++] = "-g1";

  /* Read the specs file corresponding to libgcj.
     If we didn't find the spec file on the -L path, then we hope it
     is somewhere in the standard install areas.  */
  if (want_spec_file)
    arglist[j++] = spec_file == NULL ? "-specs=libgcj.spec" : spec_file;

  if (saw_C)
    {
      arglist[j++] = "-fsyntax-only";
      arglist[j++] = "-femit-class-files";
      arglist[j++] = "-S";
      arglist[j++] = "-o";
      arglist[j++] = "NONE";
    }
  
  if (shared_libgcc)
    arglist[j++] = "-shared-libgcc";

  arglist[j] = NULL;

  *in_argc = j;
  *in_argv = arglist;
  *in_added_libraries = added_libraries;
}

int
lang_specific_pre_link ()
{
  int err;
  if (main_class_name == NULL)
    return 0;
  input_filename = main_class_name;
  input_filename_length = strlen (main_class_name);
  err = do_spec (jvgenmain_spec);
  if (err == 0)
    {
      /* Shift the outfiles array so the generated main comes first.
	 This is important when linking against (non-shared) libraries,
	 since otherwise we risk (a) nothing getting linked or
	 (b) 'main' getting picked up from a library. */
      int i = n_infiles;
      const char *generated = outfiles[i];
      while (--i >= 0)
	outfiles[i + 1] = outfiles[i];
      outfiles[0] = generated;
    }
  return err;
}
