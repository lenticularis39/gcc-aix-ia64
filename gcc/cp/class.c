/* Functions related to building classes and their related objects.
   Copyright (C) 1987, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001  Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com)

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


/* High-level class interface.  */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "cp-tree.h"
#include "flags.h"
#include "rtl.h"
#include "output.h"
#include "toplev.h"
#include "ggc.h"
#include "lex.h"

#include "obstack.h"
#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

/* The number of nested classes being processed.  If we are not in the
   scope of any class, this is zero.  */

int current_class_depth;

/* In order to deal with nested classes, we keep a stack of classes.
   The topmost entry is the innermost class, and is the entry at index
   CURRENT_CLASS_DEPTH  */

typedef struct class_stack_node {
  /* The name of the class.  */
  tree name;

  /* The _TYPE node for the class.  */
  tree type;

  /* The access specifier pending for new declarations in the scope of
     this class.  */
  tree access;

  /* If were defining TYPE, the names used in this class.  */
  splay_tree names_used;
}* class_stack_node_t;

typedef struct vtbl_init_data_s
{
  /* The base for which we're building initializers.  */
  tree binfo;
  /* The type of the most-derived type.  */
  tree derived;
  /* The binfo for the dynamic type. This will be TYPE_BINFO (derived),
     unless ctor_vtbl_p is true.  */
  tree rtti_binfo;
  /* The negative-index vtable initializers built up so far.  These
     are in order from least negative index to most negative index.  */
  tree inits;
  /* The last (i.e., most negative) entry in INITS.  */
  tree* last_init;
  /* The binfo for the virtual base for which we're building
     vcall offset initializers.  */
  tree vbase;
  /* The functions in vbase for which we have already provided vcall
     offsets.  */
  varray_type fns;
  /* The vtable index of the next vcall or vbase offset.  */
  tree index;
  /* Nonzero if we are building the initializer for the primary
     vtable.  */
  int primary_vtbl_p;
  /* Nonzero if we are building the initializer for a construction
     vtable.  */
  int ctor_vtbl_p;
} vtbl_init_data;

/* The type of a function passed to walk_subobject_offsets.  */
typedef int (*subobject_offset_fn) PARAMS ((tree, tree, splay_tree));

/* The stack itself.  This is an dynamically resized array.  The
   number of elements allocated is CURRENT_CLASS_STACK_SIZE.  */
static int current_class_stack_size;
static class_stack_node_t current_class_stack;

/* An array of all local classes present in this translation unit, in
   declaration order.  */
varray_type local_classes;

static tree get_vfield_name PARAMS ((tree));
static void finish_struct_anon PARAMS ((tree));
static tree build_vbase_pointer PARAMS ((tree, tree));
static tree build_vtable_entry PARAMS ((tree, tree, tree));
static tree get_vtable_name PARAMS ((tree));
static tree get_basefndecls PARAMS ((tree, tree));
static int build_primary_vtable PARAMS ((tree, tree));
static int build_secondary_vtable PARAMS ((tree, tree));
static tree dfs_finish_vtbls PARAMS ((tree, void *));
static void finish_vtbls PARAMS ((tree));
static void modify_vtable_entry PARAMS ((tree, tree, tree, tree, tree *));
static void add_virtual_function PARAMS ((tree *, tree *, int *, tree, tree));
static tree delete_duplicate_fields_1 PARAMS ((tree, tree));
static void delete_duplicate_fields PARAMS ((tree));
static void finish_struct_bits PARAMS ((tree));
static int alter_access PARAMS ((tree, tree, tree));
static void handle_using_decl PARAMS ((tree, tree));
static int strictly_overrides PARAMS ((tree, tree));
static void mark_overriders PARAMS ((tree, tree));
static void check_for_override PARAMS ((tree, tree));
static tree dfs_modify_vtables PARAMS ((tree, void *));
static tree modify_all_vtables PARAMS ((tree, int *, tree));
static void determine_primary_base PARAMS ((tree, int *));
static void finish_struct_methods PARAMS ((tree));
static void maybe_warn_about_overly_private_class PARAMS ((tree));
static int field_decl_cmp PARAMS ((const tree *, const tree *));
static int method_name_cmp PARAMS ((const tree *, const tree *));
static tree add_implicitly_declared_members PARAMS ((tree, int, int, int));
static tree fixed_type_or_null PARAMS ((tree, int *, int *));
static tree resolve_address_of_overloaded_function PARAMS ((tree, tree, int,
							  int, int, tree));
static void build_vtable_entry_ref PARAMS ((tree, tree, tree));
static tree build_vtbl_initializer PARAMS ((tree, tree, tree, tree, int *));
static int count_fields PARAMS ((tree));
static int add_fields_to_vec PARAMS ((tree, tree, int));
static void check_bitfield_decl PARAMS ((tree));
static void check_field_decl PARAMS ((tree, tree, int *, int *, int *, int *));
static void check_field_decls PARAMS ((tree, tree *, int *, int *, int *, 
				     int *));
static bool build_base_field PARAMS ((record_layout_info, tree, int *,
				     splay_tree, tree));
static bool build_base_fields PARAMS ((record_layout_info, int *,
				      splay_tree, tree));
static tree build_vbase_pointer_fields PARAMS ((record_layout_info, int *));
static tree build_vtbl_or_vbase_field PARAMS ((tree, tree, tree, tree, tree,
					       int *));
static void check_methods PARAMS ((tree));
static void remove_zero_width_bit_fields PARAMS ((tree));
static void check_bases PARAMS ((tree, int *, int *, int *));
static void check_bases_and_members PARAMS ((tree, int *));
static tree create_vtable_ptr PARAMS ((tree, int *, int *, tree *, tree *));
static void layout_class_type PARAMS ((tree, int *, int *, tree *, tree *));
static void fixup_pending_inline PARAMS ((tree));
static void fixup_inline_methods PARAMS ((tree));
static void set_primary_base PARAMS ((tree, tree, int *));
static void propagate_binfo_offsets PARAMS ((tree, tree, tree));
static void layout_virtual_bases PARAMS ((tree, splay_tree));
static tree dfs_set_offset_for_unshared_vbases PARAMS ((tree, void *));
static void build_vbase_offset_vtbl_entries PARAMS ((tree, vtbl_init_data *));
static void add_vcall_offset_vtbl_entries_r PARAMS ((tree, vtbl_init_data *));
static void add_vcall_offset_vtbl_entries_1 PARAMS ((tree, vtbl_init_data *));
static void build_vcall_offset_vtbl_entries PARAMS ((tree, vtbl_init_data *));
static void layout_vtable_decl PARAMS ((tree, int));
static tree dfs_find_final_overrider PARAMS ((tree, void *));
static tree find_final_overrider PARAMS ((tree, tree, tree));
static int make_new_vtable PARAMS ((tree, tree));
static int maybe_indent_hierarchy PARAMS ((FILE *, int, int));
static void dump_class_hierarchy_r PARAMS ((FILE *, int, tree, tree, int));
static void dump_class_hierarchy PARAMS ((tree));
static void dump_array PARAMS ((FILE *, tree));
static void dump_vtable PARAMS ((tree, tree, tree));
static void dump_vtt PARAMS ((tree, tree));
static tree build_vtable PARAMS ((tree, tree, tree));
static void initialize_vtable PARAMS ((tree, tree));
static void initialize_array PARAMS ((tree, tree));
static void layout_nonempty_base_or_field PARAMS ((record_layout_info,
						   tree, tree,
						   splay_tree, tree));
static unsigned HOST_WIDE_INT end_of_class PARAMS ((tree, int));
static bool layout_empty_base PARAMS ((tree, tree, splay_tree, tree));
static void accumulate_vtbl_inits PARAMS ((tree, tree, tree, tree, tree));
static tree dfs_accumulate_vtbl_inits PARAMS ((tree, tree, tree, tree,
					       tree));
static void set_vindex PARAMS ((tree, int *));
static void build_rtti_vtbl_entries PARAMS ((tree, vtbl_init_data *));
static void build_vcall_and_vbase_vtbl_entries PARAMS ((tree, 
							vtbl_init_data *));
static void force_canonical_binfo_r PARAMS ((tree, tree, tree, tree));
static void force_canonical_binfo PARAMS ((tree, tree, tree, tree));
static tree dfs_unshared_virtual_bases PARAMS ((tree, void *));
static void mark_primary_bases PARAMS ((tree));
static tree mark_primary_virtual_base PARAMS ((tree, tree, tree));
static void clone_constructors_and_destructors PARAMS ((tree));
static tree build_clone PARAMS ((tree, tree));
static void update_vtable_entry_for_fn PARAMS ((tree, tree, tree, tree *));
static tree copy_virtuals PARAMS ((tree));
static void build_ctor_vtbl_group PARAMS ((tree, tree));
static void build_vtt PARAMS ((tree));
static tree *build_vtt_inits PARAMS ((tree, tree, tree *, tree *));
static tree dfs_build_secondary_vptr_vtt_inits PARAMS ((tree, void *));
static tree dfs_ctor_vtable_bases_queue_p PARAMS ((tree, void *data));
static tree dfs_fixup_binfo_vtbls PARAMS ((tree, void *));
static tree get_original_base PARAMS ((tree, tree));
static tree dfs_get_primary_binfo PARAMS ((tree, void*));
static int record_subobject_offset PARAMS ((tree, tree, splay_tree));
static int check_subobject_offset PARAMS ((tree, tree, splay_tree));
static int walk_subobject_offsets PARAMS ((tree, subobject_offset_fn,
					   tree, splay_tree, tree, int));
static void record_subobject_offsets PARAMS ((tree, tree, splay_tree, int));
static int layout_conflict_p PARAMS ((tree, tree, splay_tree, int));
static int splay_tree_compare_integer_csts PARAMS ((splay_tree_key k1,
						    splay_tree_key k2));
static void warn_about_ambiguous_direct_bases PARAMS ((tree));

/* Macros for dfs walking during vtt construction. See
   dfs_ctor_vtable_bases_queue_p, dfs_build_secondary_vptr_vtt_inits
   and dfs_fixup_binfo_vtbls.  */
#define VTT_TOP_LEVEL_P(node) TREE_UNSIGNED(node)
#define VTT_MARKED_BINFO_P(node) TREE_USED(node)

/* Variables shared between class.c and call.c.  */

#ifdef GATHER_STATISTICS
int n_vtables = 0;
int n_vtable_entries = 0;
int n_vtable_searches = 0;
int n_vtable_elems = 0;
int n_convert_harshness = 0;
int n_compute_conversion_costs = 0;
int n_build_method_call = 0;
int n_inner_fields_searched = 0;
#endif

/* Virtual base class layout.  */

/* Returns a list of virtual base class pointers as a chain of
   FIELD_DECLS.  */

static tree
build_vbase_pointer_fields (rli, empty_p)
     record_layout_info rli;
     int *empty_p;
{
  /* Chain to hold all the new FIELD_DECLs which point at virtual
     base classes.  */
  tree rec = rli->t;
  tree vbase_decls = NULL_TREE;
  tree binfos = TYPE_BINFO_BASETYPES (rec);
  int n_baseclasses = CLASSTYPE_N_BASECLASSES (rec);
  tree decl;
  int i;

  /* Under the new ABI, there are no vbase pointers in the object.
     Instead, the offsets are stored in the vtable.  */
  if (vbase_offsets_in_vtable_p ())
    return NULL_TREE;

  /* Loop over the baseclasses, adding vbase pointers as needed.  */
  for (i = 0; i < n_baseclasses; i++)
    {
      register tree base_binfo = TREE_VEC_ELT (binfos, i);
      register tree basetype = BINFO_TYPE (base_binfo);

      if (!COMPLETE_TYPE_P (basetype))
	/* This error is now reported in xref_tag, thus giving better
	   location information.  */
	continue;

      /* All basetypes are recorded in the association list of the
	 derived type.  */

      if (TREE_VIA_VIRTUAL (base_binfo))
	{
	  int j;
	  const char *name;

	  /* The offset for a virtual base class is only used in computing
	     virtual function tables and for initializing virtual base
	     pointers.  It is built once `get_vbase_types' is called.  */

	  /* If this basetype can come from another vbase pointer
	     without an additional indirection, we will share
	     that pointer.  If an indirection is involved, we
	     make our own pointer.  */
	  for (j = 0; j < n_baseclasses; j++)
	    {
	      tree other_base_binfo = TREE_VEC_ELT (binfos, j);
	      if (! TREE_VIA_VIRTUAL (other_base_binfo)
		  && binfo_for_vbase (basetype, BINFO_TYPE (other_base_binfo)))
		goto got_it;
	    }
	  FORMAT_VBASE_NAME (name, basetype);
	  decl = build_vtbl_or_vbase_field (get_identifier (name), 
					    get_identifier (VTABLE_BASE),
					    build_pointer_type (basetype),
					    rec,
					    basetype,
					    empty_p);
	  BINFO_VPTR_FIELD (base_binfo) = decl;
	  TREE_CHAIN (decl) = vbase_decls;
	  place_field (rli, decl);
	  vbase_decls = decl;
	  *empty_p = 0;

	got_it:
	  /* The space this decl occupies has already been accounted for.  */
	  ;
	}
    }

  return vbase_decls;
}

/* Returns a pointer to the virtual base class of EXP that has the
   indicated TYPE.  EXP is of class type, not a pointer type.  */

static tree
build_vbase_pointer (exp, type)
     tree exp, type;
{
  if (vbase_offsets_in_vtable_p ())
    {
      tree vbase;
      tree vbase_ptr;

      /* Find the shared copy of TYPE; that's where the vtable offset
	 is recorded.  */
      vbase = binfo_for_vbase (type, TREE_TYPE (exp));
      /* Find the virtual function table pointer.  */
      vbase_ptr = build_vfield_ref (exp, TREE_TYPE (exp));
      /* Compute the location where the offset will lie.  */
      vbase_ptr = build (PLUS_EXPR, 
			 TREE_TYPE (vbase_ptr),
			 vbase_ptr,
			 BINFO_VPTR_FIELD (vbase));
      vbase_ptr = build1 (NOP_EXPR, 
			  build_pointer_type (ptrdiff_type_node),
			  vbase_ptr);
      /* Add the contents of this location to EXP.  */
      return build (PLUS_EXPR,
		    build_pointer_type (type),
		    build_unary_op (ADDR_EXPR, exp, /*noconvert=*/0),
		    build1 (INDIRECT_REF, ptrdiff_type_node, vbase_ptr));
    }
  else
    {
      char *name;
      FORMAT_VBASE_NAME (name, type);
      return build_component_ref (exp, get_identifier (name), NULL_TREE, 0);
    }
}

/* Build multi-level access to EXPR using hierarchy path PATH.
   CODE is PLUS_EXPR if we are going with the grain,
   and MINUS_EXPR if we are not (in which case, we cannot traverse
   virtual baseclass links).

   TYPE is the type we want this path to have on exit.

   NONNULL is non-zero if  we know (for any reason) that EXPR is
   not, in fact, zero.  */

tree
build_vbase_path (code, type, expr, path, nonnull)
     enum tree_code code;
     tree type, expr, path;
     int nonnull;
{
  register int changed = 0;
  tree last = NULL_TREE, last_virtual = NULL_TREE;
  int fixed_type_p;
  tree null_expr = 0, nonnull_expr;
  tree basetype;
  tree offset = integer_zero_node;

  if (BINFO_INHERITANCE_CHAIN (path) == NULL_TREE)
    return build1 (NOP_EXPR, type, expr);

  /* We could do better if we had additional logic to convert back to the
     unconverted type (the static type of the complete object), and then
     convert back to the type we want.  Until that is done, we only optimize
     if the complete type is the same type as expr has.  */
  fixed_type_p = resolves_to_fixed_type_p (expr, &nonnull);
  if (fixed_type_p < 0)
    /* Virtual base layout is not fixed, even in ctors and dtors. */
    fixed_type_p = 0;

  if (!fixed_type_p && TREE_SIDE_EFFECTS (expr))
    expr = save_expr (expr);
  nonnull_expr = expr;

  path = reverse_path (path);

  basetype = BINFO_TYPE (path);

  while (path)
    {
      if (TREE_VIA_VIRTUAL (TREE_VALUE (path)))
	{
	  last_virtual = BINFO_TYPE (TREE_VALUE (path));
	  if (code == PLUS_EXPR)
	    {
	      changed = ! fixed_type_p;

	      if (changed)
		{
		  tree ind;

		  /* We already check for ambiguous things in the caller, just
		     find a path.  */
		  if (last)
		    {
		      tree binfo = get_binfo (last, TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (nonnull_expr))), 0);
		      nonnull_expr = convert_pointer_to_real (binfo, nonnull_expr);
		    }
		  ind = build_indirect_ref (nonnull_expr, NULL_PTR);
		  nonnull_expr = build_vbase_pointer (ind, last_virtual);
		  if (nonnull == 0
		      && TREE_CODE (type) == POINTER_TYPE
		      && null_expr == NULL_TREE)
		    {
		      null_expr = build1 (NOP_EXPR, build_pointer_type (last_virtual), integer_zero_node);
		      expr = build (COND_EXPR, build_pointer_type (last_virtual),
				    build (EQ_EXPR, boolean_type_node, expr,
					   integer_zero_node),
				    null_expr, nonnull_expr);
		    }
		}
	      /* else we'll figure out the offset below.  */

	      /* Happens in the case of parse errors.  */
	      if (nonnull_expr == error_mark_node)
		return error_mark_node;
	    }
	  else
	    {
	      cp_error ("cannot cast up from virtual baseclass `%T'",
			  last_virtual);
	      return error_mark_node;
	    }
	}
      last = TREE_VALUE (path);
      path = TREE_CHAIN (path);
    }
  /* LAST is now the last basetype assoc on the path.  */

  /* A pointer to a virtual base member of a non-null object
     is non-null.  Therefore, we only need to test for zeroness once.
     Make EXPR the canonical expression to deal with here.  */
  if (null_expr)
    {
      TREE_OPERAND (expr, 2) = nonnull_expr;
      TREE_TYPE (expr) = TREE_TYPE (TREE_OPERAND (expr, 1))
	= TREE_TYPE (nonnull_expr);
    }
  else
    expr = nonnull_expr;

  /* If we go through any virtual base pointers, make sure that
     casts to BASETYPE from the last virtual base class use
     the right value for BASETYPE.  */
  if (changed)
    {
      tree intype = TREE_TYPE (TREE_TYPE (expr));

      if (TYPE_MAIN_VARIANT (intype) != BINFO_TYPE (last))
	offset
	  = BINFO_OFFSET (get_binfo (last, TYPE_MAIN_VARIANT (intype), 0));
    }
  else
    offset = BINFO_OFFSET (last);

  if (! integer_zerop (offset))
    {
      /* Bash types to make the backend happy.  */
      offset = cp_convert (type, offset);

      /* If expr might be 0, we need to preserve that zeroness.  */
      if (nonnull == 0)
	{
	  if (null_expr)
	    TREE_TYPE (null_expr) = type;
	  else
	    null_expr = build1 (NOP_EXPR, type, integer_zero_node);
	  if (TREE_SIDE_EFFECTS (expr))
	    expr = save_expr (expr);

	  return build (COND_EXPR, type,
			build (EQ_EXPR, boolean_type_node, expr, integer_zero_node),
			null_expr,
			build (code, type, expr, offset));
	}
      else return build (code, type, expr, offset);
    }

  /* Cannot change the TREE_TYPE of a NOP_EXPR here, since it may
     be used multiple times in initialization of multiple inheritance.  */
  if (null_expr)
    {
      TREE_TYPE (expr) = type;
      return expr;
    }
  else
    return build1 (NOP_EXPR, type, expr);
}


/* Virtual function things.  */

/* We want to give the assembler the vtable identifier as well as
   the offset to the function pointer.  So we generate

   __asm__ __volatile__ (".vtable_entry %c0, %c1"
      : : "s"(&class_vtable),
          "i"((long)&vtbl[idx].pfn - (long)&vtbl[0])); */

static void
build_vtable_entry_ref (basetype, vtbl, idx)
     tree basetype, vtbl, idx;
{
  static char asm_stmt[] = ".vtable_entry %c0, %c1";
  tree s, i, i2;

  s = build_unary_op (ADDR_EXPR, 
		      get_vtbl_decl_for_binfo (TYPE_BINFO (basetype)), 
		      0);
  s = build_tree_list (build_string (1, "s"), s);

  i = build_array_ref (vtbl, idx);
  if (!flag_vtable_thunks)
    i = build_component_ref (i, pfn_identifier, vtable_entry_type, 0);
  i = build_c_cast (ptrdiff_type_node, build_unary_op (ADDR_EXPR, i, 0));
  i2 = build_array_ref (vtbl, build_int_2(0,0));
  i2 = build_c_cast (ptrdiff_type_node, build_unary_op (ADDR_EXPR, i2, 0));
  i = cp_build_binary_op (MINUS_EXPR, i, i2);
  i = build_tree_list (build_string (1, "i"), i);

  finish_asm_stmt (ridpointers[RID_VOLATILE],
		   build_string (sizeof(asm_stmt)-1, asm_stmt),
		   NULL_TREE, chainon (s, i), NULL_TREE);
}

/* Given an object INSTANCE, return an expression which yields the
   virtual function vtable element corresponding to INDEX.  There are
   many special cases for INSTANCE which we take care of here, mainly
   to avoid creating extra tree nodes when we don't have to.  */

tree
build_vtbl_ref (instance, idx)
     tree instance, idx;
{
  tree vtbl, aref;
  tree basetype = TREE_TYPE (instance);

  if (TREE_CODE (basetype) == REFERENCE_TYPE)
    basetype = TREE_TYPE (basetype);

  if (instance == current_class_ref)
    vtbl = build_vfield_ref (instance, basetype);
  else
    {
      if (optimize)
	{
	  /* Try to figure out what a reference refers to, and
	     access its virtual function table directly.  */
	  tree ref = NULL_TREE;

	  if (TREE_CODE (instance) == INDIRECT_REF
	      && TREE_CODE (TREE_TYPE (TREE_OPERAND (instance, 0))) == REFERENCE_TYPE)
	    ref = TREE_OPERAND (instance, 0);
	  else if (TREE_CODE (TREE_TYPE (instance)) == REFERENCE_TYPE)
	    ref = instance;

	  if (ref && TREE_CODE (ref) == VAR_DECL
	      && DECL_INITIAL (ref))
	    {
	      tree init = DECL_INITIAL (ref);

	      while (TREE_CODE (init) == NOP_EXPR
		     || TREE_CODE (init) == NON_LVALUE_EXPR)
		init = TREE_OPERAND (init, 0);
	      if (TREE_CODE (init) == ADDR_EXPR)
		{
		  init = TREE_OPERAND (init, 0);
		  if (IS_AGGR_TYPE (TREE_TYPE (init))
		      && (TREE_CODE (init) == PARM_DECL
			  || TREE_CODE (init) == VAR_DECL))
		    instance = init;
		}
	    }
	}

      if (IS_AGGR_TYPE (TREE_TYPE (instance))
	  && (TREE_CODE (instance) == RESULT_DECL
	      || TREE_CODE (instance) == PARM_DECL
	      || TREE_CODE (instance) == VAR_DECL))
	{
	  vtbl = TYPE_BINFO_VTABLE (basetype);
	  /* Knowing the dynamic type of INSTANCE we can easily obtain
	     the correct vtable entry.  In the new ABI, we resolve
	     this back to be in terms of the primary vtable.  */
	  if (TREE_CODE (vtbl) == PLUS_EXPR)
	    {
	      idx = fold (build (PLUS_EXPR,
				 TREE_TYPE (idx),
				 idx,
				 build (EXACT_DIV_EXPR,
					TREE_TYPE (idx),
					TREE_OPERAND (vtbl, 1),
					TYPE_SIZE_UNIT (vtable_entry_type))));
	      vtbl = get_vtbl_decl_for_binfo (TYPE_BINFO (basetype));
	    }
	}
      else
	vtbl = build_vfield_ref (instance, basetype);
    }

  assemble_external (vtbl);

  if (flag_vtable_gc)
    build_vtable_entry_ref (basetype, vtbl, idx);

  aref = build_array_ref (vtbl, idx);

  return aref;
}

/* Given an object INSTANCE, return an expression which yields the
   virtual function corresponding to INDEX.  There are many special
   cases for INSTANCE which we take care of here, mainly to avoid
   creating extra tree nodes when we don't have to.  */

tree
build_vfn_ref (ptr_to_instptr, instance, idx)
     tree *ptr_to_instptr, instance;
     tree idx;
{
  tree aref = build_vtbl_ref (instance, idx);

  /* When using thunks, there is no extra delta, and we get the pfn
     directly.  */
  if (flag_vtable_thunks)
    return aref;

  if (ptr_to_instptr)
    {
      /* Save the intermediate result in a SAVE_EXPR so we don't have to
	 compute each component of the virtual function pointer twice.  */ 
      if (TREE_CODE (aref) == INDIRECT_REF)
	TREE_OPERAND (aref, 0) = save_expr (TREE_OPERAND (aref, 0));

      *ptr_to_instptr
	= build (PLUS_EXPR, TREE_TYPE (*ptr_to_instptr),
		 *ptr_to_instptr,
		 cp_convert (ptrdiff_type_node,
			     build_component_ref (aref, delta_identifier, NULL_TREE, 0)));
    }

  return build_component_ref (aref, pfn_identifier, NULL_TREE, 0);
}

/* Return the name of the virtual function table (as an IDENTIFIER_NODE)
   for the given TYPE.  */

static tree
get_vtable_name (type)
     tree type;
{
  return mangle_vtbl_for_type (type);
}

/* Return an IDENTIFIER_NODE for the name of the virtual table table
   for TYPE.  */

tree
get_vtt_name (type)
     tree type;
{
  return mangle_vtt_for_type (type);
}

/* Create a VAR_DECL for a primary or secondary vtable for CLASS_TYPE.
   (For a secondary vtable for B-in-D, CLASS_TYPE should be D, not B.)
   Use NAME for the name of the vtable, and VTABLE_TYPE for its type.  */

static tree
build_vtable (class_type, name, vtable_type)
     tree class_type;
     tree name;
     tree vtable_type;
{
  tree decl;

  decl = build_lang_decl (VAR_DECL, name, vtable_type);
  /* vtable names are already mangled; give them their DECL_ASSEMBLER_NAME
     now to avoid confusion in mangle_decl.  */
  SET_DECL_ASSEMBLER_NAME (decl, name);
  DECL_CONTEXT (decl) = class_type;
  DECL_ARTIFICIAL (decl) = 1;
  TREE_STATIC (decl) = 1;
  TREE_READONLY (decl) = 1;
  DECL_VIRTUAL_P (decl) = 1;
  import_export_vtable (decl, class_type, 0);

  return decl;
}

/* Get the VAR_DECL of the vtable for TYPE. TYPE need not be polymorphic,
   or even complete.  If this does not exist, create it.  If COMPLETE is
   non-zero, then complete the definition of it -- that will render it
   impossible to actually build the vtable, but is useful to get at those
   which are known to exist in the runtime.  */

tree 
get_vtable_decl (type, complete)
     tree type;
     int complete;
{
  tree name = get_vtable_name (type);
  tree decl = IDENTIFIER_GLOBAL_VALUE (name);
  
  if (decl)
    {
      my_friendly_assert (TREE_CODE (decl) == VAR_DECL
                          && DECL_VIRTUAL_P (decl), 20000118);
      return decl;
    }
  
  decl = build_vtable (type, name, void_type_node);
  decl = pushdecl_top_level (decl);
  my_friendly_assert (IDENTIFIER_GLOBAL_VALUE (name) == decl,
		      20000517);
  
  /* At one time the vtable info was grabbed 2 words at a time.  This
     fails on sparc unless you have 8-byte alignment.  (tiemann) */
  DECL_ALIGN (decl) = MAX (TYPE_ALIGN (double_type_node),
			   DECL_ALIGN (decl));

  if (complete)
    {
      DECL_EXTERNAL (decl) = 1;
      cp_finish_decl (decl, NULL_TREE, NULL_TREE, 0);
    }

  return decl;
}

/* Returns a copy of the BINFO_VIRTUALS list in BINFO.  The
   BV_VCALL_INDEX for each entry is cleared.  */

static tree
copy_virtuals (binfo)
     tree binfo;
{
  tree copies;
  tree t;

  copies = copy_list (BINFO_VIRTUALS (binfo));
  for (t = copies; t; t = TREE_CHAIN (t))
    {
      BV_VCALL_INDEX (t) = NULL_TREE;
      BV_USE_VCALL_INDEX_P (t) = 0;
    }

  return copies;
}

/* Build the primary virtual function table for TYPE.  If BINFO is
   non-NULL, build the vtable starting with the initial approximation
   that it is the same as the one which is the head of the association
   list.  Returns a non-zero value if a new vtable is actually
   created.  */

static int
build_primary_vtable (binfo, type)
     tree binfo, type;
{
  tree decl;
  tree virtuals;

  decl = get_vtable_decl (type, /*complete=*/0);
  
  if (binfo)
    {
      if (BINFO_NEW_VTABLE_MARKED (binfo, type))
	/* We have already created a vtable for this base, so there's
	   no need to do it again.  */
	return 0;
      
      virtuals = copy_virtuals (binfo);
      TREE_TYPE (decl) = TREE_TYPE (get_vtbl_decl_for_binfo (binfo));
      DECL_SIZE (decl) = TYPE_SIZE (TREE_TYPE (decl));
      DECL_SIZE_UNIT (decl) = TYPE_SIZE_UNIT (TREE_TYPE (decl));
    }
  else
    {
      my_friendly_assert (TREE_CODE (TREE_TYPE (decl)) == VOID_TYPE,
                          20000118);
      virtuals = NULL_TREE;
    }

#ifdef GATHER_STATISTICS
  n_vtables += 1;
  n_vtable_elems += list_length (virtuals);
#endif

  /* Initialize the association list for this type, based
     on our first approximation.  */
  TYPE_BINFO_VTABLE (type) = decl;
  TYPE_BINFO_VIRTUALS (type) = virtuals;
  SET_BINFO_NEW_VTABLE_MARKED (TYPE_BINFO (type), type);
  return 1;
}

/* Give TYPE a new virtual function table which is initialized
   with a skeleton-copy of its original initialization.  The only
   entry that changes is the `delta' entry, so we can really
   share a lot of structure.

   FOR_TYPE is the derived type which caused this table to
   be needed.

   BINFO is the type association which provided TYPE for FOR_TYPE.

   The order in which vtables are built (by calling this function) for
   an object must remain the same, otherwise a binary incompatibility
   can result.  */

static int
build_secondary_vtable (binfo, for_type)
     tree binfo, for_type;
{
  tree basetype;
  tree orig_decl = BINFO_VTABLE (binfo);
  tree name;
  tree new_decl;
  tree offset;
  tree path = binfo;
  char *buf;
  const char *buf2;
  char joiner = '_';
  int i;

#ifdef JOINER
  joiner = JOINER;
#endif

  if (TREE_VIA_VIRTUAL (binfo))
    my_friendly_assert (binfo == binfo_for_vbase (BINFO_TYPE (binfo),
						  current_class_type),
			170);

  if (BINFO_NEW_VTABLE_MARKED (binfo, current_class_type))
    /* We already created a vtable for this base.  There's no need to
       do it again.  */
    return 0;

  /* Remember that we've created a vtable for this BINFO, so that we
     don't try to do so again.  */
  SET_BINFO_NEW_VTABLE_MARKED (binfo, current_class_type);
  
  /* Make fresh virtual list, so we can smash it later.  */
  BINFO_VIRTUALS (binfo) = copy_virtuals (binfo);

  my_friendly_assert (binfo == CANONICAL_BINFO (binfo, for_type), 20010605);
  offset = BINFO_OFFSET (binfo);

  /* In the new ABI, secondary vtables are laid out as part of the
     same structure as the primary vtable.  */
  if (merge_primary_and_secondary_vtables_p ())
    {
      BINFO_VTABLE (binfo) = NULL_TREE;
      return 1;
    }

  /* Create the declaration for the secondary vtable.  */
  basetype = TYPE_MAIN_VARIANT (BINFO_TYPE (binfo));
  buf2 = TYPE_ASSEMBLER_NAME_STRING (basetype);
  i = TYPE_ASSEMBLER_NAME_LENGTH (basetype) + 1;

  /* We know that the vtable that we are going to create doesn't exist
     yet in the global namespace, and when we finish, it will be
     pushed into the global namespace.  In complex MI hierarchies, we
     have to loop while the name we are thinking of adding is globally
     defined, adding more name components to the vtable name as we
     loop, until the name is unique.  This is because in complex MI
     cases, we might have the same base more than once.  This means
     that the order in which this function is called for vtables must
     remain the same, otherwise binary compatibility can be
     compromised.  */

  while (1)
    {
      char *buf1 = (char *) alloca (TYPE_ASSEMBLER_NAME_LENGTH (for_type)
				    + 1 + i);
      char *new_buf2;

      sprintf (buf1, "%s%c%s", TYPE_ASSEMBLER_NAME_STRING (for_type), joiner,
	       buf2);
      buf = (char *) alloca (strlen (VTABLE_NAME_PREFIX) + strlen (buf1) + 1);
      sprintf (buf, "%s%s", VTABLE_NAME_PREFIX, buf1);
      name = get_identifier (buf);

      /* If this name doesn't clash, then we can use it, otherwise
	 we add more to the name until it is unique.  */

      if (! IDENTIFIER_GLOBAL_VALUE (name))
	break;

      /* Set values for next loop through, if the name isn't unique.  */

      path = BINFO_INHERITANCE_CHAIN (path);

      /* We better not run out of stuff to make it unique.  */
      my_friendly_assert (path != NULL_TREE, 368);

      basetype = TYPE_MAIN_VARIANT (BINFO_TYPE (path));

      if (for_type == basetype)
	{
	  /* If we run out of basetypes in the path, we have already
	     found created a vtable with that name before, we now
	     resort to tacking on _%d to distinguish them.  */
	  int j = 2;
	  i = TYPE_ASSEMBLER_NAME_LENGTH (basetype) + 1 + i + 1 + 3;
	  buf1 = (char *) alloca (i);
	  do {
	    sprintf (buf1, "%s%c%s%c%d",
		     TYPE_ASSEMBLER_NAME_STRING (basetype), joiner,
		     buf2, joiner, j);
	    buf = (char *) alloca (strlen (VTABLE_NAME_PREFIX)
				   + strlen (buf1) + 1);
	    sprintf (buf, "%s%s", VTABLE_NAME_PREFIX, buf1);
	    name = get_identifier (buf);

	    /* If this name doesn't clash, then we can use it,
	       otherwise we add something different to the name until
	       it is unique.  */
	  } while (++j <= 999 && IDENTIFIER_GLOBAL_VALUE (name));

	  /* Hey, they really like MI don't they?  Increase the 3
             above to 6, and the 999 to 999999.  :-)  */
	  my_friendly_assert (j <= 999, 369);

	  break;
	}

      i = TYPE_ASSEMBLER_NAME_LENGTH (basetype) + 1 + i;
      new_buf2 = (char *) alloca (i);
      sprintf (new_buf2, "%s%c%s",
	       TYPE_ASSEMBLER_NAME_STRING (basetype), joiner, buf2);
      buf2 = new_buf2;
    }

  new_decl = build_vtable (for_type, name, TREE_TYPE (orig_decl));
  DECL_ALIGN (new_decl) = DECL_ALIGN (orig_decl);
  DECL_USER_ALIGN (new_decl) = DECL_USER_ALIGN (orig_decl);
  BINFO_VTABLE (binfo) = pushdecl_top_level (new_decl);

#ifdef GATHER_STATISTICS
  n_vtables += 1;
  n_vtable_elems += list_length (BINFO_VIRTUALS (binfo));
#endif

  return 1;
}

/* Create a new vtable for BINFO which is the hierarchy dominated by
   T.  */

static int
make_new_vtable (t, binfo)
     tree t;
     tree binfo;
{
  if (binfo == TYPE_BINFO (t))
    /* In this case, it is *type*'s vtable we are modifying.  We start
       with the approximation that its vtable is that of the
       immediate base class.  */
    /* ??? This actually passes TYPE_BINFO (t), not the primary base binfo,
       since we've updated DECL_CONTEXT (TYPE_VFIELD (t)) by now.  */
    return build_primary_vtable (TYPE_BINFO (DECL_CONTEXT (TYPE_VFIELD (t))),
				 t);
  else
    /* This is our very own copy of `basetype' to play with.  Later,
       we will fill in all the virtual functions that override the
       virtual functions in these base classes which are not defined
       by the current type.  */
    return build_secondary_vtable (binfo, t);
}

/* Make *VIRTUALS, an entry on the BINFO_VIRTUALS list for BINFO
   (which is in the hierarchy dominated by T) list FNDECL as its
   BV_FN.  DELTA is the required constant adjustment from the `this'
   pointer where the vtable entry appears to the `this' required when
   the function is actually called.  */

static void
modify_vtable_entry (t, binfo, fndecl, delta, virtuals)
     tree t;
     tree binfo;
     tree fndecl;
     tree delta;
     tree *virtuals;
{
  tree v;

  v = *virtuals;

  if (fndecl != BV_FN (v)
      || !tree_int_cst_equal (delta, BV_DELTA (v)))
    {
      tree base_fndecl;

      /* We need a new vtable for BINFO.  */
      if (make_new_vtable (t, binfo))
	{
	  /* If we really did make a new vtable, we also made a copy
	     of the BINFO_VIRTUALS list.  Now, we have to find the
	     corresponding entry in that list.  */
	  *virtuals = BINFO_VIRTUALS (binfo);
	  while (BV_FN (*virtuals) != BV_FN (v))
	    *virtuals = TREE_CHAIN (*virtuals);
	  v = *virtuals;
	}

      base_fndecl = BV_FN (v);
      BV_DELTA (v) = delta;
      BV_VCALL_INDEX (v) = NULL_TREE;
      BV_FN (v) = fndecl;

      /* Now assign virtual dispatch information, if unset.  We can
	 dispatch this through any overridden base function.

	 FIXME this can choose a secondary vtable if the primary is not
	 also lexically first, leading to useless conversions.
	 In the V3 ABI, there's no reason for DECL_VIRTUAL_CONTEXT to
	 ever be different from DECL_CONTEXT.  */
      if (TREE_CODE (DECL_VINDEX (fndecl)) != INTEGER_CST)
	{
	  DECL_VINDEX (fndecl) = DECL_VINDEX (base_fndecl);
	  DECL_VIRTUAL_CONTEXT (fndecl) = DECL_VIRTUAL_CONTEXT (base_fndecl);
	}
    }
}

/* Set DECL_VINDEX for DECL.  VINDEX_P is the number of virtual
   functions present in the vtable so far.  */

static void
set_vindex (decl, vfuns_p)
     tree decl;
     int *vfuns_p;
{
  int vindex;

  vindex = (*vfuns_p)++;
  DECL_VINDEX (decl) = build_shared_int_cst (vindex);
}

/* Add a virtual function to all the appropriate vtables for the class
   T.  DECL_VINDEX(X) should be error_mark_node, if we want to
   allocate a new slot in our table.  If it is error_mark_node, we
   know that no other function from another vtable is overridden by X.
   VFUNS_P keeps track of how many virtuals there are in our
   main vtable for the type, and we build upon the NEW_VIRTUALS list
   and return it.  */

static void
add_virtual_function (new_virtuals_p, overridden_virtuals_p,
		      vfuns_p, fndecl, t)
     tree *new_virtuals_p;
     tree *overridden_virtuals_p;
     int *vfuns_p;
     tree fndecl;
     tree t; /* Structure type.  */
{
  tree new_virtual;

  /* If this function doesn't override anything from a base class, we
     can just assign it a new DECL_VINDEX now.  Otherwise, if it does
     override something, we keep it around and assign its DECL_VINDEX
     later, in modify_all_vtables.  */
  if (TREE_CODE (DECL_VINDEX (fndecl)) == INTEGER_CST)
    /* We've already dealt with this function.  */
    return;

  new_virtual = make_node (TREE_LIST);
  BV_FN (new_virtual) = fndecl;
  BV_DELTA (new_virtual) = integer_zero_node;

  if (DECL_VINDEX (fndecl) == error_mark_node)
    {
      /* FNDECL is a new virtual function; it doesn't override any
	 virtual function in a base class.  */

      /* We remember that this was the base sub-object for rtti.  */
      CLASSTYPE_RTTI (t) = t;

      /* Now assign virtual dispatch information.  */
      set_vindex (fndecl, vfuns_p);
      DECL_VIRTUAL_CONTEXT (fndecl) = t;

      /* Save the state we've computed on the NEW_VIRTUALS list.  */
      TREE_CHAIN (new_virtual) = *new_virtuals_p;
      *new_virtuals_p = new_virtual;
    }
  else
    {
      /* FNDECL overrides a function from a base class.  */
      TREE_CHAIN (new_virtual) = *overridden_virtuals_p;
      *overridden_virtuals_p = new_virtual;
    }
}

/* Add method METHOD to class TYPE.  If ERROR_P is true, we are adding
   the method after the class has already been defined because a
   declaration for it was seen.  (Even though that is erroneous, we
   add the method for improved error recovery.)  */

void
add_method (type, method, error_p)
     tree type;
     tree method;
     int error_p;
{
  int using = (DECL_CONTEXT (method) != type);
  int len;
  int slot;
  tree method_vec;

  if (!CLASSTYPE_METHOD_VEC (type))
    /* Make a new method vector.  We start with 8 entries.  We must
       allocate at least two (for constructors and destructors), and
       we're going to end up with an assignment operator at some point
       as well.
       
       We could use a TREE_LIST for now, and convert it to a TREE_VEC
       in finish_struct, but we would probably waste more memory
       making the links in the list than we would by over-allocating
       the size of the vector here.  Furthermore, we would complicate
       all the code that expects this to be a vector.  */
    CLASSTYPE_METHOD_VEC (type) = make_tree_vec (8);

  method_vec = CLASSTYPE_METHOD_VEC (type);
  len = TREE_VEC_LENGTH (method_vec);

  /* Constructors and destructors go in special slots.  */
  if (DECL_MAYBE_IN_CHARGE_CONSTRUCTOR_P (method))
    slot = CLASSTYPE_CONSTRUCTOR_SLOT;
  else if (DECL_MAYBE_IN_CHARGE_DESTRUCTOR_P (method))
    slot = CLASSTYPE_DESTRUCTOR_SLOT;
  else
    {
      /* See if we already have an entry with this name.  */
      for (slot = CLASSTYPE_FIRST_CONVERSION_SLOT; slot < len; ++slot)
	if (!TREE_VEC_ELT (method_vec, slot)
	    || (DECL_NAME (OVL_CURRENT (TREE_VEC_ELT (method_vec, 
						      slot))) 
		== DECL_NAME (method)))
	  break;
		
      if (slot == len)
	{
	  /* We need a bigger method vector.  */
	  int new_len;
	  tree new_vec;

	  /* In the non-error case, we are processing a class
	     definition.  Double the size of the vector to give room
	     for new methods.  */
	  if (!error_p)
	    new_len = 2 * len;
	  /* In the error case, the vector is already complete.  We
	     don't expect many errors, and the rest of the front-end
	     will get confused if there are empty slots in the vector.  */
	  else
	    new_len = len + 1;

	  new_vec = make_tree_vec (new_len);
	  bcopy ((PTR) &TREE_VEC_ELT (method_vec, 0),
		 (PTR) &TREE_VEC_ELT (new_vec, 0),
		 len * sizeof (tree));
	  len = new_len;
	  method_vec = CLASSTYPE_METHOD_VEC (type) = new_vec;
	}

      if (DECL_CONV_FN_P (method) && !TREE_VEC_ELT (method_vec, slot))
	{
	  /* Type conversion operators have to come before ordinary
	     methods; add_conversions depends on this to speed up
	     looking for conversion operators.  So, if necessary, we
	     slide some of the vector elements up.  In theory, this
	     makes this algorithm O(N^2) but we don't expect many
	     conversion operators.  */
	  for (slot = 2; slot < len; ++slot)
	    {
	      tree fn = TREE_VEC_ELT (method_vec, slot);
  
	      if (!fn)
		/* There are no more entries in the vector, so we
		   can insert the new conversion operator here.  */
		break;
  		  
	      if (!DECL_CONV_FN_P (OVL_CURRENT (fn)))
		/* We can insert the new function right at the
		   SLOTth position.  */
		break;
	    }
  
	  if (!TREE_VEC_ELT (method_vec, slot))
	    /* There is nothing in the Ith slot, so we can avoid
	       moving anything.  */
		; 
	  else
	    {
	      /* We know the last slot in the vector is empty
		 because we know that at this point there's room
		 for a new function.  */
	      bcopy ((PTR) &TREE_VEC_ELT (method_vec, slot),
		     (PTR) &TREE_VEC_ELT (method_vec, slot + 1),
		     (len - slot - 1) * sizeof (tree));
	      TREE_VEC_ELT (method_vec, slot) = NULL_TREE;
	    }
	}
    }
      
  if (template_class_depth (type))
    /* TYPE is a template class.  Don't issue any errors now; wait
       until instantiation time to complain.  */
    ;
  else
    {
      tree fns;

      /* Check to see if we've already got this method.  */
      for (fns = TREE_VEC_ELT (method_vec, slot);
	   fns;
	   fns = OVL_NEXT (fns))
	{
	  tree fn = OVL_CURRENT (fns);
		 
	  if (TREE_CODE (fn) != TREE_CODE (method))
	    continue;

	  if (TREE_CODE (method) != TEMPLATE_DECL)
	    {
	      /* [over.load] Member function declarations with the
		 same name and the same parameter types cannot be
		 overloaded if any of them is a static member
		 function declaration.  */
	      if ((DECL_STATIC_FUNCTION_P (fn)
		   != DECL_STATIC_FUNCTION_P (method))
		  || using)
		{
		  tree parms1 = TYPE_ARG_TYPES (TREE_TYPE (fn));
		  tree parms2 = TYPE_ARG_TYPES (TREE_TYPE (method));

		  if (! DECL_STATIC_FUNCTION_P (fn))
		    parms1 = TREE_CHAIN (parms1);
		  if (! DECL_STATIC_FUNCTION_P (method))
		    parms2 = TREE_CHAIN (parms2);

		  if (compparms (parms1, parms2))
		    {
		      if (using)
			/* Defer to the local function.  */
			return;
		      else
			cp_error ("`%#D' and `%#D' cannot be overloaded",
				  fn, method);
		    }
		}
	    }

	  if (!decls_match (fn, method))
	    continue;

	  /* There has already been a declaration of this method
	     or member template.  */
	  cp_error_at ("`%D' has already been declared in `%T'", 
		       method, type);

	  /* We don't call duplicate_decls here to merge the
	     declarations because that will confuse things if the
	     methods have inline definitions.  In particular, we
	     will crash while processing the definitions.  */
	  return;
	}
    }

  /* Actually insert the new method.  */
  TREE_VEC_ELT (method_vec, slot) 
    = build_overload (method, TREE_VEC_ELT (method_vec, slot));

      /* Add the new binding.  */ 
  if (!DECL_CONSTRUCTOR_P (method)
      && !DECL_DESTRUCTOR_P (method))
    push_class_level_binding (DECL_NAME (method),
			      TREE_VEC_ELT (method_vec, slot));
}

/* Subroutines of finish_struct.  */

/* Look through the list of fields for this struct, deleting
   duplicates as we go.  This must be recursive to handle
   anonymous unions.

   FIELD is the field which may not appear anywhere in FIELDS.
   FIELD_PTR, if non-null, is the starting point at which
   chained deletions may take place.
   The value returned is the first acceptable entry found
   in FIELDS.

   Note that anonymous fields which are not of UNION_TYPE are
   not duplicates, they are just anonymous fields.  This happens
   when we have unnamed bitfields, for example.  */

static tree
delete_duplicate_fields_1 (field, fields)
     tree field, fields;
{
  tree x;
  tree prev = 0;
  if (DECL_NAME (field) == 0)
    {
      if (! ANON_AGGR_TYPE_P (TREE_TYPE (field)))
	return fields;

      for (x = TYPE_FIELDS (TREE_TYPE (field)); x; x = TREE_CHAIN (x))
	fields = delete_duplicate_fields_1 (x, fields);
      return fields;
    }
  else
    {
      for (x = fields; x; prev = x, x = TREE_CHAIN (x))
	{
	  if (DECL_NAME (x) == 0)
	    {
	      if (! ANON_AGGR_TYPE_P (TREE_TYPE (x)))
		continue;
	      TYPE_FIELDS (TREE_TYPE (x))
		= delete_duplicate_fields_1 (field, TYPE_FIELDS (TREE_TYPE (x)));
	      if (TYPE_FIELDS (TREE_TYPE (x)) == 0)
		{
		  if (prev == 0)
		    fields = TREE_CHAIN (fields);
		  else
		    TREE_CHAIN (prev) = TREE_CHAIN (x);
		}
	    }
	  else if (TREE_CODE (field) == USING_DECL)
	    /* A using declaration may is allowed to appear more than
	       once.  We'll prune these from the field list later, and
	       handle_using_decl will complain about invalid multiple
	       uses.  */
	    ;
	  else if (DECL_NAME (field) == DECL_NAME (x))
	    {
	      if (TREE_CODE (field) == CONST_DECL
		  && TREE_CODE (x) == CONST_DECL)
		cp_error_at ("duplicate enum value `%D'", x);
	      else if (TREE_CODE (field) == CONST_DECL
		       || TREE_CODE (x) == CONST_DECL)
		cp_error_at ("duplicate field `%D' (as enum and non-enum)",
			     x);
	      else if (DECL_DECLARES_TYPE_P (field)
		       && DECL_DECLARES_TYPE_P (x))
		{
		  if (same_type_p (TREE_TYPE (field), TREE_TYPE (x)))
		    continue;
		  cp_error_at ("duplicate nested type `%D'", x);
		}
	      else if (DECL_DECLARES_TYPE_P (field)
		       || DECL_DECLARES_TYPE_P (x))
		{
		  /* Hide tag decls.  */
		  if ((TREE_CODE (field) == TYPE_DECL
		       && DECL_ARTIFICIAL (field))
		      || (TREE_CODE (x) == TYPE_DECL
			  && DECL_ARTIFICIAL (x)))
		    continue;
		  cp_error_at ("duplicate field `%D' (as type and non-type)",
			       x);
		}
	      else
		cp_error_at ("duplicate member `%D'", x);
	      if (prev == 0)
		fields = TREE_CHAIN (fields);
	      else
		TREE_CHAIN (prev) = TREE_CHAIN (x);
	    }
	}
    }
  return fields;
}

static void
delete_duplicate_fields (fields)
     tree fields;
{
  tree x;
  for (x = fields; x && TREE_CHAIN (x); x = TREE_CHAIN (x))
    TREE_CHAIN (x) = delete_duplicate_fields_1 (x, TREE_CHAIN (x));
}

/* Change the access of FDECL to ACCESS in T.  Return 1 if change was
   legit, otherwise return 0.  */

static int
alter_access (t, fdecl, access)
     tree t;
     tree fdecl;
     tree access;
{
  tree elem;

  if (!DECL_LANG_SPECIFIC (fdecl))
    retrofit_lang_decl (fdecl);

  if (DECL_DISCRIMINATOR_P (fdecl))
    abort ();

  elem = purpose_member (t, DECL_ACCESS (fdecl));
  if (elem)
    {
      if (TREE_VALUE (elem) != access)
	{
	  if (TREE_CODE (TREE_TYPE (fdecl)) == FUNCTION_DECL)
	    cp_error_at ("conflicting access specifications for method `%D', ignored", TREE_TYPE (fdecl));
	  else
	    error ("conflicting access specifications for field `%s', ignored",
		   IDENTIFIER_POINTER (DECL_NAME (fdecl)));
	}
      else
	{
	  /* They're changing the access to the same thing they changed
	     it to before.  That's OK.  */
	  ;
	}
    }
  else
    {
      enforce_access (t, fdecl);
      DECL_ACCESS (fdecl) = tree_cons (t, access, DECL_ACCESS (fdecl));
      return 1;
    }
  return 0;
}

/* Process the USING_DECL, which is a member of T.  */

static void
handle_using_decl (using_decl, t)
     tree using_decl;
     tree t;
{
  tree ctype = DECL_INITIAL (using_decl);
  tree name = DECL_NAME (using_decl);
  tree access
    = TREE_PRIVATE (using_decl) ? access_private_node
    : TREE_PROTECTED (using_decl) ? access_protected_node
    : access_public_node;
  tree fdecl, binfo;
  tree flist = NULL_TREE;
  tree old_value;

  binfo = binfo_or_else (ctype, t);
  if (! binfo)
    return;
  
  if (name == constructor_name (ctype)
      || name == constructor_name_full (ctype))
    {
      cp_error_at ("`%D' names constructor", using_decl);
      return;
    }
  if (name == constructor_name (t)
      || name == constructor_name_full (t))
    {
      cp_error_at ("`%D' invalid in `%T'", using_decl, t);
      return;
    }

  fdecl = lookup_member (binfo, name, 0, 0);
  
  if (!fdecl)
    {
      cp_error_at ("no members matching `%D' in `%#T'", using_decl, ctype);
      return;
    }

  if (BASELINK_P (fdecl))
    /* Ignore base type this came from. */
    fdecl = TREE_VALUE (fdecl);

  old_value = IDENTIFIER_CLASS_VALUE (name);
  if (old_value)
    {
      if (is_overloaded_fn (old_value))
	old_value = OVL_CURRENT (old_value);

      if (DECL_P (old_value) && DECL_CONTEXT (old_value) == t)
	/* OK */;
      else
	old_value = NULL_TREE;
    }

  if (is_overloaded_fn (fdecl))
    flist = fdecl;

  if (! old_value)
    ;
  else if (is_overloaded_fn (old_value))
    {
      if (flist)
	/* It's OK to use functions from a base when there are functions with
	   the same name already present in the current class.  */;
      else
	{
	  cp_error_at ("`%D' invalid in `%#T'", using_decl, t);
	  cp_error_at ("  because of local method `%#D' with same name",
		       OVL_CURRENT (old_value));
	  return;
	}
    }
  else if (!DECL_ARTIFICIAL (old_value))
    {
      cp_error_at ("`%D' invalid in `%#T'", using_decl, t);
      cp_error_at ("  because of local member `%#D' with same name", old_value);
      return;
    }
  
  /* Make type T see field decl FDECL with access ACCESS.*/
  if (flist)
    for (; flist; flist = OVL_NEXT (flist))
      {
	add_method (t, OVL_CURRENT (flist), /*error_p=*/0);
	alter_access (t, OVL_CURRENT (flist), access);
      }
  else
    alter_access (t, fdecl, access);
}

/* Run through the base clases of T, updating
   CANT_HAVE_DEFAULT_CTOR_P, CANT_HAVE_CONST_CTOR_P, and
   NO_CONST_ASN_REF_P.  Also set flag bits in T based on properties of
   the bases.  */

static void
check_bases (t, cant_have_default_ctor_p, cant_have_const_ctor_p,
	     no_const_asn_ref_p)
     tree t;
     int *cant_have_default_ctor_p;
     int *cant_have_const_ctor_p;
     int *no_const_asn_ref_p;
{
  int n_baseclasses;
  int i;
  int seen_non_virtual_nearly_empty_base_p;
  tree binfos;

  binfos = TYPE_BINFO_BASETYPES (t);
  n_baseclasses = CLASSTYPE_N_BASECLASSES (t);
  seen_non_virtual_nearly_empty_base_p = 0;

  /* An aggregate cannot have baseclasses.  */
  CLASSTYPE_NON_AGGREGATE (t) |= (n_baseclasses != 0);

  for (i = 0; i < n_baseclasses; ++i) 
    {
      tree base_binfo;
      tree basetype;

      /* Figure out what base we're looking at.  */
      base_binfo = TREE_VEC_ELT (binfos, i);
      basetype = TREE_TYPE (base_binfo);

      /* If the type of basetype is incomplete, then we already
	 complained about that fact (and we should have fixed it up as
	 well).  */
      if (!COMPLETE_TYPE_P (basetype))
	{
	  int j;
	  /* The base type is of incomplete type.  It is
	     probably best to pretend that it does not
	     exist.  */
	  if (i == n_baseclasses-1)
	    TREE_VEC_ELT (binfos, i) = NULL_TREE;
	  TREE_VEC_LENGTH (binfos) -= 1;
	  n_baseclasses -= 1;
	  for (j = i; j+1 < n_baseclasses; j++)
	    TREE_VEC_ELT (binfos, j) = TREE_VEC_ELT (binfos, j+1);
	  continue;
	}

      /* Effective C++ rule 14.  We only need to check TYPE_POLYMORPHIC_P
	 here because the case of virtual functions but non-virtual
	 dtor is handled in finish_struct_1.  */
      if (warn_ecpp && ! TYPE_POLYMORPHIC_P (basetype)
	  && TYPE_HAS_DESTRUCTOR (basetype))
	cp_warning ("base class `%#T' has a non-virtual destructor",
		    basetype);

      /* If the base class doesn't have copy constructors or
	 assignment operators that take const references, then the
	 derived class cannot have such a member automatically
	 generated.  */
      if (! TYPE_HAS_CONST_INIT_REF (basetype))
	*cant_have_const_ctor_p = 1;
      if (TYPE_HAS_ASSIGN_REF (basetype)
	  && !TYPE_HAS_CONST_ASSIGN_REF (basetype))
	*no_const_asn_ref_p = 1;
      /* Similarly, if the base class doesn't have a default
	 constructor, then the derived class won't have an
	 automatically generated default constructor.  */
      if (TYPE_HAS_CONSTRUCTOR (basetype)
	  && ! TYPE_HAS_DEFAULT_CONSTRUCTOR (basetype))
	{
	  *cant_have_default_ctor_p = 1;
	  if (! TYPE_HAS_CONSTRUCTOR (t))
            cp_pedwarn ("base `%T' with only non-default constructor in class without a constructor",
                        basetype);
	}

      if (TREE_VIA_VIRTUAL (base_binfo))
	/* A virtual base does not effect nearly emptiness. */
	;
      else if (CLASSTYPE_NEARLY_EMPTY_P (basetype))
	{
	  if (seen_non_virtual_nearly_empty_base_p)
	    /* And if there is more than one nearly empty base, then the
	       derived class is not nearly empty either.  */
	    CLASSTYPE_NEARLY_EMPTY_P (t) = 0;
	  else
	    /* Remember we've seen one. */
	    seen_non_virtual_nearly_empty_base_p = 1;
	}
      else if (!is_empty_class (basetype))
	/* If the base class is not empty or nearly empty, then this
	   class cannot be nearly empty.  */
	CLASSTYPE_NEARLY_EMPTY_P (t) = 0;

      /* A lot of properties from the bases also apply to the derived
	 class.  */
      TYPE_NEEDS_CONSTRUCTING (t) |= TYPE_NEEDS_CONSTRUCTING (basetype);
      TYPE_HAS_NONTRIVIAL_DESTRUCTOR (t) 
	|= TYPE_HAS_NONTRIVIAL_DESTRUCTOR (basetype);
      TYPE_HAS_COMPLEX_ASSIGN_REF (t) 
	|= TYPE_HAS_COMPLEX_ASSIGN_REF (basetype);
      TYPE_HAS_COMPLEX_INIT_REF (t) |= TYPE_HAS_COMPLEX_INIT_REF (basetype);
      TYPE_OVERLOADS_CALL_EXPR (t) |= TYPE_OVERLOADS_CALL_EXPR (basetype);
      TYPE_OVERLOADS_ARRAY_REF (t) |= TYPE_OVERLOADS_ARRAY_REF (basetype);
      TYPE_OVERLOADS_ARROW (t) |= TYPE_OVERLOADS_ARROW (basetype);
      TYPE_POLYMORPHIC_P (t) |= TYPE_POLYMORPHIC_P (basetype);

      /* Derived classes can implicitly become COMified if their bases
	 are COM.  */
      if (CLASSTYPE_COM_INTERFACE (basetype))
	CLASSTYPE_COM_INTERFACE (t) = 1;
      else if (i == 0 && CLASSTYPE_COM_INTERFACE (t))
	{
	  cp_error 
	    ("COM interface type `%T' with non-COM leftmost base class `%T'",
	     t, basetype);
	  CLASSTYPE_COM_INTERFACE (t) = 0;
	}
    }
}

/* Binfo FROM is within a virtual heirarchy which is being reseated to
   TO. Move primary information from FROM to TO, and recursively traverse
   into FROM's bases. The heirarchy is dominated by TYPE.  MAPPINGS is an
   assoc list of binfos that have already been reseated.  */

static void
force_canonical_binfo_r (to, from, type, mappings)
     tree to;
     tree from;
     tree type;
     tree mappings;
{
  int i, n_baseclasses = BINFO_N_BASETYPES (from);
  
  BINFO_INDIRECT_PRIMARY_P (to)
          = BINFO_INDIRECT_PRIMARY_P (from);
  BINFO_INDIRECT_PRIMARY_P (from) = 0;
  BINFO_UNSHARED_MARKED (to) = BINFO_UNSHARED_MARKED (from);
  BINFO_UNSHARED_MARKED (from) = 0;
  BINFO_LOST_PRIMARY_P (to) = BINFO_LOST_PRIMARY_P (from);
  BINFO_LOST_PRIMARY_P (from) = 0;
  if (BINFO_PRIMARY_P (from))
    {
      tree primary = BINFO_PRIMARY_BASE_OF (from);
      tree assoc;
      
      /* We might have just moved the primary base too, see if it's on our
         mappings.  */
      assoc = purpose_member (primary, mappings);
      if (assoc)
        primary = TREE_VALUE (assoc);
      BINFO_PRIMARY_BASE_OF (to) = primary;
      BINFO_PRIMARY_BASE_OF (from) = NULL_TREE;
    }
  my_friendly_assert (same_type_p (BINFO_TYPE (to), BINFO_TYPE (from)),
		      20010104);
  mappings = tree_cons (from, to, mappings);
  for (i = 0; i != n_baseclasses; i++)
    {
      tree from_binfo = BINFO_BASETYPE (from, i);
      tree to_binfo = BINFO_BASETYPE (to, i);
      
      if (TREE_VIA_VIRTUAL (from_binfo))
        {
	  if (BINFO_PRIMARY_P (from_binfo) &&
	      purpose_member (BINFO_PRIMARY_BASE_OF (from_binfo), mappings))
	    /* This base is a primary of some binfo we have already
	       reseated. We must reseat this one too.  */
            force_canonical_binfo (to_binfo, from_binfo, type, mappings);
        }
      else
        force_canonical_binfo_r (to_binfo, from_binfo, type, mappings);
    }
}

/* FROM is the canonical binfo for a virtual base. It is being reseated to
   make TO the canonical binfo, within the heirarchy dominated by TYPE.
   MAPPINGS is an assoc list of binfos that have already been reseated.
   Adjust any non-virtual bases within FROM, and also move any virtual bases
   which are canonical.  This complication arises because selecting primary
   bases walks in inheritance graph order, but we don't share binfos for
   virtual bases, hence we can fill in the primaries for a virtual base,
   and then discover that a later base requires the virtual as its
   primary.  */

static void
force_canonical_binfo (to, from, type, mappings)
     tree to;
     tree from;
     tree type;
     tree mappings;
{
  tree assoc = purpose_member (BINFO_TYPE (to),
		               CLASSTYPE_VBASECLASSES (type));
  TREE_VALUE (assoc) = to;
  force_canonical_binfo_r (to, from, type, mappings);
}

/* Make BASE_BINFO the primary virtual base of BINFO within the hierarchy
   dominated by TYPE. Returns BASE_BINFO, if it can be made so, NULL
   otherwise (because something else has already made it primary).  */

static tree
mark_primary_virtual_base (binfo, base_binfo, type)
     tree binfo;
     tree base_binfo;
     tree type;
{
  tree shared_binfo = binfo_for_vbase (BINFO_TYPE (base_binfo), type);

  if (BINFO_PRIMARY_P (shared_binfo))
    {
      /* It's already allocated in the hierarchy. BINFO won't have a
         primary base in this hierachy, even though the complete object
         BINFO is for, would do.  */
      BINFO_LOST_PRIMARY_P (binfo) = 1;
      
      return NULL_TREE;
    }
     
  /* We need to make sure that the assoc list
     CLASSTYPE_VBASECLASSES of TYPE, indicates this particular
     primary BINFO for the virtual base, as this is the one
     that'll really exist.  */
  if (base_binfo != shared_binfo)
    force_canonical_binfo (base_binfo, shared_binfo, type, NULL);

  return base_binfo;
}

/* If BINFO is an unmarked virtual binfo for a class with a primary virtual
   base, then BINFO has no primary base in this graph.  Called from
   mark_primary_bases.  DATA is the most derived type. */

static tree dfs_unshared_virtual_bases (binfo, data)
     tree binfo;
     void *data;
{
  tree t = (tree) data;
  
  if (!BINFO_UNSHARED_MARKED (binfo)
      && CLASSTYPE_HAS_PRIMARY_BASE_P (BINFO_TYPE (binfo)))
    {
      /* This morally virtual base has a primary base when it
         is a complete object. We need to locate the shared instance
         of this binfo in the type dominated by T. We duplicate the
         primary base information from there to here.  */
      tree vbase;
      tree unshared_base;
      
      for (vbase = binfo; !TREE_VIA_VIRTUAL (vbase);
	   vbase = BINFO_INHERITANCE_CHAIN (vbase))
	continue;
      unshared_base = get_original_base (binfo,
					 binfo_for_vbase (BINFO_TYPE (vbase),
							  t));
      my_friendly_assert (unshared_base != binfo, 20010612);
      BINFO_LOST_PRIMARY_P (binfo) = BINFO_LOST_PRIMARY_P (unshared_base);
      if (!BINFO_LOST_PRIMARY_P (binfo))
	BINFO_PRIMARY_BASE_OF (get_primary_binfo (binfo)) = binfo;
    }
  
  if (binfo != TYPE_BINFO (t))
    /* The vtable fields will have been copied when duplicating the
       base binfos. That information is bogus, make sure we don't try
       and use it. */
    BINFO_VTABLE (binfo) = NULL_TREE;

  /* If this is a virtual primary base, make sure its offset matches
     that which it is primary for. */
  if (BINFO_PRIMARY_P (binfo) && TREE_VIA_VIRTUAL (binfo) &&
      binfo_for_vbase (BINFO_TYPE (binfo), t) == binfo)
    {
      tree delta = size_diffop (BINFO_OFFSET (BINFO_PRIMARY_BASE_OF (binfo)),
				BINFO_OFFSET (binfo));
      if (!integer_zerop (delta))
	propagate_binfo_offsets (binfo, delta, t);
    }
  
  BINFO_UNSHARED_MARKED (binfo) = 0;
  return NULL;
}

/* Set BINFO_PRIMARY_BASE_OF for all binfos in the hierarchy
   dominated by TYPE that are primary bases.  */

static void
mark_primary_bases (type)
     tree type;
{
  tree binfo;
  
  /* Walk the bases in inheritance graph order.  */
  for (binfo = TYPE_BINFO (type); binfo; binfo = TREE_CHAIN (binfo))
    {
      tree base_binfo;
      
      if (!CLASSTYPE_HAS_PRIMARY_BASE_P (BINFO_TYPE (binfo)))
        /* Not a dynamic base. */
        continue;

      base_binfo = get_primary_binfo (binfo);

      if (TREE_VIA_VIRTUAL (base_binfo))
        base_binfo = mark_primary_virtual_base (binfo, base_binfo, type);

      if (base_binfo)
        BINFO_PRIMARY_BASE_OF (base_binfo) = binfo;
      
      BINFO_UNSHARED_MARKED (binfo) = 1;
    }
  /* There could remain unshared morally virtual bases which were not
     visited in the inheritance graph walk. These bases will have lost
     their virtual primary base (should they have one). We must now
     find them. Also we must fix up the BINFO_OFFSETs of primary
     virtual bases. We could not do that as we went along, as they
     were originally copied from the bases we inherited from by
     unshare_base_binfos. That may have decided differently about
     where a virtual primary base went.  */
  dfs_walk (TYPE_BINFO (type), dfs_unshared_virtual_bases, NULL, type);
}

/* Make the BINFO the primary base of T.  */

static void
set_primary_base (t, binfo, vfuns_p)
     tree t;
     tree binfo;
     int *vfuns_p;
{
  tree basetype;

  CLASSTYPE_PRIMARY_BINFO (t) = binfo;
  basetype = BINFO_TYPE (binfo);
  TYPE_BINFO_VTABLE (t) = TYPE_BINFO_VTABLE (basetype);
  TYPE_BINFO_VIRTUALS (t) = TYPE_BINFO_VIRTUALS (basetype);
  TYPE_VFIELD (t) = TYPE_VFIELD (basetype);
  CLASSTYPE_RTTI (t) = CLASSTYPE_RTTI (basetype);
  *vfuns_p = CLASSTYPE_VSIZE (basetype);
}

/* Determine the primary class for T.  */

static void
determine_primary_base (t, vfuns_p)
     tree t;
     int *vfuns_p;
{
  int i, n_baseclasses = CLASSTYPE_N_BASECLASSES (t);
  tree vbases;
  tree type_binfo;

  /* If there are no baseclasses, there is certainly no primary base.  */
  if (n_baseclasses == 0)
    return;

  type_binfo = TYPE_BINFO (t);

  for (i = 0; i < n_baseclasses; i++)
    {
      tree base_binfo = BINFO_BASETYPE (type_binfo, i);
      tree basetype = BINFO_TYPE (base_binfo);

      if (TYPE_CONTAINS_VPTR_P (basetype))
	{
	  /* Even a virtual baseclass can contain our RTTI
	     information.  But, we prefer a non-virtual polymorphic
	     baseclass.  */
	  if (!CLASSTYPE_HAS_PRIMARY_BASE_P (t))
	    CLASSTYPE_RTTI (t) = CLASSTYPE_RTTI (basetype);

	  /* A virtual baseclass can't be the primary base under the
	     old ABI.  And under the new ABI we still prefer a
	     non-virtual base.  */
	  if (TREE_VIA_VIRTUAL (base_binfo))
	    continue;

	  if (!CLASSTYPE_HAS_PRIMARY_BASE_P (t))
	    {
	      set_primary_base (t, base_binfo, vfuns_p);
	      CLASSTYPE_VFIELDS (t) = copy_list (CLASSTYPE_VFIELDS (basetype));
	    }
	  else
	    {
	      tree vfields;

	      /* Only add unique vfields, and flatten them out as we go.  */
	      for (vfields = CLASSTYPE_VFIELDS (basetype);
		   vfields;
		   vfields = TREE_CHAIN (vfields))
		if (VF_BINFO_VALUE (vfields) == NULL_TREE
		    || ! TREE_VIA_VIRTUAL (VF_BINFO_VALUE (vfields)))
		  CLASSTYPE_VFIELDS (t) 
		    = tree_cons (base_binfo, 
				 VF_BASETYPE_VALUE (vfields),
				 CLASSTYPE_VFIELDS (t));
	    }
	}
    }

  if (!TYPE_VFIELD (t))
    CLASSTYPE_PRIMARY_BINFO (t) = NULL_TREE;

  /* Find the indirect primary bases - those virtual bases which are primary
     bases of something else in this hierarchy.  */
  for (vbases = CLASSTYPE_VBASECLASSES (t);
       vbases;
       vbases = TREE_CHAIN (vbases)) 
    {
      tree vbase_binfo = TREE_VALUE (vbases);

      /* See if this virtual base is an indirect primary base.  To be so,
         it must be a primary base within the hierarchy of one of our
         direct bases.  */
      for (i = 0; i < n_baseclasses; ++i) 
	{
	  tree basetype = TYPE_BINFO_BASETYPE (t, i);
	  tree v;

	  for (v = CLASSTYPE_VBASECLASSES (basetype); 
	       v; 
	       v = TREE_CHAIN (v))
	    {
	      tree base_vbase = TREE_VALUE (v);
	      
	      if (BINFO_PRIMARY_P (base_vbase)
		  && same_type_p (BINFO_TYPE (base_vbase),
	                          BINFO_TYPE (vbase_binfo)))
		{
		  BINFO_INDIRECT_PRIMARY_P (vbase_binfo) = 1;
		  break;
		}
	    }

	  /* If we've discovered that this virtual base is an indirect
	     primary base, then we can move on to the next virtual
	     base.  */
	  if (BINFO_INDIRECT_PRIMARY_P (vbase_binfo))
	    break;
	}
    }

  /* The new ABI allows for the use of a "nearly-empty" virtual base
     class as the primary base class if no non-virtual polymorphic
     base can be found.  */
  if (!CLASSTYPE_HAS_PRIMARY_BASE_P (t))
    {
      /* If not NULL, this is the best primary base candidate we have
         found so far.  */
      tree candidate = NULL_TREE;
      tree base_binfo;

      /* Loop over the baseclasses.  */
      for (base_binfo = TYPE_BINFO (t);
	   base_binfo;
	   base_binfo = TREE_CHAIN (base_binfo))
	{
	  tree basetype = BINFO_TYPE (base_binfo);

	  if (TREE_VIA_VIRTUAL (base_binfo) 
	      && CLASSTYPE_NEARLY_EMPTY_P (basetype))
	    {
	      /* If this is not an indirect primary base, then it's
		 definitely our primary base.  */
	      if (!BINFO_INDIRECT_PRIMARY_P (base_binfo))
		{
		  candidate = base_binfo;
		  break;
		}

	      /* If this is an indirect primary base, it still could be
	         our primary base -- unless we later find there's another
	         nearly-empty virtual base that isn't an indirect
	         primary base.  */
	      if (!candidate)
		candidate = base_binfo;
	    }
	}

      /* If we've got a primary base, use it.  */
      if (candidate)
	{
	  set_primary_base (t, candidate, vfuns_p);
	  CLASSTYPE_VFIELDS (t) 
	    = copy_list (CLASSTYPE_VFIELDS (BINFO_TYPE (candidate)));
	}	
    }

  /* Mark the primary base classes at this point.  */
  mark_primary_bases (t);
}

/* Set memoizing fields and bits of T (and its variants) for later
   use.  */

static void
finish_struct_bits (t)
     tree t;
{
  int i, n_baseclasses = CLASSTYPE_N_BASECLASSES (t);

  /* Fix up variants (if any).  */
  tree variants = TYPE_NEXT_VARIANT (t);
  while (variants)
    {
      /* These fields are in the _TYPE part of the node, not in
	 the TYPE_LANG_SPECIFIC component, so they are not shared.  */
      TYPE_HAS_CONSTRUCTOR (variants) = TYPE_HAS_CONSTRUCTOR (t);
      TYPE_HAS_DESTRUCTOR (variants) = TYPE_HAS_DESTRUCTOR (t);
      TYPE_NEEDS_CONSTRUCTING (variants) = TYPE_NEEDS_CONSTRUCTING (t);
      TYPE_HAS_NONTRIVIAL_DESTRUCTOR (variants) 
	= TYPE_HAS_NONTRIVIAL_DESTRUCTOR (t);

      TYPE_BASE_CONVS_MAY_REQUIRE_CODE_P (variants) 
	= TYPE_BASE_CONVS_MAY_REQUIRE_CODE_P (t);
      TYPE_POLYMORPHIC_P (variants) = TYPE_POLYMORPHIC_P (t);
      TYPE_USES_VIRTUAL_BASECLASSES (variants) = TYPE_USES_VIRTUAL_BASECLASSES (t);
      /* Copy whatever these are holding today.  */
      TYPE_MIN_VALUE (variants) = TYPE_MIN_VALUE (t);
      TYPE_MAX_VALUE (variants) = TYPE_MAX_VALUE (t);
      TYPE_FIELDS (variants) = TYPE_FIELDS (t);
      TYPE_SIZE (variants) = TYPE_SIZE (t);
      TYPE_SIZE_UNIT (variants) = TYPE_SIZE_UNIT (t);
      variants = TYPE_NEXT_VARIANT (variants);
    }

  if (n_baseclasses && TYPE_POLYMORPHIC_P (t))
    /* For a class w/o baseclasses, `finish_struct' has set
       CLASS_TYPE_ABSTRACT_VIRTUALS correctly (by
       definition). Similarly for a class whose base classes do not
       have vtables. When neither of these is true, we might have
       removed abstract virtuals (by providing a definition), added
       some (by declaring new ones), or redeclared ones from a base
       class. We need to recalculate what's really an abstract virtual
       at this point (by looking in the vtables).  */
      get_pure_virtuals (t);

  if (n_baseclasses)
    {
      /* Notice whether this class has type conversion functions defined.  */
      tree binfo = TYPE_BINFO (t);
      tree binfos = BINFO_BASETYPES (binfo);
      tree basetype;

      for (i = n_baseclasses-1; i >= 0; i--)
	{
	  basetype = BINFO_TYPE (TREE_VEC_ELT (binfos, i));

	  TYPE_HAS_CONVERSION (t) |= TYPE_HAS_CONVERSION (basetype);
	}
    }

  /* If this type has a copy constructor, force its mode to be BLKmode, and
     force its TREE_ADDRESSABLE bit to be nonzero.  This will cause it to
     be passed by invisible reference and prevent it from being returned in
     a register.

     Also do this if the class has BLKmode but can still be returned in
     registers, since function_cannot_inline_p won't let us inline
     functions returning such a type.  This affects the HP-PA.  */
  if (! TYPE_HAS_TRIVIAL_INIT_REF (t)
      || (TYPE_MODE (t) == BLKmode && ! aggregate_value_p (t)
	  && CLASSTYPE_NON_AGGREGATE (t)))
    {
      tree variants;
      DECL_MODE (TYPE_MAIN_DECL (t)) = BLKmode;
      for (variants = t; variants; variants = TYPE_NEXT_VARIANT (variants))
	{
	  TYPE_MODE (variants) = BLKmode;
	  TREE_ADDRESSABLE (variants) = 1;
	}
    }
}

/* Issue warnings about T having private constructors, but no friends,
   and so forth.  

   HAS_NONPRIVATE_METHOD is nonzero if T has any non-private methods or
   static members.  HAS_NONPRIVATE_STATIC_FN is nonzero if T has any
   non-private static member functions.  */

static void
maybe_warn_about_overly_private_class (t)
     tree t;
{
  int has_member_fn = 0;
  int has_nonprivate_method = 0;
  tree fn;

  if (!warn_ctor_dtor_privacy
      /* If the class has friends, those entities might create and
	 access instances, so we should not warn.  */
      || (CLASSTYPE_FRIEND_CLASSES (t)
	  || DECL_FRIENDLIST (TYPE_MAIN_DECL (t)))
      /* We will have warned when the template was declared; there's
	 no need to warn on every instantiation.  */
      || CLASSTYPE_TEMPLATE_INSTANTIATION (t))
    /* There's no reason to even consider warning about this 
       class.  */
    return;
    
  /* We only issue one warning, if more than one applies, because
     otherwise, on code like:

     class A {
       // Oops - forgot `public:'
       A();
       A(const A&);
       ~A();
     };

     we warn several times about essentially the same problem.  */

  /* Check to see if all (non-constructor, non-destructor) member
     functions are private.  (Since there are no friends or
     non-private statics, we can't ever call any of the private member
     functions.)  */
  for (fn = TYPE_METHODS (t); fn; fn = TREE_CHAIN (fn))
    /* We're not interested in compiler-generated methods; they don't
       provide any way to call private members.  */
    if (!DECL_ARTIFICIAL (fn)) 
      {
	if (!TREE_PRIVATE (fn))
	  {
	    if (DECL_STATIC_FUNCTION_P (fn)) 
	      /* A non-private static member function is just like a
		 friend; it can create and invoke private member
		 functions, and be accessed without a class
		 instance.  */
	      return;
		
	    has_nonprivate_method = 1;
	    break;
	  }
	else if (!DECL_CONSTRUCTOR_P (fn) && !DECL_DESTRUCTOR_P (fn))
	  has_member_fn = 1;
      } 

  if (!has_nonprivate_method && has_member_fn) 
    {
      /* There are no non-private methods, and there's at least one
	 private member function that isn't a constructor or
	 destructor.  (If all the private members are
	 constructors/destructors we want to use the code below that
	 issues error messages specifically referring to
	 constructors/destructors.)  */
      int i;
      tree binfos = BINFO_BASETYPES (TYPE_BINFO (t));
      for (i = 0; i < CLASSTYPE_N_BASECLASSES (t); i++)
	if (TREE_VIA_PUBLIC (TREE_VEC_ELT (binfos, i))
	    || TREE_VIA_PROTECTED (TREE_VEC_ELT (binfos, i)))
	  {
	    has_nonprivate_method = 1;
	    break;
	  }
      if (!has_nonprivate_method) 
	{
	  cp_warning ("all member functions in class `%T' are private", t);
	  return;
	}
    }

  /* Even if some of the member functions are non-private, the class
     won't be useful for much if all the constructors or destructors
     are private: such an object can never be created or destroyed.  */
  if (TYPE_HAS_DESTRUCTOR (t))
    {
      tree dtor = TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (t), 1);

      if (TREE_PRIVATE (dtor))
	{
	  cp_warning ("`%#T' only defines a private destructor and has no friends",
		      t);
	  return;
	}
    }

  if (TYPE_HAS_CONSTRUCTOR (t))
    {
      int nonprivate_ctor = 0;
	  
      /* If a non-template class does not define a copy
	 constructor, one is defined for it, enabling it to avoid
	 this warning.  For a template class, this does not
	 happen, and so we would normally get a warning on:

	   template <class T> class C { private: C(); };  
	  
	 To avoid this asymmetry, we check TYPE_HAS_INIT_REF.  All
	 complete non-template or fully instantiated classes have this
	 flag set.  */
      if (!TYPE_HAS_INIT_REF (t))
	nonprivate_ctor = 1;
      else 
	for (fn = TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (t), 0);
	     fn;
	     fn = OVL_NEXT (fn)) 
	  {
	    tree ctor = OVL_CURRENT (fn);
	    /* Ideally, we wouldn't count copy constructors (or, in
	       fact, any constructor that takes an argument of the
	       class type as a parameter) because such things cannot
	       be used to construct an instance of the class unless
	       you already have one.  But, for now at least, we're
	       more generous.  */
	    if (! TREE_PRIVATE (ctor))
	      {
		nonprivate_ctor = 1;
		break;
	      }
	  }

      if (nonprivate_ctor == 0)
	{
	  cp_warning ("`%#T' only defines private constructors and has no friends",
		      t);
	  return;
	}
    }
}

/* Function to help qsort sort FIELD_DECLs by name order.  */

static int
field_decl_cmp (x, y)
     const tree *x, *y;
{
  if (DECL_NAME (*x) == DECL_NAME (*y))
    /* A nontype is "greater" than a type.  */
    return DECL_DECLARES_TYPE_P (*y) - DECL_DECLARES_TYPE_P (*x);
  if (DECL_NAME (*x) == NULL_TREE)
    return -1;
  if (DECL_NAME (*y) == NULL_TREE)
    return 1;
  if (DECL_NAME (*x) < DECL_NAME (*y))
    return -1;
  return 1;
}

/* Comparison function to compare two TYPE_METHOD_VEC entries by name.  */

static int
method_name_cmp (m1, m2)
     const tree *m1, *m2;
{
  if (*m1 == NULL_TREE && *m2 == NULL_TREE)
    return 0;
  if (*m1 == NULL_TREE)
    return -1;
  if (*m2 == NULL_TREE)
    return 1;
  if (DECL_NAME (OVL_CURRENT (*m1)) < DECL_NAME (OVL_CURRENT (*m2)))
    return -1;
  return 1;
}

/* Warn about duplicate methods in fn_fields.  Also compact method
   lists so that lookup can be made faster.

   Data Structure: List of method lists.  The outer list is a
   TREE_LIST, whose TREE_PURPOSE field is the field name and the
   TREE_VALUE is the DECL_CHAIN of the FUNCTION_DECLs.  TREE_CHAIN
   links the entire list of methods for TYPE_METHODS.  Friends are
   chained in the same way as member functions (? TREE_CHAIN or
   DECL_CHAIN), but they live in the TREE_TYPE field of the outer
   list.  That allows them to be quickly deleted, and requires no
   extra storage.

   Sort methods that are not special (i.e., constructors, destructors,
   and type conversion operators) so that we can find them faster in
   search.  */

static void
finish_struct_methods (t)
     tree t;
{
  tree fn_fields;
  tree method_vec;
  int slot, len;

  if (!TYPE_METHODS (t))
    {
      /* Clear these for safety; perhaps some parsing error could set
	 these incorrectly.  */
      TYPE_HAS_CONSTRUCTOR (t) = 0;
      TYPE_HAS_DESTRUCTOR (t) = 0;
      CLASSTYPE_METHOD_VEC (t) = NULL_TREE;
      return;
    }

  method_vec = CLASSTYPE_METHOD_VEC (t);
  my_friendly_assert (method_vec != NULL_TREE, 19991215);
  len = TREE_VEC_LENGTH (method_vec);

  /* First fill in entry 0 with the constructors, entry 1 with destructors,
     and the next few with type conversion operators (if any).  */
  for (fn_fields = TYPE_METHODS (t); fn_fields; 
       fn_fields = TREE_CHAIN (fn_fields))
    /* Clear out this flag.  */
    DECL_IN_AGGR_P (fn_fields) = 0;

  if (TYPE_HAS_DESTRUCTOR (t) && !CLASSTYPE_DESTRUCTORS (t))
    /* We thought there was a destructor, but there wasn't.  Some
       parse errors cause this anomalous situation.  */
    TYPE_HAS_DESTRUCTOR (t) = 0;
    
  /* Issue warnings about private constructors and such.  If there are
     no methods, then some public defaults are generated.  */
  maybe_warn_about_overly_private_class (t);

  /* Now sort the methods.  */
  while (len > 2 && TREE_VEC_ELT (method_vec, len-1) == NULL_TREE)
    len--;
  TREE_VEC_LENGTH (method_vec) = len;

  /* The type conversion ops have to live at the front of the vec, so we
     can't sort them.  */
  for (slot = 2; slot < len; ++slot)
    {
      tree fn = TREE_VEC_ELT (method_vec, slot);
  
      if (!DECL_CONV_FN_P (OVL_CURRENT (fn)))
	break;
    }
  if (len - slot > 1)
    qsort (&TREE_VEC_ELT (method_vec, slot), len-slot, sizeof (tree),
	   (int (*)(const void *, const void *))method_name_cmp);
}

/* Emit error when a duplicate definition of a type is seen.  Patch up.  */

void
duplicate_tag_error (t)
     tree t;
{
  cp_error ("redefinition of `%#T'", t);
  cp_error_at ("previous definition of `%#T'", t);

  /* Pretend we haven't defined this type.  */

  /* All of the component_decl's were TREE_CHAINed together in the parser.
     finish_struct_methods walks these chains and assembles all methods with
     the same base name into DECL_CHAINs. Now we don't need the parser chains
     anymore, so we unravel them.  */

  /* This used to be in finish_struct, but it turns out that the
     TREE_CHAIN is used by dbxout_type_methods and perhaps some other
     things...  */
  if (CLASSTYPE_METHOD_VEC (t)) 
    {
      tree method_vec = CLASSTYPE_METHOD_VEC (t);
      int i, len  = TREE_VEC_LENGTH (method_vec);
      for (i = 0; i < len; i++)
	{
	  tree unchain = TREE_VEC_ELT (method_vec, i);
	  while (unchain != NULL_TREE) 
	    {
	      TREE_CHAIN (OVL_CURRENT (unchain)) = NULL_TREE;
	      unchain = OVL_NEXT (unchain);
	    }
	}
    }

  if (TYPE_LANG_SPECIFIC (t))
    {
      tree binfo = TYPE_BINFO (t);
      int interface_only = CLASSTYPE_INTERFACE_ONLY (t);
      int interface_unknown = CLASSTYPE_INTERFACE_UNKNOWN (t);
      tree template_info = CLASSTYPE_TEMPLATE_INFO (t);
      int use_template = CLASSTYPE_USE_TEMPLATE (t);

      memset ((char *) TYPE_LANG_SPECIFIC (t), 0, sizeof (struct lang_type));
      BINFO_BASETYPES(binfo) = NULL_TREE;

      TYPE_BINFO (t) = binfo;
      CLASSTYPE_INTERFACE_ONLY (t) = interface_only;
      SET_CLASSTYPE_INTERFACE_UNKNOWN_X (t, interface_unknown);
      TYPE_REDEFINED (t) = 1;
      CLASSTYPE_TEMPLATE_INFO (t) = template_info;
      CLASSTYPE_USE_TEMPLATE (t) = use_template;
    }
  TYPE_SIZE (t) = NULL_TREE;
  TYPE_MODE (t) = VOIDmode;
  TYPE_FIELDS (t) = NULL_TREE;
  TYPE_METHODS (t) = NULL_TREE;
  TYPE_VFIELD (t) = NULL_TREE;
  TYPE_CONTEXT (t) = NULL_TREE;
  TYPE_NONCOPIED_PARTS (t) = NULL_TREE;
  
  /* Clear TYPE_LANG_FLAGS -- those in TYPE_LANG_SPECIFIC are cleared above.  */
  TYPE_LANG_FLAG_0 (t) = 0;
  TYPE_LANG_FLAG_1 (t) = 0;
  TYPE_LANG_FLAG_2 (t) = 0;
  TYPE_LANG_FLAG_3 (t) = 0;
  TYPE_LANG_FLAG_4 (t) = 0;
  TYPE_LANG_FLAG_5 (t) = 0;
  TYPE_LANG_FLAG_6 (t) = 0;
  /* But not this one.  */
  SET_IS_AGGR_TYPE (t, 1);
}

/* Make the BINFO's vtablehave N entries, including RTTI entries,
   vbase and vcall offsets, etc.  Set its type and call the backend
   to lay it out.  */

static void
layout_vtable_decl (binfo, n)
     tree binfo;
     int n;
{
  tree atype;
  tree vtable;

  atype = build_cplus_array_type (vtable_entry_type, 
				  build_index_type (size_int (n - 1)));
  layout_type (atype);

  /* We may have to grow the vtable.  */
  vtable = get_vtbl_decl_for_binfo (binfo);
  if (!same_type_p (TREE_TYPE (vtable), atype))
    {
      TREE_TYPE (vtable) = atype;
      DECL_SIZE (vtable) = DECL_SIZE_UNIT (vtable) = NULL_TREE;
      layout_decl (vtable, 0);

      /* At one time the vtable info was grabbed 2 words at a time.  This
	 fails on Sparc unless you have 8-byte alignment.  */
      DECL_ALIGN (vtable) = MAX (TYPE_ALIGN (double_type_node),
				 DECL_ALIGN (vtable));
    }
}

/* True iff FNDECL and BASE_FNDECL (both non-static member functions)
   have the same signature.  */

int
same_signature_p (fndecl, base_fndecl)
     tree fndecl, base_fndecl;
{
  /* One destructor overrides another if they are the same kind of
     destructor.  */
  if (DECL_DESTRUCTOR_P (base_fndecl) && DECL_DESTRUCTOR_P (fndecl)
      && special_function_p (base_fndecl) == special_function_p (fndecl))
    return 1;
  /* But a non-destructor never overrides a destructor, nor vice
     versa, nor do different kinds of destructors override
     one-another.  For example, a complete object destructor does not
     override a deleting destructor.  */
  if (DECL_DESTRUCTOR_P (base_fndecl) || DECL_DESTRUCTOR_P (fndecl))
    return 0;

  if (DECL_NAME (fndecl) == DECL_NAME (base_fndecl))
    {
      tree types, base_types;
      types = TYPE_ARG_TYPES (TREE_TYPE (fndecl));
      base_types = TYPE_ARG_TYPES (TREE_TYPE (base_fndecl));
      if ((TYPE_QUALS (TREE_TYPE (TREE_VALUE (base_types)))
	   == TYPE_QUALS (TREE_TYPE (TREE_VALUE (types))))
	  && compparms (TREE_CHAIN (base_types), TREE_CHAIN (types)))
	return 1;
    }
  return 0;
}

typedef struct find_final_overrider_data_s {
  /* The function for which we are trying to find a final overrider.  */
  tree fn;
  /* The base class in which the function was declared.  */
  tree declaring_base;
  /* The most derived class in the hierarchy.  */
  tree most_derived_type;
  /* The final overriding function.  */
  tree overriding_fn;
  /* The functions that we thought might be final overriders, but
     aren't.  */
  tree candidates;
  /* The BINFO for the class in which the final overriding function
     appears.  */
  tree overriding_base;
} find_final_overrider_data;

/* Called from find_final_overrider via dfs_walk.  */

static tree
dfs_find_final_overrider (binfo, data)
     tree binfo;
     void *data;
{
  find_final_overrider_data *ffod = (find_final_overrider_data *) data;

  if (same_type_p (BINFO_TYPE (binfo), 
		   BINFO_TYPE (ffod->declaring_base))
      && tree_int_cst_equal (BINFO_OFFSET (binfo),
			     BINFO_OFFSET (ffod->declaring_base)))
    {
      tree path;
      tree method;

      /* We haven't found an overrider yet.  */
      method = NULL_TREE;
      /* We've found a path to the declaring base.  Walk down the path
	 looking for an overrider for FN.  */
      for (path = reverse_path (binfo);
	   path; 
	   path = TREE_CHAIN (path))
	{
	  method = look_for_overrides_here (BINFO_TYPE (TREE_VALUE (path)),
					    ffod->fn);
	  if (method)
	    break;
	}

      /* If we found an overrider, record the overriding function, and
	 the base from which it came.  */
      if (path)
	{
	  tree base;

	  /* Assume the path is non-virtual.  See if there are any
	     virtual bases from (but not including) the overrider up
	     to and including the base where the function is
	     defined. */
	  for (base = TREE_CHAIN (path); base; base = TREE_CHAIN (base))
	    if (TREE_VIA_VIRTUAL (TREE_VALUE (base)))
	      {
		base = ffod->declaring_base;
		break;
	      }

	  /* If we didn't already have an overrider, or any
	     candidates, then this function is the best candidate so
	     far.  */
	  if (!ffod->overriding_fn && !ffod->candidates)
	    {
	      ffod->overriding_fn = method;
	      ffod->overriding_base = TREE_VALUE (path);
	    }
	  else if (ffod->overriding_fn)
	    {
	      /* We had a best overrider; let's see how this compares.  */

	      if (ffod->overriding_fn == method
		  && (tree_int_cst_equal 
		      (BINFO_OFFSET (TREE_VALUE (path)),
		       BINFO_OFFSET (ffod->overriding_base))))
		/* We found the same overrider we already have, and in the
		   same place; it's still the best.  */;
	      else if (strictly_overrides (ffod->overriding_fn, method))
		/* The old function overrides this function; it's still the
		   best.  */;
	      else if (strictly_overrides (method, ffod->overriding_fn))
		{
		  /* The new function overrides the old; it's now the
		     best.  */
		  ffod->overriding_fn = method;
		  ffod->overriding_base = TREE_VALUE (path);
		}
	      else
		{
		  /* Ambiguous.  */
		  ffod->candidates 
		    = build_tree_list (NULL_TREE,
				       ffod->overriding_fn);
		  if (method != ffod->overriding_fn)
		    ffod->candidates 
		      = tree_cons (NULL_TREE, method, ffod->candidates);
		  ffod->overriding_fn = NULL_TREE;
		  ffod->overriding_base = NULL_TREE;
		}
	    }
	  else
	    {
	      /* We had a list of ambiguous overrides; let's see how this
		 new one compares.  */

	      tree candidates;
	      bool incomparable = false;

	      /* If there were previous candidates, and this function
		 overrides all of them, then it is the new best
		 candidate.  */
	      for (candidates = ffod->candidates;
		   candidates;
		   candidates = TREE_CHAIN (candidates))
		{
		  /* If the candidate overrides the METHOD, then we
		     needn't worry about it any further.  */
		  if (strictly_overrides (TREE_VALUE (candidates),
					  method))
		    {
		      method = NULL_TREE;
		      break;
		    }

		  /* If the METHOD doesn't override the candidate,
		     then it is incomporable.  */
		  if (!strictly_overrides (method,
					   TREE_VALUE (candidates)))
		    incomparable = true;
		}

	      /* If METHOD overrode all the candidates, then it is the
		 new best candidate.  */
	      if (!candidates && !incomparable)
		{
		  ffod->overriding_fn = method;
		  ffod->overriding_base = TREE_VALUE (path);
		  ffod->candidates = NULL_TREE;
		}
	      /* If METHOD didn't override all the candidates, then it
		 is another candidate.  */
	      else if (method && incomparable)
		ffod->candidates 
		  = tree_cons (NULL_TREE, method, ffod->candidates);
	    }
	}
    }

  return NULL_TREE;
}

/* Returns a TREE_LIST whose TREE_PURPOSE is the final overrider for
   FN and whose TREE_VALUE is the binfo for the base where the
   overriding occurs.  BINFO (in the hierarchy dominated by T) is the
   base object in which FN is declared.  */

static tree
find_final_overrider (t, binfo, fn)
     tree t;
     tree binfo;
     tree fn;
{
  find_final_overrider_data ffod;

  /* Getting this right is a little tricky.  This is legal:

       struct S { virtual void f (); };
       struct T { virtual void f (); };
       struct U : public S, public T { };

     even though calling `f' in `U' is ambiguous.  But, 

       struct R { virtual void f(); };
       struct S : virtual public R { virtual void f (); };
       struct T : virtual public R { virtual void f (); };
       struct U : public S, public T { };

     is not -- there's no way to decide whether to put `S::f' or
     `T::f' in the vtable for `R'.  
     
     The solution is to look at all paths to BINFO.  If we find
     different overriders along any two, then there is a problem.  */
  ffod.fn = fn;
  ffod.declaring_base = binfo;
  ffod.most_derived_type = t;
  ffod.overriding_fn = NULL_TREE;
  ffod.overriding_base = NULL_TREE;
  ffod.candidates = NULL_TREE;

  dfs_walk (TYPE_BINFO (t),
	    dfs_find_final_overrider,
	    NULL,
	    &ffod);

  /* If there was no winner, issue an error message.  */
  if (!ffod.overriding_fn)
    {
      cp_error ("no unique final overrider for `%D' in `%T'", fn, t);
      return error_mark_node;
    }

  return build_tree_list (ffod.overriding_fn, ffod.overriding_base);
}

/* Returns the function from the BINFO_VIRTUALS entry in T which matches
   the signature of FUNCTION_DECL FN, or NULL_TREE if none.  In other words,
   the function that the slot in T's primary vtable points to.  */

static tree get_matching_virtual PARAMS ((tree, tree));
static tree
get_matching_virtual (t, fn)
     tree t, fn;
{
  tree f;

  for (f = BINFO_VIRTUALS (TYPE_BINFO (t)); f; f = TREE_CHAIN (f))
    if (same_signature_p (BV_FN (f), fn))
      return BV_FN (f);
  return NULL_TREE;
}

/* Update an entry in the vtable for BINFO, which is in the hierarchy
   dominated by T.  FN has been overriden in BINFO; VIRTUALS points to the
   corresponding position in the BINFO_VIRTUALS list.  */

static void
update_vtable_entry_for_fn (t, binfo, fn, virtuals)
     tree t;
     tree binfo;
     tree fn;
     tree *virtuals;
{
  tree b;
  tree overrider;
  tree delta;
  tree virtual_base;
  tree first_defn;

  /* Find the nearest primary base (possibly binfo itself) which defines
     this function; this is the class the caller will convert to when
     calling FN through BINFO.  */
  for (b = binfo; ; b = get_primary_binfo (b))
    {
      if (look_for_overrides_here (BINFO_TYPE (b), fn))
	break;
    }
  first_defn = b;

  /* Find the final overrider.  */
  overrider = find_final_overrider (t, b, fn);
  if (overrider == error_mark_node)
    return;

  /* Assume that we will produce a thunk that convert all the way to
     the final overrider, and not to an intermediate virtual base.  */
  virtual_base = NULL_TREE;

  /* Under the new ABI, we will convert to an intermediate virtual
     base first, and then use the vcall offset located there to finish
     the conversion.  */
  while (b)
    {
      /* If we find the final overrider, then we can stop
	 walking.  */
      if (same_type_p (BINFO_TYPE (b), 
		       BINFO_TYPE (TREE_VALUE (overrider))))
	break;

      /* If we find a virtual base, and we haven't yet found the
	 overrider, then there is a virtual base between the
	 declaring base (first_defn) and the final overrider.  */
      if (!virtual_base && TREE_VIA_VIRTUAL (b))
	virtual_base = b;

      b = BINFO_INHERITANCE_CHAIN (b);
    }

  /* Compute the constant adjustment to the `this' pointer.  The
     `this' pointer, when this function is called, will point at BINFO
     (or one of its primary bases, which are at the same offset).  */

  if (virtual_base)
    /* The `this' pointer needs to be adjusted from the declaration to
       the nearest virtual base.  */
    delta = size_diffop (BINFO_OFFSET (virtual_base),
			 BINFO_OFFSET (first_defn));
  else
    {
      /* The `this' pointer needs to be adjusted from pointing to
	 BINFO to pointing at the base where the final overrider
	 appears.  */
      delta = size_diffop (BINFO_OFFSET (TREE_VALUE (overrider)),
			   BINFO_OFFSET (binfo));

      if (! integer_zerop (delta))
	{
	  /* We'll need a thunk.  But if we have a (perhaps formerly)
	     primary virtual base, we have a vcall slot for this function,
	     so we can use it rather than create a non-virtual thunk.  */
	  
	  b = get_primary_binfo (first_defn);
	  for (; b; b = get_primary_binfo (b))
	    {
	      tree f = get_matching_virtual (BINFO_TYPE (b), fn);
	      if (!f)
		/* b doesn't have this function; no suitable vbase.  */
		break;
	      if (TREE_VIA_VIRTUAL (b))
		{
		  /* Found one; we can treat ourselves as a virtual base.  */
		  virtual_base = binfo;
		  delta = size_zero_node;
		  break;
		}
	    }
	}
    }

  modify_vtable_entry (t, 
		       binfo, 
		       TREE_PURPOSE (overrider),
		       delta,
		       virtuals);

  if (virtual_base)
    BV_USE_VCALL_INDEX_P (*virtuals) = 1;
}

/* Called from modify_all_vtables via dfs_walk.  */

static tree
dfs_modify_vtables (binfo, data)
     tree binfo;
     void *data;
{
  if (/* There's no need to modify the vtable for a non-virtual
         primary base; we're not going to use that vtable anyhow.
	 We do still need to do this for virtual primary bases, as they
	 could become non-primary in a construction vtable.  */
      (!BINFO_PRIMARY_P (binfo) || TREE_VIA_VIRTUAL (binfo))
      /* Similarly, a base without a vtable needs no modification.  */
      && CLASSTYPE_VFIELDS (BINFO_TYPE (binfo)))
    {
      tree t;
      tree virtuals;
      tree old_virtuals;

      t = (tree) data;

      /* If we're supporting RTTI then we always need a new vtable to
	 point to the RTTI information.  Under the new ABI we may need
	 a new vtable to contain vcall and vbase offsets.  */
      make_new_vtable (t, binfo);
      
      /* Now, go through each of the virtual functions in the virtual
	 function table for BINFO.  Find the final overrider, and
	 update the BINFO_VIRTUALS list appropriately.  */
      for (virtuals = BINFO_VIRTUALS (binfo),
	     old_virtuals = BINFO_VIRTUALS (TYPE_BINFO (BINFO_TYPE (binfo)));
	   virtuals;
	   virtuals = TREE_CHAIN (virtuals),
	     old_virtuals = TREE_CHAIN (old_virtuals))
	update_vtable_entry_for_fn (t, 
				    binfo, 
				    BV_FN (old_virtuals),
				    &virtuals);
    }

  SET_BINFO_MARKED (binfo);

  return NULL_TREE;
}

/* Update all of the primary and secondary vtables for T.  Create new
   vtables as required, and initialize their RTTI information.  Each
   of the functions in OVERRIDDEN_VIRTUALS overrides a virtual
   function from a base class; find and modify the appropriate entries
   to point to the overriding functions.  Returns a list, in
   declaration order, of the functions that are overridden in this
   class, but do not appear in the primary base class vtable, and
   which should therefore be appended to the end of the vtable for T.  */

static tree
modify_all_vtables (t, vfuns_p, overridden_virtuals)
     tree t;
     int *vfuns_p;
     tree overridden_virtuals;
{
  tree binfo;

  binfo = TYPE_BINFO (t);

  /* Update all of the vtables.  */
  dfs_walk (binfo, 
	    dfs_modify_vtables, 
	    dfs_unmarked_real_bases_queue_p,
	    t);
  dfs_walk (binfo, dfs_unmark, dfs_marked_real_bases_queue_p, t);

  /* If we should include overriding functions for secondary vtables
     in our primary vtable, add them now.  */
  if (all_overridden_vfuns_in_vtables_p ())
    {
      tree *fnsp = &overridden_virtuals;

      while (*fnsp)
	{
	  tree fn = TREE_VALUE (*fnsp);

	  if (!BINFO_VIRTUALS (binfo)
	      || !value_member (fn, BINFO_VIRTUALS (binfo)))
	    {
	      /* Set the vtable index.  */
	      set_vindex (fn, vfuns_p);
	      /* We don't need to convert to a base class when calling
		 this function.  */
	      DECL_VIRTUAL_CONTEXT (fn) = t;

	      /* We don't need to adjust the `this' pointer when
		 calling this function.  */
	      BV_DELTA (*fnsp) = integer_zero_node;
	      BV_VCALL_INDEX (*fnsp) = NULL_TREE;

	      /* This is an overridden function not already in our
		 vtable.  Keep it.  */
	      fnsp = &TREE_CHAIN (*fnsp);
	    }
	  else
	    /* We've already got an entry for this function.  Skip
	       it.  */
	    *fnsp = TREE_CHAIN (*fnsp);
	}
    }
  else
    overridden_virtuals = NULL_TREE;

  return overridden_virtuals;
}

/* Here, we already know that they match in every respect.
   All we have to check is where they had their declarations.  */

static int 
strictly_overrides (fndecl1, fndecl2)
     tree fndecl1, fndecl2;
{
  int distance = get_base_distance (DECL_CONTEXT (fndecl2),
				    DECL_CONTEXT (fndecl1),
				    0, (tree *)0);
  if (distance == -2 || distance > 0)
    return 1;
  return 0;
}

/* Get the base virtual function declarations in T that are either
   overridden or hidden by FNDECL as a list.  We set TREE_PURPOSE with
   the overrider/hider.  */

static tree
get_basefndecls (fndecl, t)
     tree fndecl, t;
{
  tree methods = TYPE_METHODS (t);
  tree base_fndecls = NULL_TREE;
  tree binfos = BINFO_BASETYPES (TYPE_BINFO (t));
  int i, n_baseclasses = binfos ? TREE_VEC_LENGTH (binfos) : 0;

  while (methods)
    {
      if (TREE_CODE (methods) == FUNCTION_DECL
	  && DECL_VINDEX (methods) != NULL_TREE
	  && DECL_NAME (fndecl) == DECL_NAME (methods))
	base_fndecls = tree_cons (fndecl, methods, base_fndecls);

      methods = TREE_CHAIN (methods);
    }

  if (base_fndecls)
    return base_fndecls;

  for (i = 0; i < n_baseclasses; i++)
    {
      tree base_binfo = TREE_VEC_ELT (binfos, i);
      tree basetype = BINFO_TYPE (base_binfo);

      base_fndecls = chainon (get_basefndecls (fndecl, basetype),
			      base_fndecls);
    }

  return base_fndecls;
}

/* Mark the functions that have been hidden with their overriders.
   Since we start out with all functions already marked with a hider,
   no need to mark functions that are just hidden.

   Subroutine of warn_hidden.  */

static void
mark_overriders (fndecl, base_fndecls)
     tree fndecl, base_fndecls;
{
  for (; base_fndecls; base_fndecls = TREE_CHAIN (base_fndecls))
    if (same_signature_p (fndecl, TREE_VALUE (base_fndecls)))
      TREE_PURPOSE (base_fndecls) = fndecl;
}

/* If this declaration supersedes the declaration of
   a method declared virtual in the base class, then
   mark this field as being virtual as well.  */

static void
check_for_override (decl, ctype)
     tree decl, ctype;
{
  if (TREE_CODE (decl) == TEMPLATE_DECL)
    /* In [temp.mem] we have:

         A specialization of a member function template does not
         override a virtual function from a base class.  */
    return;
  if ((DECL_DESTRUCTOR_P (decl)
       || IDENTIFIER_VIRTUAL_P (DECL_NAME (decl)))
      && look_for_overrides (ctype, decl)
      && !DECL_STATIC_FUNCTION_P (decl))
    {
      /* Set DECL_VINDEX to a value that is neither an
	 INTEGER_CST nor the error_mark_node so that
	 add_virtual_function will realize this is an
	 overriding function.  */
      DECL_VINDEX (decl) = decl;
    }
  if (DECL_VIRTUAL_P (decl))
    {
      if (DECL_VINDEX (decl) == NULL_TREE)
	DECL_VINDEX (decl) = error_mark_node;
      IDENTIFIER_VIRTUAL_P (DECL_NAME (decl)) = 1;
    }
}

/* Warn about hidden virtual functions that are not overridden in t.
   We know that constructors and destructors don't apply.  */

void
warn_hidden (t)
     tree t;
{
  tree method_vec = CLASSTYPE_METHOD_VEC (t);
  int n_methods = method_vec ? TREE_VEC_LENGTH (method_vec) : 0;
  int i;

  /* We go through each separately named virtual function.  */
  for (i = 2; i < n_methods && TREE_VEC_ELT (method_vec, i); ++i)
    {
      tree fns = TREE_VEC_ELT (method_vec, i);
      tree fndecl = NULL_TREE;

      tree base_fndecls = NULL_TREE;
      tree binfos = BINFO_BASETYPES (TYPE_BINFO (t));
      int i, n_baseclasses = binfos ? TREE_VEC_LENGTH (binfos) : 0;

      /* First see if we have any virtual functions in this batch.  */
      for (; fns; fns = OVL_NEXT (fns))
	{
	  fndecl = OVL_CURRENT (fns);
	  if (DECL_VINDEX (fndecl))
	    break;
	}

      if (fns == NULL_TREE)
	continue;

      /* First we get a list of all possible functions that might be
	 hidden from each base class.  */
      for (i = 0; i < n_baseclasses; i++)
	{
	  tree base_binfo = TREE_VEC_ELT (binfos, i);
	  tree basetype = BINFO_TYPE (base_binfo);

	  base_fndecls = chainon (get_basefndecls (fndecl, basetype),
				  base_fndecls);
	}

      fns = OVL_NEXT (fns);

      /* ...then mark up all the base functions with overriders, preferring
	 overriders to hiders.  */
      if (base_fndecls)
	for (; fns; fns = OVL_NEXT (fns))
	  {
	    fndecl = OVL_CURRENT (fns);
	    if (DECL_VINDEX (fndecl))
	      mark_overriders (fndecl, base_fndecls);
	  }

      /* Now give a warning for all base functions without overriders,
	 as they are hidden.  */
      for (; base_fndecls; base_fndecls = TREE_CHAIN (base_fndecls))
	if (!same_signature_p (TREE_PURPOSE (base_fndecls),
			       TREE_VALUE (base_fndecls)))
	  {
	    /* Here we know it is a hider, and no overrider exists.  */
	    cp_warning_at ("`%D' was hidden", TREE_VALUE (base_fndecls));
	    cp_warning_at ("  by `%D'", TREE_PURPOSE (base_fndecls));
	  }
    }
}

/* Check for things that are invalid.  There are probably plenty of other
   things we should check for also.  */

static void
finish_struct_anon (t)
     tree t;
{
  tree field;

  for (field = TYPE_FIELDS (t); field; field = TREE_CHAIN (field))
    {
      if (TREE_STATIC (field))
	continue;
      if (TREE_CODE (field) != FIELD_DECL)
	continue;

      if (DECL_NAME (field) == NULL_TREE
	  && ANON_AGGR_TYPE_P (TREE_TYPE (field)))
	{
	  tree elt = TYPE_FIELDS (TREE_TYPE (field));
	  for (; elt; elt = TREE_CHAIN (elt))
	    {
	      if (DECL_ARTIFICIAL (elt))
		continue;

	      if (DECL_NAME (elt) == constructor_name (t))
		cp_pedwarn_at ("ISO C++ forbids member `%D' with same name as enclosing class",
			       elt);

	      if (TREE_CODE (elt) != FIELD_DECL)
		{
		  cp_pedwarn_at ("`%#D' invalid; an anonymous union can only have non-static data members",
				 elt);
		  continue;
		}

	      if (TREE_PRIVATE (elt))
		cp_pedwarn_at ("private member `%#D' in anonymous union",
			       elt);
	      else if (TREE_PROTECTED (elt))
		cp_pedwarn_at ("protected member `%#D' in anonymous union",
			       elt);

	      TREE_PRIVATE (elt) = TREE_PRIVATE (field);
	      TREE_PROTECTED (elt) = TREE_PROTECTED (field);
	    }
	}
    }
}

/* Create default constructors, assignment operators, and so forth for
   the type indicated by T, if they are needed.
   CANT_HAVE_DEFAULT_CTOR, CANT_HAVE_CONST_CTOR, and
   CANT_HAVE_CONST_ASSIGNMENT are nonzero if, for whatever reason, the
   class cannot have a default constructor, copy constructor taking a
   const reference argument, or an assignment operator taking a const
   reference, respectively.  If a virtual destructor is created, its
   DECL is returned; otherwise the return value is NULL_TREE.  */

static tree
add_implicitly_declared_members (t, cant_have_default_ctor,
				 cant_have_const_cctor,
				 cant_have_const_assignment)
     tree t;
     int cant_have_default_ctor;
     int cant_have_const_cctor;
     int cant_have_const_assignment;
{
  tree default_fn;
  tree implicit_fns = NULL_TREE;
  tree virtual_dtor = NULL_TREE;
  tree *f;

  /* Destructor.  */
  if (TYPE_HAS_NONTRIVIAL_DESTRUCTOR (t) && !TYPE_HAS_DESTRUCTOR (t))
    {
      default_fn = implicitly_declare_fn (sfk_destructor, t, /*const_p=*/0);
      check_for_override (default_fn, t);

      /* If we couldn't make it work, then pretend we didn't need it.  */
      if (default_fn == void_type_node)
	TYPE_HAS_NONTRIVIAL_DESTRUCTOR (t) = 0;
      else
	{
	  TREE_CHAIN (default_fn) = implicit_fns;
	  implicit_fns = default_fn;

	  if (DECL_VINDEX (default_fn))
	    virtual_dtor = default_fn;
	}
    }
  else
    /* Any non-implicit destructor is non-trivial.  */
    TYPE_HAS_NONTRIVIAL_DESTRUCTOR (t) |= TYPE_HAS_DESTRUCTOR (t);

  /* Default constructor.  */
  if (! TYPE_HAS_CONSTRUCTOR (t) && ! cant_have_default_ctor)
    {
      default_fn = implicitly_declare_fn (sfk_constructor, t, /*const_p=*/0);
      TREE_CHAIN (default_fn) = implicit_fns;
      implicit_fns = default_fn;
    }

  /* Copy constructor.  */
  if (! TYPE_HAS_INIT_REF (t) && ! TYPE_FOR_JAVA (t))
    {
      /* ARM 12.18: You get either X(X&) or X(const X&), but
	 not both.  --Chip  */
      default_fn 
	= implicitly_declare_fn (sfk_copy_constructor, t,
				 /*const_p=*/!cant_have_const_cctor);
      TREE_CHAIN (default_fn) = implicit_fns;
      implicit_fns = default_fn;
    }

  /* Assignment operator.  */
  if (! TYPE_HAS_ASSIGN_REF (t) && ! TYPE_FOR_JAVA (t))
    {
      default_fn 
	= implicitly_declare_fn (sfk_assignment_operator, t,
				 /*const_p=*/!cant_have_const_assignment);
      TREE_CHAIN (default_fn) = implicit_fns;
      implicit_fns = default_fn;
    }

  /* Now, hook all of the new functions on to TYPE_METHODS,
     and add them to the CLASSTYPE_METHOD_VEC.  */
  for (f = &implicit_fns; *f; f = &TREE_CHAIN (*f))
    add_method (t, *f, /*error_p=*/0);
  *f = TYPE_METHODS (t);
  TYPE_METHODS (t) = implicit_fns;

  return virtual_dtor;
}

/* Subroutine of finish_struct_1.  Recursively count the number of fields
   in TYPE, including anonymous union members.  */

static int
count_fields (fields)
     tree fields;
{
  tree x;
  int n_fields = 0;
  for (x = fields; x; x = TREE_CHAIN (x))
    {
      if (TREE_CODE (x) == FIELD_DECL && ANON_AGGR_TYPE_P (TREE_TYPE (x)))
	n_fields += count_fields (TYPE_FIELDS (TREE_TYPE (x)));
      else
	n_fields += 1;
    }
  return n_fields;
}

/* Subroutine of finish_struct_1.  Recursively add all the fields in the
   TREE_LIST FIELDS to the TREE_VEC FIELD_VEC, starting at offset IDX.  */

static int
add_fields_to_vec (fields, field_vec, idx)
     tree fields, field_vec;
     int idx;
{
  tree x;
  for (x = fields; x; x = TREE_CHAIN (x))
    {
      if (TREE_CODE (x) == FIELD_DECL && ANON_AGGR_TYPE_P (TREE_TYPE (x)))
	idx = add_fields_to_vec (TYPE_FIELDS (TREE_TYPE (x)), field_vec, idx);
      else
	TREE_VEC_ELT (field_vec, idx++) = x;
    }
  return idx;
}

/* FIELD is a bit-field.  We are finishing the processing for its
   enclosing type.  Issue any appropriate messages and set appropriate
   flags.  */

static void
check_bitfield_decl (field)
     tree field;
{
  tree type = TREE_TYPE (field);
  tree w = NULL_TREE;

  /* Detect invalid bit-field type.  */
  if (DECL_INITIAL (field)
      && ! INTEGRAL_TYPE_P (TREE_TYPE (field)))
    {
      cp_error_at ("bit-field `%#D' with non-integral type", field);
      w = error_mark_node;
    }

  /* Detect and ignore out of range field width.  */
  if (DECL_INITIAL (field))
    {
      w = DECL_INITIAL (field);

      /* Avoid the non_lvalue wrapper added by fold for PLUS_EXPRs.  */
      STRIP_NOPS (w);

      /* detect invalid field size.  */
      if (TREE_CODE (w) == CONST_DECL)
	w = DECL_INITIAL (w);
      else
	w = decl_constant_value (w);

      if (TREE_CODE (w) != INTEGER_CST)
	{
	  cp_error_at ("bit-field `%D' width not an integer constant",
		       field);
	  w = error_mark_node;
	}
      else if (tree_int_cst_sgn (w) < 0)
	{
	  cp_error_at ("negative width in bit-field `%D'", field);
	  w = error_mark_node;
	}
      else if (integer_zerop (w) && DECL_NAME (field) != 0)
	{
	  cp_error_at ("zero width for bit-field `%D'", field);
	  w = error_mark_node;
	}
      else if (compare_tree_int (w, TYPE_PRECISION (type)) > 0
	       && TREE_CODE (type) != ENUMERAL_TYPE
	       && TREE_CODE (type) != BOOLEAN_TYPE)
	cp_warning_at ("width of `%D' exceeds its type", field);
      else if (TREE_CODE (type) == ENUMERAL_TYPE
	       && (0 > compare_tree_int (w,
					 min_precision (TYPE_MIN_VALUE (type),
							TREE_UNSIGNED (type)))
		   ||  0 > compare_tree_int (w,
					     min_precision
					     (TYPE_MAX_VALUE (type),
					      TREE_UNSIGNED (type)))))
	cp_warning_at ("`%D' is too small to hold all values of `%#T'",
		       field, type);
    }
  
  /* Remove the bit-field width indicator so that the rest of the
     compiler does not treat that value as an initializer.  */
  DECL_INITIAL (field) = NULL_TREE;

  if (w != error_mark_node)
    {
      DECL_SIZE (field) = convert (bitsizetype, w);
      DECL_BIT_FIELD (field) = 1;

      if (integer_zerop (w))
	{
#ifdef EMPTY_FIELD_BOUNDARY
	  DECL_ALIGN (field) = MAX (DECL_ALIGN (field), 
				    EMPTY_FIELD_BOUNDARY);
#endif
#ifdef PCC_BITFIELD_TYPE_MATTERS
	  if (PCC_BITFIELD_TYPE_MATTERS)
	    {
	      DECL_ALIGN (field) = MAX (DECL_ALIGN (field), 
					TYPE_ALIGN (type));
	      DECL_USER_ALIGN (field) |= TYPE_USER_ALIGN (type);
	    }
#endif
	}
    }
  else
    {
      /* Non-bit-fields are aligned for their type.  */
      DECL_BIT_FIELD (field) = 0;
      CLEAR_DECL_C_BIT_FIELD (field);
      DECL_ALIGN (field) = MAX (DECL_ALIGN (field), TYPE_ALIGN (type));
      DECL_USER_ALIGN (field) |= TYPE_USER_ALIGN (type);
    }
}

/* FIELD is a non bit-field.  We are finishing the processing for its
   enclosing type T.  Issue any appropriate messages and set appropriate
   flags.  */

static void
check_field_decl (field, t, cant_have_const_ctor,
		  cant_have_default_ctor, no_const_asn_ref,
		  any_default_members)
     tree field;
     tree t;
     int *cant_have_const_ctor;
     int *cant_have_default_ctor;
     int *no_const_asn_ref;
     int *any_default_members;
{
  tree type = strip_array_types (TREE_TYPE (field));

  /* An anonymous union cannot contain any fields which would change
     the settings of CANT_HAVE_CONST_CTOR and friends.  */
  if (ANON_UNION_TYPE_P (type))
    ;
  /* And, we don't set TYPE_HAS_CONST_INIT_REF, etc., for anonymous
     structs.  So, we recurse through their fields here.  */
  else if (ANON_AGGR_TYPE_P (type))
    {
      tree fields;

      for (fields = TYPE_FIELDS (type); fields; fields = TREE_CHAIN (fields))
	if (TREE_CODE (fields) == FIELD_DECL && !DECL_C_BIT_FIELD (field))
	  check_field_decl (fields, t, cant_have_const_ctor,
			    cant_have_default_ctor, no_const_asn_ref,
			    any_default_members);
    }
  /* Check members with class type for constructors, destructors,
     etc.  */
  else if (CLASS_TYPE_P (type))
    {
      /* Never let anything with uninheritable virtuals
	 make it through without complaint.  */
      abstract_virtuals_error (field, type);
		      
      if (TREE_CODE (t) == UNION_TYPE)
	{
	  if (TYPE_NEEDS_CONSTRUCTING (type))
	    cp_error_at ("member `%#D' with constructor not allowed in union",
			 field);
	  if (TYPE_HAS_NONTRIVIAL_DESTRUCTOR (type))
	    cp_error_at ("member `%#D' with destructor not allowed in union",
			 field);
	  if (TYPE_HAS_COMPLEX_ASSIGN_REF (type))
	    cp_error_at ("member `%#D' with copy assignment operator not allowed in union",
			 field);
	}
      else
	{
	  TYPE_NEEDS_CONSTRUCTING (t) |= TYPE_NEEDS_CONSTRUCTING (type);
	  TYPE_HAS_NONTRIVIAL_DESTRUCTOR (t) 
	    |= TYPE_HAS_NONTRIVIAL_DESTRUCTOR (type);
	  TYPE_HAS_COMPLEX_ASSIGN_REF (t) |= TYPE_HAS_COMPLEX_ASSIGN_REF (type);
	  TYPE_HAS_COMPLEX_INIT_REF (t) |= TYPE_HAS_COMPLEX_INIT_REF (type);
	}

      if (!TYPE_HAS_CONST_INIT_REF (type))
	*cant_have_const_ctor = 1;

      if (!TYPE_HAS_CONST_ASSIGN_REF (type))
	*no_const_asn_ref = 1;

      if (TYPE_HAS_CONSTRUCTOR (type)
	  && ! TYPE_HAS_DEFAULT_CONSTRUCTOR (type))
	*cant_have_default_ctor = 1;
    }
  if (DECL_INITIAL (field) != NULL_TREE)
    {
      /* `build_class_init_list' does not recognize
	 non-FIELD_DECLs.  */
      if (TREE_CODE (t) == UNION_TYPE && any_default_members != 0)
	cp_error_at ("multiple fields in union `%T' initialized");
      *any_default_members = 1;
    }

  /* Non-bit-fields are aligned for their type, except packed fields
     which require only BITS_PER_UNIT alignment.  */
  DECL_ALIGN (field) = MAX (DECL_ALIGN (field), 
			    (DECL_PACKED (field) 
			     ? BITS_PER_UNIT
			     : TYPE_ALIGN (TREE_TYPE (field))));
  if (! DECL_PACKED (field))
    DECL_USER_ALIGN (field) |= TYPE_USER_ALIGN (TREE_TYPE (field));
}

/* Check the data members (both static and non-static), class-scoped
   typedefs, etc., appearing in the declaration of T.  Issue
   appropriate diagnostics.  Sets ACCESS_DECLS to a list (in
   declaration order) of access declarations; each TREE_VALUE in this
   list is a USING_DECL.

   In addition, set the following flags:

     EMPTY_P
       The class is empty, i.e., contains no non-static data members.

     CANT_HAVE_DEFAULT_CTOR_P
       This class cannot have an implicitly generated default
       constructor.

     CANT_HAVE_CONST_CTOR_P
       This class cannot have an implicitly generated copy constructor
       taking a const reference.

     CANT_HAVE_CONST_ASN_REF
       This class cannot have an implicitly generated assignment
       operator taking a const reference.

   All of these flags should be initialized before calling this
   function.

   Returns a pointer to the end of the TYPE_FIELDs chain; additional
   fields can be added by adding to this chain.  */

static void
check_field_decls (t, access_decls, empty_p, 
		   cant_have_default_ctor_p, cant_have_const_ctor_p,
		   no_const_asn_ref_p)
     tree t;
     tree *access_decls;
     int *empty_p;
     int *cant_have_default_ctor_p;
     int *cant_have_const_ctor_p;
     int *no_const_asn_ref_p;
{
  tree *field;
  tree *next;
  int has_pointers;
  int any_default_members;

  /* First, delete any duplicate fields.  */
  delete_duplicate_fields (TYPE_FIELDS (t));

  /* Assume there are no access declarations.  */
  *access_decls = NULL_TREE;
  /* Assume this class has no pointer members.  */
  has_pointers = 0;
  /* Assume none of the members of this class have default
     initializations.  */
  any_default_members = 0;

  for (field = &TYPE_FIELDS (t); *field; field = next)
    {
      tree x = *field;
      tree type = TREE_TYPE (x);

      GNU_xref_member (current_class_name, x);

      next = &TREE_CHAIN (x);

      if (TREE_CODE (x) == FIELD_DECL)
	{
	  DECL_PACKED (x) |= TYPE_PACKED (t);

	  if (DECL_C_BIT_FIELD (x) && integer_zerop (DECL_INITIAL (x)))
	    /* We don't treat zero-width bitfields as making a class
	       non-empty.  */
	    ;
	  else
	    {
	      /* The class is non-empty.  */
	      *empty_p = 0;
	      /* The class is not even nearly empty.  */
	      CLASSTYPE_NEARLY_EMPTY_P (t) = 0;
	    }
	}

      if (TREE_CODE (x) == USING_DECL)
	{
	  /* Prune the access declaration from the list of fields.  */
	  *field = TREE_CHAIN (x);

	  /* Save the access declarations for our caller.  */
	  *access_decls = tree_cons (NULL_TREE, x, *access_decls);

	  /* Since we've reset *FIELD there's no reason to skip to the
	     next field.  */
	  next = field;
	  continue;
	}

      if (TREE_CODE (x) == TYPE_DECL
	  || TREE_CODE (x) == TEMPLATE_DECL)
	continue;

      /* If we've gotten this far, it's a data member, possibly static,
	 or an enumerator.  */

      DECL_CONTEXT (x) = t;

      /* ``A local class cannot have static data members.'' ARM 9.4 */
      if (current_function_decl && TREE_STATIC (x))
	cp_error_at ("field `%D' in local class cannot be static", x);

      /* Perform error checking that did not get done in
	 grokdeclarator.  */
      if (TREE_CODE (type) == FUNCTION_TYPE)
	{
	  cp_error_at ("field `%D' invalidly declared function type",
		       x);
	  type = build_pointer_type (type);
	  TREE_TYPE (x) = type;
	}
      else if (TREE_CODE (type) == METHOD_TYPE)
	{
	  cp_error_at ("field `%D' invalidly declared method type", x);
	  type = build_pointer_type (type);
	  TREE_TYPE (x) = type;
	}
      else if (TREE_CODE (type) == OFFSET_TYPE)
	{
	  cp_error_at ("field `%D' invalidly declared offset type", x);
	  type = build_pointer_type (type);
	  TREE_TYPE (x) = type;
	}

      if (type == error_mark_node)
	continue;
	  
      /* When this goes into scope, it will be a non-local reference.  */
      DECL_NONLOCAL (x) = 1;

      if (TREE_CODE (x) == CONST_DECL)
	continue;

      if (TREE_CODE (x) == VAR_DECL)
	{
	  if (TREE_CODE (t) == UNION_TYPE)
	    /* Unions cannot have static members.  */
	    cp_error_at ("field `%D' declared static in union", x);
	      
	  continue;
	}

      /* Now it can only be a FIELD_DECL.  */

      if (TREE_PRIVATE (x) || TREE_PROTECTED (x))
	CLASSTYPE_NON_AGGREGATE (t) = 1;

      /* If this is of reference type, check if it needs an init.
	 Also do a little ANSI jig if necessary.  */
      if (TREE_CODE (type) == REFERENCE_TYPE)
 	{
	  CLASSTYPE_NON_POD_P (t) = 1;
	  if (DECL_INITIAL (x) == NULL_TREE)
	    CLASSTYPE_REF_FIELDS_NEED_INIT (t) = 1;

	  /* ARM $12.6.2: [A member initializer list] (or, for an
	     aggregate, initialization by a brace-enclosed list) is the
	     only way to initialize nonstatic const and reference
	     members.  */
	  *cant_have_default_ctor_p = 1;
	  TYPE_HAS_COMPLEX_ASSIGN_REF (t) = 1;

	  if (! TYPE_HAS_CONSTRUCTOR (t) && extra_warnings)
            cp_warning_at ("non-static reference `%#D' in class without a constructor", x);
	}

      type = strip_array_types (type);
      
      if (TREE_CODE (type) == POINTER_TYPE)
	has_pointers = 1;

      if (DECL_MUTABLE_P (x) || TYPE_HAS_MUTABLE_P (type))
	CLASSTYPE_HAS_MUTABLE (t) = 1;

      if (! pod_type_p (type))
        /* DR 148 now allows pointers to members (which are POD themselves),
           to be allowed in POD structs.  */
	CLASSTYPE_NON_POD_P (t) = 1;

      /* If any field is const, the structure type is pseudo-const.  */
      if (CP_TYPE_CONST_P (type))
	{
	  C_TYPE_FIELDS_READONLY (t) = 1;
	  if (DECL_INITIAL (x) == NULL_TREE)
	    CLASSTYPE_READONLY_FIELDS_NEED_INIT (t) = 1;

	  /* ARM $12.6.2: [A member initializer list] (or, for an
	     aggregate, initialization by a brace-enclosed list) is the
	     only way to initialize nonstatic const and reference
	     members.  */
	  *cant_have_default_ctor_p = 1;
	  TYPE_HAS_COMPLEX_ASSIGN_REF (t) = 1;

	  if (! TYPE_HAS_CONSTRUCTOR (t) && extra_warnings)
            cp_warning_at ("non-static const member `%#D' in class without a constructor", x);
	}
      /* A field that is pseudo-const makes the structure likewise.  */
      else if (IS_AGGR_TYPE (type))
	{
	  C_TYPE_FIELDS_READONLY (t) |= C_TYPE_FIELDS_READONLY (type);
	  CLASSTYPE_READONLY_FIELDS_NEED_INIT (t) 
	    |= CLASSTYPE_READONLY_FIELDS_NEED_INIT (type);
	}

      /* Core issue 80: A nonstatic data member is required to have a
	 different name from the class iff the class has a
	 user-defined constructor.  */
      if (DECL_NAME (x) == constructor_name (t)
	  && TYPE_HAS_CONSTRUCTOR (t))
	cp_pedwarn_at ("field `%#D' with same name as class", x);

      /* We set DECL_C_BIT_FIELD in grokbitfield.
	 If the type and width are valid, we'll also set DECL_BIT_FIELD.  */
      if (DECL_C_BIT_FIELD (x))
	check_bitfield_decl (x);
      else
	check_field_decl (x, t,
			  cant_have_const_ctor_p,
			  cant_have_default_ctor_p, 
			  no_const_asn_ref_p,
			  &any_default_members);
    }

  /* Effective C++ rule 11.  */
  if (has_pointers && warn_ecpp && TYPE_HAS_CONSTRUCTOR (t)
      && ! (TYPE_HAS_INIT_REF (t) && TYPE_HAS_ASSIGN_REF (t)))
    {
      cp_warning ("`%#T' has pointer data members", t);
      
      if (! TYPE_HAS_INIT_REF (t))
	{
	  cp_warning ("  but does not override `%T(const %T&)'", t, t);
	  if (! TYPE_HAS_ASSIGN_REF (t))
	    cp_warning ("  or `operator=(const %T&)'", t);
	}
      else if (! TYPE_HAS_ASSIGN_REF (t))
	cp_warning ("  but does not override `operator=(const %T&)'", t);
    }


  /* Check anonymous struct/anonymous union fields.  */
  finish_struct_anon (t);

  /* We've built up the list of access declarations in reverse order.
     Fix that now.  */
  *access_decls = nreverse (*access_decls);
}

/* Return a FIELD_DECL for a pointer-to-virtual-table or
   pointer-to-virtual-base.  The NAME, ASSEMBLER_NAME, and TYPE of the
   field are as indicated.  The CLASS_TYPE in which this field occurs
   is also indicated.  FCONTEXT is the type that is needed for the debug
   info output routines.  *EMPTY_P is set to a non-zero value by this
   function to indicate that a class containing this field is
   non-empty.  */

static tree
build_vtbl_or_vbase_field (name, assembler_name, type, class_type, fcontext,
			   empty_p)
     tree name;
     tree assembler_name;
     tree type;
     tree class_type;
     tree fcontext;
     int *empty_p;
{
  tree field;

  /* This class is non-empty.  */
  *empty_p = 0;

  /* Build the FIELD_DECL.  */
  field = build_decl (FIELD_DECL, name, type);
  SET_DECL_ASSEMBLER_NAME (field, assembler_name);
  DECL_VIRTUAL_P (field) = 1;
  DECL_ARTIFICIAL (field) = 1;
  DECL_FIELD_CONTEXT (field) = class_type;
  DECL_FCONTEXT (field) = fcontext;
  DECL_ALIGN (field) = TYPE_ALIGN (type);
  DECL_USER_ALIGN (field) = TYPE_USER_ALIGN (type);

  /* Return it.  */
  return field;
}

/* If TYPE is an empty class type, records its OFFSET in the table of
   OFFSETS.  */

static int
record_subobject_offset (type, offset, offsets)
     tree type;
     tree offset;
     splay_tree offsets;
{
  splay_tree_node n;

  if (!is_empty_class (type))
    return 0;

  /* Record the location of this empty object in OFFSETS.  */
  n = splay_tree_lookup (offsets, (splay_tree_key) offset);
  if (!n)
    n = splay_tree_insert (offsets, 
			   (splay_tree_key) offset,
			   (splay_tree_value) NULL_TREE);
  n->value = ((splay_tree_value) 
	      tree_cons (NULL_TREE,
			 type,
			 (tree) n->value));

  return 0;
}

/* Returns non-zero if TYPE is an empty class type and there is
   already an entry in OFFSETS for the same TYPE as the same OFFSET.  */

static int
check_subobject_offset (type, offset, offsets)
     tree type;
     tree offset;
     splay_tree offsets;
{
  splay_tree_node n;
  tree t;

  if (!is_empty_class (type))
    return 0;

  /* Record the location of this empty object in OFFSETS.  */
  n = splay_tree_lookup (offsets, (splay_tree_key) offset);
  if (!n)
    return 0;

  for (t = (tree) n->value; t; t = TREE_CHAIN (t))
    if (same_type_p (TREE_VALUE (t), type))
      return 1;

  return 0;
}

/* Walk through all the subobjects of TYPE (located at OFFSET).  Call
   F for every subobject, passing it the type, offset, and table of
   OFFSETS.  If VBASES_P is non-zero, then even virtual non-primary
   bases should be traversed; otherwise, they are ignored.  

   If MAX_OFFSET is non-NULL, then subobjects with an offset greater
   than MAX_OFFSET will not be walked.

   If F returns a non-zero value, the traversal ceases, and that value
   is returned.  Otherwise, returns zero.  */

static int
walk_subobject_offsets (type, f, offset, offsets, max_offset, vbases_p)
     tree type;
     subobject_offset_fn f;
     tree offset;
     splay_tree offsets;
     tree max_offset;
     int vbases_p;
{
  int r = 0;

  /* If this OFFSET is bigger than the MAX_OFFSET, then we should
     stop.  */
  if (max_offset && INT_CST_LT (max_offset, offset))
    return 0;

  if (CLASS_TYPE_P (type))
    {
      tree field;
      int i;

      /* Record the location of TYPE.  */
      r = (*f) (type, offset, offsets);
      if (r)
	return r;

      /* Iterate through the direct base classes of TYPE.  */
      for (i = 0; i < CLASSTYPE_N_BASECLASSES (type); ++i)
	{
	  tree binfo = BINFO_BASETYPE (TYPE_BINFO (type), i);

	  if (!vbases_p 
	      && TREE_VIA_VIRTUAL (binfo) 
	      && !BINFO_PRIMARY_P (binfo))
	    continue;

	  r = walk_subobject_offsets (BINFO_TYPE (binfo),
				      f,
				      size_binop (PLUS_EXPR,
						  offset,
						  BINFO_OFFSET (binfo)),
				      offsets,
				      max_offset,
				      vbases_p);
	  if (r)
	    return r;
	}

      /* Iterate through the fields of TYPE.  */
      for (field = TYPE_FIELDS (type); field; field = TREE_CHAIN (field))
	if (TREE_CODE (field) == FIELD_DECL)
	  {
	    r = walk_subobject_offsets (TREE_TYPE (field),
					f,
					size_binop (PLUS_EXPR,
						    offset,
						    DECL_FIELD_OFFSET (field)),
					offsets,
					max_offset,
					/*vbases_p=*/1);
	    if (r)
	      return r;
	  }
    }
  else if (TREE_CODE (type) == ARRAY_TYPE)
    {
      tree domain = TYPE_DOMAIN (type);
      tree index;

      /* Step through each of the elements in the array.  */
      for (index = size_zero_node; 
	   INT_CST_LT (index, TYPE_MAX_VALUE (domain));
	   index = size_binop (PLUS_EXPR, index, size_one_node))
	{
	  r = walk_subobject_offsets (TREE_TYPE (type),
				      f,
				      offset,
				      offsets,
				      max_offset,
				      /*vbases_p=*/1);
	  if (r)
	    return r;
	  offset = size_binop (PLUS_EXPR, offset, 
			       TYPE_SIZE_UNIT (TREE_TYPE (type)));
	  /* If this new OFFSET is bigger than the MAX_OFFSET, then
	     there's no point in iterating through the remaining
	     elements of the array.  */
	  if (max_offset && INT_CST_LT (max_offset, offset))
	    break;
	}
    }

  return 0;
}

/* Record all of the empty subobjects of TYPE (located at OFFSET) in
   OFFSETS.  If VBASES_P is non-zero, virtual bases of TYPE are
   examined.  */

static void
record_subobject_offsets (type, offset, offsets, vbases_p)
     tree type;
     tree offset;
     splay_tree offsets;
     int vbases_p;
{
  walk_subobject_offsets (type, record_subobject_offset, offset,
			  offsets, /*max_offset=*/NULL_TREE, vbases_p);
}

/* Returns non-zero if any of the empty subobjects of TYPE (located at
   OFFSET) conflict with entries in OFFSETS.  If VBASES_P is non-zero,
   virtual bases of TYPE are examined.  */

static int
layout_conflict_p (type, offset, offsets, vbases_p)
     tree type;
     tree offset;
     splay_tree offsets;
     int vbases_p;
{
  splay_tree_node max_node;

  /* Get the node in OFFSETS that indicates the maximum offset where
     an empty subobject is located.  */
  max_node = splay_tree_max (offsets);
  /* If there aren't any empty subobjects, then there's no point in
     performing this check.  */
  if (!max_node)
    return 0;

  return walk_subobject_offsets (type, check_subobject_offset, offset,
				 offsets, (tree) (max_node->key),
				 vbases_p);
}

/* DECL is a FIELD_DECL corresponding either to a base subobject of a
   non-static data member of the type indicated by RLI.  BINFO is the
   binfo corresponding to the base subobject, OFFSETS maps offsets to
   types already located at those offsets.  T is the most derived
   type.  This function determines the position of the DECL.  */

static void
layout_nonempty_base_or_field (rli, decl, binfo, offsets, t)
     record_layout_info rli;
     tree decl;
     tree binfo;
     splay_tree offsets;
     tree t;
{
  tree offset = NULL_TREE;
  tree type = TREE_TYPE (decl);
  /* If we are laying out a base class, rather than a field, then
     DECL_ARTIFICIAL will be set on the FIELD_DECL.  */
  int field_p = !DECL_ARTIFICIAL (decl);

  /* Try to place the field.  It may take more than one try if we have
     a hard time placing the field without putting two objects of the
     same type at the same address.  */
  while (1)
    {
      struct record_layout_info_s old_rli = *rli;

      /* Place this field.  */
      place_field (rli, decl);
      offset = byte_position (decl);
 
      /* We have to check to see whether or not there is already
	 something of the same type at the offset we're about to use.
	 For example:
	 
	 struct S {};
	 struct T : public S { int i; };
	 struct U : public S, public T {};
	 
	 Here, we put S at offset zero in U.  Then, we can't put T at
	 offset zero -- its S component would be at the same address
	 as the S we already allocated.  So, we have to skip ahead.
	 Since all data members, including those whose type is an
	 empty class, have non-zero size, any overlap can happen only
	 with a direct or indirect base-class -- it can't happen with
	 a data member.  */
      if (layout_conflict_p (TREE_TYPE (decl),
			     offset,
			     offsets, 
			     field_p))
	{
	  /* Strip off the size allocated to this field.  That puts us
	     at the first place we could have put the field with
	     proper alignment.  */
	  *rli = old_rli;

	  /* Bump up by the alignment required for the type.  */
	  rli->bitpos
	    = size_binop (PLUS_EXPR, rli->bitpos, 
			  bitsize_int (binfo 
				       ? CLASSTYPE_ALIGN (type)
				       : TYPE_ALIGN (type)));
	  normalize_rli (rli);
	}
      else
	/* There was no conflict.  We're done laying out this field.  */
	break;
    }

  /* Now that we know where it will be placed, update its
     BINFO_OFFSET.  */
  if (binfo && CLASS_TYPE_P (BINFO_TYPE (binfo)))
    propagate_binfo_offsets (binfo, 
			     convert (ssizetype, offset), t);
}

/* Layout the empty base BINFO.  EOC indicates the byte currently just
   past the end of the class, and should be correctly aligned for a
   class of the type indicated by BINFO; OFFSETS gives the offsets of
   the empty bases allocated so far. T is the most derived
   type.  Return non-zero iff we added it at the end. */

static bool
layout_empty_base (binfo, eoc, offsets, t)
     tree binfo;
     tree eoc;
     splay_tree offsets;
     tree t;
{
  tree alignment;
  tree basetype = BINFO_TYPE (binfo);
  bool atend = false;
  
  /* This routine should only be used for empty classes.  */
  my_friendly_assert (is_empty_class (basetype), 20000321);
  alignment = ssize_int (CLASSTYPE_ALIGN_UNIT (basetype));

  /* This is an empty base class.  We first try to put it at offset
     zero.  */
  if (layout_conflict_p (BINFO_TYPE (binfo),
			 BINFO_OFFSET (binfo),
			 offsets, 
			 /*vbases_p=*/0))
    {
      /* That didn't work.  Now, we move forward from the next
	 available spot in the class.  */
      atend = true;
      propagate_binfo_offsets (binfo, convert (ssizetype, eoc), t);
      while (1) 
	{
	  if (!layout_conflict_p (BINFO_TYPE (binfo),
				  BINFO_OFFSET (binfo), 
				  offsets,
				  /*vbases_p=*/0))
	    /* We finally found a spot where there's no overlap.  */
	    break;

	  /* There's overlap here, too.  Bump along to the next spot.  */
	  propagate_binfo_offsets (binfo, alignment, t);
	}
    }
  return atend;
}

/* Build a FIELD_DECL for the base given by BINFO in the class
   indicated by RLI.  If the new object is non-empty, clear *EMPTY_P.
   *BASE_ALIGN is a running maximum of the alignments of any base
   class.  OFFSETS gives the location of empty base subobjects.  T is
   the most derived type.  Return non-zero if the new object cannot be
   nearly-empty. */

static bool
build_base_field (rli, binfo, empty_p, offsets, t)
     record_layout_info rli;
     tree binfo;
     int *empty_p;
     splay_tree offsets;
     tree t;
{
  tree basetype = BINFO_TYPE (binfo);
  tree decl;
  bool atend = false;

  if (!COMPLETE_TYPE_P (basetype))
    /* This error is now reported in xref_tag, thus giving better
       location information.  */
    return atend;
  
  decl = build_decl (FIELD_DECL, NULL_TREE, basetype);
  DECL_ARTIFICIAL (decl) = 1;
  DECL_FIELD_CONTEXT (decl) = rli->t;
  DECL_SIZE (decl) = CLASSTYPE_SIZE (basetype);
  DECL_SIZE_UNIT (decl) = CLASSTYPE_SIZE_UNIT (basetype);
  DECL_ALIGN (decl) = CLASSTYPE_ALIGN (basetype);
  DECL_USER_ALIGN (decl) = CLASSTYPE_USER_ALIGN (basetype);
  
  if (!integer_zerop (DECL_SIZE (decl)))
    {
      /* The containing class is non-empty because it has a non-empty
	 base class.  */
      *empty_p = 0;

      /* Try to place the field.  It may take more than one try if we
	 have a hard time placing the field without putting two
	 objects of the same type at the same address.  */
      layout_nonempty_base_or_field (rli, decl, binfo, offsets, t);
    }
  else
    {
      unsigned HOST_WIDE_INT eoc;

      /* On some platforms (ARM), even empty classes will not be
	 byte-aligned.  */
      eoc = tree_low_cst (rli_size_unit_so_far (rli), 0);
      eoc = CEIL (eoc, DECL_ALIGN_UNIT (decl)) * DECL_ALIGN_UNIT (decl);
      atend |= layout_empty_base (binfo, size_int (eoc), offsets, t);
    }

  /* Record the offsets of BINFO and its base subobjects.  */
  record_subobject_offsets (BINFO_TYPE (binfo), 
			    BINFO_OFFSET (binfo),
			    offsets, 
			    /*vbases_p=*/0);
  return atend;
}

/* Layout all of the non-virtual base classes.  Record empty
   subobjects in OFFSETS.  T is the most derived type.  Return
   non-zero if the type cannot be nearly empty.  */

static bool
build_base_fields (rli, empty_p, offsets, t)
     record_layout_info rli;
     int *empty_p;
     splay_tree offsets;
     tree t;
{
  /* Chain to hold all the new FIELD_DECLs which stand in for base class
     subobjects.  */
  tree rec = rli->t;
  int n_baseclasses = CLASSTYPE_N_BASECLASSES (rec);
  int i;
  bool atend = 0;

  /* Under the new ABI, the primary base class is always allocated
     first.  */
  if (CLASSTYPE_HAS_PRIMARY_BASE_P (rec))
    build_base_field (rli, CLASSTYPE_PRIMARY_BINFO (rec), 
		      empty_p, offsets, t);

  /* Now allocate the rest of the bases.  */
  for (i = 0; i < n_baseclasses; ++i)
    {
      tree base_binfo;

      base_binfo = BINFO_BASETYPE (TYPE_BINFO (rec), i);

      /* Under the new ABI, the primary base was already allocated
	 above, so we don't need to allocate it again here.  */
      if (base_binfo == CLASSTYPE_PRIMARY_BINFO (rec))
	continue;

      /* A primary virtual base class is allocated just like any other
	 base class, but a non-primary virtual base is allocated
	 later, in layout_virtual_bases.  */
      if (TREE_VIA_VIRTUAL (base_binfo) 
	  && !BINFO_PRIMARY_P (base_binfo))
	continue;

      atend |= build_base_field (rli, base_binfo, empty_p, offsets, t);
    }
  return atend;
}

/* Go through the TYPE_METHODS of T issuing any appropriate
   diagnostics, figuring out which methods override which other
   methods, and so forth.  */

static void
check_methods (t)
     tree t;
{
  tree x;
  int seen_one_arg_array_delete_p = 0;

  for (x = TYPE_METHODS (t); x; x = TREE_CHAIN (x))
    {
      GNU_xref_member (current_class_name, x);

      /* If this was an evil function, don't keep it in class.  */
      if (DECL_ASSEMBLER_NAME_SET_P (x) 
	  && IDENTIFIER_ERROR_LOCUS (DECL_ASSEMBLER_NAME (x)))
	continue;

      check_for_override (x, t);
      if (DECL_PURE_VIRTUAL_P (x) && ! DECL_VINDEX (x))
	cp_error_at ("initializer specified for non-virtual method `%D'", x);

      /* The name of the field is the original field name
	 Save this in auxiliary field for later overloading.  */
      if (DECL_VINDEX (x))
	{
	  TYPE_POLYMORPHIC_P (t) = 1;
	  if (DECL_PURE_VIRTUAL_P (x))
	    CLASSTYPE_PURE_VIRTUALS (t)
	      = tree_cons (NULL_TREE, x, CLASSTYPE_PURE_VIRTUALS (t));
	}

      if (DECL_ARRAY_DELETE_OPERATOR_P (x))
	{
	  tree second_parm;

	  /* When dynamically allocating an array of this type, we
	     need a "cookie" to record how many elements we allocated,
	     even if the array elements have no non-trivial
	     destructor, if the usual array deallocation function
	     takes a second argument of type size_t.  The standard (in
	     [class.free]) requires that the second argument be set
	     correctly.  */
	  second_parm = TREE_CHAIN (TYPE_ARG_TYPES (TREE_TYPE (x)));
	  /* Under the new ABI, we choose only those function that are
	     explicitly declared as `operator delete[] (void *,
	     size_t)'.  */
	  if (!seen_one_arg_array_delete_p
	      && second_parm
	      && TREE_CHAIN (second_parm) == void_list_node
	      && same_type_p (TREE_VALUE (second_parm), sizetype))
	    TYPE_VEC_DELETE_TAKES_SIZE (t) = 1;
	  /* If there's no second parameter, then this is the usual
	     deallocation function.  */
	  else if (second_parm == void_list_node)
	    seen_one_arg_array_delete_p = 1;
	}
    }
}

/* FN is a constructor or destructor.  Clone the declaration to create
   a specialized in-charge or not-in-charge version, as indicated by
   NAME.  */

static tree
build_clone (fn, name)
     tree fn;
     tree name;
{
  tree parms;
  tree clone;

  /* Copy the function.  */
  clone = copy_decl (fn);
  /* Remember where this function came from.  */
  DECL_CLONED_FUNCTION (clone) = fn;
  DECL_ABSTRACT_ORIGIN (clone) = fn;
  /* Reset the function name.  */
  DECL_NAME (clone) = name;
  SET_DECL_ASSEMBLER_NAME (clone, NULL_TREE);
  /* There's no pending inline data for this function.  */
  DECL_PENDING_INLINE_INFO (clone) = NULL;
  DECL_PENDING_INLINE_P (clone) = 0;
  /* And it hasn't yet been deferred.  */
  DECL_DEFERRED_FN (clone) = 0;

  /* The base-class destructor is not virtual.  */
  if (name == base_dtor_identifier)
    {
      DECL_VIRTUAL_P (clone) = 0;
      if (TREE_CODE (clone) != TEMPLATE_DECL)
	DECL_VINDEX (clone) = NULL_TREE;
    }

  /* If there was an in-charge parameter, drop it from the function
     type.  */
  if (DECL_HAS_IN_CHARGE_PARM_P (clone))
    {
      tree basetype;
      tree parmtypes;
      tree exceptions;

      exceptions = TYPE_RAISES_EXCEPTIONS (TREE_TYPE (clone));
      basetype = TYPE_METHOD_BASETYPE (TREE_TYPE (clone));
      parmtypes = TYPE_ARG_TYPES (TREE_TYPE (clone));
      /* Skip the `this' parameter.  */
      parmtypes = TREE_CHAIN (parmtypes);
      /* Skip the in-charge parameter.  */
      parmtypes = TREE_CHAIN (parmtypes);
      /* And the VTT parm, in a complete [cd]tor.  */
      if (DECL_HAS_VTT_PARM_P (fn)
	  && ! DECL_NEEDS_VTT_PARM_P (clone))
	parmtypes = TREE_CHAIN (parmtypes);
       /* If this is subobject constructor or destructor, add the vtt
	 parameter.  */
      TREE_TYPE (clone) 
	= build_cplus_method_type (basetype,
				   TREE_TYPE (TREE_TYPE (clone)),
				   parmtypes);
      if (exceptions)
	TREE_TYPE (clone) = build_exception_variant (TREE_TYPE (clone),
						     exceptions);
    }

  /* Copy the function parameters.  But, DECL_ARGUMENTS on a TEMPLATE_DECL
     aren't function parameters; those are the template parameters.  */
  if (TREE_CODE (clone) != TEMPLATE_DECL)
    {
      DECL_ARGUMENTS (clone) = copy_list (DECL_ARGUMENTS (clone));
      /* Remove the in-charge parameter.  */
      if (DECL_HAS_IN_CHARGE_PARM_P (clone))
	{
	  TREE_CHAIN (DECL_ARGUMENTS (clone))
	    = TREE_CHAIN (TREE_CHAIN (DECL_ARGUMENTS (clone)));
	  DECL_HAS_IN_CHARGE_PARM_P (clone) = 0;
	}
      /* And the VTT parm, in a complete [cd]tor.  */
      if (DECL_HAS_VTT_PARM_P (fn))
	{
	  if (DECL_NEEDS_VTT_PARM_P (clone))
	    DECL_HAS_VTT_PARM_P (clone) = 1;
	  else
	    {
	      TREE_CHAIN (DECL_ARGUMENTS (clone))
		= TREE_CHAIN (TREE_CHAIN (DECL_ARGUMENTS (clone)));
	      DECL_HAS_VTT_PARM_P (clone) = 0;
	    }
	}

      for (parms = DECL_ARGUMENTS (clone); parms; parms = TREE_CHAIN (parms))
	{
	  DECL_CONTEXT (parms) = clone;
	  copy_lang_decl (parms);
	}
    }

  /* Create the RTL for this function.  */
  SET_DECL_RTL (clone, NULL_RTX);
  rest_of_decl_compilation (clone, NULL, /*top_level=*/1, at_eof);
  
  /* Make it easy to find the CLONE given the FN.  */
  TREE_CHAIN (clone) = TREE_CHAIN (fn);
  TREE_CHAIN (fn) = clone;

  /* If this is a template, handle the DECL_TEMPLATE_RESULT as well.  */
  if (TREE_CODE (clone) == TEMPLATE_DECL)
    {
      tree result;

      DECL_TEMPLATE_RESULT (clone) 
	= build_clone (DECL_TEMPLATE_RESULT (clone), name);
      result = DECL_TEMPLATE_RESULT (clone);
      DECL_TEMPLATE_INFO (result) = copy_node (DECL_TEMPLATE_INFO (result));
      DECL_TI_TEMPLATE (result) = clone;
    }
  else if (DECL_DEFERRED_FN (fn))
    defer_fn (clone);

  return clone;
}

/* Produce declarations for all appropriate clones of FN.  If
   UPDATE_METHOD_VEC_P is non-zero, the clones are added to the
   CLASTYPE_METHOD_VEC as well.  */

void
clone_function_decl (fn, update_method_vec_p)
     tree fn;
     int update_method_vec_p;
{
  tree clone;

  /* Avoid inappropriate cloning.  */
  if (TREE_CHAIN (fn)
      && DECL_CLONED_FUNCTION (TREE_CHAIN (fn)))
    return;

  if (DECL_MAYBE_IN_CHARGE_CONSTRUCTOR_P (fn))
    {
      /* For each constructor, we need two variants: an in-charge version
	 and a not-in-charge version.  */
      clone = build_clone (fn, complete_ctor_identifier);
      if (update_method_vec_p)
	add_method (DECL_CONTEXT (clone), clone, /*error_p=*/0);
      clone = build_clone (fn, base_ctor_identifier);
      if (update_method_vec_p)
	add_method (DECL_CONTEXT (clone), clone, /*error_p=*/0);
    }
  else
    {
      my_friendly_assert (DECL_MAYBE_IN_CHARGE_DESTRUCTOR_P (fn), 20000411);

      /* For each destructor, we need three variants: an in-charge
	 version, a not-in-charge version, and an in-charge deleting
	 version.  We clone the deleting version first because that
	 means it will go second on the TYPE_METHODS list -- and that
	 corresponds to the correct layout order in the virtual
	 function table.  

         For a non-virtual destructor, we do not build a deleting
	 destructor.  */
      if (DECL_VIRTUAL_P (fn))
	{
	  clone = build_clone (fn, deleting_dtor_identifier);
	  if (update_method_vec_p)
	    add_method (DECL_CONTEXT (clone), clone, /*error_p=*/0);
	}
      clone = build_clone (fn, complete_dtor_identifier);
      if (update_method_vec_p)
	add_method (DECL_CONTEXT (clone), clone, /*error_p=*/0);
      clone = build_clone (fn, base_dtor_identifier);
      if (update_method_vec_p)
	add_method (DECL_CONTEXT (clone), clone, /*error_p=*/0);
    }

  /* Note that this is an abstract function that is never emitted.  */
  DECL_ABSTRACT (fn) = 1;
}

/* DECL is an in charge constructor, which is being defined. This will
   have had an in class declaration, from whence clones were
   declared. An out-of-class definition can specify additional default
   arguments. As it is the clones that are involved in overload
   resolution, we must propagate the information from the DECL to its
   clones. */

void
adjust_clone_args (decl)
     tree decl;
{
  tree clone;
  
  for (clone = TREE_CHAIN (decl); clone && DECL_CLONED_FUNCTION (clone);
       clone = TREE_CHAIN (clone))
    {
      tree orig_clone_parms = TYPE_ARG_TYPES (TREE_TYPE (clone));
      tree orig_decl_parms = TYPE_ARG_TYPES (TREE_TYPE (decl));
      tree decl_parms, clone_parms;

      clone_parms = orig_clone_parms;
      
      /* Skip the 'this' parameter. */
      orig_clone_parms = TREE_CHAIN (orig_clone_parms);
      orig_decl_parms = TREE_CHAIN (orig_decl_parms);

      if (DECL_HAS_IN_CHARGE_PARM_P (decl))
	orig_decl_parms = TREE_CHAIN (orig_decl_parms);
      if (DECL_HAS_VTT_PARM_P (decl))
	orig_decl_parms = TREE_CHAIN (orig_decl_parms);
      
      clone_parms = orig_clone_parms;
      if (DECL_HAS_VTT_PARM_P (clone))
	clone_parms = TREE_CHAIN (clone_parms);
      
      for (decl_parms = orig_decl_parms; decl_parms;
	   decl_parms = TREE_CHAIN (decl_parms),
	     clone_parms = TREE_CHAIN (clone_parms))
	{
	  my_friendly_assert (same_type_p (TREE_TYPE (decl_parms),
					   TREE_TYPE (clone_parms)), 20010424);
	  
	  if (TREE_PURPOSE (decl_parms) && !TREE_PURPOSE (clone_parms))
	    {
	      /* A default parameter has been added. Adjust the
		 clone's parameters. */
	      tree exceptions = TYPE_RAISES_EXCEPTIONS (TREE_TYPE (clone));
	      tree basetype = TYPE_METHOD_BASETYPE (TREE_TYPE (clone));
	      tree type;

	      clone_parms = orig_decl_parms;

	      if (DECL_HAS_VTT_PARM_P (clone))
		{
		  clone_parms = tree_cons (TREE_PURPOSE (orig_clone_parms),
					   TREE_VALUE (orig_clone_parms),
					   clone_parms);
		  TREE_TYPE (clone_parms) = TREE_TYPE (orig_clone_parms);
		}
	      type = build_cplus_method_type (basetype,
					      TREE_TYPE (TREE_TYPE (clone)),
					      clone_parms);
	      if (exceptions)
		type = build_exception_variant (type, exceptions);
	      TREE_TYPE (clone) = type;
	      
	      clone_parms = NULL_TREE;
	      break;
	    }
	}
      my_friendly_assert (!clone_parms, 20010424);
    }
}

/* For each of the constructors and destructors in T, create an
   in-charge and not-in-charge variant.  */

static void
clone_constructors_and_destructors (t)
     tree t;
{
  tree fns;

  /* If for some reason we don't have a CLASSTYPE_METHOD_VEC, we bail
     out now.  */
  if (!CLASSTYPE_METHOD_VEC (t))
    return;

  for (fns = CLASSTYPE_CONSTRUCTORS (t); fns; fns = OVL_NEXT (fns))
    clone_function_decl (OVL_CURRENT (fns), /*update_method_vec_p=*/1);
  for (fns = CLASSTYPE_DESTRUCTORS (t); fns; fns = OVL_NEXT (fns))
    clone_function_decl (OVL_CURRENT (fns), /*update_method_vec_p=*/1);
}

/* Remove all zero-width bit-fields from T.  */

static void
remove_zero_width_bit_fields (t)
     tree t;
{
  tree *fieldsp;

  fieldsp = &TYPE_FIELDS (t); 
  while (*fieldsp)
    {
      if (TREE_CODE (*fieldsp) == FIELD_DECL
	  && DECL_C_BIT_FIELD (*fieldsp) 
	  && DECL_INITIAL (*fieldsp))
	*fieldsp = TREE_CHAIN (*fieldsp);
      else
	fieldsp = &TREE_CHAIN (*fieldsp);
    }
}

/* Check the validity of the bases and members declared in T.  Add any
   implicitly-generated functions (like copy-constructors and
   assignment operators).  Compute various flag bits (like
   CLASSTYPE_NON_POD_T) for T.  This routine works purely at the C++
   level: i.e., independently of the ABI in use.  */

static void
check_bases_and_members (t, empty_p)
     tree t;
     int *empty_p;
{
  /* Nonzero if we are not allowed to generate a default constructor
     for this case.  */
  int cant_have_default_ctor;
  /* Nonzero if the implicitly generated copy constructor should take
     a non-const reference argument.  */
  int cant_have_const_ctor;
  /* Nonzero if the the implicitly generated assignment operator
     should take a non-const reference argument.  */
  int no_const_asn_ref;
  tree access_decls;

  /* By default, we use const reference arguments and generate default
     constructors.  */
  cant_have_default_ctor = 0;
  cant_have_const_ctor = 0;
  no_const_asn_ref = 0;

  /* Assume that the class is nearly empty; we'll clear this flag if
     it turns out not to be nearly empty.  */
  CLASSTYPE_NEARLY_EMPTY_P (t) = 1;

  /* Check all the base-classes. */
  check_bases (t, &cant_have_default_ctor, &cant_have_const_ctor,
	       &no_const_asn_ref);

  /* Check all the data member declarations.  */
  check_field_decls (t, &access_decls, empty_p,
		     &cant_have_default_ctor,
		     &cant_have_const_ctor,
		     &no_const_asn_ref);

  /* Check all the method declarations.  */
  check_methods (t);

  /* A nearly-empty class has to be vptr-containing; a nearly empty
     class contains just a vptr.  */
  if (!TYPE_CONTAINS_VPTR_P (t))
    CLASSTYPE_NEARLY_EMPTY_P (t) = 0;

  /* Do some bookkeeping that will guide the generation of implicitly
     declared member functions.  */
  TYPE_HAS_COMPLEX_INIT_REF (t)
    |= (TYPE_HAS_INIT_REF (t) 
	|| TYPE_USES_VIRTUAL_BASECLASSES (t)
	|| TYPE_POLYMORPHIC_P (t));
  TYPE_NEEDS_CONSTRUCTING (t)
    |= (TYPE_HAS_CONSTRUCTOR (t) 
	|| TYPE_USES_VIRTUAL_BASECLASSES (t)
	|| TYPE_POLYMORPHIC_P (t));
  CLASSTYPE_NON_AGGREGATE (t) |= (TYPE_HAS_CONSTRUCTOR (t)
				  || TYPE_POLYMORPHIC_P (t));
  CLASSTYPE_NON_POD_P (t)
    |= (CLASSTYPE_NON_AGGREGATE (t) || TYPE_HAS_DESTRUCTOR (t) 
	|| TYPE_HAS_ASSIGN_REF (t));
  TYPE_HAS_REAL_ASSIGN_REF (t) |= TYPE_HAS_ASSIGN_REF (t);
  TYPE_HAS_COMPLEX_ASSIGN_REF (t)
    |= TYPE_HAS_ASSIGN_REF (t) || TYPE_CONTAINS_VPTR_P (t);

  /* Synthesize any needed methods.  Note that methods will be synthesized
     for anonymous unions; grok_x_components undoes that.  */
  add_implicitly_declared_members (t, cant_have_default_ctor,
				   cant_have_const_ctor,
				   no_const_asn_ref);

  /* Create the in-charge and not-in-charge variants of constructors
     and destructors.  */
  clone_constructors_and_destructors (t);

  /* Process the using-declarations.  */
  for (; access_decls; access_decls = TREE_CHAIN (access_decls))
    handle_using_decl (TREE_VALUE (access_decls), t);

  /* Build and sort the CLASSTYPE_METHOD_VEC.  */
  finish_struct_methods (t);
}

/* If T needs a pointer to its virtual function table, set TYPE_VFIELD
   accordingly.  If a new vfield was created (because T doesn't have a
   primary base class), then the newly created field is returned.  It
   is not added to the TYPE_FIELDS list; it is the caller's
   responsibility to do that.  */

static tree
create_vtable_ptr (t, empty_p, vfuns_p,
		   new_virtuals_p, overridden_virtuals_p)
     tree t;
     int *empty_p;
     int *vfuns_p;
     tree *new_virtuals_p;
     tree *overridden_virtuals_p;
{
  tree fn;

  /* Loop over the virtual functions, adding them to our various
     vtables.  */
  for (fn = TYPE_METHODS (t); fn; fn = TREE_CHAIN (fn))
    if (DECL_VINDEX (fn) && !DECL_MAYBE_IN_CHARGE_DESTRUCTOR_P (fn))
      add_virtual_function (new_virtuals_p, overridden_virtuals_p,
			    vfuns_p, fn, t);

  /* If we couldn't find an appropriate base class, create a new field
     here.  Even if there weren't any new virtual functions, we might need a
     new virtual function table if we're supposed to include vptrs in
     all classes that need them.  */
  if (!TYPE_VFIELD (t)
      && (*vfuns_p 
	  || (TYPE_CONTAINS_VPTR_P (t) && vptrs_present_everywhere_p ())))
    {
      /* We build this decl with vtbl_ptr_type_node, which is a
	 `vtable_entry_type*'.  It might seem more precise to use
	 `vtable_entry_type (*)[N]' where N is the number of firtual
	 functions.  However, that would require the vtable pointer in
	 base classes to have a different type than the vtable pointer
	 in derived classes.  We could make that happen, but that
	 still wouldn't solve all the problems.  In particular, the
	 type-based alias analysis code would decide that assignments
	 to the base class vtable pointer can't alias assignments to
	 the derived class vtable pointer, since they have different
	 types.  Thus, in an derived class destructor, where the base
	 class constructor was inlined, we could generate bad code for
	 setting up the vtable pointer.  

         Therefore, we use one type for all vtable pointers.  We still
	 use a type-correct type; it's just doesn't indicate the array
	 bounds.  That's better than using `void*' or some such; it's
	 cleaner, and it let's the alias analysis code know that these
	 stores cannot alias stores to void*!  */
      TYPE_VFIELD (t) 
	= build_vtbl_or_vbase_field (get_vfield_name (t),
				     get_identifier (VFIELD_BASE),
				     vtbl_ptr_type_node,
				     t,
				     t,
				     empty_p);

      if (CLASSTYPE_N_BASECLASSES (t))
	/* If there were any baseclasses, they can't possibly be at
	   offset zero any more, because that's where the vtable
	   pointer is.  So, converting to a base class is going to
	   take work.  */
	TYPE_BASE_CONVS_MAY_REQUIRE_CODE_P (t) = 1;

      return TYPE_VFIELD (t);
    }

  return NULL_TREE;
}

/* Fixup the inline function given by INFO now that the class is
   complete.  */

static void
fixup_pending_inline (fn)
     tree fn;
{
  if (DECL_PENDING_INLINE_INFO (fn))
    {
      tree args = DECL_ARGUMENTS (fn);
      while (args)
	{
	  DECL_CONTEXT (args) = fn;
	  args = TREE_CHAIN (args);
	}
    }
}

/* Fixup the inline methods and friends in TYPE now that TYPE is
   complete.  */

static void
fixup_inline_methods (type)
     tree type;
{
  tree method = TYPE_METHODS (type);

  if (method && TREE_CODE (method) == TREE_VEC)
    {
      if (TREE_VEC_ELT (method, 1))
	method = TREE_VEC_ELT (method, 1);
      else if (TREE_VEC_ELT (method, 0))
	method = TREE_VEC_ELT (method, 0);
      else
	method = TREE_VEC_ELT (method, 2);
    }

  /* Do inline member functions.  */
  for (; method; method = TREE_CHAIN (method))
    fixup_pending_inline (method);

  /* Do friends.  */
  for (method = CLASSTYPE_INLINE_FRIENDS (type); 
       method; 
       method = TREE_CHAIN (method))
    fixup_pending_inline (TREE_VALUE (method));
  CLASSTYPE_INLINE_FRIENDS (type) = NULL_TREE;
}

/* Add OFFSET to all base types of BINFO which is a base in the
   hierarchy dominated by T.

   OFFSET, which is a type offset, is number of bytes.  */

static void
propagate_binfo_offsets (binfo, offset, t)
     tree binfo;
     tree offset;
     tree t;
{
  int i;
  tree primary_binfo;

  /* Update BINFO's offset.  */
  BINFO_OFFSET (binfo)
    = convert (sizetype, 
	       size_binop (PLUS_EXPR,
			   convert (ssizetype, BINFO_OFFSET (binfo)),
			   offset));

  /* Find the primary base class.  */
  primary_binfo = get_primary_binfo (binfo);

  /* Scan all of the bases, pushing the BINFO_OFFSET adjust
     downwards.  */
  for (i = -1; i < BINFO_N_BASETYPES (binfo); ++i)
    {
      tree base_binfo;

      /* On the first time through the loop, do the primary base.
	 Because the primary base need not be an immediate base, we
	 must handle the primary base specially.  */
      if (i == -1) 
	{
	  if (!primary_binfo) 
	    continue;

	  base_binfo = primary_binfo;
	}
      else
	{
	  base_binfo = BINFO_BASETYPE (binfo, i);
	  /* Don't do the primary base twice.  */
	  if (base_binfo == primary_binfo)
	    continue;
	}

      /* Skip virtual bases that aren't our canonical primary base.  */
      if (TREE_VIA_VIRTUAL (base_binfo)
	  && (BINFO_PRIMARY_BASE_OF (base_binfo) != binfo
	      || base_binfo != binfo_for_vbase (BINFO_TYPE (base_binfo), t)))
	continue;

      propagate_binfo_offsets (base_binfo, offset, t);
    }
}

/* Called via dfs_walk from layout_virtual bases.  */

static tree
dfs_set_offset_for_unshared_vbases (binfo, data)
     tree binfo;
     void *data;
{
  /* If this is a virtual base, make sure it has the same offset as
     the shared copy.  If it's a primary base, then we know it's
     correct.  */
  if (TREE_VIA_VIRTUAL (binfo))
    {
      tree t = (tree) data;
      tree vbase;
      tree offset;
      
      vbase = binfo_for_vbase (BINFO_TYPE (binfo), t);
      if (vbase != binfo)
	{
	  offset = size_diffop (BINFO_OFFSET (vbase), BINFO_OFFSET (binfo));
	  propagate_binfo_offsets (binfo, offset, t);
	}
    }

  return NULL_TREE;
}

/* Set BINFO_OFFSET for all of the virtual bases for T.  Update
   TYPE_ALIGN and TYPE_SIZE for T.  OFFSETS gives the location of
   empty subobjects of T.  */

static void
layout_virtual_bases (t, offsets)
     tree t;
     splay_tree offsets;
{
  tree vbases;
  unsigned HOST_WIDE_INT dsize;
  unsigned HOST_WIDE_INT eoc;

  if (CLASSTYPE_N_BASECLASSES (t) == 0)
    return;

#ifdef STRUCTURE_SIZE_BOUNDARY
  /* Packed structures don't need to have minimum size.  */
  if (! TYPE_PACKED (t))
    TYPE_ALIGN (t) = MAX (TYPE_ALIGN (t), STRUCTURE_SIZE_BOUNDARY);
#endif

  /* DSIZE is the size of the class without the virtual bases.  */
  dsize = tree_low_cst (TYPE_SIZE (t), 1);

  /* Make every class have alignment of at least one.  */
  TYPE_ALIGN (t) = MAX (TYPE_ALIGN (t), BITS_PER_UNIT);

  /* Go through the virtual bases, allocating space for each virtual
     base that is not already a primary base class.  Under the old
     ABI, these are allocated according to a depth-first left-to-right
     postorder traversal; in the new ABI, inheritance graph order is
     used instead.  */
  for (vbases = TYPE_BINFO (t);
       vbases; 
       vbases = TREE_CHAIN (vbases))
    {
      tree vbase;

      if (!TREE_VIA_VIRTUAL (vbases))
	continue;
      vbase = binfo_for_vbase (BINFO_TYPE (vbases), t);

      if (!BINFO_PRIMARY_P (vbase))
	{
	  /* This virtual base is not a primary base of any class in the
	     hierarchy, so we have to add space for it.  */
	  tree basetype;
	  unsigned int desired_align;

	  basetype = BINFO_TYPE (vbase);

	  desired_align = CLASSTYPE_ALIGN (basetype);
	  TYPE_ALIGN (t) = MAX (TYPE_ALIGN (t), desired_align);

	  /* Add padding so that we can put the virtual base class at an
	     appropriately aligned offset.  */
	  dsize = CEIL (dsize, desired_align) * desired_align;

	  /* Under the new ABI, we try to squish empty virtual bases in
	     just like ordinary empty bases.  */
	  if (is_empty_class (basetype))
	    layout_empty_base (vbase,
			       size_int (CEIL (dsize, BITS_PER_UNIT)),
			       offsets, t);
	  else
	    {
	      tree offset;

	      offset = ssize_int (CEIL (dsize, BITS_PER_UNIT));
	      offset = size_diffop (offset, 
				    convert (ssizetype, 
					     BINFO_OFFSET (vbase)));

	      /* And compute the offset of the virtual base.  */
	      propagate_binfo_offsets (vbase, offset, t);
	      /* Every virtual baseclass takes a least a UNIT, so that
		 we can take it's address and get something different
		 for each base.  */
	      dsize += MAX (BITS_PER_UNIT,
			    tree_low_cst (CLASSTYPE_SIZE (basetype), 0));
	    }

	  /* Keep track of the offsets assigned to this virtual base.  */
	  record_subobject_offsets (BINFO_TYPE (vbase), 
				    BINFO_OFFSET (vbase),
				    offsets,
				    /*vbases_p=*/0);
	}
    }

  /* Now, go through the TYPE_BINFO hierarchy, setting the
     BINFO_OFFSETs correctly for all non-primary copies of the virtual
     bases and their direct and indirect bases.  The ambiguity checks
     in get_base_distance depend on the BINFO_OFFSETs being set
     correctly.  */
  dfs_walk (TYPE_BINFO (t), dfs_set_offset_for_unshared_vbases, NULL, t);

  /* If we had empty base classes that protruded beyond the end of the
     class, we didn't update DSIZE above; we were hoping to overlay
     multiple such bases at the same location.  */
  eoc = end_of_class (t, /*include_virtuals_p=*/1);
  if (eoc * BITS_PER_UNIT > dsize)
    dsize = eoc * BITS_PER_UNIT;

  /* Now, make sure that the total size of the type is a multiple of
     its alignment.  */
  dsize = CEIL (dsize, TYPE_ALIGN (t)) * TYPE_ALIGN (t);
  TYPE_SIZE (t) = bitsize_int (dsize);
  TYPE_SIZE_UNIT (t) = convert (sizetype,
				size_binop (CEIL_DIV_EXPR, TYPE_SIZE (t),
					    bitsize_unit_node));

  /* Check for ambiguous virtual bases.  */
  if (extra_warnings)
    for (vbases = CLASSTYPE_VBASECLASSES (t); 
	 vbases; 
	 vbases = TREE_CHAIN (vbases))
      {
	tree basetype = BINFO_TYPE (TREE_VALUE (vbases));
	if (get_base_distance (basetype, t, 0, (tree*)0) == -2)
	  cp_warning ("virtual base `%T' inaccessible in `%T' due to ambiguity",
		      basetype, t);
      }
}

/* Returns the offset of the byte just past the end of the base class
   with the highest offset in T.  If INCLUDE_VIRTUALS_P is zero, then
   only non-virtual bases are included.  */

static unsigned HOST_WIDE_INT
end_of_class (t, include_virtuals_p)
     tree t;
     int include_virtuals_p;
{
  unsigned HOST_WIDE_INT result = 0;
  int i;

  for (i = 0; i < CLASSTYPE_N_BASECLASSES (t); ++i)
    {
      tree base_binfo;
      tree offset;
      tree size;
      unsigned HOST_WIDE_INT end_of_base;

      base_binfo = BINFO_BASETYPE (TYPE_BINFO (t), i);

      if (!include_virtuals_p
	  && TREE_VIA_VIRTUAL (base_binfo) 
	  && !BINFO_PRIMARY_P (base_binfo))
	continue;

      if (is_empty_class (BINFO_TYPE (base_binfo)))
	/* An empty class has zero CLASSTYPE_SIZE_UNIT, but we need to
	   allocate some space for it. It cannot have virtual bases,
	   so TYPE_SIZE_UNIT is fine.  */
	size = TYPE_SIZE_UNIT (BINFO_TYPE (base_binfo));
      else
	size = CLASSTYPE_SIZE_UNIT (BINFO_TYPE (base_binfo));
      offset = size_binop (PLUS_EXPR, 
			   BINFO_OFFSET (base_binfo),
			   size);
      end_of_base = tree_low_cst (offset, /*pos=*/1);
      if (end_of_base > result)
	result = end_of_base;
    }

  return result;
}

/* Warn about direct bases of T that are inaccessible because they are
   ambiguous.  For example:

     struct S {};
     struct T : public S {};
     struct U : public S, public T {};

   Here, `(S*) new U' is not allowed because there are two `S'
   subobjects of U.  */

static void
warn_about_ambiguous_direct_bases (t)
     tree t;
{
  int i;

  for (i = 0; i < CLASSTYPE_N_BASECLASSES (t); ++i)
    {
      tree basetype = TYPE_BINFO_BASETYPE (t, i);

      if (get_base_distance (basetype, t, 0, NULL) == -2)
	cp_warning ("direct base `%T' inaccessible in `%T' due to ambiguity",
		    basetype, t);
    }
}

/* Compare two INTEGER_CSTs K1 and K2.  */

static int
splay_tree_compare_integer_csts (k1, k2)
     splay_tree_key k1;
     splay_tree_key k2;
{
  return tree_int_cst_compare ((tree) k1, (tree) k2);
}

/* Calculate the TYPE_SIZE, TYPE_ALIGN, etc for T.  Calculate
   BINFO_OFFSETs for all of the base-classes.  Position the vtable
   pointer.  */

static void
layout_class_type (t, empty_p, vfuns_p, 
		   new_virtuals_p, overridden_virtuals_p)
     tree t;
     int *empty_p;
     int *vfuns_p;
     tree *new_virtuals_p;
     tree *overridden_virtuals_p;
{
  tree non_static_data_members;
  tree field;
  tree vptr;
  record_layout_info rli;
  unsigned HOST_WIDE_INT eoc;
  /* Maps offsets (represented as INTEGER_CSTs) to a TREE_LIST of
     types that appear at that offset.  */
  splay_tree empty_base_offsets;

  /* Keep track of the first non-static data member.  */
  non_static_data_members = TYPE_FIELDS (t);

  /* Start laying out the record.  */
  rli = start_record_layout (t);

  /* If possible, we reuse the virtual function table pointer from one
     of our base classes.  */
  determine_primary_base (t, vfuns_p);

  /* Create a pointer to our virtual function table.  */
  vptr = create_vtable_ptr (t, empty_p, vfuns_p,
			    new_virtuals_p, overridden_virtuals_p);

  /* Under the new ABI, the vptr is always the first thing in the
     class.  */
  if (vptr)
    {
      TYPE_FIELDS (t) = chainon (vptr, TYPE_FIELDS (t));
      place_field (rli, vptr);
    }

  /* Build FIELD_DECLs for all of the non-virtual base-types.  */
  empty_base_offsets = splay_tree_new (splay_tree_compare_integer_csts, 
				       NULL, NULL);
  if (build_base_fields (rli, empty_p, empty_base_offsets, t))
    CLASSTYPE_NEARLY_EMPTY_P (t) = 0;
  
  /* Add pointers to all of our virtual base-classes.  */
  TYPE_FIELDS (t) = chainon (build_vbase_pointer_fields (rli, empty_p),
			     TYPE_FIELDS (t));

  /* CLASSTYPE_INLINE_FRIENDS is really TYPE_NONCOPIED_PARTS.  Thus,
     we have to save this before we zap TYPE_NONCOPIED_PARTS.  */
  fixup_inline_methods (t);

  /* Layout the non-static data members.  */
  for (field = non_static_data_members; field; field = TREE_CHAIN (field))
    {
      tree type;
      tree padding;

      /* We still pass things that aren't non-static data members to
	 the back-end, in case it wants to do something with them.  */
      if (TREE_CODE (field) != FIELD_DECL)
	{
	  place_field (rli, field);
	  continue;
	}

      type = TREE_TYPE (field);

      /* If this field is a bit-field whose width is greater than its
	 type, then there are some special rules for allocating it
	 under the new ABI.  Under the old ABI, there were no special
	 rules, but the back-end can't handle bitfields longer than a
	 `long long', so we use the same mechanism.  */
      if (DECL_C_BIT_FIELD (field)
	  && INT_CST_LT (TYPE_SIZE (type), DECL_SIZE (field)))
	{
	  integer_type_kind itk;
	  tree integer_type;

	  /* We must allocate the bits as if suitably aligned for the
	     longest integer type that fits in this many bits.  type
	     of the field.  Then, we are supposed to use the left over
	     bits as additional padding.  */
	  for (itk = itk_char; itk != itk_none; ++itk)
	    if (INT_CST_LT (DECL_SIZE (field), 
			    TYPE_SIZE (integer_types[itk])))
	      break;

	  /* ITK now indicates a type that is too large for the
	     field.  We have to back up by one to find the largest
	     type that fits.  */
	  integer_type = integer_types[itk - 1];
	  padding = size_binop (MINUS_EXPR, DECL_SIZE (field), 
				TYPE_SIZE (integer_type));
	  DECL_SIZE (field) = TYPE_SIZE (integer_type);
	  DECL_ALIGN (field) = TYPE_ALIGN (integer_type);
	  DECL_USER_ALIGN (field) = TYPE_USER_ALIGN (integer_type);
	}
      else
	padding = NULL_TREE;

      layout_nonempty_base_or_field (rli, field, NULL_TREE,
				     empty_base_offsets, t);

      /* If we needed additional padding after this field, add it
	 now.  */
      if (padding)
	{
	  tree padding_field;

	  padding_field = build_decl (FIELD_DECL, 
				      NULL_TREE,
				      char_type_node); 
	  DECL_BIT_FIELD (padding_field) = 1;
	  DECL_SIZE (padding_field) = padding;
	  DECL_ALIGN (padding_field) = 1;
	  DECL_USER_ALIGN (padding_field) = 0;
	  layout_nonempty_base_or_field (rli, padding_field,
					 NULL_TREE, 
					 empty_base_offsets, t);
	}
    }

  /* It might be the case that we grew the class to allocate a
     zero-sized base class.  That won't be reflected in RLI, yet,
     because we are willing to overlay multiple bases at the same
     offset.  However, now we need to make sure that RLI is big enough
     to reflect the entire class.  */
  eoc = end_of_class (t, /*include_virtuals_p=*/0);
  if (TREE_CODE (rli_size_unit_so_far (rli)) == INTEGER_CST
      && compare_tree_int (rli_size_unit_so_far (rli), eoc) < 0)
    {
      rli->offset = size_binop (MAX_EXPR, rli->offset, size_int (eoc));
      rli->bitpos = bitsize_zero_node;
    }

  /* We make all structures have at least one element, so that they
     have non-zero size.  In the new ABI, the class may be empty even
     if it has basetypes.  Therefore, we add the fake field after all
     the other fields; if there are already FIELD_DECLs on the list,
     their offsets will not be disturbed.  */
  if (!eoc && *empty_p)
    {
      tree padding;

      padding = build_decl (FIELD_DECL, NULL_TREE, char_type_node);
      place_field (rli, padding);
    }

  /* Let the back-end lay out the type. Note that at this point we
     have only included non-virtual base-classes; we will lay out the
     virtual base classes later.  So, the TYPE_SIZE/TYPE_ALIGN after
     this call are not necessarily correct; they are just the size and
     alignment when no virtual base clases are used.  */
  finish_record_layout (rli);

  /* Delete all zero-width bit-fields from the list of fields.  Now
     that the type is laid out they are no longer important.  */
  remove_zero_width_bit_fields (t);

  /* Remember the size and alignment of the class before adding
     the virtual bases.  */
  if (*empty_p)
    {
      CLASSTYPE_SIZE (t) = bitsize_zero_node;
      CLASSTYPE_SIZE_UNIT (t) = size_zero_node;
    }
  else
    {
      CLASSTYPE_SIZE (t) = TYPE_BINFO_SIZE (t);
      CLASSTYPE_SIZE_UNIT (t) = TYPE_BINFO_SIZE_UNIT (t);
    }

  CLASSTYPE_ALIGN (t) = TYPE_ALIGN (t);
  CLASSTYPE_USER_ALIGN (t) = TYPE_USER_ALIGN (t);

  /* Set the TYPE_DECL for this type to contain the right
     value for DECL_OFFSET, so that we can use it as part
     of a COMPONENT_REF for multiple inheritance.  */
  layout_decl (TYPE_MAIN_DECL (t), 0);

  /* Now fix up any virtual base class types that we left lying
     around.  We must get these done before we try to lay out the
     virtual function table.  As a side-effect, this will remove the
     base subobject fields.  */
  layout_virtual_bases (t, empty_base_offsets);

  /* Warn about direct bases that can't be talked about due to
     ambiguity.  */
  warn_about_ambiguous_direct_bases (t);

  /* Clean up.  */
  splay_tree_delete (empty_base_offsets);
}

/* Create a RECORD_TYPE or UNION_TYPE node for a C struct or union declaration
   (or C++ class declaration).

   For C++, we must handle the building of derived classes.
   Also, C++ allows static class members.  The way that this is
   handled is to keep the field name where it is (as the DECL_NAME
   of the field), and place the overloaded decl in the bit position
   of the field.  layout_record and layout_union will know about this.

   More C++ hair: inline functions have text in their
   DECL_PENDING_INLINE_INFO nodes which must somehow be parsed into
   meaningful tree structure.  After the struct has been laid out, set
   things up so that this can happen.

   And still more: virtual functions.  In the case of single inheritance,
   when a new virtual function is seen which redefines a virtual function
   from the base class, the new virtual function is placed into
   the virtual function table at exactly the same address that
   it had in the base class.  When this is extended to multiple
   inheritance, the same thing happens, except that multiple virtual
   function tables must be maintained.  The first virtual function
   table is treated in exactly the same way as in the case of single
   inheritance.  Additional virtual function tables have different
   DELTAs, which tell how to adjust `this' to point to the right thing.

   ATTRIBUTES is the set of decl attributes to be applied, if any.  */

void
finish_struct_1 (t)
     tree t;
{
  tree x;
  int vfuns;
  /* The NEW_VIRTUALS is a TREE_LIST.  The TREE_VALUE of each node is
     a FUNCTION_DECL.  Each of these functions is a virtual function
     declared in T that does not override any virtual function from a
     base class.  */
  tree new_virtuals = NULL_TREE;
  /* The OVERRIDDEN_VIRTUALS list is like the NEW_VIRTUALS list,
     except that each declaration here overrides the declaration from
     a base class.  */
  tree overridden_virtuals = NULL_TREE;
  int n_fields = 0;
  tree vfield;
  int empty = 1;

  if (COMPLETE_TYPE_P (t))
    {
      if (IS_AGGR_TYPE (t))
	cp_error ("redefinition of `%#T'", t);
      else
	my_friendly_abort (172);
      popclass ();
      return;
    }

  GNU_xref_decl (current_function_decl, t);

  /* If this type was previously laid out as a forward reference,
     make sure we lay it out again.  */
  TYPE_SIZE (t) = NULL_TREE;
  CLASSTYPE_GOT_SEMICOLON (t) = 0;
  CLASSTYPE_PRIMARY_BINFO (t) = NULL_TREE;
  vfuns = 0;
  CLASSTYPE_RTTI (t) = NULL_TREE;

  /* Do end-of-class semantic processing: checking the validity of the
     bases and members and add implicitly generated methods.  */
  check_bases_and_members (t, &empty);

  /* Layout the class itself.  */
  layout_class_type (t, &empty, &vfuns,
		     &new_virtuals, &overridden_virtuals);

  /* Make sure that we get our own copy of the vfield FIELD_DECL.  */
  vfield = TYPE_VFIELD (t);
  if (vfield && CLASSTYPE_HAS_PRIMARY_BASE_P (t))
    {
      tree primary = CLASSTYPE_PRIMARY_BINFO (t);

      my_friendly_assert (same_type_p (DECL_FIELD_CONTEXT (vfield),
				       BINFO_TYPE (primary)),
			  20010726);
      /* The vtable better be at the start. */
      my_friendly_assert (integer_zerop (DECL_FIELD_OFFSET (vfield)),
			  20010726);
      my_friendly_assert (integer_zerop (BINFO_OFFSET (primary)),
			  20010726);
      
      vfield = copy_decl (vfield);
      DECL_FIELD_CONTEXT (vfield) = t;
      TYPE_VFIELD (t) = vfield;
    }
  else
    my_friendly_assert (!vfield || DECL_FIELD_CONTEXT (vfield) == t, 20010726);

  overridden_virtuals 
    = modify_all_vtables (t, &vfuns, nreverse (overridden_virtuals));

  /* If we created a new vtbl pointer for this class, add it to the
     list.  */
  if (TYPE_VFIELD (t) && !CLASSTYPE_HAS_PRIMARY_BASE_P (t))
    CLASSTYPE_VFIELDS (t) 
      = chainon (CLASSTYPE_VFIELDS (t), build_tree_list (NULL_TREE, t));

  /* If necessary, create the primary vtable for this class.  */
  if (new_virtuals
      || overridden_virtuals
      || (TYPE_CONTAINS_VPTR_P (t) && vptrs_present_everywhere_p ()))
    {
      new_virtuals = nreverse (new_virtuals);
      /* We must enter these virtuals into the table.  */
      if (!CLASSTYPE_HAS_PRIMARY_BASE_P (t))
	build_primary_vtable (NULL_TREE, t);
      else if (! BINFO_NEW_VTABLE_MARKED (TYPE_BINFO (t), t))
	/* Here we know enough to change the type of our virtual
	   function table, but we will wait until later this function.  */
	build_primary_vtable (CLASSTYPE_PRIMARY_BINFO (t), t);

      /* If this type has basetypes with constructors, then those
	 constructors might clobber the virtual function table.  But
	 they don't if the derived class shares the exact vtable of the base
	 class.  */

      CLASSTYPE_NEEDS_VIRTUAL_REINIT (t) = 1;
    }
  /* If we didn't need a new vtable, see if we should copy one from
     the base.  */
  else if (CLASSTYPE_HAS_PRIMARY_BASE_P (t))
    {
      tree binfo = CLASSTYPE_PRIMARY_BINFO (t);

      /* If this class uses a different vtable than its primary base
	 then when we will need to initialize our vptr after the base
	 class constructor runs.  */
      if (TYPE_BINFO_VTABLE (t) != BINFO_VTABLE (binfo))
	CLASSTYPE_NEEDS_VIRTUAL_REINIT (t) = 1;
    }

  if (TYPE_CONTAINS_VPTR_P (t))
    {
      if (TYPE_BINFO_VTABLE (t))
	my_friendly_assert (DECL_VIRTUAL_P (TYPE_BINFO_VTABLE (t)),
			    20000116);
      if (!CLASSTYPE_HAS_PRIMARY_BASE_P (t))
	my_friendly_assert (TYPE_BINFO_VIRTUALS (t) == NULL_TREE,
			    20000116);

      CLASSTYPE_VSIZE (t) = vfuns;
      /* Entries for virtual functions defined in the primary base are
	 followed by entries for new functions unique to this class.  */
      TYPE_BINFO_VIRTUALS (t) 
	= chainon (TYPE_BINFO_VIRTUALS (t), new_virtuals);
      /* Finally, add entries for functions that override virtuals
	 from non-primary bases.  */
      TYPE_BINFO_VIRTUALS (t) 
	= chainon (TYPE_BINFO_VIRTUALS (t), overridden_virtuals);
    }

  finish_struct_bits (t);

  /* Complete the rtl for any static member objects of the type we're
     working on.  */
  for (x = TYPE_FIELDS (t); x; x = TREE_CHAIN (x))
    if (TREE_CODE (x) == VAR_DECL && TREE_STATIC (x)
	&& TREE_TYPE (x) == t)
      DECL_MODE (x) = TYPE_MODE (t);

  /* Done with FIELDS...now decide whether to sort these for
     faster lookups later.

     The C front-end only does this when n_fields > 15.  We use
     a smaller number because most searches fail (succeeding
     ultimately as the search bores through the inheritance
     hierarchy), and we want this failure to occur quickly.  */

  n_fields = count_fields (TYPE_FIELDS (t));
  if (n_fields > 7)
    {
      tree field_vec = make_tree_vec (n_fields);
      add_fields_to_vec (TYPE_FIELDS (t), field_vec, 0);
      qsort (&TREE_VEC_ELT (field_vec, 0), n_fields, sizeof (tree),
	     (int (*)(const void *, const void *))field_decl_cmp);
      if (! DECL_LANG_SPECIFIC (TYPE_MAIN_DECL (t)))
	retrofit_lang_decl (TYPE_MAIN_DECL (t));
      DECL_SORTED_FIELDS (TYPE_MAIN_DECL (t)) = field_vec;
    }

  if (TYPE_HAS_CONSTRUCTOR (t))
    {
      tree vfields = CLASSTYPE_VFIELDS (t);

      while (vfields)
	{
	  /* Mark the fact that constructor for T
	     could affect anybody inheriting from T
	     who wants to initialize vtables for VFIELDS's type.  */
	  if (VF_DERIVED_VALUE (vfields))
	    TREE_ADDRESSABLE (vfields) = 1;
	  vfields = TREE_CHAIN (vfields);
	}
    }

  /* Make the rtl for any new vtables we have created, and unmark
     the base types we marked.  */
  finish_vtbls (t);
  
  /* Build the VTT for T.  */
  build_vtt (t);

  if (warn_nonvdtor && TYPE_POLYMORPHIC_P (t) && TYPE_HAS_DESTRUCTOR (t)
      && DECL_VINDEX (TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (t), 1)) == NULL_TREE)
    cp_warning ("`%#T' has virtual functions but non-virtual destructor", t);

  hack_incomplete_structures (t);

  if (warn_overloaded_virtual)
    warn_hidden (t);

  maybe_suppress_debug_info (t);

  dump_class_hierarchy (t);
  
  /* Finish debugging output for this type.  */
  rest_of_type_compilation (t, ! LOCAL_CLASS_P (t));
}

/* When T was built up, the member declarations were added in reverse
   order.  Rearrange them to declaration order.  */

void
unreverse_member_declarations (t)
     tree t;
{
  tree next;
  tree prev;
  tree x;

  /* The TYPE_FIELDS, TYPE_METHODS, and CLASSTYPE_TAGS are all in
     reverse order.  Put them in declaration order now.  */
  TYPE_METHODS (t) = nreverse (TYPE_METHODS (t));
  CLASSTYPE_TAGS (t) = nreverse (CLASSTYPE_TAGS (t));

  /* Actually, for the TYPE_FIELDS, only the non TYPE_DECLs are in
     reverse order, so we can't just use nreverse.  */
  prev = NULL_TREE;
  for (x = TYPE_FIELDS (t); 
       x && TREE_CODE (x) != TYPE_DECL; 
       x = next)
    {
      next = TREE_CHAIN (x);
      TREE_CHAIN (x) = prev;
      prev = x;
    }
  if (prev)
    {
      TREE_CHAIN (TYPE_FIELDS (t)) = x;
      if (prev)
	TYPE_FIELDS (t) = prev;
    }
}

tree
finish_struct (t, attributes)
     tree t, attributes;
{
  const char *saved_filename = input_filename;
  int saved_lineno = lineno;

  /* Now that we've got all the field declarations, reverse everything
     as necessary.  */
  unreverse_member_declarations (t);

  cplus_decl_attributes (t, attributes, NULL_TREE);

  /* Nadger the current location so that diagnostics point to the start of
     the struct, not the end.  */
  input_filename = DECL_SOURCE_FILE (TYPE_NAME (t));
  lineno = DECL_SOURCE_LINE (TYPE_NAME (t));

  if (processing_template_decl)
    {
      finish_struct_methods (t);
      TYPE_SIZE (t) = bitsize_zero_node;
    }
  else
    finish_struct_1 (t);

  input_filename = saved_filename;
  lineno = saved_lineno;

  TYPE_BEING_DEFINED (t) = 0;

  if (current_class_type)
    popclass ();
  else
    error ("trying to finish struct, but kicked out due to previous parse errors.");

  if (processing_template_decl)
    {
      tree scope = current_scope ();
      if (scope && TREE_CODE (scope) == FUNCTION_DECL)
	add_stmt (build_min (TAG_DEFN, t));
    }

  return t;
}

/* Return the dynamic type of INSTANCE, if known.
   Used to determine whether the virtual function table is needed
   or not.

   *NONNULL is set iff INSTANCE can be known to be nonnull, regardless
   of our knowledge of its type.  *NONNULL should be initialized
   before this function is called.  */

static tree
fixed_type_or_null (instance, nonnull, cdtorp)
     tree instance;
     int *nonnull;
     int *cdtorp;
{
  switch (TREE_CODE (instance))
    {
    case INDIRECT_REF:
      /* Check that we are not going through a cast of some sort.  */
      if (TREE_TYPE (instance)
	  == TREE_TYPE (TREE_TYPE (TREE_OPERAND (instance, 0))))
	instance = TREE_OPERAND (instance, 0);
      /* fall through...  */
    case CALL_EXPR:
      /* This is a call to a constructor, hence it's never zero.  */
      if (TREE_HAS_CONSTRUCTOR (instance))
	{
	  if (nonnull)
	    *nonnull = 1;
	  return TREE_TYPE (instance);
	}
      return NULL_TREE;

    case SAVE_EXPR:
      /* This is a call to a constructor, hence it's never zero.  */
      if (TREE_HAS_CONSTRUCTOR (instance))
	{
	  if (nonnull)
	    *nonnull = 1;
	  return TREE_TYPE (instance);
	}
      return fixed_type_or_null (TREE_OPERAND (instance, 0), nonnull, cdtorp);

    case RTL_EXPR:
      return NULL_TREE;

    case PLUS_EXPR:
    case MINUS_EXPR:
      if (TREE_CODE (TREE_OPERAND (instance, 0)) == ADDR_EXPR)
	return fixed_type_or_null (TREE_OPERAND (instance, 0), nonnull, cdtorp);
      if (TREE_CODE (TREE_OPERAND (instance, 1)) == INTEGER_CST)
	/* Propagate nonnull.  */
	fixed_type_or_null (TREE_OPERAND (instance, 0), nonnull, cdtorp);
      return NULL_TREE;

    case NOP_EXPR:
    case CONVERT_EXPR:
      return fixed_type_or_null (TREE_OPERAND (instance, 0), nonnull, cdtorp);

    case ADDR_EXPR:
      if (nonnull)
	*nonnull = 1;
      return fixed_type_or_null (TREE_OPERAND (instance, 0), nonnull, cdtorp);

    case COMPONENT_REF:
      return fixed_type_or_null (TREE_OPERAND (instance, 1), nonnull, cdtorp);

    case VAR_DECL:
    case FIELD_DECL:
      if (TREE_CODE (TREE_TYPE (instance)) == ARRAY_TYPE
	  && IS_AGGR_TYPE (TREE_TYPE (TREE_TYPE (instance))))
	{
	  if (nonnull)
	    *nonnull = 1;
	  return TREE_TYPE (TREE_TYPE (instance));
	}
      /* fall through...  */
    case TARGET_EXPR:
    case PARM_DECL:
      if (IS_AGGR_TYPE (TREE_TYPE (instance)))
	{
	  if (nonnull)
	    *nonnull = 1;
	  return TREE_TYPE (instance);
	}
      else if (instance == current_class_ptr)
        {
          if (nonnull)
            *nonnull = 1;
        
          /* if we're in a ctor or dtor, we know our type. */
          if (DECL_LANG_SPECIFIC (current_function_decl)
              && (DECL_CONSTRUCTOR_P (current_function_decl)
                  || DECL_DESTRUCTOR_P (current_function_decl)))
            {
              if (cdtorp)
                *cdtorp = 1;
              return TREE_TYPE (TREE_TYPE (instance));
            }
        }
      else if (TREE_CODE (TREE_TYPE (instance)) == REFERENCE_TYPE)
        {
          /* Reference variables should be references to objects.  */
          if (nonnull)
	    *nonnull = 1;
	}
      return NULL_TREE;

    default:
      return NULL_TREE;
    }
}

/* Return non-zero if the dynamic type of INSTANCE is known, and equivalent
   to the static type.  We also handle the case where INSTANCE is really
   a pointer. Return negative if this is a ctor/dtor. There the dynamic type
   is known, but this might not be the most derived base of the original object,
   and hence virtual bases may not be layed out according to this type.

   Used to determine whether the virtual function table is needed
   or not.

   *NONNULL is set iff INSTANCE can be known to be nonnull, regardless
   of our knowledge of its type.  *NONNULL should be initialized
   before this function is called.  */

int
resolves_to_fixed_type_p (instance, nonnull)
     tree instance;
     int *nonnull;
{
  tree t = TREE_TYPE (instance);
  int cdtorp = 0;
  
  tree fixed = fixed_type_or_null (instance, nonnull, &cdtorp);
  if (fixed == NULL_TREE)
    return 0;
  if (POINTER_TYPE_P (t))
    t = TREE_TYPE (t);
  if (!same_type_ignoring_top_level_qualifiers_p (t, fixed))
    return 0;
  return cdtorp ? -1 : 1;
}


void
init_class_processing ()
{
  current_class_depth = 0;
  current_class_stack_size = 10;
  current_class_stack 
    = (class_stack_node_t) xmalloc (current_class_stack_size 
				    * sizeof (struct class_stack_node));
  VARRAY_TREE_INIT (local_classes, 8, "local_classes");
  ggc_add_tree_varray_root (&local_classes, 1);

  access_default_node = build_int_2 (0, 0);
  access_public_node = build_int_2 (ak_public, 0);
  access_protected_node = build_int_2 (ak_protected, 0);
  access_private_node = build_int_2 (ak_private, 0);
  access_default_virtual_node = build_int_2 (4, 0);
  access_public_virtual_node = build_int_2 (4 | ak_public, 0);
  access_protected_virtual_node = build_int_2 (4 | ak_protected, 0);
  access_private_virtual_node = build_int_2 (4 | ak_private, 0);

  ridpointers[(int) RID_PUBLIC] = access_public_node;
  ridpointers[(int) RID_PRIVATE] = access_private_node;
  ridpointers[(int) RID_PROTECTED] = access_protected_node;
}

/* Set current scope to NAME. CODE tells us if this is a
   STRUCT, UNION, or ENUM environment.

   NAME may end up being NULL_TREE if this is an anonymous or
   late-bound struct (as in "struct { ... } foo;")  */

/* Set global variables CURRENT_CLASS_NAME and CURRENT_CLASS_TYPE to
   appropriate values, found by looking up the type definition of
   NAME (as a CODE).

   If MODIFY is 1, we set IDENTIFIER_CLASS_VALUE's of names
   which can be seen locally to the class.  They are shadowed by
   any subsequent local declaration (including parameter names).

   If MODIFY is 2, we set IDENTIFIER_CLASS_VALUE's of names
   which have static meaning (i.e., static members, static
   member functions, enum declarations, etc).

   If MODIFY is 3, we set IDENTIFIER_CLASS_VALUE of names
   which can be seen locally to the class (as in 1), but
   know that we are doing this for declaration purposes
   (i.e. friend foo::bar (int)).

   So that we may avoid calls to lookup_name, we cache the _TYPE
   nodes of local TYPE_DECLs in the TREE_TYPE field of the name.

   For multiple inheritance, we perform a two-pass depth-first search
   of the type lattice.  The first pass performs a pre-order search,
   marking types after the type has had its fields installed in
   the appropriate IDENTIFIER_CLASS_VALUE slot.  The second pass merely
   unmarks the marked types.  If a field or member function name
   appears in an ambiguous way, the IDENTIFIER_CLASS_VALUE of
   that name becomes `error_mark_node'.  */

void
pushclass (type, modify)
     tree type;
     int modify;
{
  type = TYPE_MAIN_VARIANT (type);

  /* Make sure there is enough room for the new entry on the stack.  */
  if (current_class_depth + 1 >= current_class_stack_size) 
    {
      current_class_stack_size *= 2;
      current_class_stack
	= (class_stack_node_t) xrealloc (current_class_stack,
					 current_class_stack_size
					 * sizeof (struct class_stack_node));
    }

  /* Insert a new entry on the class stack.  */
  current_class_stack[current_class_depth].name = current_class_name;
  current_class_stack[current_class_depth].type = current_class_type;
  current_class_stack[current_class_depth].access = current_access_specifier;
  current_class_stack[current_class_depth].names_used = 0;
  current_class_depth++;

  /* Now set up the new type.  */
  current_class_name = TYPE_NAME (type);
  if (TREE_CODE (current_class_name) == TYPE_DECL)
    current_class_name = DECL_NAME (current_class_name);
  current_class_type = type;

  /* By default, things in classes are private, while things in
     structures or unions are public.  */
  current_access_specifier = (CLASSTYPE_DECLARED_CLASS (type) 
			      ? access_private_node 
			      : access_public_node);

  if (previous_class_type != NULL_TREE
      && (type != previous_class_type 
	  || !COMPLETE_TYPE_P (previous_class_type))
      && current_class_depth == 1)
    {
      /* Forcibly remove any old class remnants.  */
      invalidate_class_lookup_cache ();
    }

  /* If we're about to enter a nested class, clear
     IDENTIFIER_CLASS_VALUE for the enclosing classes.  */
  if (modify && current_class_depth > 1)
    clear_identifier_class_values ();

  pushlevel_class ();

  if (modify)
    {
      if (type != previous_class_type || current_class_depth > 1)
	push_class_decls (type);
      else
	{
	  tree item;

	  /* We are re-entering the same class we just left, so we
	     don't have to search the whole inheritance matrix to find
	     all the decls to bind again.  Instead, we install the
	     cached class_shadowed list, and walk through it binding
	     names and setting up IDENTIFIER_TYPE_VALUEs.  */
	  set_class_shadows (previous_class_values);
	  for (item = previous_class_values; item; item = TREE_CHAIN (item))
	    {
	      tree id = TREE_PURPOSE (item);
	      tree decl = TREE_TYPE (item);

	      push_class_binding (id, decl);
	      if (TREE_CODE (decl) == TYPE_DECL)
		set_identifier_type_value (id, TREE_TYPE (decl));
	    }
	  unuse_fields (type);
	}

      storetags (CLASSTYPE_TAGS (type));
    }
}

/* When we exit a toplevel class scope, we save the
   IDENTIFIER_CLASS_VALUEs so that we can restore them quickly if we
   reenter the class.  Here, we've entered some other class, so we
   must invalidate our cache.  */

void
invalidate_class_lookup_cache ()
{
  tree t;
  
  /* The IDENTIFIER_CLASS_VALUEs are no longer valid.  */
  for (t = previous_class_values; t; t = TREE_CHAIN (t))
    IDENTIFIER_CLASS_VALUE (TREE_PURPOSE (t)) = NULL_TREE;

  previous_class_values = NULL_TREE;
  previous_class_type = NULL_TREE;
}
 
/* Get out of the current class scope. If we were in a class scope
   previously, that is the one popped to.  */

void
popclass ()
{
  poplevel_class ();
  /* Since poplevel_class does the popping of class decls nowadays,
     this really only frees the obstack used for these decls.  */
  pop_class_decls ();

  current_class_depth--;
  current_class_name = current_class_stack[current_class_depth].name;
  current_class_type = current_class_stack[current_class_depth].type;
  current_access_specifier = current_class_stack[current_class_depth].access;
  if (current_class_stack[current_class_depth].names_used)
    splay_tree_delete (current_class_stack[current_class_depth].names_used);
}

/* Returns 1 if current_class_type is either T or a nested type of T.
   We start looking from 1 because entry 0 is from global scope, and has
   no type.  */

int
currently_open_class (t)
     tree t;
{
  int i;
  if (t == current_class_type)
    return 1;
  for (i = 1; i < current_class_depth; ++i)
    if (current_class_stack [i].type == t)
      return 1;
  return 0;
}

/* If either current_class_type or one of its enclosing classes are derived
   from T, return the appropriate type.  Used to determine how we found
   something via unqualified lookup.  */

tree
currently_open_derived_class (t)
     tree t;
{
  int i;

  if (DERIVED_FROM_P (t, current_class_type))
    return current_class_type;

  for (i = current_class_depth - 1; i > 0; --i)
    if (DERIVED_FROM_P (t, current_class_stack[i].type))
      return current_class_stack[i].type;

  return NULL_TREE;
}

/* When entering a class scope, all enclosing class scopes' names with
   static meaning (static variables, static functions, types and enumerators)
   have to be visible.  This recursive function calls pushclass for all
   enclosing class contexts until global or a local scope is reached.
   TYPE is the enclosed class and MODIFY is equivalent with the pushclass
   formal of the same name.  */

void
push_nested_class (type, modify)
     tree type;
     int modify;
{
  tree context;

  /* A namespace might be passed in error cases, like A::B:C.  */
  if (type == NULL_TREE 
      || type == error_mark_node 
      || TREE_CODE (type) == NAMESPACE_DECL
      || ! IS_AGGR_TYPE (type)
      || TREE_CODE (type) == TEMPLATE_TYPE_PARM
      || TREE_CODE (type) == BOUND_TEMPLATE_TEMPLATE_PARM)
    return;
  
  context = DECL_CONTEXT (TYPE_MAIN_DECL (type));

  if (context && CLASS_TYPE_P (context))
    push_nested_class (context, 2);
  pushclass (type, modify);
}

/* Undoes a push_nested_class call.  MODIFY is passed on to popclass.  */

void
pop_nested_class ()
{
  tree context = DECL_CONTEXT (TYPE_MAIN_DECL (current_class_type));

  popclass ();
  if (context && CLASS_TYPE_P (context))
    pop_nested_class ();
}

/* Set global variables CURRENT_LANG_NAME to appropriate value
   so that behavior of name-mangling machinery is correct.  */

void
push_lang_context (name)
     tree name;
{
  *current_lang_stack++ = current_lang_name;
  if (current_lang_stack - &VARRAY_TREE (current_lang_base, 0)
      >= (ptrdiff_t) VARRAY_SIZE (current_lang_base))
    {
      size_t old_size = VARRAY_SIZE (current_lang_base);

      VARRAY_GROW (current_lang_base, old_size + 10);
      current_lang_stack = &VARRAY_TREE (current_lang_base, old_size);
    }

  if (name == lang_name_cplusplus)
    {
      current_lang_name = name;
    }
  else if (name == lang_name_java)
    {
      current_lang_name = name;
      /* DECL_IGNORED_P is initially set for these types, to avoid clutter.
	 (See record_builtin_java_type in decl.c.)  However, that causes
	 incorrect debug entries if these types are actually used.
	 So we re-enable debug output after extern "Java". */
      DECL_IGNORED_P (TYPE_NAME (java_byte_type_node)) = 0;
      DECL_IGNORED_P (TYPE_NAME (java_short_type_node)) = 0;
      DECL_IGNORED_P (TYPE_NAME (java_int_type_node)) = 0;
      DECL_IGNORED_P (TYPE_NAME (java_long_type_node)) = 0;
      DECL_IGNORED_P (TYPE_NAME (java_float_type_node)) = 0;
      DECL_IGNORED_P (TYPE_NAME (java_double_type_node)) = 0;
      DECL_IGNORED_P (TYPE_NAME (java_char_type_node)) = 0;
      DECL_IGNORED_P (TYPE_NAME (java_boolean_type_node)) = 0;
    }
  else if (name == lang_name_c)
    {
      current_lang_name = name;
    }
  else
    error ("language string `\"%s\"' not recognized", IDENTIFIER_POINTER (name));
}
  
/* Get out of the current language scope.  */

void
pop_lang_context ()
{
  /* Clear the current entry so that garbage collector won't hold on
     to it.  */
  *current_lang_stack = NULL_TREE;
  current_lang_name = *--current_lang_stack;
}

/* Type instantiation routines.  */

/* Given an OVERLOAD and a TARGET_TYPE, return the function that
   matches the TARGET_TYPE.  If there is no satisfactory match, return
   error_mark_node, and issue an error message if COMPLAIN is
   non-zero.  Permit pointers to member function if PTRMEM is non-zero.
   If TEMPLATE_ONLY, the name of the overloaded function
   was a template-id, and EXPLICIT_TARGS are the explicitly provided
   template arguments.  */

static tree
resolve_address_of_overloaded_function (target_type, 
					overload,
					complain,
	                                ptrmem,
					template_only,
					explicit_targs)
     tree target_type;
     tree overload;
     int complain;
     int ptrmem;
     int template_only;
     tree explicit_targs;
{
  /* Here's what the standard says:
     
       [over.over]

       If the name is a function template, template argument deduction
       is done, and if the argument deduction succeeds, the deduced
       arguments are used to generate a single template function, which
       is added to the set of overloaded functions considered.

       Non-member functions and static member functions match targets of
       type "pointer-to-function" or "reference-to-function."  Nonstatic
       member functions match targets of type "pointer-to-member
       function;" the function type of the pointer to member is used to
       select the member function from the set of overloaded member
       functions.  If a nonstatic member function is selected, the
       reference to the overloaded function name is required to have the
       form of a pointer to member as described in 5.3.1.

       If more than one function is selected, any template functions in
       the set are eliminated if the set also contains a non-template
       function, and any given template function is eliminated if the
       set contains a second template function that is more specialized
       than the first according to the partial ordering rules 14.5.5.2.
       After such eliminations, if any, there shall remain exactly one
       selected function.  */

  int is_ptrmem = 0;
  int is_reference = 0;
  /* We store the matches in a TREE_LIST rooted here.  The functions
     are the TREE_PURPOSE, not the TREE_VALUE, in this list, for easy
     interoperability with most_specialized_instantiation.  */
  tree matches = NULL_TREE;
  tree fn;

  /* By the time we get here, we should be seeing only real
     pointer-to-member types, not the internal POINTER_TYPE to
     METHOD_TYPE representation.  */
  my_friendly_assert (!(TREE_CODE (target_type) == POINTER_TYPE
			&& (TREE_CODE (TREE_TYPE (target_type)) 
			    == METHOD_TYPE)), 0);

  if (TREE_CODE (overload) == COMPONENT_REF)
    overload = TREE_OPERAND (overload, 1);

  /* Check that the TARGET_TYPE is reasonable.  */
  if (TYPE_PTRFN_P (target_type))
    /* This is OK.  */;
  else if (TYPE_PTRMEMFUNC_P (target_type))
    /* This is OK, too.  */
    is_ptrmem = 1;
  else if (TREE_CODE (target_type) == FUNCTION_TYPE)
    {
      /* This is OK, too.  This comes from a conversion to reference
	 type.  */
      target_type = build_reference_type (target_type);
      is_reference = 1;
    }
  else 
    {
      if (complain)
	cp_error ("\
cannot resolve overloaded function `%D' based on conversion to type `%T'", 
		  DECL_NAME (OVL_FUNCTION (overload)), target_type);
      return error_mark_node;
    }
  
  /* If we can find a non-template function that matches, we can just
     use it.  There's no point in generating template instantiations
     if we're just going to throw them out anyhow.  But, of course, we
     can only do this when we don't *need* a template function.  */
  if (!template_only)
    {
      tree fns;

      for (fns = overload; fns; fns = OVL_CHAIN (fns))
	{
	  tree fn = OVL_FUNCTION (fns);
	  tree fntype;

	  if (TREE_CODE (fn) == TEMPLATE_DECL)
	    /* We're not looking for templates just yet.  */
	    continue;

	  if ((TREE_CODE (TREE_TYPE (fn)) == METHOD_TYPE)
	      != is_ptrmem)
	    /* We're looking for a non-static member, and this isn't
	       one, or vice versa.  */
	    continue;
	
	  /* See if there's a match.  */
	  fntype = TREE_TYPE (fn);
	  if (is_ptrmem)
	    fntype = build_ptrmemfunc_type (build_pointer_type (fntype));
	  else if (!is_reference)
	    fntype = build_pointer_type (fntype);

	  if (can_convert_arg (target_type, fntype, fn))
	    matches = tree_cons (fn, NULL_TREE, matches);
	}
    }

  /* Now, if we've already got a match (or matches), there's no need
     to proceed to the template functions.  But, if we don't have a
     match we need to look at them, too.  */
  if (!matches) 
    {
      tree target_fn_type;
      tree target_arg_types;
      tree target_ret_type;
      tree fns;

      if (is_ptrmem)
	target_fn_type
	  = TREE_TYPE (TYPE_PTRMEMFUNC_FN_TYPE (target_type));
      else
	target_fn_type = TREE_TYPE (target_type);
      target_arg_types = TYPE_ARG_TYPES (target_fn_type);
      target_ret_type = TREE_TYPE (target_fn_type);

      /* Never do unification on the 'this' parameter.  */
      if (TREE_CODE (target_fn_type) == METHOD_TYPE)
	target_arg_types = TREE_CHAIN (target_arg_types);
	  
      for (fns = overload; fns; fns = OVL_CHAIN (fns))
	{
	  tree fn = OVL_FUNCTION (fns);
	  tree instantiation;
	  tree instantiation_type;
	  tree targs;

	  if (TREE_CODE (fn) != TEMPLATE_DECL)
	    /* We're only looking for templates.  */
	    continue;

	  if ((TREE_CODE (TREE_TYPE (fn)) == METHOD_TYPE)
	      != is_ptrmem)
	    /* We're not looking for a non-static member, and this is
	       one, or vice versa.  */
	    continue;

	  /* Try to do argument deduction.  */
	  targs = make_tree_vec (DECL_NTPARMS (fn));
	  if (fn_type_unification (fn, explicit_targs, targs,
				   target_arg_types, target_ret_type,
				   DEDUCE_EXACT, -1) != 0)
	    /* Argument deduction failed.  */
	    continue;

	  /* Instantiate the template.  */
	  instantiation = instantiate_template (fn, targs);
	  if (instantiation == error_mark_node)
	    /* Instantiation failed.  */
	    continue;

	  /* See if there's a match.  */
	  instantiation_type = TREE_TYPE (instantiation);
	  if (is_ptrmem)
	    instantiation_type = 
	      build_ptrmemfunc_type (build_pointer_type (instantiation_type));
	  else if (!is_reference)
	    instantiation_type = build_pointer_type (instantiation_type);
	  if (can_convert_arg (target_type, instantiation_type, instantiation))
	    matches = tree_cons (instantiation, fn, matches);
	}

      /* Now, remove all but the most specialized of the matches.  */
      if (matches)
	{
	  tree match = most_specialized_instantiation (matches);

	  if (match != error_mark_node)
	    matches = tree_cons (match, NULL_TREE, NULL_TREE);
	}
    }

  /* Now we should have exactly one function in MATCHES.  */
  if (matches == NULL_TREE)
    {
      /* There were *no* matches.  */
      if (complain)
	{
 	  cp_error ("no matches converting function `%D' to type `%#T'", 
		    DECL_NAME (OVL_FUNCTION (overload)),
		    target_type);

	  /* print_candidates expects a chain with the functions in
             TREE_VALUE slots, so we cons one up here (we're losing anyway,
             so why be clever?).  */
          for (; overload; overload = OVL_NEXT (overload))
            matches = tree_cons (NULL_TREE, OVL_CURRENT (overload),
				 matches);
          
	  print_candidates (matches);
	}
      return error_mark_node;
    }
  else if (TREE_CHAIN (matches))
    {
      /* There were too many matches.  */

      if (complain)
	{
	  tree match;

 	  cp_error ("converting overloaded function `%D' to type `%#T' is ambiguous", 
		    DECL_NAME (OVL_FUNCTION (overload)),
		    target_type);

	  /* Since print_candidates expects the functions in the
	     TREE_VALUE slot, we flip them here.  */
	  for (match = matches; match; match = TREE_CHAIN (match))
	    TREE_VALUE (match) = TREE_PURPOSE (match);

	  print_candidates (matches);
	}
      
      return error_mark_node;
    }

  /* Good, exactly one match.  Now, convert it to the correct type.  */
  fn = TREE_PURPOSE (matches);

  if (DECL_NONSTATIC_MEMBER_FUNCTION_P (fn)
      && !ptrmem && !flag_ms_extensions)
    {
      static int explained;
      
      if (!complain)
        return error_mark_node;

      cp_pedwarn ("assuming pointer to member `%D'", fn);
      if (!explained)
        {
          cp_pedwarn ("(a pointer to member can only be formed with `&%E')", fn);
          explained = 1;
        }
    }
  mark_used (fn);

  if (TYPE_PTRFN_P (target_type) || TYPE_PTRMEMFUNC_P (target_type))
    return build_unary_op (ADDR_EXPR, fn, 0);
  else
    {
      /* The target must be a REFERENCE_TYPE.  Above, build_unary_op
	 will mark the function as addressed, but here we must do it
	 explicitly.  */
      mark_addressable (fn);

      return fn;
    }
}

/* This function will instantiate the type of the expression given in
   RHS to match the type of LHSTYPE.  If errors exist, then return
   error_mark_node. FLAGS is a bit mask.  If ITF_COMPLAIN is set, then
   we complain on errors.  If we are not complaining, never modify rhs,
   as overload resolution wants to try many possible instantiations, in
   the hope that at least one will work.
   
   For non-recursive calls, LHSTYPE should be a function, pointer to
   function, or a pointer to member function.  */

tree
instantiate_type (lhstype, rhs, flags)
     tree lhstype, rhs;
     enum instantiate_type_flags flags;
{
  int complain = (flags & itf_complain);
  int strict = (flags & itf_no_attributes)
               ? COMPARE_NO_ATTRIBUTES : COMPARE_STRICT;
  int allow_ptrmem = flags & itf_ptrmem_ok;
  
  flags &= ~itf_ptrmem_ok;
  
  if (TREE_CODE (lhstype) == UNKNOWN_TYPE)
    {
      if (complain)
	error ("not enough type information");
      return error_mark_node;
    }

  if (TREE_TYPE (rhs) != NULL_TREE && ! (type_unknown_p (rhs)))
    {
      if (comptypes (lhstype, TREE_TYPE (rhs), strict))
	return rhs;
      if (complain)
	cp_error ("argument of type `%T' does not match `%T'",
		  TREE_TYPE (rhs), lhstype);
      return error_mark_node;
    }

  /* We don't overwrite rhs if it is an overloaded function.
     Copying it would destroy the tree link.  */
  if (TREE_CODE (rhs) != OVERLOAD)
    rhs = copy_node (rhs);

  /* This should really only be used when attempting to distinguish
     what sort of a pointer to function we have.  For now, any
     arithmetic operation which is not supported on pointers
     is rejected as an error.  */

  switch (TREE_CODE (rhs))
    {
    case TYPE_EXPR:
    case CONVERT_EXPR:
    case SAVE_EXPR:
    case CONSTRUCTOR:
    case BUFFER_REF:
      my_friendly_abort (177);
      return error_mark_node;

    case INDIRECT_REF:
    case ARRAY_REF:
      {
	tree new_rhs;

	new_rhs = instantiate_type (build_pointer_type (lhstype),
				    TREE_OPERAND (rhs, 0), flags);
	if (new_rhs == error_mark_node)
	  return error_mark_node;

	TREE_TYPE (rhs) = lhstype;
	TREE_OPERAND (rhs, 0) = new_rhs;
	return rhs;
      }

    case NOP_EXPR:
      rhs = copy_node (TREE_OPERAND (rhs, 0));
      TREE_TYPE (rhs) = unknown_type_node;
      return instantiate_type (lhstype, rhs, flags);

    case COMPONENT_REF:
      return instantiate_type (lhstype, TREE_OPERAND (rhs, 1), flags);

    case OFFSET_REF:
      rhs = TREE_OPERAND (rhs, 1);
      if (BASELINK_P (rhs))
	return instantiate_type (lhstype, TREE_VALUE (rhs),
	                         flags | allow_ptrmem);

      /* This can happen if we are forming a pointer-to-member for a
	 member template.  */
      my_friendly_assert (TREE_CODE (rhs) == TEMPLATE_ID_EXPR, 0);

      /* Fall through.  */

    case TEMPLATE_ID_EXPR:
      {
	tree fns = TREE_OPERAND (rhs, 0);
	tree args = TREE_OPERAND (rhs, 1);

	return
	  resolve_address_of_overloaded_function (lhstype,
						  fns,
						  complain,
	                                          allow_ptrmem,
						  /*template_only=*/1,
						  args);
      }

    case OVERLOAD:
      return 
	resolve_address_of_overloaded_function (lhstype, 
						rhs,
						complain,
	                                        allow_ptrmem,
						/*template_only=*/0,
						/*explicit_targs=*/NULL_TREE);

    case TREE_LIST:
      /* Now we should have a baselink. */
      my_friendly_assert (BASELINK_P (rhs), 990412);

      return instantiate_type (lhstype, TREE_VALUE (rhs), flags);

    case CALL_EXPR:
      /* This is too hard for now.  */
      my_friendly_abort (183);
      return error_mark_node;

    case PLUS_EXPR:
    case MINUS_EXPR:
    case COMPOUND_EXPR:
      TREE_OPERAND (rhs, 0)
	= instantiate_type (lhstype, TREE_OPERAND (rhs, 0), flags);
      if (TREE_OPERAND (rhs, 0) == error_mark_node)
	return error_mark_node;
      TREE_OPERAND (rhs, 1)
	= instantiate_type (lhstype, TREE_OPERAND (rhs, 1), flags);
      if (TREE_OPERAND (rhs, 1) == error_mark_node)
	return error_mark_node;

      TREE_TYPE (rhs) = lhstype;
      return rhs;

    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case RDIV_EXPR:
    case TRUNC_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case CEIL_MOD_EXPR:
    case ROUND_MOD_EXPR:
    case FIX_ROUND_EXPR:
    case FIX_FLOOR_EXPR:
    case FIX_CEIL_EXPR:
    case FIX_TRUNC_EXPR:
    case FLOAT_EXPR:
    case NEGATE_EXPR:
    case ABS_EXPR:
    case MAX_EXPR:
    case MIN_EXPR:
    case FFS_EXPR:

    case BIT_AND_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case LROTATE_EXPR:
    case RROTATE_EXPR:

    case PREINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
      if (complain)
	error ("invalid operation on uninstantiated type");
      return error_mark_node;

    case TRUTH_AND_EXPR:
    case TRUTH_OR_EXPR:
    case TRUTH_XOR_EXPR:
    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case TRUTH_NOT_EXPR:
      if (complain)
	error ("not enough type information");
      return error_mark_node;

    case COND_EXPR:
      if (type_unknown_p (TREE_OPERAND (rhs, 0)))
	{
	  if (complain)
	    error ("not enough type information");
	  return error_mark_node;
	}
      TREE_OPERAND (rhs, 1)
	= instantiate_type (lhstype, TREE_OPERAND (rhs, 1), flags);
      if (TREE_OPERAND (rhs, 1) == error_mark_node)
	return error_mark_node;
      TREE_OPERAND (rhs, 2)
	= instantiate_type (lhstype, TREE_OPERAND (rhs, 2), flags);
      if (TREE_OPERAND (rhs, 2) == error_mark_node)
	return error_mark_node;

      TREE_TYPE (rhs) = lhstype;
      return rhs;

    case MODIFY_EXPR:
      TREE_OPERAND (rhs, 1)
	= instantiate_type (lhstype, TREE_OPERAND (rhs, 1), flags);
      if (TREE_OPERAND (rhs, 1) == error_mark_node)
	return error_mark_node;

      TREE_TYPE (rhs) = lhstype;
      return rhs;
      
    case ADDR_EXPR:
    {
      if (PTRMEM_OK_P (rhs))
        flags |= itf_ptrmem_ok;
      
      return instantiate_type (lhstype, TREE_OPERAND (rhs, 0), flags);
    }
    case ENTRY_VALUE_EXPR:
      my_friendly_abort (184);
      return error_mark_node;

    case ERROR_MARK:
      return error_mark_node;

    default:
      my_friendly_abort (185);
      return error_mark_node;
    }
}

/* Return the name of the virtual function pointer field
   (as an IDENTIFIER_NODE) for the given TYPE.  Note that
   this may have to look back through base types to find the
   ultimate field name.  (For single inheritance, these could
   all be the same name.  Who knows for multiple inheritance).  */

static tree
get_vfield_name (type)
     tree type;
{
  tree binfo = TYPE_BINFO (type);
  char *buf;

  while (BINFO_BASETYPES (binfo)
	 && TYPE_CONTAINS_VPTR_P (BINFO_TYPE (BINFO_BASETYPE (binfo, 0)))
	 && ! TREE_VIA_VIRTUAL (BINFO_BASETYPE (binfo, 0)))
    binfo = BINFO_BASETYPE (binfo, 0);

  type = BINFO_TYPE (binfo);
  buf = (char *) alloca (sizeof (VFIELD_NAME_FORMAT)
			 + TYPE_NAME_LENGTH (type) + 2);
  sprintf (buf, VFIELD_NAME_FORMAT, TYPE_NAME_STRING (type));
  return get_identifier (buf);
}

void
print_class_statistics ()
{
#ifdef GATHER_STATISTICS
  fprintf (stderr, "convert_harshness = %d\n", n_convert_harshness);
  fprintf (stderr, "compute_conversion_costs = %d\n", n_compute_conversion_costs);
  fprintf (stderr, "build_method_call = %d (inner = %d)\n",
	   n_build_method_call, n_inner_fields_searched);
  if (n_vtables)
    {
      fprintf (stderr, "vtables = %d; vtable searches = %d\n",
	       n_vtables, n_vtable_searches);
      fprintf (stderr, "vtable entries = %d; vtable elems = %d\n",
	       n_vtable_entries, n_vtable_elems);
    }
#endif
}

/* Build a dummy reference to ourselves so Derived::Base (and A::A) works,
   according to [class]:
                                          The class-name is also inserted
   into  the scope of the class itself.  For purposes of access checking,
   the inserted class name is treated as if it were a public member name.  */

void
build_self_reference ()
{
  tree name = constructor_name (current_class_type);
  tree value = build_lang_decl (TYPE_DECL, name, current_class_type);
  tree saved_cas;

  DECL_NONLOCAL (value) = 1;
  DECL_CONTEXT (value) = current_class_type;
  DECL_ARTIFICIAL (value) = 1;

  if (processing_template_decl)
    value = push_template_decl (value);

  saved_cas = current_access_specifier;
  current_access_specifier = access_public_node;
  finish_member_declaration (value);
  current_access_specifier = saved_cas;
}

/* Returns 1 if TYPE contains only padding bytes.  */

int
is_empty_class (type)
     tree type;
{
  if (type == error_mark_node)
    return 0;

  if (! IS_AGGR_TYPE (type))
    return 0;

  return integer_zerop (CLASSTYPE_SIZE (type));
}

/* Find the enclosing class of the given NODE.  NODE can be a *_DECL or
   a *_TYPE node.  NODE can also be a local class.  */

tree
get_enclosing_class (type)
     tree type;
{
  tree node = type;

  while (node && TREE_CODE (node) != NAMESPACE_DECL)
    {
      switch (TREE_CODE_CLASS (TREE_CODE (node)))
	{
	case 'd':
	  node = DECL_CONTEXT (node);
	  break;

	case 't':
	  if (node != type)
	    return node;
	  node = TYPE_CONTEXT (node);
	  break;

	default:
	  my_friendly_abort (0);
	}
    }
  return NULL_TREE;
}

/* Return 1 if TYPE or one of its enclosing classes is derived from BASE.  */

int
is_base_of_enclosing_class (base, type)
     tree base, type;
{
  while (type)
    {
      if (get_binfo (base, type, 0))
	return 1;

      type = get_enclosing_class (type);
    }
  return 0;
}

/* Note that NAME was looked up while the current class was being
   defined and that the result of that lookup was DECL.  */

void
maybe_note_name_used_in_class (name, decl)
     tree name;
     tree decl;
{
  splay_tree names_used;

  /* If we're not defining a class, there's nothing to do.  */
  if (!current_class_type || !TYPE_BEING_DEFINED (current_class_type))
    return;
  
  /* If there's already a binding for this NAME, then we don't have
     anything to worry about.  */
  if (IDENTIFIER_CLASS_VALUE (name))
    return;

  if (!current_class_stack[current_class_depth - 1].names_used)
    current_class_stack[current_class_depth - 1].names_used
      = splay_tree_new (splay_tree_compare_pointers, 0, 0);
  names_used = current_class_stack[current_class_depth - 1].names_used;

  splay_tree_insert (names_used,
		     (splay_tree_key) name, 
		     (splay_tree_value) decl);
}

/* Note that NAME was declared (as DECL) in the current class.  Check
   to see that the declaration is legal.  */

void
note_name_declared_in_class (name, decl)
     tree name;
     tree decl;
{
  splay_tree names_used;
  splay_tree_node n;

  /* Look to see if we ever used this name.  */
  names_used 
    = current_class_stack[current_class_depth - 1].names_used;
  if (!names_used)
    return;

  n = splay_tree_lookup (names_used, (splay_tree_key) name);
  if (n)
    {
      /* [basic.scope.class]
	 
	 A name N used in a class S shall refer to the same declaration
	 in its context and when re-evaluated in the completed scope of
	 S.  */
      cp_error ("declaration of `%#D'", decl);
      cp_error_at ("changes meaning of `%D' from `%+#D'", 
		   DECL_NAME (OVL_CURRENT (decl)),
		   (tree) n->value);
    }
}

/* Returns the VAR_DECL for the complete vtable associated with
   BINFO.  (Under the new ABI, secondary vtables are merged with
   primary vtables; this function will return the VAR_DECL for the
   primary vtable.)  */

tree
get_vtbl_decl_for_binfo (binfo)
     tree binfo;
{
  tree decl;

  decl = BINFO_VTABLE (binfo);
  if (decl && TREE_CODE (decl) == PLUS_EXPR)
    {
      my_friendly_assert (TREE_CODE (TREE_OPERAND (decl, 0)) == ADDR_EXPR,
			  2000403);
      decl = TREE_OPERAND (TREE_OPERAND (decl, 0), 0);
    }
  if (decl)
    my_friendly_assert (TREE_CODE (decl) == VAR_DECL, 20000403);
  return decl;
}

/* Called from get_primary_binfo via dfs_walk.  DATA is a TREE_LIST
   who's TREE_PURPOSE is the TYPE of the required primary base and
   who's TREE_VALUE is a list of candidate binfos that we fill in. */

static tree
dfs_get_primary_binfo (binfo, data)
     tree binfo;
     void *data;
{
  tree cons = (tree) data;
  tree primary_base = TREE_PURPOSE (cons);

  if (TREE_VIA_VIRTUAL (binfo) 
      && same_type_p (BINFO_TYPE (binfo), primary_base))
    /* This is the right type of binfo, but it might be an unshared
       instance, and the shared instance is later in the dfs walk.  We
       must keep looking.  */
    TREE_VALUE (cons) = tree_cons (NULL, binfo, TREE_VALUE (cons));
  
  return NULL_TREE;
}

/* Returns the unshared binfo for the primary base of BINFO.  Note
   that in a complex hierarchy the resulting BINFO may not actually
   *be* primary.  In particular if the resulting BINFO is a virtual
   base, and it occurs elsewhere in the hierarchy, then this
   occurrence may not actually be a primary base in the complete
   object.  Check BINFO_PRIMARY_P to be sure.  */

tree
get_primary_binfo (binfo)
     tree binfo;
{
  tree primary_base;
  tree result;
  tree virtuals;
  
  primary_base = CLASSTYPE_PRIMARY_BINFO (BINFO_TYPE (binfo));
  if (!primary_base)
    return NULL_TREE;

  /* A non-virtual primary base is always a direct base, and easy to
     find.  */
  if (!TREE_VIA_VIRTUAL (primary_base))
    {
      int i;

      /* Scan the direct basetypes until we find a base with the same
	 type as the primary base.  */
      for (i = 0; i < BINFO_N_BASETYPES (binfo); ++i)
	{
	  tree base_binfo = BINFO_BASETYPE (binfo, i);
	  
	  if (same_type_p (BINFO_TYPE (base_binfo),
			   BINFO_TYPE (primary_base)))
	    return base_binfo;
	}

      /* We should always find the primary base.  */
      my_friendly_abort (20000729);
    }

  /* For a primary virtual base, we have to scan the entire hierarchy
     rooted at BINFO; the virtual base could be an indirect virtual
     base.  There could be more than one instance of the primary base
     in the hierarchy, and if one is the canonical binfo we want that
     one.  If it exists, it should be the first one we find, but as a
     consistency check we find them all and make sure.  */
  virtuals = build_tree_list (BINFO_TYPE (primary_base), NULL_TREE);
  dfs_walk (binfo, dfs_get_primary_binfo, NULL, virtuals);
  virtuals = TREE_VALUE (virtuals);
  
  /* We must have found at least one instance.  */
  my_friendly_assert (virtuals, 20010612);

  if (TREE_CHAIN (virtuals))
    {
      /* We found more than one instance of the base. We must make
         sure that, if one is the canonical one, it is the first one
         we found. As the chain is in reverse dfs order, that means
         the last on the list.  */
      tree complete_binfo;
      tree canonical;
      
      for (complete_binfo = binfo;
	   BINFO_INHERITANCE_CHAIN (complete_binfo);
	   complete_binfo = BINFO_INHERITANCE_CHAIN (complete_binfo))
	continue;
      canonical = binfo_for_vbase (BINFO_TYPE (primary_base),
				   BINFO_TYPE (complete_binfo));
      
      for (; virtuals; virtuals = TREE_CHAIN (virtuals))
	{
	  result = TREE_VALUE (virtuals);

	  if (canonical == result)
	    {
	      /* This is the unshared instance. Make sure it was the
		 first one found.  */
	      my_friendly_assert (!TREE_CHAIN (virtuals), 20010612);
	      break;
	    }
	}
    }
  else
    result = TREE_VALUE (virtuals);
  return result;
}

/* If INDENTED_P is zero, indent to INDENT. Return non-zero. */

static int
maybe_indent_hierarchy (stream, indent, indented_p)
     FILE *stream;
     int indent;
     int indented_p;
{
  if (!indented_p)
    fprintf (stream, "%*s", indent, "");
  return 1;
}

/* Dump the offsets of all the bases rooted at BINFO (in the hierarchy
   dominated by T) to stderr.  INDENT should be zero when called from
   the top level; it is incremented recursively.  */

static void
dump_class_hierarchy_r (stream, flags, t, binfo, indent)
     FILE *stream;
     int flags;
     tree t;
     tree binfo;
     int indent;
{
  int i;
  int indented = 0;
  
  indented = maybe_indent_hierarchy (stream, indent, 0);
  fprintf (stream, "%s (0x%lx) ",
	   type_as_string (binfo, TFF_PLAIN_IDENTIFIER),
	   (unsigned long) binfo);
  fprintf (stream, HOST_WIDE_INT_PRINT_DEC,
	   tree_low_cst (BINFO_OFFSET (binfo), 0));
  if (is_empty_class (BINFO_TYPE (binfo)))
    fprintf (stream, " empty");
  else if (CLASSTYPE_NEARLY_EMPTY_P (BINFO_TYPE (binfo)))
    fprintf (stream, " nearly-empty");
  if (TREE_VIA_VIRTUAL (binfo))
    {
      tree canonical = binfo_for_vbase (BINFO_TYPE (binfo), t);

      fprintf (stream, " virtual");
      if (canonical == binfo)
        fprintf (stream, " canonical");
      else
        fprintf (stream, " non-canonical");
    }
  fprintf (stream, "\n");

  indented = 0;
  if (BINFO_PRIMARY_BASE_OF (binfo))
    {
      indented = maybe_indent_hierarchy (stream, indent + 3, indented);
      fprintf (stream, " primary-for %s (0x%lx)",
	       type_as_string (BINFO_PRIMARY_BASE_OF (binfo),
			       TFF_PLAIN_IDENTIFIER),
	       (unsigned long)BINFO_PRIMARY_BASE_OF (binfo));
    }
  if (BINFO_LOST_PRIMARY_P (binfo))
    {
      indented = maybe_indent_hierarchy (stream, indent + 3, indented);
      fprintf (stream, " lost-primary");
    }
  if (indented)
    fprintf (stream, "\n");

  if (!(flags & TDF_SLIM))
    {
      int indented = 0;
      
      if (BINFO_SUBVTT_INDEX (binfo))
	{
	  indented = maybe_indent_hierarchy (stream, indent + 3, indented);
	  fprintf (stream, " subvttidx=%s",
		   expr_as_string (BINFO_SUBVTT_INDEX (binfo),
				   TFF_PLAIN_IDENTIFIER));
	}
      if (BINFO_VPTR_INDEX (binfo))
	{
	  indented = maybe_indent_hierarchy (stream, indent + 3, indented);
	  fprintf (stream, " vptridx=%s",
		   expr_as_string (BINFO_VPTR_INDEX (binfo),
				   TFF_PLAIN_IDENTIFIER));
	}
      if (BINFO_VPTR_FIELD (binfo))
	{
	  indented = maybe_indent_hierarchy (stream, indent + 3, indented);
	  fprintf (stream, " vbaseoffset=%s",
		   expr_as_string (BINFO_VPTR_FIELD (binfo),
				   TFF_PLAIN_IDENTIFIER));
	}
      if (BINFO_VTABLE (binfo))
	{
	  indented = maybe_indent_hierarchy (stream, indent + 3, indented);
	  fprintf (stream, " vptr=%s",
		   expr_as_string (BINFO_VTABLE (binfo),
				   TFF_PLAIN_IDENTIFIER));
	}
      
      if (indented)
	fprintf (stream, "\n");
    }
  

  for (i = 0; i < BINFO_N_BASETYPES (binfo); ++i)
    dump_class_hierarchy_r (stream, flags,
			    t, BINFO_BASETYPE (binfo, i),
			    indent + 2);
}

/* Dump the BINFO hierarchy for T.  */

static void
dump_class_hierarchy (t)
     tree t;
{
  int flags;
  FILE *stream = dump_begin (TDI_class, &flags);

  if (!stream)
    return;
  
  fprintf (stream, "Class %s\n", type_as_string (t, TFF_PLAIN_IDENTIFIER));
  fprintf (stream, "   size=%lu align=%lu\n",
	   (unsigned long)(tree_low_cst (TYPE_SIZE (t), 0) / BITS_PER_UNIT),
	   (unsigned long)(TYPE_ALIGN (t) / BITS_PER_UNIT));
  dump_class_hierarchy_r (stream, flags, t, TYPE_BINFO (t), 0);
  fprintf (stream, "\n");
  dump_end (TDI_class, stream);
}

static void
dump_array (stream, decl)
     FILE *stream;
     tree decl;
{
  tree inits;
  int ix;
  HOST_WIDE_INT elt;
  tree size = TYPE_MAX_VALUE (TYPE_DOMAIN (TREE_TYPE (decl)));

  elt = (tree_low_cst (TYPE_SIZE (TREE_TYPE (TREE_TYPE (decl))), 0)
	 / BITS_PER_UNIT);
  fprintf (stream, "%s:", decl_as_string (decl, TFF_PLAIN_IDENTIFIER));
  fprintf (stream, " %s entries",
	   expr_as_string (size_binop (PLUS_EXPR, size, size_one_node),
			   TFF_PLAIN_IDENTIFIER));
  fprintf (stream, "\n");

  for (ix = 0, inits = TREE_OPERAND (DECL_INITIAL (decl), 1);
       inits; ix++, inits = TREE_CHAIN (inits))
    fprintf (stream, "%-4d  %s\n", ix * elt,
	     expr_as_string (TREE_VALUE (inits), TFF_PLAIN_IDENTIFIER));
}

static void
dump_vtable (t, binfo, vtable)
     tree t;
     tree binfo;
     tree vtable;
{
  int flags;
  FILE *stream = dump_begin (TDI_class, &flags);

  if (!stream)
    return;

  if (!(flags & TDF_SLIM))
    {
      int ctor_vtbl_p = TYPE_BINFO (t) != binfo;
      
      fprintf (stream, "%s for %s",
	       ctor_vtbl_p ? "Construction vtable" : "Vtable",
	       type_as_string (binfo, TFF_PLAIN_IDENTIFIER));
      if (ctor_vtbl_p)
	{
	  if (!TREE_VIA_VIRTUAL (binfo))
	    fprintf (stream, " (0x%lx instance)", (unsigned long)binfo);
	  fprintf (stream, " in %s", type_as_string (t, TFF_PLAIN_IDENTIFIER));
	}
      fprintf (stream, "\n");
      dump_array (stream, vtable);
      fprintf (stream, "\n");
    }
  
  dump_end (TDI_class, stream);
}

static void
dump_vtt (t, vtt)
     tree t;
     tree vtt;
{
  int flags;
  FILE *stream = dump_begin (TDI_class, &flags);

  if (!stream)
    return;

  if (!(flags & TDF_SLIM))
    {
      fprintf (stream, "VTT for %s\n",
	       type_as_string (t, TFF_PLAIN_IDENTIFIER));
      dump_array (stream, vtt);
      fprintf (stream, "\n");
    }
  
  dump_end (TDI_class, stream);
}

/* Virtual function table initialization.  */

/* Create all the necessary vtables for T and its base classes.  */

static void
finish_vtbls (t)
     tree t;
{
  if (merge_primary_and_secondary_vtables_p ())
    {
      tree list;
      tree vbase;

      /* Under the new ABI, we lay out the primary and secondary
	 vtables in one contiguous vtable.  The primary vtable is
	 first, followed by the non-virtual secondary vtables in
	 inheritance graph order.  */
      list = build_tree_list (TYPE_BINFO_VTABLE (t), NULL_TREE);
      accumulate_vtbl_inits (TYPE_BINFO (t), TYPE_BINFO (t),
			     TYPE_BINFO (t), t, list);
      /* Then come the virtual bases, also in inheritance graph
	 order.  */
      for (vbase = TYPE_BINFO (t); vbase; vbase = TREE_CHAIN (vbase))
	{
	  tree real_base;
	  
	  if (!TREE_VIA_VIRTUAL (vbase))
	    continue;
          
          /* Although we walk in inheritance order, that might not get the
             canonical base.  */
          real_base = binfo_for_vbase (BINFO_TYPE (vbase), t);
          
	  accumulate_vtbl_inits (real_base, real_base,
	                         TYPE_BINFO (t), t, list);
	}

      if (TYPE_BINFO_VTABLE (t))
	initialize_vtable (TYPE_BINFO (t), TREE_VALUE (list));
    }
  else
    {
      dfs_walk (TYPE_BINFO (t), dfs_finish_vtbls, 
		dfs_unmarked_real_bases_queue_p, t);
      dfs_walk (TYPE_BINFO (t), dfs_unmark, 
		dfs_marked_real_bases_queue_p, t);
    }
}

/* Called from finish_vtbls via dfs_walk.  */

static tree
dfs_finish_vtbls (binfo, data)
     tree binfo;
     void *data;
{
  tree t = (tree) data;

  if (BINFO_NEW_VTABLE_MARKED (binfo, t))
    initialize_vtable (binfo, 
		       build_vtbl_initializer (binfo, binfo, t, 
					       TYPE_BINFO (t), NULL));

  SET_BINFO_MARKED (binfo);

  return NULL_TREE;
}

/* Initialize the vtable for BINFO with the INITS.  */

static void
initialize_vtable (binfo, inits)
     tree binfo;
     tree inits;
{
  tree decl;

  layout_vtable_decl (binfo, list_length (inits));
  decl = get_vtbl_decl_for_binfo (binfo);
  initialize_array (decl, inits);
  dump_vtable (BINFO_TYPE (binfo), binfo, decl);
}

/* Initialize DECL (a declaration for a namespace-scope array) with
   the INITS.  */

static void
initialize_array (decl, inits)
  tree decl;
  tree inits;
{
  tree context;

  context = DECL_CONTEXT (decl);
  DECL_CONTEXT (decl) = NULL_TREE;
  DECL_INITIAL (decl) = build_nt (CONSTRUCTOR, NULL_TREE, inits);
  cp_finish_decl (decl, DECL_INITIAL (decl), NULL_TREE, 0);
  DECL_CONTEXT (decl) = context;
}

/* Build the VTT (virtual table table) for T.
   A class requires a VTT if it has virtual bases.
   
   This holds
   1 - primary virtual pointer for complete object T
   2 - secondary VTTs for each direct non-virtual base of T which requires a VTT
   3 - secondary virtual pointers for each direct or indirect base of T which
       has virtual bases or is reachable via a virtual path from T.
   4 - secondary VTTs for each direct or indirect virtual base of T.
   
   Secondary VTTs look like complete object VTTs without part 4.  */

static void
build_vtt (t)
     tree t;
{
  tree inits;
  tree type;
  tree vtt;
  tree index;

  /* Build up the initializers for the VTT.  */
  inits = NULL_TREE;
  index = size_zero_node;
  build_vtt_inits (TYPE_BINFO (t), t, &inits, &index);

  /* If we didn't need a VTT, we're done.  */
  if (!inits)
    return;

  /* Figure out the type of the VTT.  */
  type = build_index_type (size_int (list_length (inits) - 1));
  type = build_cplus_array_type (const_ptr_type_node, type);
				 
  /* Now, build the VTT object itself.  */
  vtt = build_vtable (t, get_vtt_name (t), type);
  pushdecl_top_level (vtt);
  initialize_array (vtt, inits);

  dump_vtt (t, vtt);
}

/* The type corresponding to BASE_BINFO is a base of the type of BINFO, but
   from within some heirarchy which is inherited from the type of BINFO.
   Return BASE_BINFO's equivalent binfo from the hierarchy dominated by
   BINFO.  */

static tree
get_original_base (base_binfo, binfo)
     tree base_binfo;
     tree binfo;
{
  tree derived;
  int ix;
  
  if (same_type_p (BINFO_TYPE (base_binfo), BINFO_TYPE (binfo)))
    return binfo;
  if (TREE_VIA_VIRTUAL (base_binfo))
    return binfo_for_vbase (BINFO_TYPE (base_binfo), BINFO_TYPE (binfo));
  derived = get_original_base (BINFO_INHERITANCE_CHAIN (base_binfo), binfo);
  
  for (ix = 0; ix != BINFO_N_BASETYPES (derived); ix++)
    if (same_type_p (BINFO_TYPE (base_binfo),
                     BINFO_TYPE (BINFO_BASETYPE (derived, ix))))
      return BINFO_BASETYPE (derived, ix);
  my_friendly_abort (20010223);
  return NULL;
}

/* When building a secondary VTT, BINFO_VTABLE is set to a TREE_LIST with
   PURPOSE the RTTI_BINFO, VALUE the real vtable pointer for this binfo,
   and CHAIN the vtable pointer for this binfo after construction is
   complete.  VALUE can also be another BINFO, in which case we recurse. */

static tree
binfo_ctor_vtable (binfo)
     tree binfo;
{
  tree vt;

  while (1)
    {
      vt = BINFO_VTABLE (binfo);
      if (TREE_CODE (vt) == TREE_LIST)
	vt = TREE_VALUE (vt);
      if (TREE_CODE (vt) == TREE_VEC)
	binfo = vt;
      else
	break;
    }

  return vt;
}

/* Recursively build the VTT-initializer for BINFO (which is in the
   hierarchy dominated by T).  INITS points to the end of the initializer
   list to date.  INDEX is the VTT index where the next element will be
   replaced.  Iff BINFO is the binfo for T, this is the top level VTT (i.e.
   not a subvtt for some base of T).  When that is so, we emit the sub-VTTs
   for virtual bases of T. When it is not so, we build the constructor
   vtables for the BINFO-in-T variant.  */

static tree *
build_vtt_inits (binfo, t, inits, index)
     tree binfo;
     tree t;
     tree *inits;
     tree *index;
{
  int i;
  tree b;
  tree init;
  tree secondary_vptrs;
  int top_level_p = same_type_p (TREE_TYPE (binfo), t);

  /* We only need VTTs for subobjects with virtual bases.  */
  if (!TYPE_USES_VIRTUAL_BASECLASSES (BINFO_TYPE (binfo)))
    return inits;

  /* We need to use a construction vtable if this is not the primary
     VTT.  */
  if (!top_level_p)
    {
      build_ctor_vtbl_group (binfo, t);

      /* Record the offset in the VTT where this sub-VTT can be found.  */
      BINFO_SUBVTT_INDEX (binfo) = *index;
    }

  /* Add the address of the primary vtable for the complete object.  */
  init = binfo_ctor_vtable (binfo);
  *inits = build_tree_list (NULL_TREE, init);
  inits = &TREE_CHAIN (*inits);
  if (top_level_p)
    {
      my_friendly_assert (!BINFO_VPTR_INDEX (binfo), 20010129);
      BINFO_VPTR_INDEX (binfo) = *index;
    }
  *index = size_binop (PLUS_EXPR, *index, TYPE_SIZE_UNIT (ptr_type_node));
		       
  /* Recursively add the secondary VTTs for non-virtual bases.  */
  for (i = 0; i < BINFO_N_BASETYPES (binfo); ++i)
    {
      b = BINFO_BASETYPE (binfo, i);
      if (!TREE_VIA_VIRTUAL (b))
	inits = build_vtt_inits (BINFO_BASETYPE (binfo, i), t, 
				 inits, index);
    }
      
  /* Add secondary virtual pointers for all subobjects of BINFO with
     either virtual bases or reachable along a virtual path, except
     subobjects that are non-virtual primary bases.  */
  secondary_vptrs = tree_cons (t, NULL_TREE, BINFO_TYPE (binfo));
  TREE_TYPE (secondary_vptrs) = *index;
  VTT_TOP_LEVEL_P (secondary_vptrs) = top_level_p;
  VTT_MARKED_BINFO_P (secondary_vptrs) = 0;
  
  dfs_walk_real (binfo,
		 dfs_build_secondary_vptr_vtt_inits,
		 NULL,
	         dfs_ctor_vtable_bases_queue_p,
		 secondary_vptrs);
  VTT_MARKED_BINFO_P (secondary_vptrs) = 1;
  dfs_walk (binfo, dfs_unmark, dfs_ctor_vtable_bases_queue_p,
            secondary_vptrs);

  *index = TREE_TYPE (secondary_vptrs);

  /* The secondary vptrs come back in reverse order.  After we reverse
     them, and add the INITS, the last init will be the first element
     of the chain.  */
  secondary_vptrs = TREE_VALUE (secondary_vptrs);
  if (secondary_vptrs)
    {
      *inits = nreverse (secondary_vptrs);
      inits = &TREE_CHAIN (secondary_vptrs);
      my_friendly_assert (*inits == NULL_TREE, 20000517);
    }

  /* Add the secondary VTTs for virtual bases.  */
  if (top_level_p)
    for (b = TYPE_BINFO (BINFO_TYPE (binfo)); b; b = TREE_CHAIN (b))
      {
	tree vbase;
	
	if (!TREE_VIA_VIRTUAL (b))
	  continue;
	
	vbase = binfo_for_vbase (BINFO_TYPE (b), t);
	inits = build_vtt_inits (vbase, t, inits, index);
      }

  if (!top_level_p)
    {
      tree data = tree_cons (t, binfo, NULL_TREE);
      VTT_TOP_LEVEL_P (data) = 0;
      VTT_MARKED_BINFO_P (data) = 0;
      
      dfs_walk (binfo, dfs_fixup_binfo_vtbls,
	        dfs_ctor_vtable_bases_queue_p,
	        data);
    }

  return inits;
}

/* Called from build_vtt_inits via dfs_walk.  BINFO is the binfo
   for the base in most derived. DATA is a TREE_LIST who's
   TREE_CHAIN is the type of the base being
   constructed whilst this secondary vptr is live.  The TREE_UNSIGNED
   flag of DATA indicates that this is a constructor vtable.  The
   TREE_TOP_LEVEL flag indicates that this is the primary VTT.  */

static tree
dfs_build_secondary_vptr_vtt_inits (binfo, data)
     tree binfo;
     void *data;
{
  tree l; 
  tree t;
  tree init;
  tree index;
  int top_level_p;

  l = (tree) data;
  t = TREE_CHAIN (l);
  top_level_p = VTT_TOP_LEVEL_P (l);
  
  SET_BINFO_MARKED (binfo);

  /* We don't care about bases that don't have vtables.  */
  if (!TYPE_VFIELD (BINFO_TYPE (binfo)))
    return NULL_TREE;

  /* We're only interested in proper subobjects of T.  */
  if (same_type_p (BINFO_TYPE (binfo), t))
    return NULL_TREE;

  /* We're not interested in non-virtual primary bases.  */
  if (!TREE_VIA_VIRTUAL (binfo) && BINFO_PRIMARY_P (binfo))
    return NULL_TREE;

  /* If BINFO has virtual bases or is reachable via a virtual path
     from T, it'll have a secondary vptr.  */
  if (!TYPE_USES_VIRTUAL_BASECLASSES (BINFO_TYPE (binfo))
      && !binfo_via_virtual (binfo, t))
    return NULL_TREE;

  /* Record the index where this secondary vptr can be found.  */
  index = TREE_TYPE (l);
  if (top_level_p)
    {
      my_friendly_assert (!BINFO_VPTR_INDEX (binfo), 20010129);
      BINFO_VPTR_INDEX (binfo) = index;
    }
  TREE_TYPE (l) = size_binop (PLUS_EXPR, index, 
			      TYPE_SIZE_UNIT (ptr_type_node));

  /* Add the initializer for the secondary vptr itself.  */
  if (top_level_p && TREE_VIA_VIRTUAL (binfo))
    {
      /* It's a primary virtual base, and this is not the construction
         vtable. Find the base this is primary of in the inheritance graph,
         and use that base's vtable now. */
      while (BINFO_PRIMARY_BASE_OF (binfo))
        binfo = BINFO_PRIMARY_BASE_OF (binfo);
    }
  init = binfo_ctor_vtable (binfo);
  TREE_VALUE (l) = tree_cons (NULL_TREE, init, TREE_VALUE (l));

  return NULL_TREE;
}

/* dfs_walk_real predicate for building vtables. DATA is a TREE_LIST,
   VTT_MARKED_BINFO_P indicates whether marked or unmarked bases
   should be walked.  TREE_PURPOSE is the TREE_TYPE that dominates the
   hierarchy.  */

static tree
dfs_ctor_vtable_bases_queue_p (binfo, data)
     tree binfo;
     void *data;
{
  if (TREE_VIA_VIRTUAL (binfo))
     /* Get the shared version.  */
    binfo = binfo_for_vbase (BINFO_TYPE (binfo), TREE_PURPOSE ((tree) data));

  if (!BINFO_MARKED (binfo) == VTT_MARKED_BINFO_P ((tree) data))
    return NULL_TREE;
  return binfo;
}

/* Called from build_vtt_inits via dfs_walk. After building constructor
   vtables and generating the sub-vtt from them, we need to restore the
   BINFO_VTABLES that were scribbled on.  DATA is a TREE_LIST whose
   TREE_VALUE is the TREE_TYPE of the base whose sub vtt was generated.  */

static tree
dfs_fixup_binfo_vtbls (binfo, data)
     tree binfo;
     void *data;
{
  CLEAR_BINFO_MARKED (binfo);

  /* We don't care about bases that don't have vtables.  */
  if (!TYPE_VFIELD (BINFO_TYPE (binfo)))
    return NULL_TREE;

  /* If we scribbled the construction vtable vptr into BINFO, clear it
     out now.  */
  if (BINFO_VTABLE (binfo)
      && TREE_CODE (BINFO_VTABLE (binfo)) == TREE_LIST
      && (TREE_PURPOSE (BINFO_VTABLE (binfo)) 
	  == TREE_VALUE ((tree) data)))
    BINFO_VTABLE (binfo) = TREE_CHAIN (BINFO_VTABLE (binfo));

  return NULL_TREE;
}

/* Build the construction vtable group for BINFO which is in the
   hierarchy dominated by T.  */

static void
build_ctor_vtbl_group (binfo, t)
     tree binfo;
     tree t;
{
  tree list;
  tree type;
  tree vtbl;
  tree inits;
  tree id;
  tree vbase;

  /* See if we've already created this construction vtable group.  */
  id = mangle_ctor_vtbl_for_type (t, binfo);
  if (IDENTIFIER_GLOBAL_VALUE (id))
    return;

  my_friendly_assert (!same_type_p (BINFO_TYPE (binfo), t), 20010124);
  /* Build a version of VTBL (with the wrong type) for use in
     constructing the addresses of secondary vtables in the
     construction vtable group.  */
  vtbl = build_vtable (t, id, ptr_type_node);
  list = build_tree_list (vtbl, NULL_TREE);
  accumulate_vtbl_inits (binfo, TYPE_BINFO (TREE_TYPE (binfo)),
			 binfo, t, list);

  /* Add the vtables for each of our virtual bases using the vbase in T
     binfo.  */
  for (vbase = TYPE_BINFO (BINFO_TYPE (binfo)); 
       vbase; 
       vbase = TREE_CHAIN (vbase))
    {
      tree b;
      tree orig_base;

      if (!TREE_VIA_VIRTUAL (vbase))
	continue;
      b = binfo_for_vbase (BINFO_TYPE (vbase), t);
      orig_base = binfo_for_vbase (BINFO_TYPE (vbase), BINFO_TYPE (binfo));
      
      accumulate_vtbl_inits (b, orig_base, binfo, t, list);
    }
  inits = TREE_VALUE (list);

  /* Figure out the type of the construction vtable.  */
  type = build_index_type (size_int (list_length (inits) - 1));
  type = build_cplus_array_type (vtable_entry_type, type);
  TREE_TYPE (vtbl) = type;

  /* Initialize the construction vtable.  */
  pushdecl_top_level (vtbl);
  initialize_array (vtbl, inits);
  dump_vtable (t, binfo, vtbl);
}

/* Add the vtbl initializers for BINFO (and its bases other than
   non-virtual primaries) to the list of INITS.  BINFO is in the
   hierarchy dominated by T.  RTTI_BINFO is the binfo within T of
   the constructor the vtbl inits should be accumulated for. (If this
   is the complete object vtbl then RTTI_BINFO will be TYPE_BINFO (T).)
   ORIG_BINFO is the binfo for this object within BINFO_TYPE (RTTI_BINFO).
   BINFO is the active base equivalent of ORIG_BINFO in the inheritance
   graph of T. Both BINFO and ORIG_BINFO will have the same BINFO_TYPE,
   but are not necessarily the same in terms of layout.  */

static void
accumulate_vtbl_inits (binfo, orig_binfo, rtti_binfo, t, inits)
     tree binfo;
     tree orig_binfo;
     tree rtti_binfo;
     tree t;
     tree inits;
{
  int i;
  int ctor_vtbl_p = !same_type_p (BINFO_TYPE (rtti_binfo), t);

  my_friendly_assert (same_type_p (BINFO_TYPE (binfo),
				   BINFO_TYPE (orig_binfo)),
		      20000517);

  /* If it doesn't have a vptr, we don't do anything. */
  if (!TYPE_CONTAINS_VPTR_P (BINFO_TYPE (binfo)))
    return;
  
  /* If we're building a construction vtable, we're not interested in
     subobjects that don't require construction vtables.  */
  if (ctor_vtbl_p 
      && !TYPE_USES_VIRTUAL_BASECLASSES (BINFO_TYPE (binfo))
      && !binfo_via_virtual (orig_binfo, BINFO_TYPE (rtti_binfo)))
    return;

  /* Build the initializers for the BINFO-in-T vtable.  */
  TREE_VALUE (inits) 
    = chainon (TREE_VALUE (inits),
	       dfs_accumulate_vtbl_inits (binfo, orig_binfo,
					  rtti_binfo, t, inits));
		      
  /* Walk the BINFO and its bases.  We walk in preorder so that as we
     initialize each vtable we can figure out at what offset the
     secondary vtable lies from the primary vtable.  We can't use
     dfs_walk here because we need to iterate through bases of BINFO
     and RTTI_BINFO simultaneously.  */
  for (i = 0; i < BINFO_N_BASETYPES (binfo); ++i)
    {
      tree base_binfo = BINFO_BASETYPE (binfo, i);
      
      /* Skip virtual bases.  */
      if (TREE_VIA_VIRTUAL (base_binfo))
	continue;
      accumulate_vtbl_inits (base_binfo,
			     BINFO_BASETYPE (orig_binfo, i),
			     rtti_binfo, t,
			     inits);
    }
}

/* Called from accumulate_vtbl_inits when using the new ABI.
   Accumulates the vtable initializers for all of the vtables into
   TREE_VALUE (DATA).  Returns the initializers for the BINFO vtable.  */

static tree
dfs_accumulate_vtbl_inits (binfo, orig_binfo, rtti_binfo, t, l)
     tree binfo;
     tree orig_binfo;
     tree rtti_binfo;
     tree t;
     tree l;
{
  tree inits = NULL_TREE;
  tree vtbl = NULL_TREE;
  int ctor_vtbl_p = !same_type_p (BINFO_TYPE (rtti_binfo), t);

  if (ctor_vtbl_p
      && TREE_VIA_VIRTUAL (orig_binfo) && BINFO_PRIMARY_P (orig_binfo))
    {
      /* In the hierarchy of BINFO_TYPE (RTTI_BINFO), this is a
	 primary virtual base.  If it is not the same primary in
	 the hierarchy of T, we'll need to generate a ctor vtable
	 for it, to place at its location in T.  If it is the same
	 primary, we still need a VTT entry for the vtable, but it
	 should point to the ctor vtable for the base it is a
	 primary for within the sub-hierarchy of RTTI_BINFO.
	      
	 There are three possible cases:
	      
	 1) We are in the same place.
	 2) We are a primary base within a lost primary virtual base of
	 RTTI_BINFO.
	 3) We are primary to something not a base of RTTI_BINFO.  */
	  
      tree b = BINFO_PRIMARY_BASE_OF (binfo);
      tree last = NULL_TREE;

      /* First, look through the bases we are primary to for RTTI_BINFO
	 or a virtual base.  */
      for (; b; b = BINFO_PRIMARY_BASE_OF (b))
	{
	  last = b;
	  if (TREE_VIA_VIRTUAL (b) || b == rtti_binfo)
	    break;
	}
      /* If we run out of primary links, keep looking down our
	 inheritance chain; we might be an indirect primary.  */
      if (b == NULL_TREE)
	for (b = last; b; b = BINFO_INHERITANCE_CHAIN (b))
	  if (TREE_VIA_VIRTUAL (b) || b == rtti_binfo)
	    break;

      /* If we found RTTI_BINFO, this is case 1.  If we found a virtual
	 base B and it is a base of RTTI_BINFO, this is case 2.  In
	 either case, we share our vtable with LAST, i.e. the
	 derived-most base within B of which we are a primary.  */
      if (b == rtti_binfo
	  || (b && binfo_for_vbase (BINFO_TYPE (b),
				    BINFO_TYPE (rtti_binfo))))
	/* Just set our BINFO_VTABLE to point to LAST, as we may not have
	   set LAST's BINFO_VTABLE yet.  We'll extract the actual vptr in
	   binfo_ctor_vtable after everything's been set up.  */
	vtbl = last;

      /* Otherwise, this is case 3 and we get our own.  */
    }
  else if (!BINFO_NEW_VTABLE_MARKED (orig_binfo, BINFO_TYPE (rtti_binfo)))
    return inits;

  if (!vtbl)
    {
      tree index;
      int non_fn_entries;

      /* Compute the initializer for this vtable.  */
      inits = build_vtbl_initializer (binfo, orig_binfo, t, rtti_binfo,
				      &non_fn_entries);

      /* Figure out the position to which the VPTR should point.  */
      vtbl = TREE_PURPOSE (l);
      vtbl = build1 (ADDR_EXPR, 
		     vtbl_ptr_type_node,
		     vtbl);
      index = size_binop (PLUS_EXPR,
			  size_int (non_fn_entries),
			  size_int (list_length (TREE_VALUE (l))));
      index = size_binop (MULT_EXPR,
			  TYPE_SIZE_UNIT (vtable_entry_type),
			  index);
      vtbl = build (PLUS_EXPR, TREE_TYPE (vtbl), vtbl, index);
      TREE_CONSTANT (vtbl) = 1;
    }

  if (ctor_vtbl_p)
    /* For a construction vtable, we can't overwrite BINFO_VTABLE.
       So, we make a TREE_LIST.  Later, dfs_fixup_binfo_vtbls will
       straighten this out.  */
    BINFO_VTABLE (binfo) = tree_cons (rtti_binfo, vtbl, BINFO_VTABLE (binfo));
  else if (BINFO_PRIMARY_P (binfo) && TREE_VIA_VIRTUAL (binfo))
    inits = NULL_TREE;
  else
     /* For an ordinary vtable, set BINFO_VTABLE.  */
    BINFO_VTABLE (binfo) = vtbl;

  return inits;
}

/* Construct the initializer for BINFOs virtual function table.  BINFO
   is part of the hierarchy dominated by T.  If we're building a
   construction vtable, the ORIG_BINFO is the binfo we should use to
   find the actual function pointers to put in the vtable - but they
   can be overridden on the path to most-derived in the graph that
   ORIG_BINFO belongs.  Otherwise,
   ORIG_BINFO should be the same as BINFO.  The RTTI_BINFO is the
   BINFO that should be indicated by the RTTI information in the
   vtable; it will be a base class of T, rather than T itself, if we
   are building a construction vtable.

   The value returned is a TREE_LIST suitable for wrapping in a
   CONSTRUCTOR to use as the DECL_INITIAL for a vtable.  If
   NON_FN_ENTRIES_P is not NULL, *NON_FN_ENTRIES_P is set to the
   number of non-function entries in the vtable.  

   It might seem that this function should never be called with a
   BINFO for which BINFO_PRIMARY_P holds, the vtable for such a
   base is always subsumed by a derived class vtable.  However, when
   we are building construction vtables, we do build vtables for
   primary bases; we need these while the primary base is being
   constructed.  */

static tree
build_vtbl_initializer (binfo, orig_binfo, t, rtti_binfo, non_fn_entries_p)
     tree binfo;
     tree orig_binfo;
     tree t;
     tree rtti_binfo;
     int *non_fn_entries_p;
{
  tree v, b;
  tree vfun_inits;
  tree vbase;
  vtbl_init_data vid;

  /* Initialize VID.  */
  memset (&vid, 0, sizeof (vid));
  vid.binfo = binfo;
  vid.derived = t;
  vid.rtti_binfo = rtti_binfo;
  vid.last_init = &vid.inits;
  vid.primary_vtbl_p = (binfo == TYPE_BINFO (t));
  vid.ctor_vtbl_p = !same_type_p (BINFO_TYPE (rtti_binfo), t);
  /* The first vbase or vcall offset is at index -3 in the vtable.  */
  vid.index = ssize_int (-3);

  /* Add entries to the vtable for RTTI.  */
  build_rtti_vtbl_entries (binfo, &vid);

  /* Create an array for keeping track of the functions we've
     processed.  When we see multiple functions with the same
     signature, we share the vcall offsets.  */
  VARRAY_TREE_INIT (vid.fns, 32, "fns");
  /* Add the vcall and vbase offset entries.  */
  build_vcall_and_vbase_vtbl_entries (binfo, &vid);
  /* Clean up.  */
  VARRAY_FREE (vid.fns);
  /* Clear BINFO_VTABLE_PATH_MARKED; it's set by
     build_vbase_offset_vtbl_entries.  */
  for (vbase = CLASSTYPE_VBASECLASSES (t); 
       vbase; 
       vbase = TREE_CHAIN (vbase))
    CLEAR_BINFO_VTABLE_PATH_MARKED (TREE_VALUE (vbase));

  if (non_fn_entries_p)
    *non_fn_entries_p = list_length (vid.inits);

  /* Go through all the ordinary virtual functions, building up
     initializers.  */
  vfun_inits = NULL_TREE;
  for (v = BINFO_VIRTUALS (orig_binfo); v; v = TREE_CHAIN (v))
    {
      tree delta;
      tree vcall_index;
      tree fn;
      tree pfn;
      tree init;
      
      /* Pull the offset for `this', and the function to call, out of
	 the list.  */
      delta = BV_DELTA (v);

      if (BV_USE_VCALL_INDEX_P (v))
	{
	  vcall_index = BV_VCALL_INDEX (v);
	  my_friendly_assert (vcall_index != NULL_TREE, 20000621);
	}
      else
        vcall_index = NULL_TREE;

      fn = BV_FN (v);
      my_friendly_assert (TREE_CODE (delta) == INTEGER_CST, 19990727);
      my_friendly_assert (TREE_CODE (fn) == FUNCTION_DECL, 19990727);

      /* You can't call an abstract virtual function; it's abstract.
	 So, we replace these functions with __pure_virtual.  */
      if (DECL_PURE_VIRTUAL_P (fn))
	fn = abort_fndecl;

      /* Take the address of the function, considering it to be of an
	 appropriate generic type.  */
      pfn = build1 (ADDR_EXPR, vfunc_ptr_type_node, fn);
      /* The address of a function can't change.  */
      TREE_CONSTANT (pfn) = 1;

      /* Enter it in the vtable.  */
      init = build_vtable_entry (delta, vcall_index, pfn);

      /* If the only definition of this function signature along our
	 primary base chain is from a lost primary, this vtable slot will
	 never be used, so just zero it out.  This is important to avoid
	 requiring extra thunks which cannot be generated with the function.

	 We could also handle this in update_vtable_entry_for_fn; doing it
	 here means we zero out unused slots in ctor vtables as well,
	 rather than filling them with erroneous values (though harmless,
	 apart from relocation costs).  */
      if (fn != abort_fndecl)
	for (b = binfo; ; b = get_primary_binfo (b))
	  {
	    /* We found a defn before a lost primary; go ahead as normal.  */
	    if (look_for_overrides_here (BINFO_TYPE (b), fn))
	      break;

	    /* The nearest definition is from a lost primary; clear the
	       slot.  */
	    if (BINFO_LOST_PRIMARY_P (b))
	      {
		init = size_zero_node;
		break;
	      }
	  }

      /* And add it to the chain of initializers.  */
      vfun_inits = tree_cons (NULL_TREE, init, vfun_inits);
    }

  /* The initializers for virtual functions were built up in reverse
     order; straighten them out now.  */
  vfun_inits = nreverse (vfun_inits);
  
  /* The negative offset initializers are also in reverse order.  */
  vid.inits = nreverse (vid.inits);

  /* Chain the two together.  */
  return chainon (vid.inits, vfun_inits);
}

/* Adds to vid->inits the initializers for the vbase and vcall
   offsets in BINFO, which is in the hierarchy dominated by T.  */

static void
build_vcall_and_vbase_vtbl_entries (binfo, vid)
     tree binfo;
     vtbl_init_data *vid;
{
  tree b;

  /* If this is a derived class, we must first create entries
     corresponding to the primary base class.  */
  b = get_primary_binfo (binfo);
  if (b)
    build_vcall_and_vbase_vtbl_entries (b, vid);

  /* Add the vbase entries for this base.  */
  build_vbase_offset_vtbl_entries (binfo, vid);
  /* Add the vcall entries for this base.  */
  build_vcall_offset_vtbl_entries (binfo, vid);
}

/* Returns the initializers for the vbase offset entries in the vtable
   for BINFO (which is part of the class hierarchy dominated by T), in
   reverse order.  VBASE_OFFSET_INDEX gives the vtable index
   where the next vbase offset will go.  */

static void
build_vbase_offset_vtbl_entries (binfo, vid)
     tree binfo;
     vtbl_init_data *vid;
{
  tree vbase;
  tree t;
  tree non_primary_binfo;

  /* Under the old ABI, pointers to virtual bases are stored in each
     object.  */
  if (!vbase_offsets_in_vtable_p ())
    return;

  /* If there are no virtual baseclasses, then there is nothing to
     do.  */
  if (!TYPE_USES_VIRTUAL_BASECLASSES (BINFO_TYPE (binfo)))
    return;

  t = vid->derived;
  
  /* We might be a primary base class.  Go up the inheritance hierarchy
     until we find the most derived class of which we are a primary base:
     it is the offset of that which we need to use.  */
  non_primary_binfo = binfo;
  while (BINFO_INHERITANCE_CHAIN (non_primary_binfo))
    {
      tree b;

      /* If we have reached a virtual base, then it must be a primary
	 base (possibly multi-level) of vid->binfo, or we wouldn't
	 have called build_vcall_and_vbase_vtbl_entries for it.  But it
	 might be a lost primary, so just skip down to vid->binfo.  */
      if (TREE_VIA_VIRTUAL (non_primary_binfo))
	{
	  non_primary_binfo = vid->binfo;
	  break;
	}

      b = BINFO_INHERITANCE_CHAIN (non_primary_binfo);
      if (get_primary_binfo (b) != non_primary_binfo)
	break;
      non_primary_binfo = b;
    }

  /* Go through the virtual bases, adding the offsets.  */
  for (vbase = TYPE_BINFO (BINFO_TYPE (binfo));
       vbase;
       vbase = TREE_CHAIN (vbase))
    {
      tree b;
      tree delta;
      
      if (!TREE_VIA_VIRTUAL (vbase))
	continue;

      /* Find the instance of this virtual base in the complete
	 object.  */
      b = binfo_for_vbase (BINFO_TYPE (vbase), t);

      /* If we've already got an offset for this virtual base, we
	 don't need another one.  */
      if (BINFO_VTABLE_PATH_MARKED (b))
	continue;
      SET_BINFO_VTABLE_PATH_MARKED (b);

      /* Figure out where we can find this vbase offset.  */
      delta = size_binop (MULT_EXPR, 
			  vid->index,
			  convert (ssizetype,
				   TYPE_SIZE_UNIT (vtable_entry_type)));
      if (vid->primary_vtbl_p)
	BINFO_VPTR_FIELD (b) = delta;

      if (binfo != TYPE_BINFO (t))
	{
	  tree orig_vbase;

	  /* Find the instance of this virtual base in the type of BINFO.  */
	  orig_vbase = binfo_for_vbase (BINFO_TYPE (vbase),
					BINFO_TYPE (binfo));

	  /* The vbase offset had better be the same.  */
	  if (!tree_int_cst_equal (delta,
				   BINFO_VPTR_FIELD (orig_vbase)))
	    my_friendly_abort (20000403);
	}

      /* The next vbase will come at a more negative offset.  */
      vid->index = size_binop (MINUS_EXPR, vid->index, ssize_int (1));

      /* The initializer is the delta from BINFO to this virtual base.
	 The vbase offsets go in reverse inheritance-graph order, and
	 we are walking in inheritance graph order so these end up in
	 the right order.  */
      delta = size_diffop (BINFO_OFFSET (b), BINFO_OFFSET (non_primary_binfo));
      
      *vid->last_init 
	= build_tree_list (NULL_TREE,
			   fold (build1 (NOP_EXPR, 
					 vtable_entry_type,
					 delta)));
      vid->last_init = &TREE_CHAIN (*vid->last_init);
    }
}

/* Adds the initializers for the vcall offset entries in the vtable
   for BINFO (which is part of the class hierarchy dominated by VID->DERIVED)
   to VID->INITS.  */

static void
build_vcall_offset_vtbl_entries (binfo, vid)
     tree binfo;
     vtbl_init_data *vid;
{
  /* Under the old ABI, the adjustments to the `this' pointer were made
     elsewhere.  */
  if (!vcall_offsets_in_vtable_p ())
    return;

  /* We only need these entries if this base is a virtual base.  */
  if (!TREE_VIA_VIRTUAL (binfo))
    return;

  /* We need a vcall offset for each of the virtual functions in this
     vtable.  For example:

       class A { virtual void f (); };
       class B1 : virtual public A { virtual void f (); };
       class B2 : virtual public A { virtual void f (); };
       class C: public B1, public B2 { virtual void f (); };

     A C object has a primary base of B1, which has a primary base of A.  A
     C also has a secondary base of B2, which no longer has a primary base
     of A.  So the B2-in-C construction vtable needs a secondary vtable for
     A, which will adjust the A* to a B2* to call f.  We have no way of
     knowing what (or even whether) this offset will be when we define B2,
     so we store this "vcall offset" in the A sub-vtable and look it up in
     a "virtual thunk" for B2::f.

     We need entries for all the functions in our primary vtable and
     in our non-virtual bases' secondary vtables.  */
  vid->vbase = binfo;
  /* Now, walk through the non-virtual bases, adding vcall offsets.  */
  add_vcall_offset_vtbl_entries_r (binfo, vid);
}

/* Build vcall offsets, starting with those for BINFO.  */

static void
add_vcall_offset_vtbl_entries_r (binfo, vid)
     tree binfo;
     vtbl_init_data *vid;
{
  int i;
  tree primary_binfo;

  /* Don't walk into virtual bases -- except, of course, for the
     virtual base for which we are building vcall offsets.  Any
     primary virtual base will have already had its offsets generated
     through the recursion in build_vcall_and_vbase_vtbl_entries.  */
  if (TREE_VIA_VIRTUAL (binfo) && vid->vbase != binfo)
    return;
  
  /* If BINFO has a primary base, process it first.  */
  primary_binfo = get_primary_binfo (binfo);
  if (primary_binfo)
    add_vcall_offset_vtbl_entries_r (primary_binfo, vid);

  /* Add BINFO itself to the list.  */
  add_vcall_offset_vtbl_entries_1 (binfo, vid);

  /* Scan the non-primary bases of BINFO.  */
  for (i = 0; i < BINFO_N_BASETYPES (binfo); ++i) 
    {
      tree base_binfo;
      
      base_binfo = BINFO_BASETYPE (binfo, i);
      if (base_binfo != primary_binfo)
	add_vcall_offset_vtbl_entries_r (base_binfo, vid);
    }
}

/* Called from build_vcall_offset_vtbl_entries_r.  */

static void
add_vcall_offset_vtbl_entries_1 (binfo, vid)
     tree binfo;
     vtbl_init_data* vid;
{
  tree derived_virtuals;
  tree base_virtuals;
  tree orig_virtuals;
  tree binfo_inits;
  /* If BINFO is a primary base, the most derived class which has BINFO as
     a primary base; otherwise, just BINFO.  */
  tree non_primary_binfo;

  binfo_inits = NULL_TREE;

  /* We might be a primary base class.  Go up the inheritance hierarchy
     until we find the most derived class of which we are a primary base:
     it is the BINFO_VIRTUALS there that we need to consider.  */
  non_primary_binfo = binfo;
  while (BINFO_INHERITANCE_CHAIN (non_primary_binfo))
    {
      tree b;

      /* If we have reached a virtual base, then it must be vid->vbase,
	 because we ignore other virtual bases in
	 add_vcall_offset_vtbl_entries_r.  In turn, it must be a primary
	 base (possibly multi-level) of vid->binfo, or we wouldn't
	 have called build_vcall_and_vbase_vtbl_entries for it.  But it
	 might be a lost primary, so just skip down to vid->binfo.  */
      if (TREE_VIA_VIRTUAL (non_primary_binfo))
	{
	  if (non_primary_binfo != vid->vbase)
	    abort ();
	  non_primary_binfo = vid->binfo;
	  break;
	}

      b = BINFO_INHERITANCE_CHAIN (non_primary_binfo);
      if (get_primary_binfo (b) != non_primary_binfo)
	break;
      non_primary_binfo = b;
    }

  if (vid->ctor_vtbl_p)
    /* For a ctor vtable we need the equivalent binfo within the hierarchy
       where rtti_binfo is the most derived type.  */
    non_primary_binfo = get_original_base
          (non_primary_binfo, TYPE_BINFO (BINFO_TYPE (vid->rtti_binfo)));

  /* Make entries for the rest of the virtuals.  */
  for (base_virtuals = BINFO_VIRTUALS (binfo),
	 derived_virtuals = BINFO_VIRTUALS (non_primary_binfo),
	 orig_virtuals = BINFO_VIRTUALS (TYPE_BINFO (BINFO_TYPE (binfo)));
       base_virtuals;
       base_virtuals = TREE_CHAIN (base_virtuals),
	 derived_virtuals = TREE_CHAIN (derived_virtuals),
	 orig_virtuals = TREE_CHAIN (orig_virtuals))
    {
      tree orig_fn;
      tree fn;
      tree base;
      tree base_binfo;
      size_t i;
      tree vcall_offset;

      /* Find the declaration that originally caused this function to
	 be present in BINFO_TYPE (binfo).  */
      orig_fn = BV_FN (orig_virtuals);

      /* When processing BINFO, we only want to generate vcall slots for
	 function slots introduced in BINFO.  So don't try to generate
	 one if the function isn't even defined in BINFO.  */
      if (!same_type_p (DECL_CONTEXT (orig_fn), BINFO_TYPE (binfo)))
	continue;

      /* Find the overriding function.  */
      fn = BV_FN (derived_virtuals);

      /* If there is already an entry for a function with the same
	 signature as FN, then we do not need a second vcall offset.
	 Check the list of functions already present in the derived
	 class vtable.  */
      for (i = 0; i < VARRAY_ACTIVE_SIZE (vid->fns); ++i) 
	{
	  tree derived_entry;

	  derived_entry = VARRAY_TREE (vid->fns, i);
	  if (same_signature_p (BV_FN (derived_entry), fn)
	      /* We only use one vcall offset for virtual destructors,
		 even though there are two virtual table entries.  */
	      || (DECL_DESTRUCTOR_P (BV_FN (derived_entry))
		  && DECL_DESTRUCTOR_P (fn)))
	    {
	      if (!vid->ctor_vtbl_p)
  	        BV_VCALL_INDEX (derived_virtuals) 
		  = BV_VCALL_INDEX (derived_entry);
	      break;
	    }
	}
      if (i != VARRAY_ACTIVE_SIZE (vid->fns))
	continue;

      /* The FN comes from BASE.  So, we must calculate the adjustment from
	 vid->vbase to BASE.  We can just look for BASE in the complete
	 object because we are converting from a virtual base, so if there
	 were multiple copies, there would not be a unique final overrider
	 and vid->derived would be ill-formed.  */
      base = DECL_CONTEXT (fn);
      base_binfo = get_binfo (base, vid->derived, /*protect=*/0);

      /* Compute the vcall offset.  */
      /* As mentioned above, the vbase we're working on is a primary base of
	 vid->binfo.  But it might be a lost primary, so its BINFO_OFFSET
         might be wrong, so we just use the BINFO_OFFSET from vid->binfo.  */
      vcall_offset = BINFO_OFFSET (vid->binfo);
      vcall_offset = size_diffop (BINFO_OFFSET (base_binfo),
		                  vcall_offset);
      vcall_offset = fold (build1 (NOP_EXPR, vtable_entry_type,
    			           vcall_offset));
      
      *vid->last_init = build_tree_list (NULL_TREE, vcall_offset);
      vid->last_init = &TREE_CHAIN (*vid->last_init);

      /* Keep track of the vtable index where this vcall offset can be
	 found.  For a construction vtable, we already made this
	 annotation when we built the original vtable.  */
      if (!vid->ctor_vtbl_p)
	BV_VCALL_INDEX (derived_virtuals) = vid->index;

      /* The next vcall offset will be found at a more negative
	 offset.  */
      vid->index = size_binop (MINUS_EXPR, vid->index, ssize_int (1));

      /* Keep track of this function.  */
      VARRAY_PUSH_TREE (vid->fns, derived_virtuals);
    }
}

/* Return vtbl initializers for the RTTI entries coresponding to the
   BINFO's vtable.  The RTTI entries should indicate the object given
   by VID->rtti_binfo.  */

static void
build_rtti_vtbl_entries (binfo, vid)
     tree binfo;
     vtbl_init_data *vid;
{
  tree b;
  tree t;
  tree basetype;
  tree offset;
  tree decl;
  tree init;

  basetype = BINFO_TYPE (binfo);
  t = BINFO_TYPE (vid->rtti_binfo);

  /* For a COM object there is no RTTI entry.  */
  if (CLASSTYPE_COM_INTERFACE (basetype))
    return;

  /* To find the complete object, we will first convert to our most
     primary base, and then add the offset in the vtbl to that value.  */
  b = binfo;
  while (CLASSTYPE_HAS_PRIMARY_BASE_P (BINFO_TYPE (b))
         && !BINFO_LOST_PRIMARY_P (b))
    {
      tree primary_base;

      primary_base = get_primary_binfo (b);
      my_friendly_assert (BINFO_PRIMARY_BASE_OF (primary_base) == b, 20010127);
      b = primary_base;
    }
  offset = size_diffop (BINFO_OFFSET (vid->rtti_binfo), BINFO_OFFSET (b));

  /* The second entry is the address of the typeinfo object.  */
  if (flag_rtti)
    decl = build_unary_op (ADDR_EXPR, get_tinfo_decl (t), 0);
  else
    decl = integer_zero_node;
  
  /* Convert the declaration to a type that can be stored in the
     vtable.  */
  init = build1 (NOP_EXPR, vfunc_ptr_type_node, decl);
  TREE_CONSTANT (init) = 1;
  *vid->last_init = build_tree_list (NULL_TREE, init);
  vid->last_init = &TREE_CHAIN (*vid->last_init);

  /* Add the offset-to-top entry.  It comes earlier in the vtable that
     the the typeinfo entry.  */
  if (flag_vtable_thunks)
    {
      /* Convert the offset to look like a function pointer, so that
	 we can put it in the vtable.  */
      init = build1 (NOP_EXPR, vfunc_ptr_type_node, offset);
      TREE_CONSTANT (init) = 1;
      *vid->last_init = build_tree_list (NULL_TREE, init);
      vid->last_init = &TREE_CHAIN (*vid->last_init);
    }
}

/* Build an entry in the virtual function table.  DELTA is the offset
   for the `this' pointer.  VCALL_INDEX is the vtable index containing
   the vcall offset; NULL_TREE if none.  ENTRY is the virtual function
   table entry itself.  It's TREE_TYPE must be VFUNC_PTR_TYPE_NODE,
   but it may not actually be a virtual function table pointer.  (For
   example, it might be the address of the RTTI object, under the new
   ABI.)  */

static tree
build_vtable_entry (delta, vcall_index, entry)
     tree delta;
     tree vcall_index;
     tree entry;
{
  if (flag_vtable_thunks)
    {
      tree fn;

      fn = TREE_OPERAND (entry, 0);
      if ((!integer_zerop (delta) || vcall_index != NULL_TREE)
	  && fn != abort_fndecl
	  && !DECL_TINFO_FN_P (fn))
	{
	  entry = make_thunk (entry, delta, vcall_index);
	  entry = build1 (ADDR_EXPR, vtable_entry_type, entry);
	  TREE_READONLY (entry) = 1;
	  TREE_CONSTANT (entry) = 1;
	}
#ifdef GATHER_STATISTICS
      n_vtable_entries += 1;
#endif
      return entry;
    }
  else
    {
      tree elems = tree_cons (NULL_TREE, delta,
			      tree_cons (NULL_TREE, integer_zero_node,
					 build_tree_list (NULL_TREE, entry)));
      tree entry = build (CONSTRUCTOR, vtable_entry_type, NULL_TREE, elems);

      /* We don't use vcall offsets when not using vtable thunks.  */
      my_friendly_assert (vcall_index == NULL_TREE, 20000125);

      /* DELTA used to be constructed by `size_int' and/or size_binop,
	 which caused overflow problems when it was negative.  That should
	 be fixed now.  */

      if (! int_fits_type_p (delta, delta_type_node))
	{
	  if (flag_huge_objects)
	    sorry ("object size exceeds built-in limit for virtual function table implementation");
	  else
	    sorry ("object size exceeds normal limit for virtual function table implementation, recompile all source and use -fhuge-objects");
	}
      
      TREE_CONSTANT (entry) = 1;
      TREE_STATIC (entry) = 1;
      TREE_READONLY (entry) = 1;

#ifdef GATHER_STATISTICS
      n_vtable_entries += 1;
#endif

      return entry;
    }
}
