/* Functions related to mangling class names for the GNU compiler
   for the Java(TM) language.
   Copyright (C) 1998, 1999, 2001 Free Software Foundation, Inc.

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
#include "jcf.h"
#include "tree.h"
#include "java-tree.h"
#include "obstack.h"
#include "toplev.h"
#include "obstack.h"
#include "ggc.h"

static void mangle_field_decl PARAMS ((tree));
static void mangle_method_decl PARAMS ((tree));

static void mangle_type PARAMS ((tree));
static void mangle_pointer_type PARAMS ((tree));
static void mangle_array_type PARAMS ((tree));
static int  mangle_record_type PARAMS ((tree, int));

static int find_compression_pointer_match PARAMS ((tree));
static int find_compression_array_match PARAMS ((tree));
static int find_compression_record_match PARAMS ((tree, tree *));
static int find_compression_array_template_match PARAMS ((tree));

static void set_type_package_list PARAMS ((tree));
static int  entry_match_pointer_p PARAMS ((tree, int));
static void emit_compression_string PARAMS ((int));

static void init_mangling PARAMS ((struct obstack *));
static tree finish_mangling PARAMS ((void));
static void compression_table_add PARAMS ((tree));

static void mangle_member_name PARAMS ((tree));

/* We use an incoming obstack, always to be provided to the interface
   functions. */
struct obstack *mangle_obstack;
#define MANGLE_RAW_STRING(S) \
  obstack_grow (mangle_obstack, (S), sizeof (S)-1)

/* This is the mangling interface: a decl, a class field (.class) and
   the vtable. */

tree
java_mangle_decl (obstack, decl)
     struct obstack *obstack;
     tree decl;
{
  init_mangling (obstack);
  switch (TREE_CODE (decl))
    {
    case VAR_DECL:
      mangle_field_decl (decl);
      break;
    case FUNCTION_DECL:
      mangle_method_decl (decl);
      break;
    default:
      internal_error ("Can't mangle %s", tree_code_name [TREE_CODE (decl)]);
    }
  return finish_mangling ();
}

tree 
java_mangle_class_field (obstack, type)
     struct obstack *obstack;
     tree type;
{
  init_mangling (obstack);
  mangle_record_type (type, /* for_pointer = */ 0);
  MANGLE_RAW_STRING ("6class$");
  obstack_1grow (mangle_obstack, 'E');
  return finish_mangling ();
}

tree
java_mangle_vtable (obstack, type)
     struct obstack *obstack;
     tree type;
{
  init_mangling (obstack);
  MANGLE_RAW_STRING ("TV");
  mangle_record_type (type, /* for_pointer = */ 0);
  obstack_1grow (mangle_obstack, 'E');
  return finish_mangling ();
}

/* Beginning of the helper functions */

/* This mangles a field decl */

static void
mangle_field_decl (decl)
     tree decl;
{
  /* Mangle the name of the this the field belongs to */
  mangle_record_type (DECL_CONTEXT (decl), /* for_pointer = */ 0);
  
  /* Mangle the name of the field */
  mangle_member_name (DECL_NAME (decl));

  /* Terminate the mangled name */
  obstack_1grow (mangle_obstack, 'E');
}

/* This mangles a method decl, first mangling its name and then all
   its arguments. */

static void
mangle_method_decl (mdecl)
     tree mdecl;
{
  tree method_name = DECL_NAME (mdecl);
  tree arglist;

  /* Mangle the name of the type that contains mdecl */
  mangle_record_type (DECL_CONTEXT (mdecl), /* for_pointer = */ 0);

  /* Mangle the function name. There three cases
       - mdecl is java.lang.Object.Object(), use `C2' for its name
         (denotes a base object constructor.)
       - mdecl is a constructor, use `C1' for its name, (denotes a
         complete object constructor.)
       - mdecl is not a constructor, standard mangling is performed.
     We terminate the mangled function name with a `E'. */
  if (ID_INIT_P (method_name))
    {
      if (DECL_CONTEXT (mdecl) == object_type_node)
	obstack_grow (mangle_obstack, "C2", 2);
      else
	obstack_grow (mangle_obstack, "C1", 2);
    }
  else
    mangle_member_name (method_name);
  obstack_1grow (mangle_obstack, 'E');

  /* We mangled type.methodName. Now onto the arguments. */
  arglist = TYPE_ARG_TYPES (TREE_TYPE (mdecl));
  if (TREE_CODE (TREE_TYPE (mdecl)) == METHOD_TYPE)
    arglist = TREE_CHAIN (arglist);
  
  /* No arguments is easy. We shortcut it. */
  if (arglist == end_params_node)
    obstack_1grow (mangle_obstack, 'v');
  else
    {
      tree arg;
      for (arg = arglist; arg != end_params_node;  arg = TREE_CHAIN (arg))
	mangle_type (TREE_VALUE (arg));
    }
}

/* This mangles a member name, like a function name or a field
   name. Handle cases were `name' is a C++ keyword.  Return a non zero
   value if unicode encoding was required.  */

static void
mangle_member_name (name)
     tree name;
{
  append_gpp_mangled_name (IDENTIFIER_POINTER (name),
			   IDENTIFIER_LENGTH (name));

  /* If NAME happens to be a C++ keyword, add `$' or `.' or `_'. */
  if (cxx_keyword_p (IDENTIFIER_POINTER (name), IDENTIFIER_LENGTH (name)))
    {
#ifndef NO_DOLLAR_IN_LABEL
      obstack_1grow (mangle_obstack, '$');
#else  /* NO_DOLLAR_IN_LABEL */
#ifndef NO_DOT_IN_LABEL
      obstack_1grow (mangle_obstack, '.');
#else  /* NO_DOT_IN_LABEL */
      obstack_1grow (mangle_obstack, '_');
#endif /* NO_DOT_IN_LABEL */
#endif /* NO_DOLLAR_IN_LABEL */
    }
}

/* Append the mangled name of TYPE onto OBSTACK.  */

static void
mangle_type (type)
     tree type;
{
  switch (TREE_CODE (type))
    {
      char code;
    case BOOLEAN_TYPE: code = 'b';  goto primitive;
    case CHAR_TYPE:    code = 'w';  goto primitive;
    case VOID_TYPE:    code = 'v';  goto primitive;
    case INTEGER_TYPE:
      /* Get the original type instead of the arguments promoted type.
	 Avoid symbol name clashes. Should call a function to do that.
	 FIXME.  */
      if (type == promoted_short_type_node)
	type = short_type_node;
      if (type == promoted_byte_type_node)
        type = byte_type_node;
      switch (TYPE_PRECISION (type))
	{
	case  8:       code = 'c';  goto primitive;
	case 16:       code = 's';  goto primitive;
	case 32:       code = 'i';  goto primitive;
	case 64:       code = 'x';  goto primitive;
	default:  goto bad_type;
	}
    primitive:
      obstack_1grow (mangle_obstack, code);
      break;

    case REAL_TYPE:
      switch (TYPE_PRECISION (type))
	{
	case 32:       code = 'f';  goto primitive;
	case 64:       code = 'd';  goto primitive;
	default:  goto bad_type;
	}
    case POINTER_TYPE:
      if (TYPE_ARRAY_P (TREE_TYPE (type)))
	mangle_array_type (type);
      else
	mangle_pointer_type (type);
      break;
    bad_type:
    default:
      abort ();
    }
}

/* The compression table is a vector that keeps track of things we've
   already seen, so they can be reused. For example, java.lang.Object
   Would generate three entries: two package names and a type. If
   java.lang.String is presented next, the java.lang will be matched
   against the first two entries (and kept for compression as S_0), and
   type String would be added to the table. See mangle_record_type.
   COMPRESSION_NEXT is the index to the location of the next insertion
   of an element.  */

static tree compression_table;
static int  compression_next;

/* Find a POINTER_TYPE in the compression table. Use a special
   function to match pointer entries and start from the end */

static int
find_compression_pointer_match (type)
     tree type;
{
  int i;

  for (i = compression_next-1; i >= 0; i--)
    if (entry_match_pointer_p (type, i))
      return i;
  return -1;
}

/* Already recorder arrays are handled like pointer as they're always
   associated with it.  */

static int
find_compression_array_match (type)
     tree type;
{
  return find_compression_pointer_match (type);
}

/* Match the table of type against STRING.  */

static int
find_compression_array_template_match (string)
     tree string;
{
  int i;
  for (i = 0; i < compression_next; i++)
    if (TREE_VEC_ELT (compression_table, i) == string) 
      return i;
  return -1;
}

/* We go through the compression table and try to find a complete or
   partial match. The function returns the compression table entry
   that (evenutally partially) matches TYPE. *NEXT_CURRENT can be set
   to the rest of TYPE to be mangled. */

static int
find_compression_record_match (type, next_current)
     tree type;
     tree *next_current;
{
  int i, match;
  tree current, saved_current = NULL_TREE;

  /* Search from the beginning for something that matches TYPE, even
     partially. */
  for (current = TYPE_PACKAGE_LIST (type), i = 0, match = -1; current;
       current = TREE_CHAIN (current))
    {
      int j;
      for (j = i; j < compression_next; j++)
	if (TREE_VEC_ELT (compression_table, j) == TREE_PURPOSE (current))
	  {
	    match = i = j;
	    saved_current = current;
	    break;
	  }
    }

  if (!next_current)
    return match;

  /* If we have a match, set next_current to the item next to the last
     matched value. */
  if (match >= 0)
    *next_current = TREE_CHAIN (saved_current);
  /* We had no match: we'll have to start from the beginning. */
  if (match < 0)
    *next_current = TYPE_PACKAGE_LIST (type);

  return match;
}

/* Mangle a record type. If a non zero value is returned, it means
   that a 'N' was emitted (so that a matching 'E' can be emitted if
   necessary.)  FOR_POINTER indicates that this element is for a pointer
   symbol, meaning it was preceded by a 'P'. */

static int
mangle_record_type (type, for_pointer)
     tree type;
     int for_pointer;
{
  tree current;
  int match;
  int nadded_p = 0;
  int qualified;
  
  /* Does this name have a package qualifier? */
  qualified = QUALIFIED_P (DECL_NAME (TYPE_NAME (type)));

#define ADD_N() \
  do { obstack_1grow (mangle_obstack, 'N'); nadded_p = 1; } while (0)

  if (TREE_CODE (type) != RECORD_TYPE)
    abort ();

  if (!TYPE_PACKAGE_LIST (type))
    set_type_package_list (type);

  match = find_compression_record_match (type, &current);
  if (match >= 0)
    {
      /* If we had a pointer, and there's more, we need to emit
	 'N' after 'P' (for_pointer tells us we already emitted it.) */
      if (for_pointer && current)
	ADD_N();
      emit_compression_string (match);
    }
  while (current)
    {
      /* Add the new type to the table */
      compression_table_add (TREE_PURPOSE (current));
      /* Add 'N' if we never got a chance to, but only if we have a qualified
         name.  For non-pointer elements, the name is always qualified. */
      if ((qualified || !for_pointer) && !nadded_p)
	ADD_N();
      /* Use the bare type name for the mangle. */
      append_gpp_mangled_name (IDENTIFIER_POINTER (TREE_VALUE (current)),
			       IDENTIFIER_LENGTH (TREE_VALUE (current)));
      current = TREE_CHAIN (current);
    }
  return nadded_p;
#undef ADD_N
}

/* Mangle a pointer type. There are two cases: the pointer is already
   in the compression table: the compression is emited sans 'P'
   indicator. Otherwise, a 'P' is emitted and, depending on the type,
   a partial compression or/plus the rest of the mangling. */

static void
mangle_pointer_type (type)
     tree type;
{
  int match;
  tree pointer_type;

  /* Search for the type already in the compression table */
  if ((match = find_compression_pointer_match (type)) >= 0) 
    {
      emit_compression_string (match);
      return;
    }
  
  /* This didn't work. We start by mangling the pointed-to type */
  pointer_type = type;
  type = TREE_TYPE (type);
  if (TREE_CODE (type) != RECORD_TYPE)
    abort ();
  
  obstack_1grow (mangle_obstack, 'P');
  if (mangle_record_type (type, /* for_pointer = */ 1))
    obstack_1grow (mangle_obstack, 'E');

  /* Don't forget to insert the pointer type in the table */
  compression_table_add (pointer_type);
}

/* Mangle an array type. Search for an easy solution first, then go
   through the process of finding out whether the bare array type or even
   the template indicator where already used an compress appropriately.
   It handles pointers. */

static void
mangle_array_type (p_type)
     tree p_type;
{
  /* atms: array template mangled string. */
  static tree atms = NULL_TREE;
  tree type, elt_type;
  int match;

  type = TREE_TYPE (p_type);
  if (!type)
    abort ();

  elt_type = TYPE_ARRAY_ELEMENT (type);

  /* We cache a bit of the Jarray <> mangle. */
  if (!atms)
    {
      atms = get_identifier ("6JArray");
      ggc_add_tree_root (&atms, 1);
    }

  /* Maybe we have what we're looking in the compression table. */
  if ((match = find_compression_array_match (p_type)) >= 0)
    {
      emit_compression_string (match);
      return;
    }

  /* We know for a fact that all arrays are pointers */
  obstack_1grow (mangle_obstack, 'P');
  /* Maybe we already have a Jarray<t> somewhere. PSx_ will be enough. */
  if ((match = find_compression_record_match (type, NULL)) > 0)
    {
      emit_compression_string (match);
      return;
    }

  /* Maybe we already have just JArray somewhere */
  if ((match = find_compression_array_template_match (atms)) > 0)
    emit_compression_string (match);
  else
    {
      /* Start the template mangled name */
      obstack_grow (mangle_obstack, 
		    IDENTIFIER_POINTER (atms), IDENTIFIER_LENGTH (atms));
      /* Insert in the compression table */
      compression_table_add (atms);
    } 

  /* Mangle Jarray <elt_type> */
  obstack_1grow (mangle_obstack, 'I');
  mangle_type (elt_type);
  obstack_1grow (mangle_obstack, 'E');

  /* Add `Jarray <elt_type>' and `Jarray <elt_type> *' to the table */
  compression_table_add (type);
  compression_table_add (p_type);
}

/* Write a substition string for entry I. Substitution string starts a
   -1 (encoded S_.) The base is 36, and the code shamlessly taken from
   cp/mangle.c.  */

static void
emit_compression_string (int i)
{
  i -= 1;			/* Adjust */
  obstack_1grow (mangle_obstack, 'S');
  if (i >= 0)
    {
      static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
      unsigned HOST_WIDE_INT n;
      unsigned HOST_WIDE_INT m=1;
      /* How many digits for I in base 36? */
      for (n = i; n >= 36; n /= 36, m *=36);
      /* Write the digits out */
      while (m > 0)
	{
	  int digit = i / m;
	  obstack_1grow (mangle_obstack, digits [digit]);
	  i -= digit * m;
	  m /= 36;
	}
    }
  obstack_1grow (mangle_obstack, '_');
}

/* If search the compression table at index I for a pointer type
   equivalent to TYPE (meaning that after all the indirection, which
   might all be unique, we find the same RECORD_TYPE.) */

static int
entry_match_pointer_p (type, i)
     tree type;
     int i;
{
  tree t = TREE_VEC_ELT (compression_table, i);
  
  while (TREE_CODE (type) == POINTER_TYPE
	 && TREE_CODE (t) == POINTER_TYPE)
    {
      t = TREE_TYPE (t);
      type = TREE_TYPE (type);
    }
  return (TREE_CODE (type) == RECORD_TYPE
	  && TREE_CODE (t) == RECORD_TYPE
	  && t == type);
}

/* Go through all qualification of type and build a list of list node
   elements containings as a purpose what should be used for a match and
   inserted in the compression table; and as it value the raw name of the
   part. The result is stored in TYPE_PACKAGE_LIST to be reused.  */

static void
set_type_package_list (type)
     tree type;
{
  int i;
  const char *type_string = IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type)));
  char *ptr;
  int qualifications;
  tree list = NULL_TREE, elt;

  for (ptr = (char *)type_string, qualifications = 0; *ptr; ptr++)
    if (*ptr == '.')
      qualifications += 1;

  for (ptr = (char *)type_string, i = 0; i < qualifications; ptr++)
    {
      if (ptr [0] == '.')
	{
	  char c;
	  tree identifier;

	  /* Can't use an obstack, we're already using it to
	     accumulate the mangling. */
	  c = ptr [0];
	  ptr [0] = '\0';
	  identifier = get_identifier (type_string);
	  ptr [0] = c;
	  elt = build_tree_list (identifier, identifier);
	  TREE_CHAIN (elt) = list;
	  list = elt;
	  type_string = ptr+1;
	  i += 1;
	}
    }

  elt = build_tree_list (type, get_identifier (type_string));
  TREE_CHAIN (elt) = list;
  list = elt;
  TYPE_PACKAGE_LIST (type) = nreverse (list);
}

/* Add TYPE as the last element of the compression table. Resize the
   compression table if necessary.  */

static void
compression_table_add (type)
     tree type;
{
  if (compression_next == TREE_VEC_LENGTH (compression_table))
    {
      tree new = make_tree_vec (2*compression_next);
      int i;

      for (i = 0; i < compression_next; i++)
	TREE_VEC_ELT (new, i) = TREE_VEC_ELT (compression_table, i);

      ggc_del_root (&compression_table);
      compression_table = new;
      ggc_add_tree_root (&compression_table, 1);
    }
  TREE_VEC_ELT (compression_table, compression_next++) = type;
}

/* Mangling initialization routine.  */

static void
init_mangling (obstack)
     struct obstack *obstack;
{
  mangle_obstack = obstack;
  if (!compression_table)
    compression_table = make_tree_vec (10);
  else
    /* Mangling already in progress.  */
    abort ();

  /* Mangled name are to be suffixed */
  obstack_grow (mangle_obstack, "_Z", 2);

  /* Register the compression table with the GC */
  ggc_add_tree_root (&compression_table, 1);
}

/* Mangling finalization routine. The mangled name is returned as a
   IDENTIFIER_NODE.  */

static tree
finish_mangling ()
{
  tree result;

  if (!compression_table)
    /* Mangling already finished.  */
    abort ();

  ggc_del_root (&compression_table);
  compression_table = NULL_TREE;
  compression_next = 0;
  obstack_1grow (mangle_obstack, '\0');
  result = get_identifier (obstack_base (mangle_obstack));
  obstack_free (mangle_obstack, obstack_base (mangle_obstack));
#if 0
  printf ("// %s\n", IDENTIFIER_POINTER (result));
#endif
  return result;
}
