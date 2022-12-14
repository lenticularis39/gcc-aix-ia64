dnl
dnl Initialize configure bits.
dnl
dnl GLIBCPP_CONFIGURE
AC_DEFUN(GLIBCPP_CONFIGURE, [
  dnl Default to --enable-multilib
  AC_ARG_ENABLE(multilib,
  [  --enable-multilib       build hella library versions (default)],
  [case "${enableval}" in
    yes) multilib=yes ;;
    no)  multilib=no ;;
    *)   AC_MSG_ERROR(bad value ${enableval} for multilib option) ;;
   esac], [multilib=yes])dnl

  glibcpp_basedir=$srcdir/$toprel/$1/libstdc++-v3
  AC_SUBST(glibcpp_basedir)

  AM_INIT_AUTOMAKE(libstdc++, 3.0.0)

  # Never versions of autoconf add an underscore to these functions.
  # Prevent future problems ...
  ifdef([AC_PROG_CC_G],[],[define([AC_PROG_CC_G],defn([_AC_PROG_CC_G]))])
  ifdef([AC_PROG_CC_GNU],[],[define([AC_PROG_CC_GNU],defn([_AC_PROG_CC_GNU]))])
  ifdef([AC_PROG_CXX_G],[],[define([AC_PROG_CXX_G],defn([_AC_PROG_CXX_G]))])
  ifdef([AC_PROG_CXX_GNU],[],[define([AC_PROG_CXX_GNU],defn([_AC_PROG_CXX_GNU]))])

#  AC_PROG_CC

# FIXME: We temporarily define our own version of AC_PROG_CC.  This is
# copied from autoconf 2.12, but does not call AC_PROG_CC_WORKS.  We
# are probably using a cross compiler, which will not be able to fully
# link an executable.  This should really be fixed in autoconf
# itself.

AC_DEFUN(LIB_AC_PROG_CC,
[AC_BEFORE([$0], [AC_PROG_CPP])dnl
dnl Fool anybody using AC_PROG_CC.
AC_PROVIDE([AC_PROG_CC])
AC_CHECK_PROG(CC, gcc, gcc)
if test -z "$CC"; then
  AC_CHECK_PROG(CC, cc, cc, , , /usr/ucb/cc)
  test -z "$CC" && AC_MSG_ERROR([no acceptable cc found in \$PATH])
fi

AC_PROG_CC_GNU

if test $ac_cv_prog_gcc = yes; then
  GCC=yes
dnl Check whether -g works, even if CFLAGS is set, in case the package
dnl plays around with CFLAGS (such as to build both debugging and
dnl normal versions of a library), tasteless as that idea is.
  ac_test_CFLAGS="${CFLAGS+set}"
  ac_save_CFLAGS="$CFLAGS"
  CFLAGS=
  AC_PROG_CC_G
  if test "$ac_test_CFLAGS" = set; then
    CFLAGS="$ac_save_CFLAGS"
  elif test $ac_cv_prog_cc_g = yes; then
    CFLAGS="-g -O2"
  else
    CFLAGS="-O2"
  fi
else
  GCC=
  test "${CFLAGS+set}" = set || CFLAGS="-g"
fi
])

LIB_AC_PROG_CC

# Can't just call these here as g++ requires libstc++ to be built....
#  AC_PROG_CXX

# Likewise for AC_PROG_CXX.
AC_DEFUN(LIB_AC_PROG_CXX,
[AC_BEFORE([$0], [AC_PROG_CXXCPP])dnl
dnl Fool anybody using AC_PROG_CXX.
AC_PROVIDE([AC_PROG_CXX])
# Use glibcpp_CXX so that we do not cause CXX to be cached with the
# flags that come in CXX while configuring libstdc++.  They're different
# from those used for all other target libraries.  If CXX is set in
# the environment, respect that here.
glibcpp_CXX=$CXX
AC_CHECK_PROGS(glibcpp_CXX, $CCC c++ g++ gcc CC cxx cc++, gcc)
AC_SUBST(glibcpp_CXX)
CXX=$glibcpp_CXX
test -z "$glibcpp_CXX" && AC_MSG_ERROR([no acceptable c++ found in \$PATH])

AC_PROG_CXX_GNU

if test $ac_cv_prog_gxx = yes; then
  GXX=yes
dnl Check whether -g works, even if CXXFLAGS is set, in case the package
dnl plays around with CXXFLAGS (such as to build both debugging and
dnl normal versions of a library), tasteless as that idea is.
  ac_test_CXXFLAGS="${CXXFLAGS+set}"
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS=
  AC_PROG_CXX_G
  if test "$ac_test_CXXFLAGS" = set; then
    CXXFLAGS="$ac_save_CXXFLAGS"
  elif test $ac_cv_prog_cxx_g = yes; then
    CXXFLAGS="-g -O2"
  else
    CXXFLAGS="-O2"
  fi
else
  GXX=
  test "${CXXFLAGS+set}" = set || CXXFLAGS="-g"
fi
])

LIB_AC_PROG_CXX

  AC_CHECK_TOOL(AS, as)
  AC_CHECK_TOOL(AR, ar)
  AC_CHECK_TOOL(RANLIB, ranlib, ranlib-not-found-in-path-error)
  AC_PROG_INSTALL

  AM_MAINTAINER_MODE

  # We need AC_EXEEXT to keep automake happy in cygnus mode.  However,
  # at least currently, we never actually build a program, so we never
  # need to use $(EXEEXT).  Moreover, the test for EXEEXT normally
  # fails, because we are probably configuring with a cross compiler
  # which cant create executables.  So we include AC_EXEEXT to keep
  # automake happy, but we dont execute it, since we dont care about
  # the result.
  if false; then
    # autoconf 2.50 runs AC_EXEEXT by default, and the macro expands
    # to nothing, so nothing would remain between `then' and `fi' if it
    # were not for the `:' below.
    :
    AC_EXEEXT
  fi

  . [$]{glibcpp_basedir}/configure.host

  case [$]{glibcpp_basedir} in
    /* | [A-Za-z]:[\\/]*) libgcj_flagbasedir=[$]{glibcpp_basedir} ;;
    *) glibcpp_flagbasedir='[$](top_builddir)/'[$]{glibcpp_basedir} ;;
  esac

  # This does for the target what configure.host does for the host.  In
  # addition to possibly modifying the same flags, it also sets up symlinks.
  GLIBCPP_CHECK_TARGET
])


dnl
dnl Check to see if g++ can compile this library, and if so, if any version-
dnl specific precautions need to be taken. 
dnl 
dnl GLIBCPP_CHECK_COMPILER_VERSION
AC_DEFUN(GLIBCPP_CHECK_COMPILER_VERSION, [
if test ! -f stamp-sanity-compiler; then
  AC_MSG_CHECKING([for g++ that will successfully compile libstdc++-v3])
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE(, [
  #if __GNUC__ < 3
    not_ok
  #endif
  ], gpp_satisfactory=yes, AC_MSG_ERROR([please upgrade to GCC 3.0 or above]))
  AC_LANG_RESTORE
  AC_MSG_RESULT($gpp_satisfactory)
  touch stamp-sanity-compiler
fi
])


dnl
dnl Tests for newer compiler features, or features that are present in newer
dnl compiler versions but not older compiler versions still in use, should
dnl be placed here.
dnl
dnl Define WERROR='-Werror' if requested and possible; g++'s that lack the
dnl new inlining code or the new system_header pragma will die on -Werror.
dnl Leave it out by default and use maint-mode to use it.
dnl
dnl Define SECTION_FLAGS='-ffunction-sections -fdata-sections' if
dnl compiler supports it and the user has not requested debug mode.
dnl
dnl GLIBCPP_CHECK_COMPILER_FEATURES
AC_DEFUN(GLIBCPP_CHECK_COMPILER_FEATURES, [
  # All these tests are for C++; save the language and the compiler flags.
  # The CXXFLAGS thing is suspicious, but based on similar bits previously
  # found in GLIBCPP_CONFIGURE.
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_test_CXXFLAGS="${CXXFLAGS+set}"
  ac_save_CXXFLAGS="$CXXFLAGS"

  # Check for maintainer-mode bits.
  if test x"$USE_MAINTAINER_MODE" = xno; then
    WERROR=''
  else
    WERROR='-Werror'
  fi

  # Check for -ffunction-sections -fdata-sections
  AC_MSG_CHECKING([for g++ that supports -ffunction-sections -fdata-sections])
  CXXFLAGS='-Werror -ffunction-sections -fdata-sections'
  AC_TRY_COMPILE(, [int foo;
  ], [ac_fdsections=yes], [ac_fdsections=no])
  if test "$ac_test_CXXFLAGS" = set; then
    CXXFLAGS="$ac_save_CXXFLAGS"
  else
    # this is the suspicious part
    CXXFLAGS=''
  fi
  if test x"$ac_fdsections" = x"yes" && test x"$enable_debug" = x"no"; then
    SECTION_FLAGS='-ffunction-sections -fdata-sections'
  fi
  AC_MSG_RESULT($ac_fdsections)

  AC_LANG_RESTORE
  AC_SUBST(WERROR)
  AC_SUBST(SECTION_FLAGS)
])


dnl
dnl If GNU ld is in use, check to see if tricky linker opts can be used.  If
dnl the native linker is in use, all variables will be defined to something
dnl safe (like an empty string).
dnl
dnl Define SECTION_LDFLAGS='-Wl,--gc-sections' if possible.
dnl Define OPT_LDFLAGS='-Wl,-O1' if possible.
dnl
dnl GLIBCPP_CHECK_LINKER_FEATURES
AC_DEFUN(GLIBCPP_CHECK_LINKER_FEATURES, [
  # If we're not using GNU ld, then there's no point in even trying these
  # tests.  Check for that first.  We should have already tested for gld
  # by now (in libtool), but require it now just to be safe...
  SECTION_LDFLAGS=''
  OPT_LDFLAGS=''
  AC_REQUIRE([AC_PROG_LD])

  # Set --gc-sections.
  if test "$ac_cv_prog_gnu_ld" = "notbroken"; then
    # GNU ld it is!  Joy and bunny rabbits!

    # All these tests are for C++; save the language and the compiler flags.
    # Need to do this so that g++ won't try to link in libstdc++
    ac_test_CFLAGS="${CFLAGS+set}"
    ac_save_CFLAGS="$CFLAGS"
    CFLAGS='-x c++  -Wl,--gc-sections'

    # Check for -Wl,--gc-sections
    # XXX This test is broken at the moment, as symbols required for
    # linking are now in libsupc++ (not built yet.....). In addition, 
    # this test has cored on solaris in the past. In addition,
    # --gc-sections doesn't really work at the moment (keeps on discarding
    # used sections, first .eh_frame and now some of the glibc sections for
    # iconv). Bzzzzt. Thanks for playing, maybe next time.
    AC_MSG_CHECKING([for ld that supports -Wl,--gc-sections])
    AC_TRY_RUN([
     int main(void) 
     {
       try { throw 1; }
       catch (...) { };
       return 0;
     }
    ], [ac_sectionLDflags=yes],[ac_sectionLFflags=no], [ac_sectionLDflags=yes])
    if test "$ac_test_CFLAGS" = set; then
      CFLAGS="$ac_save_CFLAGS"
    else
      # this is the suspicious part
      CFLAGS=''
    fi
    if test "$ac_sectionLDflags" = "yes"; then
      SECTION_LDFLAGS='-Wl,--gc-sections'
    fi
    AC_MSG_RESULT($ac_sectionLDflags)
  fi

  # Set linker optimization flags.
  if test x"$ac_cv_prog_gnu_ld" = x"yes" && test x"$enable_debug" = x"no"; then
    OPT_LDFLAGS='-Wl,-O1'
  fi

  AC_SUBST(SECTION_LDFLAGS)
  AC_SUBST(OPT_LDFLAGS)
])


dnl
dnl Check to see if the (math function) argument passed is
dnl declared when using the c++ compiler
dnl ASSUMES argument is a math function with ONE parameter
dnl
dnl GLIBCPP_CHECK_MATH_DECL_1
AC_DEFUN(GLIBCPP_CHECK_MATH_DECL_1, [
  AC_MSG_CHECKING([for $1 declaration])
  if test x${glibcpp_cv_func_$1_use+set} != xset; then
    AC_CACHE_VAL(glibcpp_cv_func_$1_use, [
      AC_LANG_SAVE
      AC_LANG_CPLUSPLUS
      AC_TRY_COMPILE([#include <math.h>
		      #ifdef HAVE_IEEEFP_H
		      #include <ieeefp.h>
		      #endif
		     ], 
                     [ $1(0);], 
                     [glibcpp_cv_func_$1_use=yes], [glibcpp_cv_func_$1_use=no])
      AC_LANG_RESTORE
    ])
  fi
  AC_MSG_RESULT($glibcpp_cv_func_$1_use)
])

dnl
dnl Check to see if the (math function) argument passed is
dnl 1) declared when using the c++ compiler
dnl 2) has "C" linkage
dnl 3) if not, see if 1) and 2) for argument prepended with '_'
dnl
dnl Define HAVE_CARGF etc if "cargf" is declared and links
dnl
dnl argument 1 is name of function to check
dnl
dnl ASSUMES argument is a math function with ONE parameter
dnl
dnl GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1
AC_DEFUN(GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1, [
  GLIBCPP_CHECK_MATH_DECL_1($1)
  if test x$glibcpp_cv_func_$1_use = x"yes"; then
    AC_CHECK_FUNCS($1)    
  else
    GLIBCPP_CHECK_MATH_DECL_1(_$1)
    if test x$glibcpp_cv_func__$1_use = x"yes"; then
      AC_CHECK_FUNCS(_$1)    
    fi	
  fi
])


dnl
dnl Like GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1, but does a bunch of
dnl of functions at once.  It's an all-or-nothing check -- either 
dnl HAVE_XYZ is defined for each of the functions, or for none of them.
dnl Doing it this way saves significant configure time.
AC_DEFUN(GLIBCPP_CHECK_MATH_DECLS_AND_LINKAGES_1, [
  AC_MSG_CHECKING([for $1 functions])
  AC_CACHE_VAL(glibcpp_cv_func_$2_use, [
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    AC_TRY_COMPILE([#include <math.h>],
                   [ `for x in $3; do echo "$x (0);"; done` ],
                   [glibcpp_cv_func_$2_use=yes],
                   [glibcpp_cv_func_$2_use=no])
    AC_LANG_RESTORE])
  AC_MSG_RESULT($glibcpp_cv_func_$2_use)
  if test x$glibcpp_cv_func_$2_use = x"yes"; then
    AC_CHECK_FUNCS($3)
  fi
])

dnl
dnl Check to see if the (math function) argument passed is
dnl declared when using the c++ compiler
dnl ASSUMES argument is a math function with TWO parameters
dnl
dnl GLIBCPP_CHECK_MATH_DECL_2
AC_DEFUN(GLIBCPP_CHECK_MATH_DECL_2, [
  AC_MSG_CHECKING([for $1 declaration])
  if test x${glibcpp_cv_func_$1_use+set} != xset; then
    AC_CACHE_VAL(glibcpp_cv_func_$1_use, [
      AC_LANG_SAVE
      AC_LANG_CPLUSPLUS
      AC_TRY_COMPILE([#include <math.h>], 
                     [ $1(0, 0);], 
                     [glibcpp_cv_func_$1_use=yes], [glibcpp_cv_func_$1_use=no])
      AC_LANG_RESTORE
    ])
  fi
  AC_MSG_RESULT($glibcpp_cv_func_$1_use)
])

dnl
dnl Check to see if the (math function) argument passed is
dnl 1) declared when using the c++ compiler
dnl 2) has "C" linkage
dnl
dnl Define HAVE_CARGF etc if "cargf" is declared and links
dnl
dnl argument 1 is name of function to check
dnl
dnl ASSUMES argument is a math function with TWO parameters
dnl
dnl GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2
AC_DEFUN(GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2, [
  GLIBCPP_CHECK_MATH_DECL_2($1)
  if test x$glibcpp_cv_func_$1_use = x"yes"; then
    AC_CHECK_FUNCS($1)    
  else
    GLIBCPP_CHECK_MATH_DECL_2(_$1)
    if test x$glibcpp_cv_func__$1_use = x"yes"; then
      AC_CHECK_FUNCS(_$1)    
    fi	
  fi
])


dnl
dnl Check to see if the (math function) argument passed is
dnl declared when using the c++ compiler
dnl ASSUMES argument is a math function with THREE parameters
dnl
dnl GLIBCPP_CHECK_MATH_DECL_3
AC_DEFUN(GLIBCPP_CHECK_MATH_DECL_3, [
  AC_MSG_CHECKING([for $1 declaration])
  if test x${glibcpp_cv_func_$1_use+set} != xset; then
    AC_CACHE_VAL(glibcpp_cv_func_$1_use, [
      AC_LANG_SAVE
      AC_LANG_CPLUSPLUS
      AC_TRY_COMPILE([#include <math.h>], 
                     [ $1(0, 0, 0);], 
                     [glibcpp_cv_func_$1_use=yes], [glibcpp_cv_func_$1_use=no])
      AC_LANG_RESTORE
    ])
  fi
  AC_MSG_RESULT($glibcpp_cv_func_$1_use)
])

dnl
dnl Check to see if the (math function) argument passed is
dnl 1) declared when using the c++ compiler
dnl 2) has "C" linkage
dnl
dnl Define HAVE_CARGF etc if "cargf" is declared and links
dnl
dnl argument 1 is name of function to check
dnl
dnl ASSUMES argument is a math function with THREE parameters
dnl
dnl GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_3
AC_DEFUN(GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_3, [
  GLIBCPP_CHECK_MATH_DECL_3($1)
  if test x$glibcpp_cv_func_$1_use = x"yes"; then
    AC_CHECK_FUNCS($1)    
  else
    GLIBCPP_CHECK_MATH_DECL_3(_$1)
    if test x$glibcpp_cv_func__$1_use = x"yes"; then
      AC_CHECK_FUNCS(_$1)    
    fi	
  fi
])


dnl
dnl Check to see if the (stdlib function) argument passed is
dnl 1) declared when using the c++ compiler
dnl 2) has "C" linkage
dnl
dnl argument 1 is name of function to check
dnl
dnl ASSUMES argument is a math function with TWO parameters
dnl
dnl GLIBCPP_CHECK_STDLIB_DECL_AND_LINKAGE_2
AC_DEFUN(GLIBCPP_CHECK_STDLIB_DECL_AND_LINKAGE_2, [
  AC_MSG_CHECKING([for $1 declaration])
  if test x${glibcpp_cv_func_$1_use+set} != xset; then
    AC_CACHE_VAL(glibcpp_cv_func_$1_use, [
      AC_LANG_SAVE
      AC_LANG_CPLUSPLUS
      AC_TRY_COMPILE([#include <stdlib.h>], 
                     [ $1(0, 0);], 
                     [glibcpp_cv_func_$1_use=yes], [glibcpp_cv_func_$1_use=no])
      AC_LANG_RESTORE
    ])
  fi
  AC_MSG_RESULT($glibcpp_cv_func_$1_use)
  if test x$glibcpp_cv_func_$1_use = x"yes"; then
    AC_CHECK_FUNCS($1)    
  fi
])


dnl
dnl Because the builtins are picky picky picky about the arguments they take, 
dnl do an explict linkage tests here.
dnl Check to see if the (math function) argument passed is
dnl 1) declared when using the c++ compiler
dnl 2) has "C" linkage
dnl
dnl Define HAVE_CARGF etc if "cargf" is declared and links
dnl
dnl argument 1 is name of function to check
dnl
dnl ASSUMES argument is a math function with ONE parameter
dnl
dnl GLIBCPP_CHECK_BUILTIN_MATH_DECL_LINKAGE_1
AC_DEFUN(GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1, [
  AC_MSG_CHECKING([for $1 declaration])
  if test x${glibcpp_cv_func_$1_use+set} != xset; then
    AC_CACHE_VAL(glibcpp_cv_func_$1_use, [
      AC_LANG_SAVE
      AC_LANG_CPLUSPLUS
      AC_TRY_COMPILE([#include <math.h>], 
                     [ $1(0);], 
                     [glibcpp_cv_func_$1_use=yes], [glibcpp_cv_func_$1_use=no])
      AC_LANG_RESTORE
    ])
  fi
  AC_MSG_RESULT($glibcpp_cv_func_$1_use)
  if test x$glibcpp_cv_func_$1_use = x"yes"; then
    AC_MSG_CHECKING([for $1 linkage])
    if test x${glibcpp_cv_func_$1_link+set} != xset; then
      AC_CACHE_VAL(glibcpp_cv_func_$1_link, [
        AC_TRY_LINK([#include <math.h>], 
                    [ $1(0);], 
                    [glibcpp_cv_func_$1_link=yes], [glibcpp_cv_func_$1_link=no])
      ])
    fi
    AC_MSG_RESULT($glibcpp_cv_func_$1_link)
    if test x$glibcpp_cv_func_$1_link = x"yes"; then
      ac_tr_func=HAVE_`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
      AC_DEFINE_UNQUOTED(${ac_tr_func})
    fi
  fi
])


dnl
dnl Check to see what builtin math functions are supported
dnl
dnl check for __builtin_abs
dnl check for __builtin_fabsf
dnl check for __builtin_fabs
dnl check for __builtin_fabl
dnl check for __builtin_labs
dnl check for __builtin_sqrtf
dnl check for __builtin_sqrtl
dnl check for __builtin_fsqrt
dnl check for __builtin_sinf
dnl check for __builtin_sin
dnl check for __builtin_sinl
dnl check for __builtin_cosf
dnl check for __builtin_cos
dnl check for __builtin_cosl
dnl
dnl GLIBCPP_CHECK_BUILTIN_MATH_SUPPORT
AC_DEFUN(GLIBCPP_CHECK_BUILTIN_MATH_SUPPORT, [
  dnl Test for builtin math functions.
  dnl These are made in gcc/c-common.c 
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_abs)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_fabsf)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_fabs)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_fabsl)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_labs)

  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_sqrtf)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_fsqrt)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_sqrtl)

  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_sinf)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_sin)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_sinl)

  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_cosf)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_cos)
  GLIBCPP_CHECK_BUILTIN_MATH_DECL_AND_LINKAGE_1(__builtin_cosl)

  dnl There is, without a doubt, a more elegant way to have these
  dnl names exported so that they won't be stripped out of acconfig.h by
  dnl autoheader. I leave this as an exercise to somebody less frustrated
  dnl than I.... please email the libstdc++ list if you can figure out a
  dnl more elegant approach (see autoconf/acgen.m4 and specifically
  dnl AC_CHECK_FUNC for things to steal.)
  dummyvar=no
  if test x$dummyvar = x"yes"; then
    AC_DEFINE(HAVE___BUILTIN_ABS)
    AC_DEFINE(HAVE___BUILTIN_LABS)
    AC_DEFINE(HAVE___BUILTIN_COS)
    AC_DEFINE(HAVE___BUILTIN_COSF)
    AC_DEFINE(HAVE___BUILTIN_COSL)
    AC_DEFINE(HAVE___BUILTIN_FABS)
    AC_DEFINE(HAVE___BUILTIN_FABSF)
    AC_DEFINE(HAVE___BUILTIN_FABSL)
    AC_DEFINE(HAVE___BUILTIN_SIN)
    AC_DEFINE(HAVE___BUILTIN_SINF)
    AC_DEFINE(HAVE___BUILTIN_SINL)
    AC_DEFINE(HAVE___BUILTIN_FSQRT)
    AC_DEFINE(HAVE___BUILTIN_SQRTF)
    AC_DEFINE(HAVE___BUILTIN_SQRTL)
  fi
])


dnl
dnl Check to see what the underlying c library 
dnl These checks need to do two things: 
dnl 1) make sure the name is declared when using the c++ compiler
dnl 2) make sure the name has "C" linkage
dnl This might seem like overkill but experience has shown that it's not...
dnl
dnl Define HAVE_STRTOLD if "strtold" is declared and links
dnl Define HAVE_STRTOF if "strtof" is declared and links
dnl Define HAVE_DRAND48 if "drand48" is declared and links
dnl
dnl GLIBCPP_CHECK_STDLIB_SUPPORT
AC_DEFUN(GLIBCPP_CHECK_STDLIB_SUPPORT, [
  ac_test_CXXFLAGS="${CXXFLAGS+set}"
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS='-fno-builtins -D_GNU_SOURCE'

  GLIBCPP_CHECK_STDLIB_DECL_AND_LINKAGE_2(strtold)
  AC_CHECK_FUNCS(drand48)

  CXXFLAGS="$ac_save_CXXFLAGS"
])


dnl
dnl Check to see what the underlying c library or math library is like.
dnl These checks need to do two things: 
dnl 1) make sure the name is declared when using the c++ compiler
dnl 2) make sure the name has "C" linkage
dnl This might seem like overkill but experience has shown that it's not...
dnl
dnl Define HAVE_CARGF etc if "cargf" is found.
dnl
dnl GLIBCPP_CHECK_MATH_SUPPORT
AC_DEFUN(GLIBCPP_CHECK_MATH_SUPPORT, [
  ac_test_CXXFLAGS="${CXXFLAGS+set}"
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS='-fno-builtins -D_GNU_SOURCE'

  dnl Check libm
  AC_CHECK_LIB(m, sin, libm="-lm")
  ac_save_LIBS="$LIBS"
  LIBS="$LIBS $libm"

  dnl Check to see if certain C math functions exist.
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(isinf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(isnan)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(finite)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(copysign)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_3(sincos)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(fpclass)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(qfpclass)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(hypot)

  dnl Check to see if basic C math functions have float versions.
  GLIBCPP_CHECK_MATH_DECLS_AND_LINKAGES_1(float trig,
                                          float_trig,
                                          acosf asinf atanf \
                                          cosf sinf tanf \
                                          coshf sinhf tanhf)
  GLIBCPP_CHECK_MATH_DECLS_AND_LINKAGES_1(float round,
                                          float_round,
                                          ceilf floorf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(isnanf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(isinff)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(atan2f)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(fabsf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(fmodf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(frexpf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(hypotf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(ldexpf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(logf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(log10f)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(modff)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(powf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(sqrtf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_3(sincosf)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(finitef)

  dnl Check to see if basic C math functions have long double versions.
  GLIBCPP_CHECK_MATH_DECLS_AND_LINKAGES_1(long double trig,
                                          long_double_trig,
                                          acosl asinl atanl \
                                          cosl sinl tanl \
                                          coshl sinhl tanhl)
  GLIBCPP_CHECK_MATH_DECLS_AND_LINKAGES_1(long double round,
                                          long_double_round,
                                          ceill floorl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(isnanl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(isinfl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(copysignl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(atan2l)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(expl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(fabsl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(fmodl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(frexpl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(hypotl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(ldexpl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(logl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(log10l)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(modfl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_2(powl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(sqrtl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_3(sincosl)
  GLIBCPP_CHECK_MATH_DECL_AND_LINKAGE_1(finitel)

  dnl Some runtimes have these functions with a preceding underscore. Please
  dnl keep this sync'd with the one above. And if you add any new symbol,
  dnl please add the corresponding block in the @BOTTOM@ section of acconfig.h.
  dnl Check to see if certain C math functions exist.

  dnl Check to see if basic C math functions have float versions.
  GLIBCPP_CHECK_MATH_DECLS_AND_LINKAGES_1(_float trig,
                                          _float_trig,
                                          _acosf _asinf _atanf \
                                          _cosf _sinf _tanf \
                                          _coshf _sinhf _tanhf)
  GLIBCPP_CHECK_MATH_DECLS_AND_LINKAGES_1(_float round,
                                          _float_round,
                                          _ceilf _floorf)

  dnl Check to see if basic C math functions have long double versions.
  GLIBCPP_CHECK_MATH_DECLS_AND_LINKAGES_1(_long double trig,
                                          _long_double_trig,
                                          _acosl _asinl _atanl \
                                          _cosl _sinl _tanl \
                                          _coshl _sinhl _tanhl)
  GLIBCPP_CHECK_MATH_DECLS_AND_LINKAGES_1(_long double round,
                                          _long_double_round,
                                          _ceill _floorl)

  LIBS="$ac_save_LIBS"
  CXXFLAGS="$ac_save_CXXFLAGS"
])


dnl
dnl Check to see if there is native support for complex 
dnl
dnl Don't compile bits in math/* if native support exits.
dnl
dnl Define USE_COMPLEX_LONG_DOUBLE etc if "copysignl" is found.
dnl
dnl GLIBCPP_CHECK_COMPLEX_MATH_SUPPORT
AC_DEFUN(GLIBCPP_CHECK_COMPLEX_MATH_SUPPORT, [
  dnl Check for complex versions of math functions of platform.
  AC_CHECK_LIB(m, main)
  AC_REPLACE_MATHFUNCS(nan copysignf)

  dnl Compile the long double complex functions only if the function 
  dnl provides the non-complex long double functions that are needed.
  dnl Currently this includes copysignl, which should be
  dnl cached from the GLIBCPP_CHECK_MATH_SUPPORT macro, above.
  USE_COMPLEX_LONG_DOUBLE=no
  if test x$ac_cv_func_copysignl = x"yes"; then
    USE_COMPLEX_LONG_DOUBLE=yes
    AC_REPLACE_MATHFUNCS(signbitl)
  fi

  AC_SUBST(USE_COMPLEX_LONG_DOUBLE)
])


dnl Check to see what architecture and operating system we are compiling
dnl for.  Also, if architecture- or OS-specific flags are required for
dnl compilation, pick them up here.
dnl 
dnl GLIBCPP_CHECK_TARGET
AC_DEFUN(GLIBCPP_CHECK_TARGET, [
    . [$]{glibcpp_basedir}/configure.target
    AC_MSG_RESULT(CPU config directory is $cpu_include_dir)
    AC_MSG_RESULT(OS config directory is $os_include_dir)
])


dnl
dnl Check to see if this target can enable the wchar_t parts of libstdc++.
dnl If --disable-c-mbchar was given, no wchar_t stuff is enabled.  (This
dnl must have been previously checked.)
dnl
dnl Define _GLIBCPP_USE_WCHAR_T if all the bits are found 
dnl Define HAVE_MBSTATE_T if mbstate_t is not in wchar.h
dnl
dnl GLIBCPP_CHECK_WCHAR_T_SUPPORT
AC_DEFUN(GLIBCPP_CHECK_WCHAR_T_SUPPORT, [

  dnl Test wchar.h for mbstate_t, which is needed for char_traits and
  dnl others even if wchar_t support is not on.
  AC_MSG_CHECKING([for mbstate_t])
  AC_TRY_COMPILE([#include <wchar.h>],
  [mbstate_t teststate;], 
  have_mbstate_t=yes, have_mbstate_t=no)
  AC_MSG_RESULT($have_mbstate_t)
  if test x"$have_mbstate_t" = xyes; then
    AC_DEFINE(HAVE_MBSTATE_T)
  fi

  dnl Sanity check for existence of ISO C99 headers for extended encoding.
  AC_CHECK_HEADERS(wchar.h, ac_has_wchar_h=yes, ac_has_wchar_h=no)
  AC_CHECK_HEADER(wctype.h, ac_has_wctype_h=yes, ac_has_wctype_h=no)
  
  dnl Only continue checking if the ISO C99 headers exist and support is on.
  if test x"$ac_has_wchar_h" = xyes && test x"$ac_has_wctype_h" = xyes \
     && test x"$enable_c_mbchar" != xno; then
      
    dnl Test wchar.h for WCHAR_MIN, WCHAR_MAX, which is needed before
    dnl numeric_limits can instantiate type_traits<wchar_t>
    AC_MSG_CHECKING([for WCHAR_MIN and WCHAR_MAX])
    AC_TRY_COMPILE([#include <wchar.h>],
    [int i = WCHAR_MIN; int j = WCHAR_MAX;], 
    has_wchar_minmax=yes, has_wchar_minmax=no)
    AC_MSG_RESULT($has_wchar_minmax)
    
    dnl Test wchar.h for WEOF, which is what we use to determine whether
    dnl to specialize for char_traits<wchar_t> or not.
    AC_MSG_CHECKING([for WEOF])
    AC_TRY_COMPILE([
      #include <wchar.h>
      #include <stddef.h>],
    [wint_t i = WEOF;],
    has_weof=yes, has_weof=no)
    AC_MSG_RESULT($has_weof)
  
    dnl Tests for wide character functions used in char_traits<wchar_t>.
    ac_wfuncs=yes
    AC_CHECK_FUNCS(wcslen wmemchr wmemcmp wmemcpy wmemmove wmemset,, \
    ac_wfuncs=no)
  
    dnl Checks for names injected into std:: by the c_std headers.
    AC_CHECK_FUNCS(btowc wctob fgetwc fgetwc fgetws fputwc fputws fwide \
    fwprintf fwscanf swprintf swscanf vfwprintf vfwscanf vswprintf vswscanf \
    vwprintf vwscanf wprintf wscanf getwc getwchar mbsinit mbrlen mbrtowc \
    mbsrtowcs wcsrtombs putwc putwchar ungetwc wcrtomb wcstod wcstof wcstol \
    wcstoul wcscpy wcsncpy wcscat wcsncat wcscmp wcscoll wcsncmp wcsxfrm \
    wcscspn wcsspn wcstok wcsftime wcschr wcspbrk wcsrchr wcsstr,, \
    ac_wfuncs=no)

    AC_MSG_CHECKING([for ISO C99 wchar_t support])
    if test x"$has_weof" = xyes && test x"$has_wchar_minmax" = xyes \
       && test x"$ac_wfuncs" = xyes; then
      ac_isoC99_wchar_t=yes
    else
      ac_isoC99_wchar_t=no
    fi
    AC_MSG_RESULT($ac_isoC99_wchar_t)
  
    dnl Use iconv for wchar_t to char conversions. As such, check for 
    dnl X/Open Portability Guide, version 2 features (XPG2).
    AC_CHECK_HEADER(iconv.h, ac_has_iconv_h=yes, ac_has_iconv_h=no)
    AC_CHECK_HEADER(langinfo.h, ac_has_langinfo_h=yes, ac_has_langinfo_h=no)

    dnl Check for existence of libiconv.a providing XPG2 wchar_t support.
    AC_CHECK_LIB(iconv, iconv, libiconv="-liconv")
    ac_save_LIBS="$LIBS"
    LIBS="$LIBS $libiconv"

    AC_CHECK_FUNCS(iconv_open iconv_close iconv nl_langinfo, \
    ac_XPG2funcs=yes, ac_XPG2funcs=no)
  
    LIBS="$ac_save_LIBS"

    AC_MSG_CHECKING([for XPG2 wchar_t support])
    if test x"$ac_has_iconv_h" = xyes && test x"$ac_has_langinfo_h" = xyes \
       && test x"$ac_XPG2funcs" = xyes; then
      ac_XPG2_wchar_t=yes
    else
      ac_XPG2_wchar_t=no
    fi
    AC_MSG_RESULT($ac_XPG2_wchar_t)
  
    dnl At the moment, only enable wchar_t specializations if all the
    dnl above support is present.
    AC_MSG_CHECKING([for enabled wchar_t specializations])
    if test x"$ac_isoC99_wchar_t" = xyes \
    && test x"$ac_XPG2_wchar_t" = xyes; then
      AC_DEFINE(_GLIBCPP_USE_WCHAR_T)
      AC_MSG_RESULT("yes")
    else
      AC_MSG_RESULT("no")
    fi
  else
    dnl Wide characters disabled by the user. 
    AC_MSG_WARN([wchar_t support disabled.])
  fi
])


dnl
dnl Check for special debugging mode; not for production use.
dnl
dnl GLIBCPP_ENABLE_DEBUG
dnl --enable-debug sets '-ggdb3 -O0'.
dnl --disable-debug sets '-g' and whatever optimization options the
dnl     compiler can handle.
dnl  +  --enable-maintainer-mode automatically defaults this to on.
dnl  +  Perhaps -D/-U of NDEBUG, DEBUG, DEBUG_ASSERT, ...?
dnl  +  Usage:  GLIBCPP_ENABLE_DEBUG[(DEFAULT)]
dnl       Where DEFAULT is either `yes' or `no'.  If ommitted, it
dnl       defaults to `no'.
AC_DEFUN(GLIBCPP_ENABLE_DEBUG, [dnl
define([GLIBCPP_ENABLE_DEBUG_DEFAULT], ifelse($1, yes, yes, no))dnl
AC_ARG_ENABLE(debug,
changequote(<<, >>)dnl
<<  --enable-debug          extra debugging, turn off optimization [default=>>GLIBCPP_ENABLE_DEBUG_DEFAULT],
changequote([, ])dnl
[case "${enableval}" in
 yes) enable_debug=yes ;;
 no)  enable_debug=no ;;
 *)   AC_MSG_ERROR([Unknown argument to enable/disable extra debugging]) ;;
 esac],
enable_debug=GLIBCPP_ENABLE_DEBUG_DEFAULT)dnl

dnl Option parsed, now set things appropriately
case "${enable_debug}" in
    yes) 
        DEBUG_FLAGS='-O0 -ggdb3'                        
        ;; 
    no)   
        DEBUG_FLAGS='-g'
        ;;
esac
AC_SUBST(DEBUG_FLAGS)
])


dnl
dnl Check for "unusual" flags to pass to the compiler while building.
dnl
dnl GLIBCPP_ENABLE_CXX_FLAGS
dnl --enable-cxx-flags='-foo -bar -baz' is a general method for passing
dnl     experimental flags such as -fhonor-std, -fsquangle, -Dfloat=char, etc.
dnl     Somehow this same set of flags must be passed when [re]building
dnl     libgcc.
dnl --disable-cxx-flags passes nothing.
dnl  +  See http://gcc.gnu.org/ml/libstdc++/2000-q2/msg00131.html
dnl         http://gcc.gnu.org/ml/libstdc++/2000-q2/msg00284.html
dnl         http://gcc.gnu.org/ml/libstdc++/2000-q1/msg00035.html
dnl  +  Usage:  GLIBCPP_ENABLE_CXX_FLAGS(default flags)
dnl       If "default flags" is an empty string (or "none"), the effect is
dnl       the same as --disable or --enable=no.
AC_DEFUN(GLIBCPP_ENABLE_CXX_FLAGS, [dnl
define([GLIBCPP_ENABLE_CXX_FLAGS_DEFAULT], ifelse($1,,, $1))dnl
AC_ARG_ENABLE(cxx-flags,
changequote(<<, >>)dnl
<<  --enable-cxx-flags=FLAGS      pass compiler FLAGS when building library;
                                [default=>>GLIBCPP_ENABLE_CXX_FLAGS_DEFAULT],
changequote([, ])dnl
[case "x$enableval" in
 xyes)   
        AC_MSG_ERROR([--enable-cxx-flags needs compiler flags as arguments]) ;;
 xno|x)  
        enable_cxx_flags='' ;;
 *)      
        enable_cxx_flags="$enableval" ;;
 esac],
enable_cxx_flags='GLIBCPP_ENABLE_CXX_FLAGS_DEFAULT')

dnl Thinko on my part during design.  This kludge is the workaround.
if test "$enable_cxx_flags" = "none"; then 
  enable_cxx_flags=''; 
fi

dnl Run through flags (either default or command-line) and set anything
dnl extra (e.g., #defines) that must accompany particular g++ options.
if test -n "$enable_cxx_flags"; then
    for f in $enable_cxx_flags; do
        case "$f" in
            -fhonor-std)  ;;
            -*)  ;;
            *)   # and we're trying to pass /what/ exactly?
                 AC_MSG_ERROR([compiler flags start with a -]) ;;
        esac
    done
fi
EXTRA_CXX_FLAGS="$enable_cxx_flags"
AC_SUBST(EXTRA_CXX_FLAGS)
])


dnl
dnl Check for which locale library to use:  gnu or generic.
dnl
dnl GLIBCPP_ENABLE_CLOCALE
dnl --enable-clocale=gnu sets config/c_locale_gnu.cc and friends
dnl --enable-clocale=generic sets config/c_locale_generic.cc and friends
dnl 
dnl default is generic
dnl
AC_DEFUN(GLIBCPP_ENABLE_CLOCALE, [
  AC_MSG_CHECKING([for clocale to use])
  AC_ARG_ENABLE(clocale,
  [  --enable-clocale        enable model for target locale package. 
  --enable-clocale=MODEL  use MODEL target-speific locale package. [default=generic]
  ], 
  if test x$enable_clocale = xno; then
     enable_clocale=generic
  fi,
     enable_clocale=generic)

  enable_clocale_flag=$enable_clocale

  dnl Check if a valid locale package
  case x${enable_clocale_flag} in
    xgnu)
      CLOCALE_H=config/c_locale_gnu.h
      CLOCALE_CC=config/c_locale_gnu.cc
      AC_MSG_RESULT(gnu)
      ;;
    xgeneric)
      CLOCALE_H=config/c_locale_generic.h
      CLOCALE_CC=config/c_locale_generic.cc
      AC_MSG_RESULT(generic)
      ;;
    *)
      echo "$enable_clocale is an unknown locale package" 1>&2
      exit 1
      ;;
  esac

  AC_SUBST(CLOCALE_H)
  AC_LINK_FILES($CLOCALE_CC, src/c++locale.cc)
])


dnl
dnl Check for which I/O library to use:  libio, or something specific.
dnl
dnl GLIBCPP_ENABLE_CSTDIO
dnl --enable-cstdio=libio sets config/c_io_libio.h and friends
dnl 
dnl default is stdio
dnl
AC_DEFUN(GLIBCPP_ENABLE_CSTDIO, [
  AC_MSG_CHECKING([for cstdio to use])
  AC_ARG_ENABLE(cstdio,
  [  --enable-cstdio         enable stdio for target io package. 
  --enable-cstdio=LIB     use LIB target-speific io package. [default=stdio]
  ], 
  if test x$enable_cstdio = xno; then
     enable_cstdio=stdio
  fi,
     enable_cstdio=stdio)

  enable_cstdio_flag=$enable_cstdio

  dnl Check if a valid I/O package
  case x${enable_cstdio_flag} in
    xlibio)
      CSTDIO_H=config/c_io_libio.h
      BASIC_FILE_H=config/basic_file_libio.h
      BASIC_FILE_CC=config/basic_file_libio.cc
      AC_MSG_RESULT(libio)

      # see if we are on a system with libio native (ie, linux)
      AC_CHECK_HEADER(libio.h,  has_libio=yes, has_libio=no)

      # Need to check and see what version of glibc is being used. If
      # it's not glibc-2.2 or higher, then we'll need to go ahead and 
      # compile most of libio for linux systems.
      if test x$has_libio = x"yes"; then
        case "$target" in
          *-*-linux*)
              AC_MSG_CHECKING([for glibc version >= 2.2])
              AC_EGREP_CPP([ok], [
            #include <features.h>
              #if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 2) 
                    ok
              #endif
              ], glibc_satisfactory=yes, glibc_satisfactory=no)
              AC_MSG_RESULT($glibc_satisfactory)
            ;;
        esac

        # XXX at the moment, admit defeat and force the recompilation
        # XXX of glibc even on glibc-2.2 systems, because libio is not synched.
        glibc_satisfactory=no        

        if test x$glibc_satisfactory = x"yes"; then
           need_libio=no
           need_wlibio=no        
        else
           need_libio=yes
           # bkoz XXX need to add checks to enable this
           # pme XXX here's a first pass at such a check
           if test x$enable_c_mbchar != xno; then
              need_wlibio=yes
           else
              need_wlibio=no
           fi
        fi

      else
         # Using libio, but <libio.h> doesn't exist on the target system. . .
         need_libio=yes
         # bkoz XXX need to add checks to enable this
         # pme XXX here's a first pass at such a check
         if test x$enable_c_mbchar != xno; then
             need_wlibio=yes
         else
             need_wlibio=no
         fi
      fi
      ;;
    xstdio | x | xno | xnone | xyes)
      # default
      CSTDIO_H=config/c_io_stdio.h
      BASIC_FILE_H=config/basic_file_stdio.h
      BASIC_FILE_CC=config/basic_file_stdio.cc
      AC_MSG_RESULT(stdio)

      # We're not using stdio.
      need_libio=no
      need_wlibio=no
      ;;
    *)
      echo "$enable_cstdio is an unknown io package" 1>&2
      exit 1
      ;;
  esac
  AC_SUBST(CSTDIO_H)
  AC_SUBST(BASIC_FILE_H)
  AC_LINK_FILES($BASIC_FILE_CC, src/basic_file.cc)

  # 2000-08-04 bkoz hack
  CCODECVT_C=config/c_io_libio_codecvt.c
  AC_SUBST(CCODECVT_C)
  # 2000-08-04 bkoz hack

  AM_CONDITIONAL(GLIBCPP_BUILD_LIBIO,
                 test "$need_libio" = yes || test "$need_wlibio" = yes)
  AM_CONDITIONAL(GLIBCPP_NEED_LIBIO, test "$need_libio" = yes)
  AM_CONDITIONAL(GLIBCPP_NEED_WLIBIO, test "$need_wlibio" = yes)
  if test "$need_libio" = yes || test "$need_wlibio" = yes; then
    libio_la=../libio/libio.la
  else
    libio_la=
  fi
  AC_SUBST(libio_la)
])


dnl
dnl Setup to use the gcc gthr.h thread-specific memory and mutex model.
dnl We must stage the required headers so that they will be installed
dnl with the library (unlike libgcc, the STL implementation is provided
dnl solely within headers).  Since we must not inject random user-space
dnl macro names into user-provided C++ code, we first stage into <file>-in
dnl and process to <file> with an output command.  The reason for a two-
dnl stage process here is to correctly handle $srcdir!=$objdir without
dnl having to write complex code (the sed commands to clean the macro
dnl namespace are complex and fragile enough as it is).  We must also
dnl add a relative path so that -I- is supported properly.
dnl
AC_DEFUN(GLIBCPP_ENABLE_THREADS, [
  AC_MSG_CHECKING([for thread model used by GCC])
  target_thread_file=`$CC -v 2>&1 | sed -n 's/^Thread model: //p'`
  AC_MSG_RESULT([$target_thread_file])

  if test $target_thread_file != single; then
    AC_DEFINE(HAVE_GTHR_DEFAULT)
    AC_DEFINE(_GLIBCPP_SUPPORTS_WEAK, __GXX_WEAK__)
  fi

  glibcpp_thread_h=gthr-$target_thread_file.h
  AC_SUBST(glibcpp_thread_h)
])


dnl
dnl Check for exception handling support.  If an explicit enable/disable
dnl sjlj exceptions is given, we don't have to detect.  Otherwise the
dnl target may or may not support call frame exceptions.
dnl
dnl GLIBCPP_ENABLE_SJLJ_EXCEPTIONS
dnl --enable-sjlj-exceptions forces the use of builtin setjmp.
dnl --disable-sjlj-exceptions forces the use of call frame unwinding.
dnl
dnl Define _GLIBCPP_SJLJ_EXCEPTIONS if the compiler is configured for it.
dnl
AC_DEFUN(GLIBCPP_ENABLE_SJLJ_EXCEPTIONS, [
  AC_MSG_CHECKING([for exception model to use])
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_ARG_ENABLE(sjlj-exceptions,
  [  --enable-sjlj-exceptions  force use of builtin_setjmp for exceptions],
  [:],
  [dnl Botheration.  Now we've got to detect the exception model.
   dnl Link tests against libgcc.a are problematic since -- at least
   dnl as of this writing -- we've not been given proper -L bits for
   dnl single-tree newlib and libgloss.
   dnl
   dnl This is what AC_TRY_COMPILE would do if it didn't delete the
   dnl conftest files before we got a change to grep them first.
   cat > conftest.$ac_ext << EOF
[#]line __oline__ "configure"
struct S { ~S(); };
void bar();
void foo()
{
  S s;
  bar();
}
EOF
   old_CXXFLAGS="$CXXFLAGS"  
   CXXFLAGS=-S
   if AC_TRY_EVAL(ac_compile); then
     if grep _Unwind_SjLj_Resume conftest.s >/dev/null 2>&1 ; then
       enable_sjlj_exceptions=yes
     elif grep _Unwind_Resume conftest.s >/dev/null 2>&1 ; then
       enable_sjlj_exceptions=no
     fi
   fi
   CXXFLAGS="$old_CXXFLAGS"
   rm -f conftest*])
   if test x$enable_sjlj_exceptions = xyes; then
     AC_DEFINE(_GLIBCPP_SJLJ_EXCEPTIONS, 1,
	[Define if the compiler is configured for setjmp/longjmp exceptions.])
     ac_exception_model_name=sjlj
   elif test x$enable_sjlj_exceptions = xno; then
     ac_exception_model_name="call frame"
   else
     AC_MSG_ERROR([unable to detect exception model])
   fi
   AC_LANG_RESTORE
   AC_MSG_RESULT($ac_exception_model_name)
])


dnl
dnl Check for ISO/IEC 9899:1999 "C99" support.
dnl
dnl GLIBCPP_ENABLE_C99
dnl --enable-c99 defines _GLIBCPP_USE_C99
dnl --disable-c99 leaves _GLIBCPP_USE_C99 undefined
dnl  +  Usage:  GLIBCPP_ENABLE_C99[(DEFAULT)]
dnl       Where DEFAULT is either `yes' or `no'.  If omitted, it
dnl       defaults to `no'.
dnl  +  If 'C99' stuff is not available, ignores DEFAULT and sets `no'.
dnl
dnl GLIBCPP_ENABLE_C99
AC_DEFUN(GLIBCPP_ENABLE_C99, [dnl
  define([GLIBCPP_ENABLE_C99_DEFAULT], ifelse($1, yes, yes, no))dnl

  AC_ARG_ENABLE(c99,
  changequote(<<, >>)dnl
  <<--enable-c99      turns on 'ISO/IEC 9899:1999 support' [default=>>GLIBCPP_ENABLE_C99_DEFAULT],
  changequote([, ])dnl
  [case "$enableval" in
   yes) enable_c99=yes ;;
   no)  enable_c99=no ;;
   *)   AC_MSG_ERROR([Unknown argument to enable/disable C99]) ;;
   esac],
  enable_c99=GLIBCPP_ENABLE_C99_DEFAULT)dnl
 
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS

  # Check for the existence of <math.h> functions used if C99 is enabled.
  ac_c99_math=yes;
  AC_MSG_CHECKING([for ISO C99 support in <math.h>])
  AC_TRY_COMPILE([#include <math.h>],[fpclassify(0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],[isfinite(0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],[isinf(0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],[isnan(0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],[isnormal(0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],[signbit(0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],[isgreater(0.0,0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],
                 [isgreaterequal(0.0,0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],[isless(0.0,0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],[islessequal(0.0,0.0);],,[ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],
	         [islessgreater(0.0,0.0);],, [ac_c99_math=no])
  AC_TRY_COMPILE([#include <math.h>],
	         [isunordered(0.0,0.0);],, [ac_c99_math=no])
  AC_MSG_RESULT($ac_c99_math)

  # Check for the existence in <stdio.h> of vscanf, et. al.
  ac_c99_stdio=yes;
  AC_MSG_CHECKING([for ISO C99 support in <stdio.h>])
  AC_TRY_COMPILE([#include <stdio.h>],
		 [snprintf("12", 0, "%i");],, [ac_c99_stdio=no])
  AC_TRY_COMPILE([#include <stdio.h>
		  #include <stdarg.h>
		  void foo(char* fmt, ...)
		  {va_list args; va_start(args, fmt);
	          vfscanf(stderr, "%i", args);}],
	          [],, [ac_c99_stdio=no])	
  AC_TRY_COMPILE([#include <stdio.h>
		  #include <stdarg.h>
		  void foo(char* fmt, ...)
		  {va_list args; va_start(args, fmt);
	          vscanf("%i", args);}],
	          [],, [ac_c99_stdio=no])
  AC_TRY_COMPILE([#include <stdio.h>
		  #include <stdarg.h>
		  void foo(char* fmt, ...)
		  {va_list args; va_start(args, fmt);
	          vsnprintf(fmt, 0, "%i", args);}],
	          [],, [ac_c99_stdio=no])
  AC_TRY_COMPILE([#include <stdio.h>
		  #include <stdarg.h>
		  void foo(char* fmt, ...)
		  {va_list args; va_start(args, fmt);
	          vsscanf(fmt, "%i", args);}],
	          [],, [ac_c99_stdio=no])
  AC_MSG_RESULT($ac_c99_stdio)

  # Check for the existence in <stdlib.h> of lldiv_t, et. al.
  ac_c99_stdlib=yes;
  AC_MSG_CHECKING([for lldiv_t declaration])
  AC_CACHE_VAL(ac_c99_lldiv_t, [
  AC_TRY_COMPILE([#include <stdlib.h>], 
                   [ lldiv_t mydivt;], 
                   [ac_c99_lldiv_t=yes], [ac_c99_lldiv_t=no])
  ])
  AC_MSG_RESULT($ac_c99_lldiv_t)

  AC_MSG_CHECKING([for ISO C99 support in <stdlib.h>])
  AC_TRY_COMPILE([#include <stdlib.h>],
	         [char* tmp; strtof("gnu", &tmp);],, [ac_c99_stdlib=no])
  AC_TRY_COMPILE([#include <stdlib.h>],
	         [char* tmp; strtold("gnu", &tmp);],, [ac_c99_stdlib=no])
  AC_TRY_COMPILE([#include <stdlib.h>],
	         [char* tmp; strtoll("gnu", &tmp, 10);],, [ac_c99_stdlib=no])
  AC_TRY_COMPILE([#include <stdlib.h>],
	         [char* tmp; strtoull("gnu", &tmp, 10);],, [ac_c99_stdlib=no])
  AC_TRY_COMPILE([#include <stdlib.h>], [llabs(10);],, [ac_c99_stdlib=no])
  AC_TRY_COMPILE([#include <stdlib.h>], [lldiv(10,1);],, [ac_c99_stdlib=no])
  AC_TRY_COMPILE([#include <stdlib.h>], [atoll("10");],, [ac_c99_stdlib=no])
  AC_TRY_COMPILE([#include <stdlib.h>], [_Exit(0);],, [ac_c99_stdlib=no])
  if test x"$ac_c99_lldiv_t" = x"no"; then
    ac_c99_stdlib=no; 
  fi; 
  AC_MSG_RESULT($ac_c99_stdlib)

  # Check for the existence of <wchar.h> functions used if C99 is enabled.
  # XXX the wchar.h checks should be rolled into the general C99 bits.
  ac_c99_wchar=yes;
  AC_MSG_CHECKING([for additional ISO C99 support in <wchar.h>])
  AC_TRY_COMPILE([#include <wchar.h>], 
	         [wcstold(L"10.0", NULL);],, [ac_c99_wchar=no])
  AC_TRY_COMPILE([#include <wchar.h>], 
	         [wcstoll(L"10", NULL, 10);],, [ac_c99_wchar=no])
  AC_TRY_COMPILE([#include <wchar.h>], 
	         [wcstoull(L"10", NULL, 10);],, [ac_c99_wchar=no])
  AC_MSG_RESULT($ac_c99_wchar)

  AC_MSG_CHECKING([for enabled ISO C99 support])
  if test x"$ac_c99_math" = x"no" || test x"$ac_c99_stdio" = x"no" \
      || test x"$ac_c99_stdlib" = x"no" \
      || test x"$ac_c99_wchar" = x"no"; then
    enable_c99=no; 
  fi; 
  AC_MSG_RESULT($enable_c99)

  # Option parsed, now set things appropriately
  if test x"$enable_c99" = x"yes"; then
    AC_DEFINE(_GLIBCPP_USE_C99)
  fi

  AC_LANG_RESTORE
])


dnl
dnl Check for template specializations for the 'long long' type extension.
dnl NB: Must check for C99 support before calling _GLIBCPP_ENABLE_LONG_LONG
dnl
dnl GLIBCPP_ENABLE_LONG_LONG
dnl --enable-long-long defines _GLIBCPP_USE_LONG_LONG
dnl --disable-long-long leaves _GLIBCPP_USE_LONG_LONG undefined
dnl  +  Usage:  GLIBCPP_ENABLE_LONG_LONG[(DEFAULT)]
dnl       Where DEFAULT is either `yes' or `no'.  If omitted, it
dnl       defaults to `no'.
dnl  +  If 'long long' stuff is not available, ignores DEFAULT and sets `no'.
dnl
dnl GLIBCPP_ENABLE_LONG_LONG
AC_DEFUN(GLIBCPP_ENABLE_LONG_LONG, [dnl
  define([GLIBCPP_ENABLE_LONG_LONG_DEFAULT], ifelse($1, yes, yes, no))dnl

  AC_ARG_ENABLE(long-long,
  changequote(<<, >>)dnl
  <<--enable-long-long      turns on 'long long' [default=>>GLIBCPP_ENABLE_LONG_LONG_DEFAULT],
  changequote([, ])dnl
  [case "$enableval" in
   yes) enable_long_long=yes ;;
   no)  enable_long_long=no ;;
   *)   AC_MSG_ERROR([Unknown argument to enable/disable long long]) ;;
   esac],
  enable_long_long=GLIBCPP_ENABLE_LONG_LONG_DEFAULT)dnl

  # iostreams require strtoll, strtoull to compile. If the
  # GLIBCPP_ENABLE_C99 tests found these, and if C99 support is enabled,
  # go ahead and allow long long to be used.
  if test x"$enable_c99" = x"no"; then
    enable_long_long=no; 
  fi

  # Option parsed, now set things appropriately
  AC_MSG_CHECKING([for enabled long long support])
  if test x"$enable_long_long" = xyes; then
    AC_DEFINE(_GLIBCPP_USE_LONG_LONG)
  fi
  AC_MSG_RESULT($enable_long_long)
])


dnl
dnl Check for what kind of C headers to use.
dnl
dnl GLIBCPP_ENABLE_CHEADERS
dnl --enable-cheaders= [does stuff].
dnl --disable-cheaders [does not do anything, really].
dnl  +  This will eventually need to be 'c_shadow' by default.
dnl  +  Usage:  GLIBCPP_ENABLE_CHEADERS[(DEFAULT)]
dnl       Where DEFAULT is either `c' or `c_std' or 'c_shadow'.  
dnl       If ommitted, it defaults to `c_std'.
AC_DEFUN(GLIBCPP_ENABLE_CHEADERS, [dnl
define([GLIBCPP_ENABLE_CHEADERS_DEFAULT], ifelse($1, c_std, c_std, c_std))dnl
AC_MSG_CHECKING([for c header strategy to use])
AC_ARG_ENABLE(cheaders,
changequote(<<, >>)dnl
<<  --enable-cheaders       construct "C" header files for g++ [default=>>GLIBCPP_ENABLE_CHEADERS_DEFAULT],
changequote([, ])
  [case "$enableval" in
   c) 
        enable_cheaders=c 
        ;;
   c_std)  
        enable_cheaders=c_std 
        ;;
   c_shadow)  
        enable_cheaders=c_shadow 
        ;;
   *)   AC_MSG_ERROR([Unknown argument to enable/disable "C" headers]) 
        ;;
  esac],
  enable_cheaders=GLIBCPP_ENABLE_CHEADERS_DEFAULT)
  AC_MSG_RESULT($enable_cheaders)

  dnl Option parsed, now set things appropriately
  case "$enable_cheaders" in
    c_shadow) 
        CSHADOW_FLAGS="-fno-builtin"
        C_INCLUDE_DIR='${glibcpp_srcdir}/include/c_shadow'
        AC_DEFINE(_GLIBCPP_USE_SHADOW_HEADERS)
        ;;
    c_std)   
        CSHADOW_FLAGS=""
        C_INCLUDE_DIR='${glibcpp_srcdir}/include/c_std'
        ;;
    c)   
        CSHADOW_FLAGS=""
        C_INCLUDE_DIR='${glibcpp_srcdir}/include/c'
        ;;
  esac

  AC_SUBST(CSHADOW_FLAGS)
  AC_SUBST(C_INCLUDE_DIR)
  AM_CONDITIONAL(GLIBCPP_USE_CSHADOW, test "$enable_cheaders" = c_shadow)
])


dnl
dnl Check for wide character support.  Has the same effect as the option
dnl in gcc's configure, but in a form that autoconf can mess with.
dnl
dnl GLIBCPP_ENABLE_C_MBCHAR
dnl --enable-c-mbchar requests all the wchar_t stuff.
dnl --disable-c-mbchar doesn't.
dnl  +  Usage:  GLIBCPP_ENABLE_C_MBCHAR[(DEFAULT)]
dnl       Where DEFAULT is either `yes' or `no'.  If ommitted, it
dnl       defaults to `no'.
AC_DEFUN(GLIBCPP_ENABLE_C_MBCHAR, [dnl
define([GLIBCPP_ENABLE_C_MBCHAR_DEFAULT], ifelse($1, yes, yes, no))dnl
AC_ARG_ENABLE(c-mbchar,
changequote(<<, >>)dnl
<<  --enable-c-mbchar       enable multibyte (wide) characters [default=>>GLIBCPP_ENABLE_C_MBCHAR_DEFAULT],
changequote([, ])dnl
[case "$enableval" in
 yes) enable_c_mbchar=yes ;;
 no)  enable_c_mbchar=no ;;
 *)   AC_MSG_ERROR([Unknown argument to enable/disable c-mbchar]) ;;
 esac],
enable_c_mbchar=GLIBCPP_ENABLE_C_MBCHAR_DEFAULT)dnl
dnl Option parsed, now other scripts can test enable_c_mbchar for yes/no.
])


dnl
dnl Set up *_INCLUDES and *_INCLUDE_DIR variables for all sundry Makefile.am's.
dnl
dnl TOPLEVEL_INCLUDES
dnl LIBMATH_INCLUDES
dnl LIBSUPCXX_INCLUDES
dnl LIBIO_INCLUDES
dnl CSHADOW_INCLUDES
dnl
dnl GLIBCPP_EXPORT_INCLUDES
AC_DEFUN(GLIBCPP_EXPORT_INCLUDES, [
  # Root level of the build directory include sources.
  GLIBCPP_INCLUDES="-I${glibcpp_builddir}/include/${target_alias} -I${glibcpp_builddir}/include"

  # Passed down for canadian crosses.
  if test x"$CANADIAN" = xyes; then
    TOPLEVEL_INCLUDES='-I$(includedir)'
  fi

  LIBMATH_INCLUDES='-I$(top_srcdir)/libmath'

  LIBSUPCXX_INCLUDES='-I$(top_srcdir)/libsupc++'

  if test x"$need_libio" = xyes; then
    LIBIO_INCLUDES='-I$(top_builddir)/libio -I$(top_srcdir)/libio'
    AC_SUBST(LIBIO_INCLUDES)
  fi

  # Now, export this to all the little Makefiles....
  AC_SUBST(GLIBCPP_INCLUDES)
  AC_SUBST(TOPLEVEL_INCLUDES)
  AC_SUBST(LIBMATH_INCLUDES)
  AC_SUBST(LIBSUPCXX_INCLUDES)
])


dnl
dnl Set up *_FLAGS and *FLAGS variables for all sundry Makefile.am's.
dnl
AC_DEFUN(GLIBCPP_EXPORT_FLAGS, [
  # Optimization flags that are probably a good idea for thrill-seekers. Just
  # uncomment the lines below and make, everything else is ready to go... 
  # OPTIMIZE_CXXFLAGS = -O3 -fstrict-aliasing -fvtable-gc 
  OPTIMIZE_CXXFLAGS=
  AC_SUBST(OPTIMIZE_CXXFLAGS)

  WARN_FLAGS='-Wall -Wno-format -W -Wwrite-strings -Winline'
  AC_SUBST(WARN_FLAGS)
])

dnl
dnl  GLIBCPP_EXPORT_INSTALL_INFO
dnl  calculates gxx_install_dir
dnl  exports glibcpp_toolexecdir
dnl  exports glibcpp_toolexeclibdir
dnl  exports glibcpp_builddir
dnl  exports glibcpp_srcdir
dnl  exports glibcpp_prefixdir
dnl
dnl Assumes cross_compiling bits already done, and with_cross_host in
dnl particular
dnl
dnl GLIBCPP_EXPORT_INSTALL_INFO
AC_DEFUN(GLIBCPP_EXPORT_INSTALL_INFO, [

glibcpp_toolexecdir=no
glibcpp_toolexeclibdir=no

# Export build and source directories.
# These need to be absolute paths, yet at the same time need to
# canonicalize only relative paths, because then amd will not unmount
# drives. Thus the use of PWDCMD: set it to 'pawd' or 'amq -w' if using amd.
glibcpp_builddir=`pwd`
case $srcdir in
[\\/$]* | ?:[\\/]*) glibcpp_srcdir=${srcdir} ;;
*) glibcpp_srcdir=`cd "$srcdir" && ${PWDCMD-pwd} || echo "$srcdir"` ;;
esac
glibcpp_prefixdir=${prefix}

AC_MSG_CHECKING([for interface version number])
libstdcxx_interface=$INTERFACE
AC_MSG_RESULT($libstdcxx_interface)

# Process the option --with-gxx-include-dir=<path to include-files directory>
AC_MSG_CHECKING([for --with-gxx-include-dir])
AC_ARG_WITH(gxx-include-dir,
[  --with-gxx-include-dir  the installation directory for include files],
[case "${withval}" in
  yes)
    AC_MSG_ERROR(Missing directory for --with-gxx-include-dir)
    gxx_include_dir=no
    ;;
  no)
    gxx_include_dir=no
    ;;
  *)
    gxx_include_dir=${withval}
    ;;
esac], [gxx_include_dir=no])
AC_MSG_RESULT($gxx_include_dir)

# Process the option "--enable-version-specific-runtime-libs"
AC_MSG_CHECKING([for --enable-version-specific-runtime-libs])
AC_ARG_ENABLE(version-specific-runtime-libs,
[  --enable-version-specific-runtime-libs    Specify that runtime libraries should be installed in a compiler-specific directory ],
[  version_specific_libs=yes
# Need the gcc compiler version to know where to install libraries
# and header files if --enable-version-specific-runtime-libs option
# is selected.
changequote(,)dnl
gcc_version_trigger=${srcdir}/../gcc/version.c
gcc_version_full=`grep version_string ${gcc_version_trigger} | sed -e 's/.*\"\([^\"]*\)\".*/\1/'`
gcc_version=`echo ${gcc_version_full} | sed -e 's/\([^ ]*\) .*/\1/'`
gxx_include_dir='$(libdir)/gcc-lib/$(target_alias)/'${gcc_version}/include/g++
glibcpp_toolexecdir='$(libdir)/gcc-lib/$(target_alias)'
glibcpp_toolexeclibdir='$(toolexecdir)/'${gcc_version}
changequote([,])dnl
],version_specific_libs=no)
AC_MSG_RESULT($version_specific_libs)

# Default case for install directory for include files.
if test x"$version_specific_libs" = x"no" \
   && test x"$gxx_include_dir" = x"no"; then
  gxx_include_dir='$(prefix)'/include/g++-${libstdcxx_interface}
fi

# Calculate glibcpp_toolexecdir, glibcpp_toolexeclibdir
# Install a library built with a cross compiler in tooldir, not libdir.
if test x"$glibcpp_toolexecdir" = x"no"; then 
  if test -n "$with_cross_host" && test x"$with_cross_host" != x"no"; then
    glibcpp_toolexecdir='$(exec_prefix)/$(target_alias)'
    glibcpp_toolexeclibdir='$(toolexecdir)/lib$(MULTISUBDIR)'
  else
    glibcpp_toolexecdir='$(libdir)/gcc-lib/$(target_alias)'
    glibcpp_toolexeclibdir='$(libdir)$(MULTISUBDIR)'
  fi
fi

AC_MSG_CHECKING([for install location])
AC_MSG_RESULT($gxx_include_dir)

AC_SUBST(glibcpp_builddir)
AC_SUBST(glibcpp_srcdir)
AC_SUBST(glibcpp_prefixdir)
AC_SUBST(gxx_include_dir)
AC_SUBST(glibcpp_toolexecdir)
AC_SUBST(glibcpp_toolexeclibdir)
])


# Check whether LC_MESSAGES is available in <locale.h>.
# Ulrich Drepper <drepper@cygnus.com>, 1995.
#
# This file file be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
# Please note that the actual code is *not* freely available.

# serial 1

AC_DEFUN(AC_LC_MESSAGES, [
  AC_CHECK_HEADER(locale.h, [
    AC_CACHE_CHECK([for LC_MESSAGES], ac_cv_val_LC_MESSAGES,
      [AC_TRY_LINK([#include <locale.h>], [return LC_MESSAGES],
       ac_cv_val_LC_MESSAGES=yes, ac_cv_val_LC_MESSAGES=no)])
    if test $ac_cv_val_LC_MESSAGES = yes; then
      AC_DEFINE(HAVE_LC_MESSAGES)
    fi
  ])
])


# Check for functions in math library.
# Ulrich Drepper <drepper@cygnus.com>, 1998.
#
# This file can be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
# Please note that the actual code is *not* freely available.

# serial 1

dnl AC_REPLACE_MATHFUNCS(FUNCTION...)
AC_DEFUN(AC_REPLACE_MATHFUNCS,
[AC_CHECK_FUNCS([$1], , [LIBMATHOBJS="$LIBMATHOBJS ${ac_func}.lo"])
AC_SUBST(LIBMATHOBJS)dnl
])


# Check for string functions.
# Ulrich Drepper <drepper@cygnus.com>, 1998.
#
# This file can be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
# Please note that the actual code is *not* freely available.

# serial 1

dnl AC_REPLACE_STRINGFUNCS(FUNCTION...)
AC_DEFUN(AC_REPLACE_STRINGFUNCS,
[AC_CHECK_FUNCS([$1], , [LIBSTRINGOBJS="$LIBSTRINGOBJS ${ac_func}.lo"])
AC_SUBST(LIBSTRINGOBJS)dnl
])


dnl This macro searches for a GNU version of make.  If a match is found, the
dnl makefile variable `ifGNUmake' is set to the empty string, otherwise it is
dnl set to "#". This is useful for  including a special features in a Makefile,
dnl which cannot be handled by other versions of make.  The variable
dnl _cv_gnu_make_command is set to the command to invoke GNU make if it exists,
dnl the empty string otherwise.
dnl
dnl Here is an example of its use:
dnl
dnl Makefile.in might contain:
dnl
dnl     # A failsafe way of putting a dependency rule into a makefile
dnl     $(DEPEND):
dnl             $(CC) -MM $(srcdir)/*.c > $(DEPEND)
dnl
dnl     @ifGNUmake@ ifeq ($(DEPEND),$(wildcard $(DEPEND)))
dnl     @ifGNUmake@ include $(DEPEND)
dnl     @ifGNUmake@ endif
dnl
dnl Then configure.in would normally contain:
dnl
dnl     CHECK_GNU_MAKE()
dnl     AC_OUTPUT(Makefile)
dnl
dnl Then perhaps to cause gnu make to override any other make, we could do
dnl something like this (note that GNU make always looks for GNUmakefile first):
dnl
dnl     if  ! test x$_cv_gnu_make_command = x ; then
dnl             mv Makefile GNUmakefile
dnl             echo .DEFAULT: > Makefile ;
dnl             echo \  $_cv_gnu_make_command \$@ >> Makefile;
dnl     fi
dnl
dnl Then, if any (well almost any) other make is called, and GNU make also
dnl exists, then the other make wraps the GNU make.
dnl
dnl @author John Darrington <j.darrington@elvis.murdoch.edu.au>
dnl @version 1.1 #### replaced Id string now that Id is for lib-v3; pme
dnl
dnl #### Changes for libstdc++-v3:  reformatting and linewrapping; prepending
dnl #### GLIBCPP_ to the macro name; adding the :-make fallback in the
dnl #### conditional's subshell (" --version" is not a command), using a
dnl #### different option to grep(1).
dnl #### -pme
dnl #### Fixed Bourne shell portability bug (use ${MAKE-make}, not
dnl #### ${MAKE:-make}).
dnl #### -msokolov
AC_DEFUN(
  GLIBCPP_CHECK_GNU_MAKE, [AC_CACHE_CHECK( for GNU make,_cv_gnu_make_command,
          _cv_gnu_make_command='' ;
dnl Search all the common names for GNU make
          for a in "${MAKE-make}" make gmake gnumake ; do
                  if ( $a --version 2> /dev/null | grep -c GNU > /dev/null )
                  then
                          _cv_gnu_make_command=$a ;
                          break;
                  fi
          done ;
  ) ;
dnl If there was a GNU version, then set @ifGNUmake@ to the empty
dnl string, '#' otherwise
  if test  "x$_cv_gnu_make_command" != "x"  ; then
          ifGNUmake='' ;
  else
          ifGNUmake='#' ;
  fi
  AC_SUBST(ifGNUmake)
])

sinclude(../libtool.m4)
dnl The lines below arrange for aclocal not to bring an installed
dnl libtool.m4 into aclocal.m4, while still arranging for automake to
dnl add a definition of LIBTOOL to Makefile.in.
ifelse(,,,[AC_SUBST(LIBTOOL)
AC_DEFUN([AM_PROG_LIBTOOL])
AC_DEFUN([AC_LIBTOOL_DLOPEN])
AC_DEFUN([AC_PROG_LD])
])


