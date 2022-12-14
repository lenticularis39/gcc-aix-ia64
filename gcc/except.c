/* Implements exception handling.
   Copyright (C) 1989, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 
   1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Mike Stump <mrs@cygnus.com>.

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


/* An exception is an event that can be signaled from within a
   function. This event can then be "caught" or "trapped" by the
   callers of this function. This potentially allows program flow to
   be transferred to any arbitrary code associated with a function call
   several levels up the stack.

   The intended use for this mechanism is for signaling "exceptional
   events" in an out-of-band fashion, hence its name. The C++ language
   (and many other OO-styled or functional languages) practically
   requires such a mechanism, as otherwise it becomes very difficult
   or even impossible to signal failure conditions in complex
   situations.  The traditional C++ example is when an error occurs in
   the process of constructing an object; without such a mechanism, it
   is impossible to signal that the error occurs without adding global
   state variables and error checks around every object construction.

   The act of causing this event to occur is referred to as "throwing
   an exception". (Alternate terms include "raising an exception" or
   "signaling an exception".) The term "throw" is used because control
   is returned to the callers of the function that is signaling the
   exception, and thus there is the concept of "throwing" the
   exception up the call stack.

   [ Add updated documentation on how to use this.  ]  */


#include "config.h"
#include "system.h"
#include "rtl.h"
#include "tree.h"
#include "flags.h"
#include "function.h"
#include "expr.h"
#include "insn-config.h"
#include "except.h"
#include "integrate.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "output.h"
#include "dwarf2asm.h"
#include "dwarf2out.h"
#include "dwarf2.h"
#include "toplev.h"
#include "hashtab.h"
#include "intl.h"
#include "ggc.h"
#include "tm_p.h"


/* Provide defaults for stuff that may not be defined when using
   sjlj exceptions.  */
#ifndef EH_RETURN_STACKADJ_RTX
#define EH_RETURN_STACKADJ_RTX 0
#endif
#ifndef EH_RETURN_HANDLER_RTX
#define EH_RETURN_HANDLER_RTX 0
#endif
#ifndef EH_RETURN_DATA_REGNO
#define EH_RETURN_DATA_REGNO(N) INVALID_REGNUM
#endif


/* Nonzero means enable synchronous exceptions for non-call instructions.  */
int flag_non_call_exceptions;

/* Protect cleanup actions with must-not-throw regions, with a call
   to the given failure handler.  */
tree (*lang_protect_cleanup_actions) PARAMS ((void));

/* Return true if type A catches type B.  */
int (*lang_eh_type_covers) PARAMS ((tree a, tree b));

/* Map a type to a runtime object to match type.  */
tree (*lang_eh_runtime_type) PARAMS ((tree));

/* A list of labels used for exception handlers.  */
rtx exception_handler_labels;

static int call_site_base;
static int sjlj_funcdef_number;
static htab_t type_to_runtime_map;

/* Describe the SjLj_Function_Context structure.  */
static tree sjlj_fc_type_node;
static int sjlj_fc_call_site_ofs;
static int sjlj_fc_data_ofs;
static int sjlj_fc_personality_ofs;
static int sjlj_fc_lsda_ofs;
static int sjlj_fc_jbuf_ofs;

/* Describes one exception region.  */
struct eh_region
{
  /* The immediately surrounding region.  */
  struct eh_region *outer;

  /* The list of immediately contained regions.  */
  struct eh_region *inner;
  struct eh_region *next_peer;

  /* An identifier for this region.  */
  int region_number;

  /* Each region does exactly one thing.  */
  enum eh_region_type
  {
    ERT_CLEANUP = 1,
    ERT_TRY,
    ERT_CATCH,
    ERT_ALLOWED_EXCEPTIONS,
    ERT_MUST_NOT_THROW,
    ERT_THROW,
    ERT_FIXUP
  } type;

  /* Holds the action to perform based on the preceeding type.  */
  union {
    /* A list of catch blocks, a surrounding try block,
       and the label for continuing after a catch.  */
    struct {
      struct eh_region *catch;
      struct eh_region *last_catch;
      struct eh_region *prev_try;
      rtx continue_label;
    } try;

    /* The list through the catch handlers, the type object
       matched, and a pointer to the generated code.  */
    struct {
      struct eh_region *next_catch;
      struct eh_region *prev_catch;
      tree type;
      int filter;
    } catch;

    /* A tree_list of allowed types.  */
    struct {
      tree type_list;
      int filter;
    } allowed;

    /* The type given by a call to "throw foo();", or discovered 
       for a throw.  */
    struct {
      tree type;
    } throw;

    /* Retain the cleanup expression even after expansion so that
       we can match up fixup regions.  */
    struct {
      tree exp;
    } cleanup;

    /* The real region (by expression and by pointer) that fixup code
       should live in.  */
    struct {
      tree cleanup_exp;
      struct eh_region *real_region;
    } fixup;
  } u;

  /* Entry point for this region's handler before landing pads are built.  */
  rtx label;

  /* Entry point for this region's handler from the runtime eh library.  */
  rtx landing_pad;

  /* Entry point for this region's handler from an inner region.  */
  rtx post_landing_pad;

  /* The RESX insn for handing off control to the next outermost handler,
     if appropriate.  */
  rtx resume;
};

/* Used to save exception status for each function.  */
struct eh_status
{
  /* The tree of all regions for this function.  */
  struct eh_region *region_tree;

  /* The same information as an indexable array.  */
  struct eh_region **region_array;

  /* The most recently open region.  */
  struct eh_region *cur_region;

  /* This is the region for which we are processing catch blocks.  */
  struct eh_region *try_region;

  /* A stack (TREE_LIST) of lists of handlers.  The TREE_VALUE of each
     node is itself a TREE_CHAINed list of handlers for regions that
     are not yet closed. The TREE_VALUE of each entry contains the
     handler for the corresponding entry on the ehstack.  */
  tree protect_list;

  rtx filter;
  rtx exc_ptr;

  int built_landing_pads;
  int last_region_number;

  varray_type ttype_data;
  varray_type ehspec_data;
  varray_type action_record_data;

  struct call_site_record
  {
    rtx landing_pad;
    int action;
  } *call_site_data;
  int call_site_data_used;
  int call_site_data_size;

  rtx ehr_stackadj;
  rtx ehr_handler;
  rtx ehr_label;

  rtx sjlj_fc;
  rtx sjlj_exit_after;
};


static void mark_eh_region			PARAMS ((struct eh_region *));

static int t2r_eq				PARAMS ((const PTR,
							 const PTR));
static hashval_t t2r_hash			PARAMS ((const PTR));
static int t2r_mark_1				PARAMS ((PTR *, PTR));
static void t2r_mark				PARAMS ((PTR));
static void add_type_for_runtime		PARAMS ((tree));
static tree lookup_type_for_runtime		PARAMS ((tree));

static struct eh_region *expand_eh_region_end	PARAMS ((void));

static rtx get_exception_filter			PARAMS ((struct function *));

static void collect_eh_region_array		PARAMS ((void));
static void resolve_fixup_regions		PARAMS ((void));
static void remove_fixup_regions		PARAMS ((void));
static void convert_from_eh_region_ranges_1	PARAMS ((rtx *, int *, int));

static struct eh_region *duplicate_eh_region_1	PARAMS ((struct eh_region *,
						     struct inline_remap *));
static void duplicate_eh_region_2		PARAMS ((struct eh_region *,
							 struct eh_region **));
static int ttypes_filter_eq			PARAMS ((const PTR,
							 const PTR));
static hashval_t ttypes_filter_hash		PARAMS ((const PTR));
static int ehspec_filter_eq			PARAMS ((const PTR,
							 const PTR));
static hashval_t ehspec_filter_hash		PARAMS ((const PTR));
static int add_ttypes_entry			PARAMS ((htab_t, tree));
static int add_ehspec_entry			PARAMS ((htab_t, htab_t,
							 tree));
static void assign_filter_values		PARAMS ((void));
static void build_post_landing_pads		PARAMS ((void));
static void connect_post_landing_pads		PARAMS ((void));
static void dw2_build_landing_pads		PARAMS ((void));

struct sjlj_lp_info;
static bool sjlj_find_directly_reachable_regions
     PARAMS ((struct sjlj_lp_info *));
static void sjlj_assign_call_site_values
     PARAMS ((rtx, struct sjlj_lp_info *));
static void sjlj_mark_call_sites
     PARAMS ((struct sjlj_lp_info *));
static void sjlj_emit_function_enter		PARAMS ((rtx));
static void sjlj_emit_function_exit		PARAMS ((void));
static void sjlj_emit_dispatch_table
     PARAMS ((rtx, struct sjlj_lp_info *));
static void sjlj_build_landing_pads		PARAMS ((void));

static void remove_exception_handler_label	PARAMS ((rtx));
static void remove_eh_handler			PARAMS ((struct eh_region *));

struct reachable_info;

/* The return value of reachable_next_level.  */
enum reachable_code
{
  /* The given exception is not processed by the given region.  */
  RNL_NOT_CAUGHT,
  /* The given exception may need processing by the given region.  */
  RNL_MAYBE_CAUGHT,
  /* The given exception is completely processed by the given region.  */
  RNL_CAUGHT,
  /* The given exception is completely processed by the runtime.  */
  RNL_BLOCKED
};

static int check_handled			PARAMS ((tree, tree));
static void add_reachable_handler
     PARAMS ((struct reachable_info *, struct eh_region *,
	      struct eh_region *));
static enum reachable_code reachable_next_level
     PARAMS ((struct eh_region *, tree, struct reachable_info *));

static int action_record_eq			PARAMS ((const PTR,
							 const PTR));
static hashval_t action_record_hash		PARAMS ((const PTR));
static int add_action_record			PARAMS ((htab_t, int, int));
static int collect_one_action_chain		PARAMS ((htab_t,
							 struct eh_region *));
static int add_call_site			PARAMS ((rtx, int));

static void push_uleb128			PARAMS ((varray_type *,
							 unsigned int));
static void push_sleb128			PARAMS ((varray_type *, int));
#ifndef HAVE_AS_LEB128
static int dw2_size_of_call_site_table		PARAMS ((void));
static int sjlj_size_of_call_site_table		PARAMS ((void));
#endif
static void dw2_output_call_site_table		PARAMS ((void));
static void sjlj_output_call_site_table		PARAMS ((void));


/* Routine to see if exception handling is turned on.
   DO_WARN is non-zero if we want to inform the user that exception
   handling is turned off. 

   This is used to ensure that -fexceptions has been specified if the
   compiler tries to use any exception-specific functions.  */

int
doing_eh (do_warn)
     int do_warn;
{
  if (! flag_exceptions)
    {
      static int warned = 0;
      if (! warned && do_warn)
	{
	  error ("exception handling disabled, use -fexceptions to enable");
	  warned = 1;
	}
      return 0;
    }
  return 1;
}


void
init_eh ()
{
  ggc_add_rtx_root (&exception_handler_labels, 1);

  if (! flag_exceptions)
    return;

  type_to_runtime_map = htab_create (31, t2r_hash, t2r_eq, NULL);
  ggc_add_root (&type_to_runtime_map, 1, sizeof (htab_t), t2r_mark);

  /* Create the SjLj_Function_Context structure.  This should match
     the definition in unwind-sjlj.c.  */
  if (USING_SJLJ_EXCEPTIONS)
    {
      tree f_jbuf, f_per, f_lsda, f_prev, f_cs, f_data, tmp;

      sjlj_fc_type_node = make_lang_type (RECORD_TYPE);
      ggc_add_tree_root (&sjlj_fc_type_node, 1);

      f_prev = build_decl (FIELD_DECL, get_identifier ("__prev"),
			   build_pointer_type (sjlj_fc_type_node));
      DECL_FIELD_CONTEXT (f_prev) = sjlj_fc_type_node;

      f_cs = build_decl (FIELD_DECL, get_identifier ("__call_site"),
			 integer_type_node);
      DECL_FIELD_CONTEXT (f_cs) = sjlj_fc_type_node;

      tmp = build_index_type (build_int_2 (4 - 1, 0));
      tmp = build_array_type (type_for_mode (word_mode, 1), tmp);
      f_data = build_decl (FIELD_DECL, get_identifier ("__data"), tmp);
      DECL_FIELD_CONTEXT (f_data) = sjlj_fc_type_node;

      f_per = build_decl (FIELD_DECL, get_identifier ("__personality"),
			  ptr_type_node);
      DECL_FIELD_CONTEXT (f_per) = sjlj_fc_type_node;

      f_lsda = build_decl (FIELD_DECL, get_identifier ("__lsda"),
			   ptr_type_node);
      DECL_FIELD_CONTEXT (f_lsda) = sjlj_fc_type_node;

#ifdef DONT_USE_BUILTIN_SETJMP
#ifdef JMP_BUF_SIZE
      tmp = build_int_2 (JMP_BUF_SIZE - 1, 0);
#else
      /* Should be large enough for most systems, if it is not,
	 JMP_BUF_SIZE should be defined with the proper value.  It will
	 also tend to be larger than necessary for most systems, a more
	 optimal port will define JMP_BUF_SIZE.  */
      tmp = build_int_2 (FIRST_PSEUDO_REGISTER + 2 - 1, 0);
#endif
#else
      /* This is 2 for builtin_setjmp, plus whatever the target requires
	 via STACK_SAVEAREA_MODE (SAVE_NONLOCAL).  */
      tmp = build_int_2 ((GET_MODE_SIZE (STACK_SAVEAREA_MODE (SAVE_NONLOCAL))
			  / GET_MODE_SIZE (Pmode)) + 2 - 1, 0);
#endif
      tmp = build_index_type (tmp);
      tmp = build_array_type (ptr_type_node, tmp);
      f_jbuf = build_decl (FIELD_DECL, get_identifier ("__jbuf"), tmp);
#ifdef DONT_USE_BUILTIN_SETJMP
      /* We don't know what the alignment requirements of the
	 runtime's jmp_buf has.  Overestimate.  */
      DECL_ALIGN (f_jbuf) = BIGGEST_ALIGNMENT;
      DECL_USER_ALIGN (f_jbuf) = 1;
#endif
      DECL_FIELD_CONTEXT (f_jbuf) = sjlj_fc_type_node;

      TYPE_FIELDS (sjlj_fc_type_node) = f_prev;
      TREE_CHAIN (f_prev) = f_cs;
      TREE_CHAIN (f_cs) = f_data;
      TREE_CHAIN (f_data) = f_per;
      TREE_CHAIN (f_per) = f_lsda;
      TREE_CHAIN (f_lsda) = f_jbuf;

      layout_type (sjlj_fc_type_node);

      /* Cache the interesting field offsets so that we have
	 easy access from rtl.  */
      sjlj_fc_call_site_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_cs), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_cs), 1) / BITS_PER_UNIT);
      sjlj_fc_data_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_data), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_data), 1) / BITS_PER_UNIT);
      sjlj_fc_personality_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_per), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_per), 1) / BITS_PER_UNIT);
      sjlj_fc_lsda_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_lsda), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_lsda), 1) / BITS_PER_UNIT);
      sjlj_fc_jbuf_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_jbuf), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_jbuf), 1) / BITS_PER_UNIT);
    }
}

void
init_eh_for_function ()
{
  cfun->eh = (struct eh_status *) xcalloc (1, sizeof (struct eh_status));
}

/* Mark EH for GC.  */

static void
mark_eh_region (region)
     struct eh_region *region;
{
  if (! region)
    return;

  switch (region->type)
    {
    case ERT_CLEANUP:
      ggc_mark_tree (region->u.cleanup.exp);
      break;
    case ERT_TRY:
      ggc_mark_rtx (region->u.try.continue_label);
      break;
    case ERT_CATCH:
      ggc_mark_tree (region->u.catch.type);
      break;
    case ERT_ALLOWED_EXCEPTIONS:
      ggc_mark_tree (region->u.allowed.type_list);
      break;
    case ERT_MUST_NOT_THROW:
      break;
    case ERT_THROW:
      ggc_mark_tree (region->u.throw.type);
      break;
    case ERT_FIXUP:
      ggc_mark_tree (region->u.fixup.cleanup_exp);
      break;
    default:
      abort ();
    }

  ggc_mark_rtx (region->label);
  ggc_mark_rtx (region->resume);
  ggc_mark_rtx (region->landing_pad);
  ggc_mark_rtx (region->post_landing_pad);
}

void
mark_eh_status (eh)
     struct eh_status *eh;
{
  int i;

  if (eh == 0)
    return;

  /* If we've called collect_eh_region_array, use it.  Otherwise walk
     the tree non-recursively.  */
  if (eh->region_array)
    {
      for (i = eh->last_region_number; i > 0; --i)
	{
	  struct eh_region *r = eh->region_array[i];
	  if (r && r->region_number == i)
	    mark_eh_region (r);
	}
    }
  else if (eh->region_tree)
    {
      struct eh_region *r = eh->region_tree;
      while (1)
	{
	  mark_eh_region (r);
	  if (r->inner)
	    r = r->inner;
	  else if (r->next_peer)
	    r = r->next_peer;
	  else
	    {
	      do {
		r = r->outer;
		if (r == NULL)
		  goto tree_done;
	      } while (r->next_peer == NULL);
	      r = r->next_peer;
	    }
	}
    tree_done:;
    }

  ggc_mark_tree (eh->protect_list);
  ggc_mark_rtx (eh->filter);
  ggc_mark_rtx (eh->exc_ptr);
  ggc_mark_tree_varray (eh->ttype_data);

  if (eh->call_site_data)
    {
      for (i = eh->call_site_data_used - 1; i >= 0; --i)
	ggc_mark_rtx (eh->call_site_data[i].landing_pad);
    }

  ggc_mark_rtx (eh->ehr_stackadj);
  ggc_mark_rtx (eh->ehr_handler);
  ggc_mark_rtx (eh->ehr_label);

  ggc_mark_rtx (eh->sjlj_fc);
  ggc_mark_rtx (eh->sjlj_exit_after);
}

void
free_eh_status (f)
     struct function *f;
{
  struct eh_status *eh = f->eh;

  if (eh->region_array)
    {
      int i;
      for (i = eh->last_region_number; i > 0; --i)
	{
	  struct eh_region *r = eh->region_array[i];
	  /* Mind we don't free a region struct more than once.  */
	  if (r && r->region_number == i)
	    free (r);
	}
      free (eh->region_array);
    }
  else if (eh->region_tree)
    {
      struct eh_region *next, *r = eh->region_tree;
      while (1)
	{
	  if (r->inner)
	    r = r->inner;
	  else if (r->next_peer)
	    {
	      next = r->next_peer;
	      free (r);
	      r = next;
	    }
	  else
	    {
	      do {
	        next = r->outer;
	        free (r);
	        r = next;
		if (r == NULL)
		  goto tree_done;
	      } while (r->next_peer == NULL);
	      next = r->next_peer;
	      free (r);
	      r = next;
	    }
	}
    tree_done:;
    }

  VARRAY_FREE (eh->ttype_data);
  VARRAY_FREE (eh->ehspec_data);
  VARRAY_FREE (eh->action_record_data);
  if (eh->call_site_data)
    free (eh->call_site_data);

  free (eh);
  f->eh = NULL;
}


/* Start an exception handling region.  All instructions emitted
   after this point are considered to be part of the region until
   expand_eh_region_end is invoked.  */

void
expand_eh_region_start ()
{
  struct eh_region *new_region;
  struct eh_region *cur_region;
  rtx note;

  if (! doing_eh (0))
    return;

  /* Insert a new blank region as a leaf in the tree.  */
  new_region = (struct eh_region *) xcalloc (1, sizeof (*new_region));
  cur_region = cfun->eh->cur_region;
  new_region->outer = cur_region;
  if (cur_region)
    {
      new_region->next_peer = cur_region->inner;
      cur_region->inner = new_region;
    }
  else
    {
      new_region->next_peer = cfun->eh->region_tree;
      cfun->eh->region_tree = new_region;
    }
  cfun->eh->cur_region = new_region;

  /* Create a note marking the start of this region.  */
  new_region->region_number = ++cfun->eh->last_region_number;
  note = emit_note (NULL, NOTE_INSN_EH_REGION_BEG);
  NOTE_EH_HANDLER (note) = new_region->region_number;
}

/* Common code to end a region.  Returns the region just ended.  */

static struct eh_region *
expand_eh_region_end ()
{
  struct eh_region *cur_region = cfun->eh->cur_region;
  rtx note;

  /* Create a nute marking the end of this region.  */
  note = emit_note (NULL, NOTE_INSN_EH_REGION_END);
  NOTE_EH_HANDLER (note) = cur_region->region_number;

  /* Pop.  */
  cfun->eh->cur_region = cur_region->outer;

  return cur_region;
}

/* End an exception handling region for a cleanup.  HANDLER is an
   expression to expand for the cleanup.  */

void
expand_eh_region_end_cleanup (handler)
     tree handler;
{
  struct eh_region *region;
  tree protect_cleanup_actions;
  rtx around_label;
  rtx data_save[2];

  if (! doing_eh (0))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_CLEANUP;
  region->label = gen_label_rtx ();
  region->u.cleanup.exp = handler;

  around_label = gen_label_rtx ();
  emit_jump (around_label);

  emit_label (region->label);

  /* Give the language a chance to specify an action to be taken if an
     exception is thrown that would propogate out of the HANDLER.  */
  protect_cleanup_actions 
    = (lang_protect_cleanup_actions 
       ? (*lang_protect_cleanup_actions) () 
       : NULL_TREE);

  if (protect_cleanup_actions)
    expand_eh_region_start ();

  /* In case this cleanup involves an inline destructor with a try block in
     it, we need to save the EH return data registers around it.  */
  data_save[0] = gen_reg_rtx (Pmode);
  emit_move_insn (data_save[0], get_exception_pointer (cfun));
  data_save[1] = gen_reg_rtx (word_mode);
  emit_move_insn (data_save[1], get_exception_filter (cfun));

  expand_expr (handler, const0_rtx, VOIDmode, 0);

  emit_move_insn (cfun->eh->exc_ptr, data_save[0]);
  emit_move_insn (cfun->eh->filter, data_save[1]);

  if (protect_cleanup_actions)
    expand_eh_region_end_must_not_throw (protect_cleanup_actions);

  /* We need any stack adjustment complete before the around_label.  */
  do_pending_stack_adjust ();

  /* We delay the generation of the _Unwind_Resume until we generate
     landing pads.  We emit a marker here so as to get good control
     flow data in the meantime.  */
  region->resume
    = emit_jump_insn (gen_rtx_RESX (VOIDmode, region->region_number));
  emit_barrier ();

  emit_label (around_label);
}

/* End an exception handling region for a try block, and prepares
   for subsequent calls to expand_start_catch.  */

void
expand_start_all_catch ()
{
  struct eh_region *region;

  if (! doing_eh (1))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_TRY;
  region->u.try.prev_try = cfun->eh->try_region;
  region->u.try.continue_label = gen_label_rtx ();

  cfun->eh->try_region = region;

  emit_jump (region->u.try.continue_label);
}

/* Begin a catch clause.  TYPE is the type caught, or null if this is
   a catch-all clause.  */

void
expand_start_catch (type)
     tree type;
{
  struct eh_region *t, *c, *l;

  if (! doing_eh (0))
    return;

  if (type)
    add_type_for_runtime (type);
  expand_eh_region_start ();

  t = cfun->eh->try_region;
  c = cfun->eh->cur_region;
  c->type = ERT_CATCH;
  c->u.catch.type = type;
  c->label = gen_label_rtx ();

  l = t->u.try.last_catch;
  c->u.catch.prev_catch = l;
  if (l)
    l->u.catch.next_catch = c;
  else
    t->u.try.catch = c;
  t->u.try.last_catch = c;

  emit_label (c->label);
}

/* End a catch clause.  Control will resume after the try/catch block.  */

void
expand_end_catch ()
{
  struct eh_region *try_region, *catch_region;

  if (! doing_eh (0))
    return;

  catch_region = expand_eh_region_end ();
  try_region = cfun->eh->try_region;

  emit_jump (try_region->u.try.continue_label);
}

/* End a sequence of catch handlers for a try block.  */

void
expand_end_all_catch ()
{
  struct eh_region *try_region;

  if (! doing_eh (0))
    return;

  try_region = cfun->eh->try_region;
  cfun->eh->try_region = try_region->u.try.prev_try;

  emit_label (try_region->u.try.continue_label);
}

/* End an exception region for an exception type filter.  ALLOWED is a
   TREE_LIST of types to be matched by the runtime.  FAILURE is an
   expression to invoke if a mismatch ocurrs.  */

void
expand_eh_region_end_allowed (allowed, failure)
     tree allowed, failure;
{
  struct eh_region *region;
  rtx around_label;

  if (! doing_eh (0))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_ALLOWED_EXCEPTIONS;
  region->u.allowed.type_list = allowed;
  region->label = gen_label_rtx ();

  for (; allowed ; allowed = TREE_CHAIN (allowed))
    add_type_for_runtime (TREE_VALUE (allowed));

  /* We must emit the call to FAILURE here, so that if this function
     throws a different exception, that it will be processed by the
     correct region.  */

  /* If there are any pending stack adjustments, we must emit them
     before we branch -- otherwise, we won't know how much adjustment
     is required later.  */
  do_pending_stack_adjust ();
  around_label = gen_label_rtx ();
  emit_jump (around_label);

  emit_label (region->label);
  expand_expr (failure, const0_rtx, VOIDmode, EXPAND_NORMAL);
  /* We must adjust the stack before we reach the AROUND_LABEL because
     the call to FAILURE does not occur on all paths to the
     AROUND_LABEL.  */
  do_pending_stack_adjust ();

  emit_label (around_label);
}

/* End an exception region for a must-not-throw filter.  FAILURE is an
   expression invoke if an uncaught exception propagates this far.

   This is conceptually identical to expand_eh_region_end_allowed with
   an empty allowed list (if you passed "std::terminate" instead of
   "__cxa_call_unexpected"), but they are represented differently in
   the C++ LSDA.  */

void
expand_eh_region_end_must_not_throw (failure)
     tree failure;
{
  struct eh_region *region;
  rtx around_label;

  if (! doing_eh (0))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_MUST_NOT_THROW;
  region->label = gen_label_rtx ();

  /* We must emit the call to FAILURE here, so that if this function
     throws a different exception, that it will be processed by the
     correct region.  */

  around_label = gen_label_rtx ();
  emit_jump (around_label);

  emit_label (region->label);
  expand_expr (failure, const0_rtx, VOIDmode, EXPAND_NORMAL);

  emit_label (around_label);
}

/* End an exception region for a throw.  No handling goes on here,
   but it's the easiest way for the front-end to indicate what type
   is being thrown.  */

void
expand_eh_region_end_throw (type)
     tree type;
{
  struct eh_region *region;

  if (! doing_eh (0))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_THROW;
  region->u.throw.type = type;
}

/* End a fixup region.  Within this region the cleanups for the immediately
   enclosing region are _not_ run.  This is used for goto cleanup to avoid
   destroying an object twice.

   This would be an extraordinarily simple prospect, were it not for the
   fact that we don't actually know what the immediately enclosing region
   is.  This surprising fact is because expand_cleanups is currently
   generating a sequence that it will insert somewhere else.  We collect
   the proper notion of "enclosing" in convert_from_eh_region_ranges.  */

void
expand_eh_region_end_fixup (handler)
     tree handler;
{
  struct eh_region *fixup;

  if (! doing_eh (0))
    return;

  fixup = expand_eh_region_end ();
  fixup->type = ERT_FIXUP;
  fixup->u.fixup.cleanup_exp = handler;
}

/* Return an rtl expression for a pointer to the exception object
   within a handler.  */

rtx
get_exception_pointer (fun)
     struct function *fun;
{
  rtx exc_ptr = fun->eh->exc_ptr;
  if (fun == cfun && ! exc_ptr)
    {
      exc_ptr = gen_reg_rtx (Pmode);
      fun->eh->exc_ptr = exc_ptr;
    }
  return exc_ptr;
}

/* Return an rtl expression for the exception dispatch filter
   within a handler.  */

static rtx
get_exception_filter (fun)
     struct function *fun;
{
  rtx filter = fun->eh->filter;
  if (fun == cfun && ! filter)
    {
      filter = gen_reg_rtx (word_mode);
      fun->eh->filter = filter;
    }
  return filter;
}

/* Begin a region that will contain entries created with
   add_partial_entry.  */

void
begin_protect_partials ()
{
  /* Push room for a new list.  */
  cfun->eh->protect_list
    = tree_cons (NULL_TREE, NULL_TREE, cfun->eh->protect_list);
}

/* Start a new exception region for a region of code that has a
   cleanup action and push the HANDLER for the region onto
   protect_list. All of the regions created with add_partial_entry
   will be ended when end_protect_partials is invoked.  */

void
add_partial_entry (handler)
     tree handler;
{
  expand_eh_region_start ();

  /* ??? This comment was old before the most recent rewrite.  We
     really ought to fix the callers at some point.  */
  /* For backwards compatibility, we allow callers to omit calls to
     begin_protect_partials for the outermost region.  So, we must
     explicitly do so here.  */
  if (!cfun->eh->protect_list)
    begin_protect_partials ();

  /* Add this entry to the front of the list.  */
  TREE_VALUE (cfun->eh->protect_list) 
    = tree_cons (NULL_TREE, handler, TREE_VALUE (cfun->eh->protect_list));
}

/* End all the pending exception regions on protect_list.  */

void
end_protect_partials ()
{
  tree t;

  /* ??? This comment was old before the most recent rewrite.  We
     really ought to fix the callers at some point.  */
  /* For backwards compatibility, we allow callers to omit the call to
     begin_protect_partials for the outermost region.  So,
     PROTECT_LIST may be NULL.  */
  if (!cfun->eh->protect_list)
    return;

  /* Pop the topmost entry.  */
  t = TREE_VALUE (cfun->eh->protect_list);
  cfun->eh->protect_list = TREE_CHAIN (cfun->eh->protect_list);

  /* End all the exception regions.  */
  for (; t; t = TREE_CHAIN (t))
    expand_eh_region_end_cleanup (TREE_VALUE (t));
}


/* This section is for the exception handling specific optimization pass.  */

/* Random access the exception region tree.  It's just as simple to
   collect the regions this way as in expand_eh_region_start, but
   without having to realloc memory.  */

static void
collect_eh_region_array ()
{
  struct eh_region **array, *i;

  i = cfun->eh->region_tree;
  if (! i)
    return;

  array = xcalloc (cfun->eh->last_region_number + 1, sizeof (*array));
  cfun->eh->region_array = array;

  while (1)
    {
      array[i->region_number] = i;

      /* If there are sub-regions, process them.  */
      if (i->inner)
	i = i->inner;
      /* If there are peers, process them.  */
      else if (i->next_peer)
	i = i->next_peer;
      /* Otherwise, step back up the tree to the next peer.  */
      else
	{
	  do {
	    i = i->outer;
	    if (i == NULL)
	      return;
	  } while (i->next_peer == NULL);
	  i = i->next_peer;
	}
    }
}

static void
resolve_fixup_regions ()
{
  int i, j, n = cfun->eh->last_region_number;

  for (i = 1; i <= n; ++i)
    {
      struct eh_region *fixup = cfun->eh->region_array[i];
      struct eh_region *cleanup;

      if (! fixup || fixup->type != ERT_FIXUP)
	continue;

      for (j = 1; j <= n; ++j)
	{
	  cleanup = cfun->eh->region_array[j];
	  if (cleanup->type == ERT_CLEANUP
	      && cleanup->u.cleanup.exp == fixup->u.fixup.cleanup_exp)
	    break;
	}
      if (j > n)
	abort ();

      fixup->u.fixup.real_region = cleanup->outer;
    }
}

/* Now that we've discovered what region actually encloses a fixup,
   we can shuffle pointers and remove them from the tree.  */

static void
remove_fixup_regions ()
{
  int i;
  rtx insn, note;
  struct eh_region *fixup;

  /* Walk the insn chain and adjust the REG_EH_REGION numbers
     for instructions referencing fixup regions.  This is only
     strictly necessary for fixup regions with no parent, but
     doesn't hurt to do it for all regions.  */
  for (insn = get_insns(); insn ; insn = NEXT_INSN (insn))
    if (INSN_P (insn)
	&& (note = find_reg_note (insn, REG_EH_REGION, NULL))
	&& INTVAL (XEXP (note, 0)) > 0
	&& (fixup = cfun->eh->region_array[INTVAL (XEXP (note, 0))])
	&& fixup->type == ERT_FIXUP)
      {
	if (fixup->u.fixup.real_region)
	  XEXP (note, 0) = GEN_INT (fixup->u.fixup.real_region->region_number);
	else
	  remove_note (insn, note);
      }

  /* Remove the fixup regions from the tree.  */
  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      fixup = cfun->eh->region_array[i];
      if (! fixup)
	continue;

      /* Allow GC to maybe free some memory.  */
      if (fixup->type == ERT_CLEANUP)
        fixup->u.cleanup.exp = NULL_TREE;

      if (fixup->type != ERT_FIXUP)
	continue;

      if (fixup->inner)
	{
	  struct eh_region *parent, *p, **pp;

	  parent = fixup->u.fixup.real_region;

	  /* Fix up the children's parent pointers; find the end of
	     the list.  */
	  for (p = fixup->inner; ; p = p->next_peer)
	    {
	      p->outer = parent;
	      if (! p->next_peer)
		break;
	    }

	  /* In the tree of cleanups, only outer-inner ordering matters.
	     So link the children back in anywhere at the correct level.  */
	  if (parent)
	    pp = &parent->inner;
	  else
	    pp = &cfun->eh->region_tree;
	  p->next_peer = *pp;
	  *pp = fixup->inner;
	  fixup->inner = NULL;
	}

      remove_eh_handler (fixup);
    }
}

/* Turn NOTE_INSN_EH_REGION notes into REG_EH_REGION notes for each
   can_throw instruction in the region.  */

static void
convert_from_eh_region_ranges_1 (pinsns, orig_sp, cur)
     rtx *pinsns;
     int *orig_sp;
     int cur;
{
  int *sp = orig_sp;
  rtx insn, next;

  for (insn = *pinsns; insn ; insn = next)
    {
      next = NEXT_INSN (insn);
      if (GET_CODE (insn) == NOTE)
	{
	  int kind = NOTE_LINE_NUMBER (insn);
	  if (kind == NOTE_INSN_EH_REGION_BEG
	      || kind == NOTE_INSN_EH_REGION_END)
	    {
	      if (kind == NOTE_INSN_EH_REGION_BEG)
		{
		  struct eh_region *r;

		  *sp++ = cur;
		  cur = NOTE_EH_HANDLER (insn);

		  r = cfun->eh->region_array[cur];
		  if (r->type == ERT_FIXUP)
		    {
		      r = r->u.fixup.real_region;
		      cur = r ? r->region_number : 0;
		    }
		  else if (r->type == ERT_CATCH)
		    {
		      r = r->outer;
		      cur = r ? r->region_number : 0;
		    }
		}
	      else
		cur = *--sp;

	      /* Removing the first insn of a CALL_PLACEHOLDER sequence
		 requires extra care to adjust sequence start.  */
	      if (insn == *pinsns)
		*pinsns = next;
	      remove_insn (insn);
	      continue;
	    }
	}
      else if (INSN_P (insn))
	{
	  if (cur > 0
	      && ! find_reg_note (insn, REG_EH_REGION, NULL_RTX)
	      /* Calls can always potentially throw exceptions, unless
		 they have a REG_EH_REGION note with a value of 0 or less.
		 Which should be the only possible kind so far.  */
	      && (GET_CODE (insn) == CALL_INSN
		  /* If we wanted exceptions for non-call insns, then
		     any may_trap_p instruction could throw.  */
		  || (flag_non_call_exceptions
		      && may_trap_p (PATTERN (insn)))))
	    {
	      REG_NOTES (insn) = alloc_EXPR_LIST (REG_EH_REGION, GEN_INT (cur),
						  REG_NOTES (insn));
	    }

	  if (GET_CODE (insn) == CALL_INSN
	      && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
	    {
	      convert_from_eh_region_ranges_1 (&XEXP (PATTERN (insn), 0),
					       sp, cur);
	      convert_from_eh_region_ranges_1 (&XEXP (PATTERN (insn), 1),
					       sp, cur);
	      convert_from_eh_region_ranges_1 (&XEXP (PATTERN (insn), 2),
					       sp, cur);
	    }
	}
    }

  if (sp != orig_sp)
    abort ();
}

void
convert_from_eh_region_ranges ()
{
  int *stack;
  rtx insns;

  collect_eh_region_array ();
  resolve_fixup_regions ();

  stack = xmalloc (sizeof (int) * (cfun->eh->last_region_number + 1));
  insns = get_insns ();
  convert_from_eh_region_ranges_1 (&insns, stack, 0);
  free (stack);

  remove_fixup_regions ();
}

void
find_exception_handler_labels ()
{
  rtx list = NULL_RTX;
  int i;

  free_EXPR_LIST_list (&exception_handler_labels);

  if (cfun->eh->region_tree == NULL)
    return;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];
      rtx lab;

      if (! region)
	continue;
      if (cfun->eh->built_landing_pads)
	lab = region->landing_pad;
      else
	lab = region->label;

      if (lab)
	list = alloc_EXPR_LIST (0, lab, list);
    }

  /* For sjlj exceptions, need the return label to remain live until
     after landing pad generation.  */
  if (USING_SJLJ_EXCEPTIONS && ! cfun->eh->built_landing_pads)
    list = alloc_EXPR_LIST (0, return_label, list);

  exception_handler_labels = list;
}


static struct eh_region *
duplicate_eh_region_1 (o, map)
     struct eh_region *o;
     struct inline_remap *map;
{
  struct eh_region *n
    = (struct eh_region *) xcalloc (1, sizeof (struct eh_region));

  n->region_number = o->region_number + cfun->eh->last_region_number;
  n->type = o->type;

  switch (n->type)
    {
    case ERT_CLEANUP:
    case ERT_MUST_NOT_THROW:
      break;

    case ERT_TRY:
      if (o->u.try.continue_label)
	n->u.try.continue_label
	  = get_label_from_map (map,
				CODE_LABEL_NUMBER (o->u.try.continue_label));
      break;

    case ERT_CATCH:
      n->u.catch.type = o->u.catch.type;
      break;

    case ERT_ALLOWED_EXCEPTIONS:
      n->u.allowed.type_list = o->u.allowed.type_list;
      break;

    case ERT_THROW:
      n->u.throw.type = o->u.throw.type;
      
    default:
      abort ();
    }

  if (o->label)
    n->label = get_label_from_map (map, CODE_LABEL_NUMBER (o->label));
  if (o->resume)
    {
      n->resume = map->insn_map[INSN_UID (o->resume)];
      if (n->resume == NULL)
	abort ();
    }

  return n;
}

static void
duplicate_eh_region_2 (o, n_array)
     struct eh_region *o;
     struct eh_region **n_array;
{
  struct eh_region *n = n_array[o->region_number];

  switch (n->type)
    {
    case ERT_TRY:
      n->u.try.catch = n_array[o->u.try.catch->region_number];
      n->u.try.last_catch = n_array[o->u.try.last_catch->region_number];
      break;

    case ERT_CATCH:
      if (o->u.catch.next_catch)
        n->u.catch.next_catch = n_array[o->u.catch.next_catch->region_number];
      if (o->u.catch.prev_catch)
        n->u.catch.prev_catch = n_array[o->u.catch.prev_catch->region_number];
      break;

    default:
      break;
    }

  if (o->outer)
    n->outer = n_array[o->outer->region_number];
  if (o->inner)
    n->inner = n_array[o->inner->region_number];
  if (o->next_peer)
    n->next_peer = n_array[o->next_peer->region_number];
}    

int
duplicate_eh_regions (ifun, map)
     struct function *ifun;
     struct inline_remap *map;
{
  int ifun_last_region_number = ifun->eh->last_region_number;
  struct eh_region **n_array, *root, *cur;
  int i;

  if (ifun_last_region_number == 0)
    return 0;

  n_array = xcalloc (ifun_last_region_number + 1, sizeof (*n_array));

  for (i = 1; i <= ifun_last_region_number; ++i)
    {
      cur = ifun->eh->region_array[i];
      if (!cur || cur->region_number != i)
	continue;
      n_array[i] = duplicate_eh_region_1 (cur, map);
    }
  for (i = 1; i <= ifun_last_region_number; ++i)
    {
      cur = ifun->eh->region_array[i];
      if (!cur || cur->region_number != i)
	continue;
      duplicate_eh_region_2 (cur, n_array);
    }

  root = n_array[ifun->eh->region_tree->region_number];
  cur = cfun->eh->cur_region;
  if (cur)
    {
      struct eh_region *p = cur->inner;
      if (p)
	{
	  while (p->next_peer)
	    p = p->next_peer;
	  p->next_peer = root;
	}
      else
	cur->inner = root;

      for (i = 1; i <= ifun_last_region_number; ++i)
	if (n_array[i] && n_array[i]->outer == NULL)
	  n_array[i]->outer = cur;
    }
  else
    {
      struct eh_region *p = cfun->eh->region_tree;
      if (p)
	{
	  while (p->next_peer)
	    p = p->next_peer;
	  p->next_peer = root;
	}
      else
	cfun->eh->region_tree = root;
    }

  free (n_array);

  i = cfun->eh->last_region_number;
  cfun->eh->last_region_number = i + ifun_last_region_number;
  return i;
}


/* ??? Move from tree.c to tree.h.  */
#define TYPE_HASH(TYPE) ((HOST_WIDE_INT) (TYPE) & 0777777)

static int
t2r_eq (pentry, pdata)
     const PTR pentry;
     const PTR pdata;
{
  tree entry = (tree) pentry;
  tree data = (tree) pdata;

  return TREE_PURPOSE (entry) == data;
}

static hashval_t
t2r_hash (pentry)
     const PTR pentry;
{
  tree entry = (tree) pentry;
  return TYPE_HASH (TREE_PURPOSE (entry));
}

static int
t2r_mark_1 (slot, data)
     PTR *slot;
     PTR data ATTRIBUTE_UNUSED;
{
  tree contents = (tree) *slot;
  ggc_mark_tree (contents);
  return 1;
}

static void
t2r_mark (addr)
     PTR addr;
{
  htab_traverse (*(htab_t *)addr, t2r_mark_1, NULL);
}

static void
add_type_for_runtime (type)
     tree type;
{
  tree *slot;

  slot = (tree *) htab_find_slot_with_hash (type_to_runtime_map, type,
					    TYPE_HASH (type), INSERT);
  if (*slot == NULL)
    {
      tree runtime = (*lang_eh_runtime_type) (type);
      *slot = tree_cons (type, runtime, NULL_TREE);
    }
}
  
static tree
lookup_type_for_runtime (type)
     tree type;
{
  tree *slot;

  slot = (tree *) htab_find_slot_with_hash (type_to_runtime_map, type,
					    TYPE_HASH (type), NO_INSERT);

  /* We should have always inserrted the data earlier.  */
  return TREE_VALUE (*slot);
}


/* Represent an entry in @TTypes for either catch actions
   or exception filter actions.  */
struct ttypes_filter
{
  tree t;
  int filter;
};

/* Compare ENTRY (a ttypes_filter entry in the hash table) with DATA
   (a tree) for a @TTypes type node we are thinking about adding.  */

static int
ttypes_filter_eq (pentry, pdata)
     const PTR pentry;
     const PTR pdata;
{
  const struct ttypes_filter *entry = (const struct ttypes_filter *) pentry;
  tree data = (tree) pdata;

  return entry->t == data;
}

static hashval_t
ttypes_filter_hash (pentry)
     const PTR pentry;
{
  const struct ttypes_filter *entry = (const struct ttypes_filter *) pentry;
  return TYPE_HASH (entry->t);
}

/* Compare ENTRY with DATA (both struct ttypes_filter) for a @TTypes
   exception specification list we are thinking about adding.  */
/* ??? Currently we use the type lists in the order given.  Someone
   should put these in some canonical order.  */

static int
ehspec_filter_eq (pentry, pdata)
     const PTR pentry;
     const PTR pdata;
{
  const struct ttypes_filter *entry = (const struct ttypes_filter *) pentry;
  const struct ttypes_filter *data = (const struct ttypes_filter *) pdata;

  return type_list_equal (entry->t, data->t);
}

/* Hash function for exception specification lists.  */

static hashval_t
ehspec_filter_hash (pentry)
     const PTR pentry;
{
  const struct ttypes_filter *entry = (const struct ttypes_filter *) pentry;
  hashval_t h = 0;
  tree list;

  for (list = entry->t; list ; list = TREE_CHAIN (list))
    h = (h << 5) + (h >> 27) + TYPE_HASH (TREE_VALUE (list));
  return h;
}

/* Add TYPE to cfun->eh->ttype_data, using TYPES_HASH to speed
   up the search.  Return the filter value to be used.  */

static int
add_ttypes_entry (ttypes_hash, type)
     htab_t ttypes_hash;
     tree type;
{
  struct ttypes_filter **slot, *n;

  slot = (struct ttypes_filter **)
    htab_find_slot_with_hash (ttypes_hash, type, TYPE_HASH (type), INSERT);

  if ((n = *slot) == NULL)
    {
      /* Filter value is a 1 based table index.  */

      n = (struct ttypes_filter *) xmalloc (sizeof (*n));
      n->t = type;
      n->filter = VARRAY_ACTIVE_SIZE (cfun->eh->ttype_data) + 1;
      *slot = n;

      VARRAY_PUSH_TREE (cfun->eh->ttype_data, type);
    }

  return n->filter;
}

/* Add LIST to cfun->eh->ehspec_data, using EHSPEC_HASH and TYPES_HASH
   to speed up the search.  Return the filter value to be used.  */

static int
add_ehspec_entry (ehspec_hash, ttypes_hash, list)
     htab_t ehspec_hash;
     htab_t ttypes_hash;
     tree list;
{
  struct ttypes_filter **slot, *n;
  struct ttypes_filter dummy;

  dummy.t = list;
  slot = (struct ttypes_filter **)
    htab_find_slot (ehspec_hash, &dummy, INSERT);

  if ((n = *slot) == NULL)
    {
      /* Filter value is a -1 based byte index into a uleb128 buffer.  */

      n = (struct ttypes_filter *) xmalloc (sizeof (*n));
      n->t = list;
      n->filter = -(VARRAY_ACTIVE_SIZE (cfun->eh->ehspec_data) + 1);
      *slot = n;

      /* Look up each type in the list and encode its filter
	 value as a uleb128.  Terminate the list with 0.  */
      for (; list ; list = TREE_CHAIN (list))
	push_uleb128 (&cfun->eh->ehspec_data, 
		      add_ttypes_entry (ttypes_hash, TREE_VALUE (list)));
      VARRAY_PUSH_UCHAR (cfun->eh->ehspec_data, 0);
    }

  return n->filter;
}

/* Generate the action filter values to be used for CATCH and
   ALLOWED_EXCEPTIONS regions.  When using dwarf2 exception regions,
   we use lots of landing pads, and so every type or list can share
   the same filter value, which saves table space.  */

static void
assign_filter_values ()
{
  int i;
  htab_t ttypes, ehspec;

  VARRAY_TREE_INIT (cfun->eh->ttype_data, 16, "ttype_data");
  VARRAY_UCHAR_INIT (cfun->eh->ehspec_data, 64, "ehspec_data");

  ttypes = htab_create (31, ttypes_filter_hash, ttypes_filter_eq, free);
  ehspec = htab_create (31, ehspec_filter_hash, ehspec_filter_eq, free);

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *r = cfun->eh->region_array[i];

      /* Mind we don't process a region more than once.  */
      if (!r || r->region_number != i)
	continue;

      switch (r->type)
	{
	case ERT_CATCH:
	  r->u.catch.filter = add_ttypes_entry (ttypes, r->u.catch.type);
	  break;

	case ERT_ALLOWED_EXCEPTIONS:
	  r->u.allowed.filter
	    = add_ehspec_entry (ehspec, ttypes, r->u.allowed.type_list);
	  break;

	default:
	  break;
	}
    }

  htab_delete (ttypes);
  htab_delete (ehspec);
}

static void
build_post_landing_pads ()
{
  int i;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];
      rtx seq;

      /* Mind we don't process a region more than once.  */
      if (!region || region->region_number != i)
	continue;

      switch (region->type)
	{
	case ERT_TRY:
	  /* ??? Collect the set of all non-overlapping catch handlers
	       all the way up the chain until blocked by a cleanup.  */
	  /* ??? Outer try regions can share landing pads with inner
	     try regions if the types are completely non-overlapping,
	     and there are no interveaning cleanups.  */

	  region->post_landing_pad = gen_label_rtx ();

	  start_sequence ();

	  emit_label (region->post_landing_pad);

	  /* ??? It is mighty inconvenient to call back into the
	     switch statement generation code in expand_end_case.
	     Rapid prototyping sez a sequence of ifs.  */
	  {
	    struct eh_region *c;
	    for (c = region->u.try.catch; c ; c = c->u.catch.next_catch)
	      {
		/* ??? _Unwind_ForcedUnwind wants no match here.  */
		if (c->u.catch.type == NULL)
		  emit_jump (c->label);
		else
		  emit_cmp_and_jump_insns (cfun->eh->filter,
					   GEN_INT (c->u.catch.filter),
					   EQ, NULL_RTX, word_mode,
					   0, 0, c->label);
	      }
	  }

	  /* We delay the generation of the _Unwind_Resume until we generate
	     landing pads.  We emit a marker here so as to get good control
	     flow data in the meantime.  */
	  region->resume
	    = emit_jump_insn (gen_rtx_RESX (VOIDmode, region->region_number));
	  emit_barrier ();

	  seq = get_insns ();
	  end_sequence ();

	  emit_insns_before (seq, region->u.try.catch->label);
	  break;

	case ERT_ALLOWED_EXCEPTIONS:
	  region->post_landing_pad = gen_label_rtx ();

	  start_sequence ();

	  emit_label (region->post_landing_pad);

	  emit_cmp_and_jump_insns (cfun->eh->filter,
				   GEN_INT (region->u.allowed.filter),
				   EQ, NULL_RTX, word_mode, 0, 0,
				   region->label);

	  /* We delay the generation of the _Unwind_Resume until we generate
	     landing pads.  We emit a marker here so as to get good control
	     flow data in the meantime.  */
	  region->resume
	    = emit_jump_insn (gen_rtx_RESX (VOIDmode, region->region_number));
	  emit_barrier ();

	  seq = get_insns ();
	  end_sequence ();

	  emit_insns_before (seq, region->label);
	  break;

	case ERT_CLEANUP:
	case ERT_MUST_NOT_THROW:
	  region->post_landing_pad = region->label;
	  break;

	case ERT_CATCH:
	case ERT_THROW:
	  /* Nothing to do.  */
	  break;

	default:
	  abort ();
	}
    }
}

/* Replace RESX patterns with jumps to the next handler if any, or calls to
   _Unwind_Resume otherwise.  */

static void
connect_post_landing_pads ()
{
  int i;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];
      struct eh_region *outer;
      rtx seq;

      /* Mind we don't process a region more than once.  */
      if (!region || region->region_number != i)
	continue;

      /* If there is no RESX, or it has been deleted by flow, there's
	 nothing to fix up.  */
      if (! region->resume || INSN_DELETED_P (region->resume))
	continue;

      /* Search for another landing pad in this function.  */
      for (outer = region->outer; outer ; outer = outer->outer)
	if (outer->post_landing_pad)
	  break;

      start_sequence ();

      if (outer)
	emit_jump (outer->post_landing_pad);
      else
	emit_library_call (unwind_resume_libfunc, LCT_THROW,
			   VOIDmode, 1, cfun->eh->exc_ptr, Pmode);

      seq = get_insns ();
      end_sequence ();
      emit_insns_before (seq, region->resume);

      /* Leave the RESX to be deleted by flow.  */
    }
}


static void
dw2_build_landing_pads ()
{
  int i, j;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];
      rtx seq;

      /* Mind we don't process a region more than once.  */
      if (!region || region->region_number != i)
	continue;

      if (region->type != ERT_CLEANUP
	  && region->type != ERT_TRY
	  && region->type != ERT_ALLOWED_EXCEPTIONS)
	continue;

      start_sequence ();

      region->landing_pad = gen_label_rtx ();
      emit_label (region->landing_pad);

#ifdef HAVE_exception_receiver
      if (HAVE_exception_receiver)
	emit_insn (gen_exception_receiver ());
      else
#endif
#ifdef HAVE_nonlocal_goto_receiver
	if (HAVE_nonlocal_goto_receiver)
	  emit_insn (gen_nonlocal_goto_receiver ());
	else
#endif
	  { /* Nothing */ }

      /* If the eh_return data registers are call-saved, then we
	 won't have considered them clobbered from the call that
	 threw.  Kill them now.  */
      for (j = 0; ; ++j)
	{
	  unsigned r = EH_RETURN_DATA_REGNO (j);
	  if (r == INVALID_REGNUM)
	    break;
	  if (! call_used_regs[r])
	    emit_insn (gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (Pmode, r)));
	}

      emit_move_insn (cfun->eh->exc_ptr,
		      gen_rtx_REG (Pmode, EH_RETURN_DATA_REGNO (0)));
      emit_move_insn (cfun->eh->filter,
		      gen_rtx_REG (word_mode, EH_RETURN_DATA_REGNO (1)));

      seq = get_insns ();
      end_sequence ();

      emit_insns_before (seq, region->post_landing_pad);
    }
}


struct sjlj_lp_info
{
  int directly_reachable;
  int action_index;
  int dispatch_index;
  int call_site_index;
};

static bool
sjlj_find_directly_reachable_regions (lp_info)
     struct sjlj_lp_info *lp_info;
{
  rtx insn;
  bool found_one = false;

  for (insn = get_insns (); insn ; insn = NEXT_INSN (insn))
    {
      struct eh_region *region;
      tree type_thrown;
      rtx note;

      if (! INSN_P (insn))
	continue;

      note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
      if (!note || INTVAL (XEXP (note, 0)) <= 0)
	continue;

      region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];

      type_thrown = NULL_TREE;
      if (region->type == ERT_THROW)
	{
	  type_thrown = region->u.throw.type;
	  region = region->outer;
	}

      /* Find the first containing region that might handle the exception.
	 That's the landing pad to which we will transfer control.  */
      for (; region; region = region->outer)
	if (reachable_next_level (region, type_thrown, 0) != RNL_NOT_CAUGHT)
	  break;

      if (region)
	{
	  lp_info[region->region_number].directly_reachable = 1;
	  found_one = true;
	}
    }

  return found_one;
}

static void
sjlj_assign_call_site_values (dispatch_label, lp_info)
     rtx dispatch_label;
     struct sjlj_lp_info *lp_info;
{
  htab_t ar_hash;
  int i, index;

  /* First task: build the action table.  */

  VARRAY_UCHAR_INIT (cfun->eh->action_record_data, 64, "action_record_data");
  ar_hash = htab_create (31, action_record_hash, action_record_eq, free);

  for (i = cfun->eh->last_region_number; i > 0; --i)
    if (lp_info[i].directly_reachable)
      {
	struct eh_region *r = cfun->eh->region_array[i];
	r->landing_pad = dispatch_label;
	lp_info[i].action_index = collect_one_action_chain (ar_hash, r);
	if (lp_info[i].action_index != -1)
	  cfun->uses_eh_lsda = 1;
      }

  htab_delete (ar_hash);

  /* Next: assign dispatch values.  In dwarf2 terms, this would be the
     landing pad label for the region.  For sjlj though, there is one
     common landing pad from which we dispatch to the post-landing pads.

     A region receives a dispatch index if it is directly reachable
     and requires in-function processing.  Regions that share post-landing
     pads may share dispatch indicies.  */
  /* ??? Post-landing pad sharing doesn't actually happen at the moment
     (see build_post_landing_pads) so we don't bother checking for it.  */

  index = 0;
  for (i = cfun->eh->last_region_number; i > 0; --i)
    if (lp_info[i].directly_reachable
	&& lp_info[i].action_index >= 0)
      lp_info[i].dispatch_index = index++;

  /* Finally: assign call-site values.  If dwarf2 terms, this would be
     the region number assigned by convert_to_eh_region_ranges, but
     handles no-action and must-not-throw differently.  */

  call_site_base = 1;
  for (i = cfun->eh->last_region_number; i > 0; --i)
    if (lp_info[i].directly_reachable)
      {
	int action = lp_info[i].action_index;

	/* Map must-not-throw to otherwise unused call-site index 0.  */
	if (action == -2)
	  index = 0;
	/* Map no-action to otherwise unused call-site index -1.  */
	else if (action == -1)
	  index = -1;
	/* Otherwise, look it up in the table.  */
	else
	  index = add_call_site (GEN_INT (lp_info[i].dispatch_index), action);

	lp_info[i].call_site_index = index;
      }
}

static void
sjlj_mark_call_sites (lp_info)
     struct sjlj_lp_info *lp_info;
{
  int last_call_site = -2;
  rtx insn, mem;

  mem = change_address (cfun->eh->sjlj_fc, TYPE_MODE (integer_type_node),
			plus_constant (XEXP (cfun->eh->sjlj_fc, 0),
				       sjlj_fc_call_site_ofs));

  for (insn = get_insns (); insn ; insn = NEXT_INSN (insn))
    {
      struct eh_region *region;
      int this_call_site;
      rtx note, before, p;

      /* Reset value tracking at extended basic block boundaries.  */
      if (GET_CODE (insn) == CODE_LABEL)
	last_call_site = -2;

      if (! INSN_P (insn))
	continue;

      note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
      if (!note)
	{
	  /* Calls (and trapping insns) without notes are outside any
	     exception handling region in this function.  Mark them as
	     no action.  */
	  if (GET_CODE (insn) == CALL_INSN
	      || (flag_non_call_exceptions
		  && may_trap_p (PATTERN (insn))))
	    this_call_site = -1;
	  else
	    continue;
	}
      else
	{
	  /* Calls that are known to not throw need not be marked.  */
	  if (INTVAL (XEXP (note, 0)) <= 0)
	    continue;

	  region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];
	  this_call_site = lp_info[region->region_number].call_site_index;
	}

      if (this_call_site == last_call_site)
	continue;

      /* Don't separate a call from it's argument loads.  */
      before = insn;
      if (GET_CODE (insn) == CALL_INSN)
	{
	  HARD_REG_SET parm_regs;
	  int nparm_regs;
	  
	  /* Since different machines initialize their parameter registers
	     in different orders, assume nothing.  Collect the set of all
	     parameter registers.  */
	  CLEAR_HARD_REG_SET (parm_regs);
	  nparm_regs = 0;
	  for (p = CALL_INSN_FUNCTION_USAGE (insn); p ; p = XEXP (p, 1))
	    if (GET_CODE (XEXP (p, 0)) == USE
		&& GET_CODE (XEXP (XEXP (p, 0), 0)) == REG)
	      {
		if (REGNO (XEXP (XEXP (p, 0), 0)) >= FIRST_PSEUDO_REGISTER)
		  abort ();

		/* We only care about registers which can hold function
		   arguments.  */
		if (! FUNCTION_ARG_REGNO_P (REGNO (XEXP (XEXP (p, 0), 0))))
		  continue;

		SET_HARD_REG_BIT (parm_regs, REGNO (XEXP (XEXP (p, 0), 0)));
		nparm_regs++;
	      }

	  /* Search backward for the first set of a register in this set.  */
	  while (nparm_regs)
	    {
	      before = PREV_INSN (before);

	      /* Given that we've done no other optimizations yet,
		 the arguments should be immediately available.  */
	      if (GET_CODE (before) == CODE_LABEL)
		abort ();

	      p = single_set (before);
	      if (p && GET_CODE (SET_DEST (p)) == REG
		  && REGNO (SET_DEST (p)) < FIRST_PSEUDO_REGISTER
		  && TEST_HARD_REG_BIT (parm_regs, REGNO (SET_DEST (p))))
		{
		  CLEAR_HARD_REG_BIT (parm_regs, REGNO (SET_DEST (p)));
		  nparm_regs--;
		}
	    }
	}

      start_sequence ();
      emit_move_insn (mem, GEN_INT (this_call_site));
      p = get_insns ();
      end_sequence ();

      emit_insns_before (p, before);
      last_call_site = this_call_site;
    }
}

/* Construct the SjLj_Function_Context.  */

static void
sjlj_emit_function_enter (dispatch_label)
     rtx dispatch_label;
{
  rtx fn_begin, fc, mem, seq;

  fc = cfun->eh->sjlj_fc;

  start_sequence ();

  /* We're storing this libcall's address into memory instead of
     calling it directly.  Thus, we must call assemble_external_libcall
     here, as we can not depend on emit_library_call to do it for us.  */
  assemble_external_libcall (eh_personality_libfunc);
  mem = change_address (fc, Pmode,
			plus_constant (XEXP (fc, 0), sjlj_fc_personality_ofs));
  emit_move_insn (mem, eh_personality_libfunc);

  mem = change_address (fc, Pmode,
			plus_constant (XEXP (fc, 0), sjlj_fc_lsda_ofs));
  if (cfun->uses_eh_lsda)
    {
      char buf[20];
      ASM_GENERATE_INTERNAL_LABEL (buf, "LLSDA", sjlj_funcdef_number);
      emit_move_insn (mem, gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (buf)));
    }
  else
    emit_move_insn (mem, const0_rtx);
  
#ifdef DONT_USE_BUILTIN_SETJMP
  {
    rtx x, note;
    x = emit_library_call_value (setjmp_libfunc, NULL_RTX, LCT_NORMAL,
				 TYPE_MODE (integer_type_node), 1,
				 plus_constant (XEXP (fc, 0),
						sjlj_fc_jbuf_ofs), Pmode);

    note = emit_note (NULL, NOTE_INSN_EXPECTED_VALUE);
    NOTE_EXPECTED_VALUE (note) = gen_rtx_EQ (VOIDmode, x, const0_rtx);

    emit_cmp_and_jump_insns (x, const0_rtx, NE, 0,
			     TYPE_MODE (integer_type_node), 0, 0,
			     dispatch_label);
  }
#else
  expand_builtin_setjmp_setup (plus_constant (XEXP (fc, 0), sjlj_fc_jbuf_ofs),
			       dispatch_label);
#endif

  emit_library_call (unwind_sjlj_register_libfunc, LCT_NORMAL, VOIDmode,
		     1, XEXP (fc, 0), Pmode);

  seq = get_insns ();
  end_sequence ();

  /* ??? Instead of doing this at the beginning of the function,
     do this in a block that is at loop level 0 and dominates all
     can_throw_internal instructions.  */

  for (fn_begin = get_insns (); ; fn_begin = NEXT_INSN (fn_begin))
    if (GET_CODE (fn_begin) == NOTE
	&& NOTE_LINE_NUMBER (fn_begin) == NOTE_INSN_FUNCTION_BEG)
      break;
  emit_insns_after (seq, fn_begin);
}

/* Call back from expand_function_end to know where we should put
   the call to unwind_sjlj_unregister_libfunc if needed.  */

void
sjlj_emit_function_exit_after (after)
     rtx after;
{
  cfun->eh->sjlj_exit_after = after;
}

static void
sjlj_emit_function_exit ()
{
  rtx seq;

  start_sequence ();

  emit_library_call (unwind_sjlj_unregister_libfunc, LCT_NORMAL, VOIDmode,
		     1, XEXP (cfun->eh->sjlj_fc, 0), Pmode);

  seq = get_insns ();
  end_sequence ();

  /* ??? Really this can be done in any block at loop level 0 that
     post-dominates all can_throw_internal instructions.  This is
     the last possible moment.  */

  emit_insns_after (seq, cfun->eh->sjlj_exit_after);
}

static void
sjlj_emit_dispatch_table (dispatch_label, lp_info)
     rtx dispatch_label;
     struct sjlj_lp_info *lp_info;
{
  int i, first_reachable;
  rtx mem, dispatch, seq, fc;

  fc = cfun->eh->sjlj_fc;

  start_sequence ();

  emit_label (dispatch_label);
  
#ifndef DONT_USE_BUILTIN_SETJMP
  expand_builtin_setjmp_receiver (dispatch_label);
#endif

  /* Load up dispatch index, exc_ptr and filter values from the
     function context.  */
  mem = change_address (fc, TYPE_MODE (integer_type_node),
			plus_constant (XEXP (fc, 0), sjlj_fc_call_site_ofs));
  dispatch = copy_to_reg (mem);

  mem = change_address (fc, word_mode,
			plus_constant (XEXP (fc, 0), sjlj_fc_data_ofs));
  if (word_mode != Pmode)
    {
#ifdef POINTERS_EXTEND_UNSIGNED
      mem = convert_memory_address (Pmode, mem);
#else
      mem = convert_to_mode (Pmode, mem, 0);
#endif
    }
  emit_move_insn (cfun->eh->exc_ptr, mem);

  mem = change_address (fc, word_mode,
			plus_constant (XEXP (fc, 0),
				       sjlj_fc_data_ofs + UNITS_PER_WORD));
  emit_move_insn (cfun->eh->filter, mem);

  /* Jump to one of the directly reachable regions.  */
  /* ??? This really ought to be using a switch statement.  */

  first_reachable = 0;
  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      if (! lp_info[i].directly_reachable
	  || lp_info[i].action_index < 0)
	continue;

      if (! first_reachable)
	{
	  first_reachable = i;
	  continue;
	}

      emit_cmp_and_jump_insns (dispatch,
			       GEN_INT (lp_info[i].dispatch_index), EQ,
			       NULL_RTX, TYPE_MODE (integer_type_node), 0, 0,
			       cfun->eh->region_array[i]->post_landing_pad);
    }

  seq = get_insns ();
  end_sequence ();

  emit_insns_before (seq, (cfun->eh->region_array[first_reachable]
			   ->post_landing_pad));
}

static void
sjlj_build_landing_pads ()
{
  struct sjlj_lp_info *lp_info;

  lp_info = (struct sjlj_lp_info *) xcalloc (cfun->eh->last_region_number + 1,
					     sizeof (struct sjlj_lp_info));

  if (sjlj_find_directly_reachable_regions (lp_info))
    {
      rtx dispatch_label = gen_label_rtx ();

      cfun->eh->sjlj_fc
	= assign_stack_local (TYPE_MODE (sjlj_fc_type_node),
			      int_size_in_bytes (sjlj_fc_type_node),
			      TYPE_ALIGN (sjlj_fc_type_node));

      sjlj_assign_call_site_values (dispatch_label, lp_info);
      sjlj_mark_call_sites (lp_info);

      sjlj_emit_function_enter (dispatch_label);
      sjlj_emit_dispatch_table (dispatch_label, lp_info);
      sjlj_emit_function_exit ();
    }

  free (lp_info);
}

void
finish_eh_generation ()
{
  /* Nothing to do if no regions created.  */
  if (cfun->eh->region_tree == NULL)
    return;

  /* The object here is to provide find_basic_blocks with detailed
     information (via reachable_handlers) on how exception control
     flows within the function.  In this first pass, we can include
     type information garnered from ERT_THROW and ERT_ALLOWED_EXCEPTIONS
     regions, and hope that it will be useful in deleting unreachable
     handlers.  Subsequently, we will generate landing pads which will
     connect many of the handlers, and then type information will not
     be effective.  Still, this is a win over previous implementations.  */

  jump_optimize_minimal (get_insns ());
  find_basic_blocks (get_insns (), max_reg_num (), 0);
  cleanup_cfg ();

  /* These registers are used by the landing pads.  Make sure they
     have been generated.  */
  get_exception_pointer (cfun);
  get_exception_filter (cfun);

  /* Construct the landing pads.  */

  assign_filter_values ();
  build_post_landing_pads ();
  connect_post_landing_pads ();
  if (USING_SJLJ_EXCEPTIONS)
    sjlj_build_landing_pads ();
  else
    dw2_build_landing_pads ();

  cfun->eh->built_landing_pads = 1;

  /* We've totally changed the CFG.  Start over.  */
  find_exception_handler_labels ();
  jump_optimize_minimal (get_insns ());
  find_basic_blocks (get_insns (), max_reg_num (), 0);
  cleanup_cfg ();
}

/* This section handles removing dead code for flow.  */

/* Remove LABEL from the exception_handler_labels list.  */

static void
remove_exception_handler_label (label)
     rtx label;
{
  rtx *pl, l;

  for (pl = &exception_handler_labels, l = *pl;
       XEXP (l, 0) != label;
       pl = &XEXP (l, 1), l = *pl)
    continue;

  *pl = XEXP (l, 1);
  free_EXPR_LIST_node (l);
}

/* Splice REGION from the region tree etc.  */

static void
remove_eh_handler (region)
     struct eh_region *region;
{
  struct eh_region **pp, *p;
  rtx lab;
  int i;

  /* For the benefit of efficiently handling REG_EH_REGION notes,
     replace this region in the region array with its containing
     region.  Note that previous region deletions may result in
     multiple copies of this region in the array, so we have to
     search the whole thing.  */
  for (i = cfun->eh->last_region_number; i > 0; --i)
    if (cfun->eh->region_array[i] == region)
      cfun->eh->region_array[i] = region->outer;

  if (cfun->eh->built_landing_pads)
    lab = region->landing_pad;
  else
    lab = region->label;
  if (lab)
    remove_exception_handler_label (lab);

  if (region->outer)
    pp = &region->outer->inner;
  else
    pp = &cfun->eh->region_tree;
  for (p = *pp; p != region; pp = &p->next_peer, p = *pp)
    continue;

  if (region->inner)
    {
      for (p = region->inner; p->next_peer ; p = p->next_peer)
	p->outer = region->outer;
      p->next_peer = region->next_peer;
      p->outer = region->outer;
      *pp = region->inner;
    }
  else
    *pp = region->next_peer;

  if (region->type == ERT_CATCH)
    {
      struct eh_region *try, *next, *prev;

      for (try = region->next_peer;
	   try->type == ERT_CATCH;
	   try = try->next_peer)
	continue;
      if (try->type != ERT_TRY)
	abort ();

      next = region->u.catch.next_catch;
      prev = region->u.catch.prev_catch;

      if (next)
	next->u.catch.prev_catch = prev;
      else
	try->u.try.last_catch = prev;
      if (prev)
	prev->u.catch.next_catch = next;
      else
	{
	  try->u.try.catch = next;
	  if (! next)
	    remove_eh_handler (try);
	}
    }

  free (region);
}

/* LABEL heads a basic block that is about to be deleted.  If this
   label corresponds to an exception region, we may be able to
   delete the region.  */

void
maybe_remove_eh_handler (label)
     rtx label;
{
  int i;

  /* ??? After generating landing pads, it's not so simple to determine
     if the region data is completely unused.  One must examine the
     landing pad and the post landing pad, and whether an inner try block
     is referencing the catch handlers directly.  */
  if (cfun->eh->built_landing_pads)
    return;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];
      if (region && region->label == label)
	{
	  /* Flow will want to remove MUST_NOT_THROW regions as unreachable
	     because there is no path to the fallback call to terminate.
	     But the region continues to affect call-site data until there
	     are no more contained calls, which we don't see here.  */
	  if (region->type == ERT_MUST_NOT_THROW)
	    {
	      remove_exception_handler_label (region->label);
	      region->label = NULL_RTX;
	    }
	  else
	    remove_eh_handler (region);
	  break;
	}
    }
}


/* This section describes CFG exception edges for flow.  */

/* For communicating between calls to reachable_next_level.  */
struct reachable_info
{
  tree types_caught;
  tree types_allowed;
  rtx handlers;
};

/* A subroutine of reachable_next_level.  Return true if TYPE, or a
   base class of TYPE, is in HANDLED.  */

static int
check_handled (handled, type)
     tree handled, type;
{
  tree t;

  /* We can check for exact matches without front-end help.  */
  if (! lang_eh_type_covers)
    {
      for (t = handled; t ; t = TREE_CHAIN (t))
	if (TREE_VALUE (t) == type)
	  return 1;
    }
  else
    {
      for (t = handled; t ; t = TREE_CHAIN (t))
	if ((*lang_eh_type_covers) (TREE_VALUE (t), type))
	  return 1;
    }

  return 0;
}

/* A subroutine of reachable_next_level.  If we are collecting a list
   of handlers, add one.  After landing pad generation, reference
   it instead of the handlers themselves.  Further, the handlers are
   all wired together, so by referencing one, we've got them all. 
   Before landing pad generation we reference each handler individually.

   LP_REGION contains the landing pad; REGION is the handler.  */

static void
add_reachable_handler (info, lp_region, region)
     struct reachable_info *info;
     struct eh_region *lp_region;
     struct eh_region *region;
{
  if (! info)
    return;

  if (cfun->eh->built_landing_pads)
    {
      if (! info->handlers)
	info->handlers = alloc_INSN_LIST (lp_region->landing_pad, NULL_RTX);
    }
  else
    info->handlers = alloc_INSN_LIST (region->label, info->handlers);
}

/* Process one level of exception regions for reachability.  
   If TYPE_THROWN is non-null, then it is the *exact* type being
   propagated.  If INFO is non-null, then collect handler labels
   and caught/allowed type information between invocations.  */

static enum reachable_code
reachable_next_level (region, type_thrown, info)
     struct eh_region *region;
     tree type_thrown;
     struct reachable_info *info;
{
  switch (region->type)
    {
    case ERT_CLEANUP:
      /* Before landing-pad generation, we model control flow
	 directly to the individual handlers.  In this way we can
	 see that catch handler types may shadow one another.  */
      add_reachable_handler (info, region, region);
      return RNL_MAYBE_CAUGHT;

    case ERT_TRY:
      {
	struct eh_region *c;
	enum reachable_code ret = RNL_NOT_CAUGHT;

	for (c = region->u.try.catch; c ; c = c->u.catch.next_catch)
	  {
	    /* A catch-all handler ends the search.  */
	    /* ??? _Unwind_ForcedUnwind will want outer cleanups
	       to be run as well.  */
	    if (c->u.catch.type == NULL)
	      {
		add_reachable_handler (info, region, c);
		return RNL_CAUGHT;
	      }

	    if (type_thrown)
	      {
		/* If we have a type match, end the search.  */
		if (c->u.catch.type == type_thrown
		    || (lang_eh_type_covers
			&& (*lang_eh_type_covers) (c->u.catch.type,
						   type_thrown)))
		  {
		    add_reachable_handler (info, region, c);
		    return RNL_CAUGHT;
		  }

		/* If we have definitive information of a match failure,
		   the catch won't trigger.  */
		if (lang_eh_type_covers)
		  return RNL_NOT_CAUGHT;
	      }

	    if (! info)
	      ret = RNL_MAYBE_CAUGHT;

	    /* A type must not have been previously caught.  */
	    else if (! check_handled (info->types_caught, c->u.catch.type))
	      {
		add_reachable_handler (info, region, c);
		info->types_caught = tree_cons (NULL, c->u.catch.type,
						info->types_caught);

		/* ??? If the catch type is a base class of every allowed
		   type, then we know we can stop the search.  */
		ret = RNL_MAYBE_CAUGHT;
	      }
	  }

	return ret;
      }

    case ERT_ALLOWED_EXCEPTIONS:
      /* An empty list of types definitely ends the search.  */
      if (region->u.allowed.type_list == NULL_TREE)
	{
	  add_reachable_handler (info, region, region);
	  return RNL_CAUGHT;
	}

      /* Collect a list of lists of allowed types for use in detecting
	 when a catch may be transformed into a catch-all.  */
      if (info)
	info->types_allowed = tree_cons (NULL_TREE,
					 region->u.allowed.type_list,
					 info->types_allowed);
	    
      /* If we have definitive information about the type heirarchy,
	 then we can tell if the thrown type will pass through the
	 filter.  */
      if (type_thrown && lang_eh_type_covers)
	{
	  if (check_handled (region->u.allowed.type_list, type_thrown))
	    return RNL_NOT_CAUGHT;
	  else
	    {
	      add_reachable_handler (info, region, region);
	      return RNL_CAUGHT;
	    }
	}

      add_reachable_handler (info, region, region);
      return RNL_MAYBE_CAUGHT;

    case ERT_CATCH:
      /* Catch regions are handled by their controling try region.  */
      return RNL_NOT_CAUGHT;

    case ERT_MUST_NOT_THROW:
      /* Here we end our search, since no exceptions may propagate.
	 If we've touched down at some landing pad previous, then the
	 explicit function call we generated may be used.  Otherwise
	 the call is made by the runtime.  */
      if (info && info->handlers)
	{
	  add_reachable_handler (info, region, region);
          return RNL_CAUGHT;
	}
      else
	return RNL_BLOCKED;

    case ERT_THROW:
    case ERT_FIXUP:
      /* Shouldn't see these here.  */
      break;
    }

  abort ();
}

/* Retrieve a list of labels of exception handlers which can be
   reached by a given insn.  */

rtx
reachable_handlers (insn)
     rtx insn;
{
  struct reachable_info info;
  struct eh_region *region;
  tree type_thrown;
  int region_number;

  if (GET_CODE (insn) == JUMP_INSN
      && GET_CODE (PATTERN (insn)) == RESX)
    region_number = XINT (PATTERN (insn), 0);
  else
    {
      rtx note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
      if (!note || INTVAL (XEXP (note, 0)) <= 0)
	return NULL;
      region_number = INTVAL (XEXP (note, 0));
    }

  memset (&info, 0, sizeof (info));

  region = cfun->eh->region_array[region_number];

  type_thrown = NULL_TREE;
  if (region->type == ERT_THROW)
    {
      type_thrown = region->u.throw.type;
      region = region->outer;
    }
  else if (GET_CODE (insn) == JUMP_INSN
	   && GET_CODE (PATTERN (insn)) == RESX)
    region = region->outer;

  for (; region; region = region->outer)
    if (reachable_next_level (region, type_thrown, &info) >= RNL_CAUGHT)
      break;

  return info.handlers;
}

/* Determine if the given INSN can throw an exception that is caught
   within the function.  */

bool
can_throw_internal (insn)
     rtx insn;
{
  struct eh_region *region;
  tree type_thrown;
  rtx note;

  if (! INSN_P (insn))
    return false;

  if (GET_CODE (insn) == INSN
      && GET_CODE (PATTERN (insn)) == SEQUENCE)
    insn = XVECEXP (PATTERN (insn), 0, 0);

  if (GET_CODE (insn) == CALL_INSN
      && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
    {
      int i;
      for (i = 0; i < 3; ++i)
	{
	  rtx sub = XEXP (PATTERN (insn), i);
	  for (; sub ; sub = NEXT_INSN (sub))
	    if (can_throw_internal (sub))
	      return true;
	}
      return false;
    }

  /* Every insn that might throw has an EH_REGION note.  */
  note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
  if (!note || INTVAL (XEXP (note, 0)) <= 0)
    return false;

  region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];

  type_thrown = NULL_TREE;
  if (region->type == ERT_THROW)
    {
      type_thrown = region->u.throw.type;
      region = region->outer;
    }

  /* If this exception is ignored by each and every containing region,
     then control passes straight out.  The runtime may handle some
     regions, which also do not require processing internally.  */
  for (; region; region = region->outer)
    {
      enum reachable_code how = reachable_next_level (region, type_thrown, 0);
      if (how == RNL_BLOCKED)
	return false;
      if (how != RNL_NOT_CAUGHT)
        return true;
    }

  return false;
}

/* Determine if the given INSN can throw an exception that is
   visible outside the function.  */

bool
can_throw_external (insn)
     rtx insn;
{
  struct eh_region *region;
  tree type_thrown;
  rtx note;

  if (! INSN_P (insn))
    return false;

  if (GET_CODE (insn) == INSN
      && GET_CODE (PATTERN (insn)) == SEQUENCE)
    insn = XVECEXP (PATTERN (insn), 0, 0);

  if (GET_CODE (insn) == CALL_INSN
      && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
    {
      int i;
      for (i = 0; i < 3; ++i)
	{
	  rtx sub = XEXP (PATTERN (insn), i);
	  for (; sub ; sub = NEXT_INSN (sub))
	    if (can_throw_external (sub))
	      return true;
	}
      return false;
    }

  note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
  if (!note)
    {
      /* Calls (and trapping insns) without notes are outside any
	 exception handling region in this function.  We have to
	 assume it might throw.  Given that the front end and middle
	 ends mark known NOTHROW functions, this isn't so wildly
	 inaccurate.  */
      return (GET_CODE (insn) == CALL_INSN
	      || (flag_non_call_exceptions
		  && may_trap_p (PATTERN (insn))));
    }
  if (INTVAL (XEXP (note, 0)) <= 0)
    return false;

  region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];

  type_thrown = NULL_TREE;
  if (region->type == ERT_THROW)
    {
      type_thrown = region->u.throw.type;
      region = region->outer;
    }

  /* If the exception is caught or blocked by any containing region,
     then it is not seen by any calling function.  */
  for (; region ; region = region->outer)
    if (reachable_next_level (region, type_thrown, NULL) >= RNL_CAUGHT)
      return false;

  return true;
}

/* True if nothing in this function can throw outside this function.  */

bool
nothrow_function_p ()
{
  rtx insn;

  if (! flag_exceptions)
    return true;

  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    if (can_throw_external (insn))
      return false;
  for (insn = current_function_epilogue_delay_list; insn;
       insn = XEXP (insn, 1))
    if (can_throw_external (insn))
      return false;

  return true;
}


/* Various hooks for unwind library.  */

/* Do any necessary initialization to access arbitrary stack frames.
   On the SPARC, this means flushing the register windows.  */

void
expand_builtin_unwind_init ()
{
  /* Set this so all the registers get saved in our frame; we need to be
     able to copy the saved values for any registers from frames we unwind. */
  current_function_has_nonlocal_label = 1;

#ifdef SETUP_FRAME_ADDRESSES
  SETUP_FRAME_ADDRESSES ();
#endif
}

rtx
expand_builtin_eh_return_data_regno (arglist)
     tree arglist;
{
  tree which = TREE_VALUE (arglist);
  unsigned HOST_WIDE_INT iwhich;

  if (TREE_CODE (which) != INTEGER_CST)
    {
      error ("argument of `__builtin_eh_return_regno' must be constant");
      return constm1_rtx;
    }

  iwhich = tree_low_cst (which, 1);
  iwhich = EH_RETURN_DATA_REGNO (iwhich);
  if (iwhich == INVALID_REGNUM)
    return constm1_rtx;

#ifdef DWARF_FRAME_REGNUM
  iwhich = DWARF_FRAME_REGNUM (iwhich);
#else
  iwhich = DBX_REGISTER_NUMBER (iwhich);
#endif

  return GEN_INT (iwhich);      
}

/* Given a value extracted from the return address register or stack slot,
   return the actual address encoded in that value.  */

rtx
expand_builtin_extract_return_addr (addr_tree)
     tree addr_tree;
{
  rtx addr = expand_expr (addr_tree, NULL_RTX, Pmode, 0);

  /* First mask out any unwanted bits.  */
#ifdef MASK_RETURN_ADDR
  expand_and (addr, MASK_RETURN_ADDR, addr);
#endif

  /* Then adjust to find the real return address.  */
#if defined (RETURN_ADDR_OFFSET)
  addr = plus_constant (addr, RETURN_ADDR_OFFSET);
#endif

  return addr;
}

/* Given an actual address in addr_tree, do any necessary encoding
   and return the value to be stored in the return address register or
   stack slot so the epilogue will return to that address.  */

rtx
expand_builtin_frob_return_addr (addr_tree)
     tree addr_tree;
{
  rtx addr = expand_expr (addr_tree, NULL_RTX, Pmode, 0);

#ifdef RETURN_ADDR_OFFSET
  addr = force_reg (Pmode, addr);
  addr = plus_constant (addr, -RETURN_ADDR_OFFSET);
#endif

  return addr;
}

/* Set up the epilogue with the magic bits we'll need to return to the
   exception handler.  */

void
expand_builtin_eh_return (stackadj_tree, handler_tree)
    tree stackadj_tree, handler_tree;
{
  rtx stackadj, handler;

  stackadj = expand_expr (stackadj_tree, cfun->eh->ehr_stackadj, VOIDmode, 0);
  handler = expand_expr (handler_tree, cfun->eh->ehr_handler, VOIDmode, 0);

  if (! cfun->eh->ehr_label)
    {
      cfun->eh->ehr_stackadj = copy_to_reg (stackadj);
      cfun->eh->ehr_handler = copy_to_reg (handler);
      cfun->eh->ehr_label = gen_label_rtx ();
    }
  else
    {
      if (stackadj != cfun->eh->ehr_stackadj)
	emit_move_insn (cfun->eh->ehr_stackadj, stackadj);
      if (handler != cfun->eh->ehr_handler)
	emit_move_insn (cfun->eh->ehr_handler, handler);
    }

  emit_jump (cfun->eh->ehr_label);
}

void
expand_eh_return ()
{
  rtx sa, ra, around_label;

  if (! cfun->eh->ehr_label)
    return;

  sa = EH_RETURN_STACKADJ_RTX;
  if (! sa)
    {
      error ("__builtin_eh_return not supported on this target");
      return;
    }

  current_function_calls_eh_return = 1;

  around_label = gen_label_rtx ();
  emit_move_insn (sa, const0_rtx);
  emit_jump (around_label);

  emit_label (cfun->eh->ehr_label);
  clobber_return_register ();

#ifdef HAVE_eh_return
  if (HAVE_eh_return)
    emit_insn (gen_eh_return (cfun->eh->ehr_stackadj, cfun->eh->ehr_handler));
  else
#endif
    {
      rtx handler;

      ra = EH_RETURN_HANDLER_RTX;
      if (! ra)
	{
	  error ("__builtin_eh_return not supported on this target");
	  ra = gen_reg_rtx (Pmode);
	}

      emit_move_insn (sa, cfun->eh->ehr_stackadj);

      handler = cfun->eh->ehr_handler;
      if (GET_MODE (ra) != Pmode)
	{
#ifdef POINTERS_EXTEND_UNSIGNED
	  handler = convert_memory_address (GET_MODE (ra), handler);
#else
	  handler = convert_to_mode (GET_MODE (ra), handler, 0);
#endif
	}
      emit_move_insn (ra, handler);
    }

  emit_label (around_label);
}

/* In the following functions, we represent entries in the action table
   as 1-based indicies.  Special cases are:

	 0:	null action record, non-null landing pad; implies cleanups
	-1:	null action record, null landing pad; implies no action
	-2:	no call-site entry; implies must_not_throw
	-3:	we have yet to process outer regions

   Further, no special cases apply to the "next" field of the record.
   For next, 0 means end of list.  */

struct action_record
{
  int offset;
  int filter;
  int next;
};

static int
action_record_eq (pentry, pdata)
     const PTR pentry;
     const PTR pdata;
{
  const struct action_record *entry = (const struct action_record *) pentry;
  const struct action_record *data = (const struct action_record *) pdata;
  return entry->filter == data->filter && entry->next == data->next;
}

static hashval_t
action_record_hash (pentry)
     const PTR pentry;
{
  const struct action_record *entry = (const struct action_record *) pentry;
  return entry->next * 1009 + entry->filter;
}

static int
add_action_record (ar_hash, filter, next)
     htab_t ar_hash;
     int filter, next;
{
  struct action_record **slot, *new, tmp;

  tmp.filter = filter;
  tmp.next = next;
  slot = (struct action_record **) htab_find_slot (ar_hash, &tmp, INSERT);

  if ((new = *slot) == NULL)
    {
      new = (struct action_record *) xmalloc (sizeof (*new));
      new->offset = VARRAY_ACTIVE_SIZE (cfun->eh->action_record_data) + 1;
      new->filter = filter;
      new->next = next;
      *slot = new;

      /* The filter value goes in untouched.  The link to the next
	 record is a "self-relative" byte offset, or zero to indicate
	 that there is no next record.  So convert the absolute 1 based
	 indicies we've been carrying around into a displacement.  */

      push_sleb128 (&cfun->eh->action_record_data, filter);
      if (next)
	next -= VARRAY_ACTIVE_SIZE (cfun->eh->action_record_data) + 1;
      push_sleb128 (&cfun->eh->action_record_data, next);
    }

  return new->offset;
}

static int
collect_one_action_chain (ar_hash, region)
     htab_t ar_hash;
     struct eh_region *region;
{
  struct eh_region *c;
  int next;

  /* If we've reached the top of the region chain, then we have
     no actions, and require no landing pad.  */
  if (region == NULL)
    return -1;

  switch (region->type)
    {
    case ERT_CLEANUP:
      /* A cleanup adds a zero filter to the beginning of the chain, but
	 there are special cases to look out for.  If there are *only*
	 cleanups along a path, then it compresses to a zero action.
	 Further, if there are multiple cleanups along a path, we only
	 need to represent one of them, as that is enough to trigger
	 entry to the landing pad at runtime.  */
      next = collect_one_action_chain (ar_hash, region->outer);
      if (next <= 0)
	return 0;
      for (c = region->outer; c ; c = c->outer)
	if (c->type == ERT_CLEANUP)
	  return next;
      return add_action_record (ar_hash, 0, next);

    case ERT_TRY:
      /* Process the associated catch regions in reverse order.
	 If there's a catch-all handler, then we don't need to
	 search outer regions.  Use a magic -3 value to record
	 that we havn't done the outer search.  */
      next = -3;
      for (c = region->u.try.last_catch; c ; c = c->u.catch.prev_catch)
	{
	  if (c->u.catch.type == NULL)
	    next = add_action_record (ar_hash, c->u.catch.filter, 0);
	  else
	    {
	      if (next == -3)
		{
		  next = collect_one_action_chain (ar_hash, region->outer);

		  /* If there is no next action, terminate the chain.  */
		  if (next == -1)
		    next = 0;
		  /* If all outer actions are cleanups or must_not_throw,
		     we'll have no action record for it, since we had wanted
		     to encode these states in the call-site record directly.
		     Add a cleanup action to the chain to catch these.  */
		  else if (next <= 0)
		    next = add_action_record (ar_hash, 0, 0);
		}
	      next = add_action_record (ar_hash, c->u.catch.filter, next);
	    }
	}
      return next;

    case ERT_ALLOWED_EXCEPTIONS:
      /* An exception specification adds its filter to the
	 beginning of the chain.  */
      next = collect_one_action_chain (ar_hash, region->outer);
      return add_action_record (ar_hash, region->u.allowed.filter,
				next < 0 ? 0 : next);

    case ERT_MUST_NOT_THROW:
      /* A must-not-throw region with no inner handlers or cleanups
	 requires no call-site entry.  Note that this differs from
	 the no handler or cleanup case in that we do require an lsda
	 to be generated.  Return a magic -2 value to record this.  */
      return -2;

    case ERT_CATCH:
    case ERT_THROW:
      /* CATCH regions are handled in TRY above.  THROW regions are
	 for optimization information only and produce no output.  */
      return collect_one_action_chain (ar_hash, region->outer);

    default:
      abort ();
    }
}

static int
add_call_site (landing_pad, action)
     rtx landing_pad;
     int action;
{
  struct call_site_record *data = cfun->eh->call_site_data;
  int used = cfun->eh->call_site_data_used;
  int size = cfun->eh->call_site_data_size;

  if (used >= size)
    {
      size = (size ? size * 2 : 64);
      data = (struct call_site_record *)
	xrealloc (data, sizeof (*data) * size);
      cfun->eh->call_site_data = data;
      cfun->eh->call_site_data_size = size;
    }

  data[used].landing_pad = landing_pad;
  data[used].action = action;

  cfun->eh->call_site_data_used = used + 1;

  return used + call_site_base;
}

/* Turn REG_EH_REGION notes back into NOTE_INSN_EH_REGION notes.
   The new note numbers will not refer to region numbers, but
   instead to call site entries.  */

void
convert_to_eh_region_ranges ()
{
  rtx insn, iter, note;
  htab_t ar_hash;
  int last_action = -3;
  rtx last_action_insn = NULL_RTX;
  rtx last_landing_pad = NULL_RTX;
  rtx first_no_action_insn = NULL_RTX;
  int call_site;

  if (USING_SJLJ_EXCEPTIONS || cfun->eh->region_tree == NULL)
    return;

  VARRAY_UCHAR_INIT (cfun->eh->action_record_data, 64, "action_record_data");

  ar_hash = htab_create (31, action_record_hash, action_record_eq, free);

  for (iter = get_insns (); iter ; iter = NEXT_INSN (iter))
    if (INSN_P (iter))
      {
	struct eh_region *region;
	int this_action;
	rtx this_landing_pad;

	insn = iter;
	if (GET_CODE (insn) == INSN
	    && GET_CODE (PATTERN (insn)) == SEQUENCE)
	  insn = XVECEXP (PATTERN (insn), 0, 0);

	note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
	if (!note)
	  {
	    if (! (GET_CODE (insn) == CALL_INSN
		   || (flag_non_call_exceptions
		       && may_trap_p (PATTERN (insn)))))
	      continue;
	    this_action = -1;
	    region = NULL;
	  }
	else
	  {
	    if (INTVAL (XEXP (note, 0)) <= 0)
	      continue;
	    region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];
	    this_action = collect_one_action_chain (ar_hash, region);
	  }

	/* Existence of catch handlers, or must-not-throw regions
	   implies that an lsda is needed (even if empty).  */
	if (this_action != -1)
	  cfun->uses_eh_lsda = 1;

	/* Delay creation of region notes for no-action regions
	   until we're sure that an lsda will be required.  */
	else if (last_action == -3)
	  {
	    first_no_action_insn = iter;
	    last_action = -1;
	  }

	/* Cleanups and handlers may share action chains but not
	   landing pads.  Collect the landing pad for this region.  */
	if (this_action >= 0)
	  {
	    struct eh_region *o;
	    for (o = region; ! o->landing_pad ; o = o->outer)
	      continue;
	    this_landing_pad = o->landing_pad;
	  }
	else
	  this_landing_pad = NULL_RTX;

	/* Differing actions or landing pads implies a change in call-site
	   info, which implies some EH_REGION note should be emitted.  */
	if (last_action != this_action
	    || last_landing_pad != this_landing_pad)
	  {
	    /* If we'd not seen a previous action (-3) or the previous
	       action was must-not-throw (-2), then we do not need an
	       end note.  */
	    if (last_action >= -1)
	      {
		/* If we delayed the creation of the begin, do it now.  */
		if (first_no_action_insn)
		  {
		    call_site = add_call_site (NULL_RTX, 0);
		    note = emit_note_before (NOTE_INSN_EH_REGION_BEG,
					     first_no_action_insn);
		    NOTE_EH_HANDLER (note) = call_site;
		    first_no_action_insn = NULL_RTX;
		  }

		note = emit_note_after (NOTE_INSN_EH_REGION_END,
					last_action_insn);
		NOTE_EH_HANDLER (note) = call_site;
	      }

	    /* If the new action is must-not-throw, then no region notes
	       are created.  */
	    if (this_action >= -1)
	      {
		call_site = add_call_site (this_landing_pad, 
					   this_action < 0 ? 0 : this_action);
		note = emit_note_before (NOTE_INSN_EH_REGION_BEG, iter);
		NOTE_EH_HANDLER (note) = call_site;
	      }

	    last_action = this_action;
	    last_landing_pad = this_landing_pad;
	  }
	last_action_insn = iter;
      }

  if (last_action >= -1 && ! first_no_action_insn)
    {
      note = emit_note_after (NOTE_INSN_EH_REGION_END, last_action_insn);
      NOTE_EH_HANDLER (note) = call_site;
    }

  htab_delete (ar_hash);
}


static void
push_uleb128 (data_area, value)
     varray_type *data_area;
     unsigned int value;
{
  do
    {
      unsigned char byte = value & 0x7f;
      value >>= 7;
      if (value)
	byte |= 0x80;
      VARRAY_PUSH_UCHAR (*data_area, byte);
    }
  while (value);
}

static void
push_sleb128 (data_area, value)
     varray_type *data_area;
     int value;
{
  unsigned char byte;
  int more;

  do
    {
      byte = value & 0x7f;
      value >>= 7;
      more = ! ((value == 0 && (byte & 0x40) == 0)
		|| (value == -1 && (byte & 0x40) != 0));
      if (more)
	byte |= 0x80;
      VARRAY_PUSH_UCHAR (*data_area, byte);
    }
  while (more);
}


#ifndef HAVE_AS_LEB128
static int
dw2_size_of_call_site_table ()
{
  int n = cfun->eh->call_site_data_used;
  int size = n * (4 + 4 + 4);
  int i;

  for (i = 0; i < n; ++i)
    {
      struct call_site_record *cs = &cfun->eh->call_site_data[i];
      size += size_of_uleb128 (cs->action);
    }

  return size;
}

static int
sjlj_size_of_call_site_table ()
{
  int n = cfun->eh->call_site_data_used;
  int size = 0;
  int i;

  for (i = 0; i < n; ++i)
    {
      struct call_site_record *cs = &cfun->eh->call_site_data[i];
      size += size_of_uleb128 (INTVAL (cs->landing_pad));
      size += size_of_uleb128 (cs->action);
    }

  return size;
}
#endif

static void
dw2_output_call_site_table ()
{
  const char *function_start_lab
    = IDENTIFIER_POINTER (current_function_func_begin_label);
  int n = cfun->eh->call_site_data_used;
  int i;

  for (i = 0; i < n; ++i)
    {
      struct call_site_record *cs = &cfun->eh->call_site_data[i];
      char reg_start_lab[32];
      char reg_end_lab[32];
      char landing_pad_lab[32];

      ASM_GENERATE_INTERNAL_LABEL (reg_start_lab, "LEHB", call_site_base + i);
      ASM_GENERATE_INTERNAL_LABEL (reg_end_lab, "LEHE", call_site_base + i);

      if (cs->landing_pad)
	ASM_GENERATE_INTERNAL_LABEL (landing_pad_lab, "L",
				     CODE_LABEL_NUMBER (cs->landing_pad));

      /* ??? Perhaps use insn length scaling if the assembler supports
	 generic arithmetic.  */
      /* ??? Perhaps use attr_length to choose data1 or data2 instead of
	 data4 if the function is small enough.  */
#ifdef HAVE_AS_LEB128
      dw2_asm_output_delta_uleb128 (reg_start_lab, function_start_lab,
				    "region %d start", i);
      dw2_asm_output_delta_uleb128 (reg_end_lab, reg_start_lab,
				    "length");
      if (cs->landing_pad)
	dw2_asm_output_delta_uleb128 (landing_pad_lab, function_start_lab,
				      "landing pad");
      else
	dw2_asm_output_data_uleb128 (0, "landing pad");
#else
      dw2_asm_output_delta (4, reg_start_lab, function_start_lab,
			    "region %d start", i);
      dw2_asm_output_delta (4, reg_end_lab, reg_start_lab, "length");
      if (cs->landing_pad)
	dw2_asm_output_delta (4, landing_pad_lab, function_start_lab,
			      "landing pad");
      else
	dw2_asm_output_data (4, 0, "landing pad");
#endif
      dw2_asm_output_data_uleb128 (cs->action, "action");
    }

  call_site_base += n;
}

static void
sjlj_output_call_site_table ()
{
  int n = cfun->eh->call_site_data_used;
  int i;

  for (i = 0; i < n; ++i)
    {
      struct call_site_record *cs = &cfun->eh->call_site_data[i];

      dw2_asm_output_data_uleb128 (INTVAL (cs->landing_pad),
				   "region %d landing pad", i);
      dw2_asm_output_data_uleb128 (cs->action, "action");
    }

  call_site_base += n;
}

void
output_function_exception_table ()
{
  int tt_format, cs_format, lp_format, i, n;
#ifdef HAVE_AS_LEB128
  char ttype_label[32];
  char cs_after_size_label[32];
  char cs_end_label[32];
#else
  int call_site_len;
#endif
  int have_tt_data;
  int funcdef_number;
  int tt_format_size;

  /* Not all functions need anything.  */
  if (! cfun->uses_eh_lsda)
    return;

  funcdef_number = (USING_SJLJ_EXCEPTIONS
		    ? sjlj_funcdef_number
		    : current_funcdef_number);

#ifdef IA64_UNWIND_INFO
  fputs ("\t.personality\t", asm_out_file);
  output_addr_const (asm_out_file, eh_personality_libfunc);
  fputs ("\n\t.handlerdata\n", asm_out_file);
  /* Note that varasm still thinks we're in the function's code section.
     The ".endp" directive that will immediately follow will take us back.  */
#else
  exception_section ();
#endif

  have_tt_data = (VARRAY_ACTIVE_SIZE (cfun->eh->ttype_data) > 0
		  || VARRAY_ACTIVE_SIZE (cfun->eh->ehspec_data) > 0);

  /* Indicate the format of the @TType entries.  */
  if (! have_tt_data)
    tt_format = DW_EH_PE_omit;
  else
    {
      tt_format = ASM_PREFERRED_EH_DATA_FORMAT (/*code=*/0, /*global=*/1);
#ifdef HAVE_AS_LEB128
      ASM_GENERATE_INTERNAL_LABEL (ttype_label, "LLSDATT", funcdef_number);
#endif
      tt_format_size = size_of_encoded_value (tt_format);

      assemble_eh_align (tt_format_size * BITS_PER_UNIT);
    }

  ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "LLSDA", funcdef_number);

  /* The LSDA header.  */

  /* Indicate the format of the landing pad start pointer.  An omitted
     field implies @LPStart == @Start.  */
  /* Currently we always put @LPStart == @Start.  This field would
     be most useful in moving the landing pads completely out of
     line to another section, but it could also be used to minimize
     the size of uleb128 landing pad offsets.  */
  lp_format = DW_EH_PE_omit;
  dw2_asm_output_data (1, lp_format, "@LPStart format (%s)",
		       eh_data_format_name (lp_format));

  /* @LPStart pointer would go here.  */

  dw2_asm_output_data (1, tt_format, "@TType format (%s)",
		       eh_data_format_name (tt_format));

#ifndef HAVE_AS_LEB128
  if (USING_SJLJ_EXCEPTIONS)
    call_site_len = sjlj_size_of_call_site_table ();
  else
    call_site_len = dw2_size_of_call_site_table ();
#endif

  /* A pc-relative 4-byte displacement to the @TType data.  */
  if (have_tt_data)
    {
#ifdef HAVE_AS_LEB128
      char ttype_after_disp_label[32];
      ASM_GENERATE_INTERNAL_LABEL (ttype_after_disp_label, "LLSDATTD", 
				   funcdef_number);
      dw2_asm_output_delta_uleb128 (ttype_label, ttype_after_disp_label,
				    "@TType base offset");
      ASM_OUTPUT_LABEL (asm_out_file, ttype_after_disp_label);
#else
      /* Ug.  Alignment queers things.  */
      unsigned int before_disp, after_disp, last_disp, disp;

      before_disp = 1 + 1;
      after_disp = (1 + size_of_uleb128 (call_site_len)
		    + call_site_len
		    + VARRAY_ACTIVE_SIZE (cfun->eh->action_record_data)
		    + (VARRAY_ACTIVE_SIZE (cfun->eh->ttype_data)
		       * tt_format_size));

      disp = after_disp;
      do
	{
	  unsigned int disp_size, pad;

	  last_disp = disp;
	  disp_size = size_of_uleb128 (disp);
	  pad = before_disp + disp_size + after_disp;
	  if (pad % tt_format_size)
	    pad = tt_format_size - (pad % tt_format_size);
	  else
	    pad = 0;
	  disp = after_disp + pad;
	}
      while (disp != last_disp);

      dw2_asm_output_data_uleb128 (disp, "@TType base offset");
#endif
    }

  /* Indicate the format of the call-site offsets.  */
#ifdef HAVE_AS_LEB128
  cs_format = DW_EH_PE_uleb128;
#else
  cs_format = DW_EH_PE_udata4;
#endif
  dw2_asm_output_data (1, cs_format, "call-site format (%s)",
		       eh_data_format_name (cs_format));

#ifdef HAVE_AS_LEB128
  ASM_GENERATE_INTERNAL_LABEL (cs_after_size_label, "LLSDACSB",
			       funcdef_number);
  ASM_GENERATE_INTERNAL_LABEL (cs_end_label, "LLSDACSE",
			       funcdef_number);
  dw2_asm_output_delta_uleb128 (cs_end_label, cs_after_size_label,
				"Call-site table length");
  ASM_OUTPUT_LABEL (asm_out_file, cs_after_size_label);
  if (USING_SJLJ_EXCEPTIONS)
    sjlj_output_call_site_table ();
  else
    dw2_output_call_site_table ();
  ASM_OUTPUT_LABEL (asm_out_file, cs_end_label);
#else
  dw2_asm_output_data_uleb128 (call_site_len,"Call-site table length");
  if (USING_SJLJ_EXCEPTIONS)
    sjlj_output_call_site_table ();
  else
    dw2_output_call_site_table ();
#endif

  /* ??? Decode and interpret the data for flag_debug_asm.  */
  n = VARRAY_ACTIVE_SIZE (cfun->eh->action_record_data);
  for (i = 0; i < n; ++i)
    dw2_asm_output_data (1, VARRAY_UCHAR (cfun->eh->action_record_data, i),
			 (i ? NULL : "Action record table"));

  if (have_tt_data)
    assemble_eh_align (tt_format_size * BITS_PER_UNIT);

  i = VARRAY_ACTIVE_SIZE (cfun->eh->ttype_data);
  while (i-- > 0)
    {
      tree type = VARRAY_TREE (cfun->eh->ttype_data, i);

      if (type == NULL_TREE)
	type = integer_zero_node;
      else
	type = lookup_type_for_runtime (type);

      dw2_asm_output_encoded_addr_rtx (tt_format,
				       expand_expr (type, NULL_RTX, VOIDmode,
						    EXPAND_INITIALIZER),
				       NULL);
    }

#ifdef HAVE_AS_LEB128
  if (have_tt_data)
      ASM_OUTPUT_LABEL (asm_out_file, ttype_label);
#endif

  /* ??? Decode and interpret the data for flag_debug_asm.  */
  n = VARRAY_ACTIVE_SIZE (cfun->eh->ehspec_data);
  for (i = 0; i < n; ++i)
    dw2_asm_output_data (1, VARRAY_UCHAR (cfun->eh->ehspec_data, i),
			 (i ? NULL : "Exception specification table"));

  function_section (current_function_decl);

  if (USING_SJLJ_EXCEPTIONS)
    sjlj_funcdef_number += 1;
}
