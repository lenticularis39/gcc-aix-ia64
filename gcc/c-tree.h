/* Definitions for C parsing and type checking.
   Copyright (C) 1987, 1993, 1994, 1995, 1997, 1998,
   1999, 2000 Free Software Foundation, Inc.

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

#ifndef _C_TREE_H
#define _C_TREE_H

#include "c-common.h"

/* Language-dependent contents of an identifier.  */

/* The limbo_value is used for block level extern declarations, which need
   to be type checked against subsequent extern declarations.  They can't
   be referenced after they fall out of scope, so they can't be global.

   The rid_code field is used for keywords.  It is in all
   lang_identifier nodes, because some keywords are only special in a
   particular context.  */

struct lang_identifier
{
  struct tree_identifier ignore;
  tree global_value, local_value, label_value, implicit_decl;
  tree error_locus, limbo_value;
  enum rid rid_code;
};

/* Language-specific declaration information.  */

struct lang_decl
{
  struct c_lang_decl base;
  /* The return types and parameter types may have variable size.
     This is a list of any SAVE_EXPRs that need to be evaluated to
     compute those sizes.  */
  tree pending_sizes;
};

/* Macros for access to language-specific slots in an identifier.  */
/* Each of these slots contains a DECL node or null.  */

/* This represents the value which the identifier has in the
   file-scope namespace.  */
#define IDENTIFIER_GLOBAL_VALUE(NODE)	\
  (((struct lang_identifier *) (NODE))->global_value)
/* This represents the value which the identifier has in the current
   scope.  */
#define IDENTIFIER_LOCAL_VALUE(NODE)	\
  (((struct lang_identifier *) (NODE))->local_value)
/* This represents the value which the identifier has as a label in
   the current label scope.  */
#define IDENTIFIER_LABEL_VALUE(NODE)	\
  (((struct lang_identifier *) (NODE))->label_value)
/* This records the extern decl of this identifier, if it has had one
   at any point in this compilation.  */
#define IDENTIFIER_LIMBO_VALUE(NODE)	\
  (((struct lang_identifier *) (NODE))->limbo_value)
/* This records the implicit function decl of this identifier, if it
   has had one at any point in this compilation.  */
#define IDENTIFIER_IMPLICIT_DECL(NODE)	\
  (((struct lang_identifier *) (NODE))->implicit_decl)
/* This is the last function in which we printed an "undefined variable"
   message for this identifier.  Value is a FUNCTION_DECL or null.  */
#define IDENTIFIER_ERROR_LOCUS(NODE)	\
  (((struct lang_identifier *) (NODE))->error_locus)

/* In identifiers, C uses the following fields in a special way:
   TREE_PUBLIC        to record that there was a previous local extern decl.
   TREE_USED          to record that such a decl was used.
   TREE_ADDRESSABLE   to record that the address of such a decl was used.  */

/* Nonzero means reject anything that ANSI standard C forbids.  */
extern int pedantic;

/* In a RECORD_TYPE or UNION_TYPE, nonzero if any component is read-only.  */
#define C_TYPE_FIELDS_READONLY(type) TREE_LANG_FLAG_1 (type)

/* In a RECORD_TYPE or UNION_TYPE, nonzero if any component is volatile.  */
#define C_TYPE_FIELDS_VOLATILE(type) TREE_LANG_FLAG_2 (type)

/* In a RECORD_TYPE or UNION_TYPE or ENUMERAL_TYPE
   nonzero if the definition of the type has already started.  */
#define C_TYPE_BEING_DEFINED(type) TYPE_LANG_FLAG_0 (type)

/* In an IDENTIFIER_NODE, nonzero if this identifier is actually a
   keyword.  C_RID_CODE (node) is then the RID_* value of the keyword,
   and C_RID_YYCODE is the token number wanted by Yacc.  */

#define C_IS_RESERVED_WORD(id) TREE_LANG_FLAG_0 (id)
#define C_RID_CODE(id) \
  (((struct lang_identifier *) (id))->rid_code)

/* In a RECORD_TYPE, a sorted array of the fields of the type.  */
struct lang_type
{
  int len;
  tree elts[1];
};

/* Record whether a type or decl was written with nonconstant size.
   Note that TYPE_SIZE may have simplified to a constant.  */
#define C_TYPE_VARIABLE_SIZE(type) TYPE_LANG_FLAG_1 (type)
#define C_DECL_VARIABLE_SIZE(type) DECL_LANG_FLAG_0 (type)

#if 0 /* Not used.  */
/* Record whether a decl for a function or function pointer has
   already been mentioned (in a warning) because it was called
   but didn't have a prototype.  */
#define C_MISSING_PROTOTYPE_WARNED(decl) DECL_LANG_FLAG_2(decl)
#endif

/* Store a value in that field.  */
#define C_SET_EXP_ORIGINAL_CODE(exp, code) \
  (TREE_COMPLEXITY (exp) = (int) (code))

/* Record whether a typedef for type `int' was actually `signed int'.  */
#define C_TYPEDEF_EXPLICITLY_SIGNED(exp) DECL_LANG_FLAG_1 ((exp))

/* Nonzero for a declaration of a built in function if there has been no
   occasion that would declare the function in ordinary C.
   Using the function draws a pedantic warning in this case.  */
#define C_DECL_ANTICIPATED(exp) DECL_LANG_FLAG_3 ((exp))

/* For FUNCTION_TYPE, a hidden list of types of arguments.  The same as
   TYPE_ARG_TYPES for functions with prototypes, but created for functions
   without prototypes.  */
#define TYPE_ACTUAL_ARG_TYPES(NODE) TYPE_NONCOPIED_PARTS (NODE)


/* in c-lang.c and objc-act.c */
extern tree lookup_interface			PARAMS ((tree));
extern tree is_class_name			PARAMS ((tree));
extern void maybe_objc_check_decl		PARAMS ((tree));
extern void finish_file				PARAMS ((void));
extern int maybe_objc_comptypes                 PARAMS ((tree, tree, int));
extern tree maybe_building_objc_message_expr    PARAMS ((void));
extern tree maybe_objc_method_name		PARAMS ((tree));
extern int recognize_objc_keyword		PARAMS ((void));
extern tree lookup_objc_ivar			PARAMS ((tree));

/* in c-parse.in */
extern void c_parse_init			PARAMS ((void));
extern int yyparse_1				PARAMS ((void));

/* in c-aux-info.c */
extern void gen_aux_info_record                 PARAMS ((tree, int, int, int));

/* in c-convert.c */
extern tree convert                             PARAMS ((tree, tree));

/* in c-decl.c */
extern tree build_enumerator                    PARAMS ((tree, tree));

#define c_build_type_variant(TYPE, CONST_P, VOLATILE_P)		  \
  c_build_qualified_type (TYPE, 				  \
			  ((CONST_P) ? TYPE_QUAL_CONST : 0) |	  \
			  ((VOLATILE_P) ? TYPE_QUAL_VOLATILE : 0))
extern int  c_decode_option                     PARAMS ((int, char **));
extern void c_mark_varargs                      PARAMS ((void));
extern void check_for_loop_decls                PARAMS ((void));
extern tree check_identifier                    PARAMS ((tree, tree));
extern void clear_parm_order                    PARAMS ((void));
extern tree combine_parm_decls                  PARAMS ((tree, tree, int));
extern int  complete_array_type                 PARAMS ((tree, tree, int));
extern void declare_parm_level                  PARAMS ((int));
extern tree define_label                        PARAMS ((const char *, int,
							 tree));
extern void delete_block                        PARAMS ((tree));
extern void finish_decl                         PARAMS ((tree, tree, tree));
extern void finish_decl_top_level               PARAMS ((tree, tree, tree));
extern tree finish_enum                         PARAMS ((tree, tree, tree));
extern void finish_function                     PARAMS ((int));
extern tree finish_struct                       PARAMS ((tree, tree, tree));
extern tree get_parm_info                       PARAMS ((int));
extern tree getdecls                            PARAMS ((void));
extern tree gettags                             PARAMS ((void));
extern int  global_bindings_p                   PARAMS ((void));
extern tree grokfield                           PARAMS ((const char *, int, tree, tree, tree));
extern tree groktypename                        PARAMS ((tree));
extern tree groktypename_in_parm_context        PARAMS ((tree));
extern tree implicitly_declare                  PARAMS ((tree));
extern void implicit_decl_warning               PARAMS ((tree));
extern int  in_parm_level_p                     PARAMS ((void));
extern void init_decl_processing                PARAMS ((void));
extern void insert_block                        PARAMS ((tree));
extern void keep_next_level                     PARAMS ((void));
extern int  kept_level_p                        PARAMS ((void));
extern tree lookup_label                        PARAMS ((tree));
extern tree lookup_name                         PARAMS ((tree));
extern tree lookup_name_current_level		PARAMS ((tree));
extern tree lookup_name_current_level_global	PARAMS ((tree));
extern tree maybe_build_cleanup                 PARAMS ((tree));
extern void parmlist_tags_warning               PARAMS ((void));
extern void pending_xref_error                  PARAMS ((void));
extern void mark_c_function_context             PARAMS ((struct function *));
extern void push_c_function_context             PARAMS ((struct function *));
extern void pop_c_function_context              PARAMS ((struct function *));
extern void pop_label_level                     PARAMS ((void));
extern tree poplevel                            PARAMS ((int, int, int));
extern void print_lang_decl                     PARAMS ((FILE *, tree, int));
extern void print_lang_identifier               PARAMS ((FILE *, tree, int));
extern void print_lang_type                     PARAMS ((FILE *, tree, int));
extern void push_label_level                    PARAMS ((void));
extern void push_parm_decl                      PARAMS ((tree));
extern tree pushdecl                            PARAMS ((tree));
extern tree pushdecl_top_level                  PARAMS ((tree));
extern void pushlevel                           PARAMS ((int));
extern void pushtag                             PARAMS ((tree, tree));
extern void set_block                           PARAMS ((tree));
extern tree shadow_label                        PARAMS ((tree));
extern void shadow_record_fields                PARAMS ((tree));
extern void shadow_tag                          PARAMS ((tree));
extern void shadow_tag_warned                   PARAMS ((tree, int));
extern tree start_enum                          PARAMS ((tree));
extern int  start_function                      PARAMS ((tree, tree, tree,
							 tree));
extern tree start_decl                          PARAMS ((tree, tree, int,
							 tree, tree));
extern tree start_struct                        PARAMS ((enum tree_code, tree));
extern void store_parm_decls                    PARAMS ((void));
extern tree xref_tag                            PARAMS ((enum tree_code, tree));
extern tree c_begin_compound_stmt               PARAMS ((void));
extern void c_expand_decl_stmt                  PARAMS ((tree));

/* in c-typeck.c */
extern tree require_complete_type		PARAMS ((tree));
extern void incomplete_type_error		PARAMS ((tree, tree));
extern int comptypes				PARAMS ((tree, tree));
extern tree c_sizeof                            PARAMS ((tree));
extern tree c_sizeof_nowarn                     PARAMS ((tree));
extern tree c_size_in_bytes                     PARAMS ((tree));
extern tree c_alignof				PARAMS ((tree));
extern tree c_alignof_expr			PARAMS ((tree));
extern tree build_component_ref                 PARAMS ((tree, tree));
extern tree build_indirect_ref                  PARAMS ((tree, const char *));
extern tree build_array_ref                     PARAMS ((tree, tree));
extern tree build_external_ref			PARAMS ((tree, int));
extern tree build_function_call                 PARAMS ((tree, tree));
extern tree parser_build_binary_op              PARAMS ((enum tree_code,
							 tree, tree));
extern int lvalue_or_else			PARAMS ((tree, const char *));
extern void readonly_warning			PARAMS ((tree, const char *));
extern int mark_addressable			PARAMS ((tree));
extern tree build_conditional_expr              PARAMS ((tree, tree, tree));
extern tree build_compound_expr                 PARAMS ((tree));
extern tree build_c_cast                        PARAMS ((tree, tree));
extern tree build_modify_expr                   PARAMS ((tree, enum tree_code,
							 tree));
extern void store_init_value                    PARAMS ((tree, tree));
extern void error_init				PARAMS ((const char *));
extern void pedwarn_init			PARAMS ((const char *));
extern void start_init				PARAMS ((tree, tree, int));
extern void finish_init				PARAMS ((void));
extern void really_start_incremental_init	PARAMS ((tree));
extern void push_init_level			PARAMS ((int));
extern tree pop_init_level			PARAMS ((int));
extern void set_init_index			PARAMS ((tree, tree));
extern void set_init_label			PARAMS ((tree));
extern void process_init_element		PARAMS ((tree));
extern void pedwarn_c99				PARAMS ((const char *, ...))
							ATTRIBUTE_PRINTF_1;
extern tree c_start_case                        PARAMS ((tree));
extern void c_finish_case                       PARAMS ((void));
extern tree build_asm_stmt			PARAMS ((tree, tree, tree,
							 tree, tree));

/* Set to 0 at beginning of a function definition, set to 1 if
   a return statement that specifies a return value is seen.  */

extern int current_function_returns_value;

/* Set to 0 at beginning of a function definition, set to 1 if
   a return statement with no argument is seen.  */

extern int current_function_returns_null;

/* Nonzero means the expression being parsed will never be evaluated.
   This is a count, since unevaluated expressions can nest.  */

extern int skip_evaluation;

/* Nonzero means `$' can be in an identifier.  */

extern int dollars_in_ident;

/* Nonzero means allow type mismatches in conditional expressions;
   just make their values `void'.   */

extern int flag_cond_mismatch;

/* Nonzero means don't recognize the keyword `asm'.  */

extern int flag_no_asm;

/* Nonzero means warn about implicit declarations.  */

extern int warn_implicit;

/* Nonzero means warn about sizeof (function) or addition/subtraction
   of function pointers.  */

extern int warn_pointer_arith;

/* Nonzero means warn for all old-style non-prototype function decls.  */

extern int warn_strict_prototypes;

/* Nonzero means warn about multiple (redundant) decls for the same single
   variable or function.  */

extern int warn_redundant_decls;

/* Nonzero means warn about extern declarations of objects not at
   file-scope level and about *all* declarations of functions (whether
   extern or static) not at file-scope level.  Note that we exclude
   implicit function declarations.  To get warnings about those, use
   -Wimplicit.  */

extern int warn_nested_externs;

/* Nonzero means warn about pointer casts that can drop a type qualifier
   from the pointer target type.  */

extern int warn_cast_qual;

/* Nonzero means warn when casting a function call to a type that does
   not match the return type (e.g. (float)sqrt() or (anything*)malloc()
   when there is no previous declaration of sqrt or malloc.  */

extern int warn_bad_function_cast;

/* Warn about traditional constructs whose meanings changed in ANSI C.  */

extern int warn_traditional;

/* Warn about a subscript that has type char.  */

extern int warn_char_subscripts;

/* Warn if main is suspicious. */

extern int warn_main;

/* Nonzero means to allow single precision math even if we're generally
   being traditional. */
extern int flag_allow_single_precision;

/* Warn if initializer is not completely bracketed.  */

extern int warn_missing_braces;

/* Warn about comparison of signed and unsigned values.  */

extern int warn_sign_compare;

/* Warn about testing equality of floating point numbers. */

extern int warn_float_equal;

/* Warn about multicharacter constants.  */

extern int warn_multichar;

/* Nonzero means we are reading code that came from a system header file.  */

extern int system_header_p;

/* Warn about implicit declarations.  1 = warning, 2 = error.  */
extern int mesg_implicit_function_declaration;

/* Nonzero enables objc features.  */

#define doing_objc_thang \
  (c_language == clk_objective_c)

/* In c-decl.c */
extern void finish_incomplete_decl PARAMS ((tree));

#endif /* not _C_TREE_H */
