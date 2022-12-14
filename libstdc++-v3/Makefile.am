## Makefile for the toplevel directory of the GNU C++ Standard library.
##
## Copyright (C) 1997,1998, 1999, 2000, 2001 Free Software Foundation, Inc.
##
## This file is part of the libstdc++ version 3 distribution.
## Process this file with automake to produce Makefile.in.

## This file is part of the GNU ISO C++ Library.  This library is free
## software; you can redistribute it and/or modify it under the
## terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2, or (at your option)
## any later version.

## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License along
## with this library; see the file COPYING.  If not, write to the Free
## Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
## USA.

AUTOMAKE_OPTIONS = 1.3 cygnus
MAINT_CHARSET = latin1

SUBDIRS = include libio libmath libsupc++ src testsuite

mkinstalldirs = $(SHELL) $(toplevel_srcdir)/mkinstalldirs

# These two special 'check-script' rules use the bash script 'mkcheck'
# to do testing. This script is not as portable as the dejagnu test
# harness, and is thus off by default. It does produce interesting
# output however, including various performance analysis items like
# compile time, execution time, and binary size.
check-script: $(top_builddir)/mkcheck
	-(chmod + $(top_builddir)/mkcheck; \
	  test -d testsuite || (mkdir testsuite; chmod u+w testsuite); \
	  cd testsuite; \
	  @glibcpp_builddir@/mkcheck 0)

check-script-install: $(top_builddir)/mkcheck
	-(chmod + $(top_builddir)/mkcheck; \
	  test -d testsuite || (mkdir testsuite; chmod u+w testsuite); \
	  cd testsuite; \
	  @glibcpp_builddir@/mkcheck 1)

# These two rules might seem messy, but are hella worth it.
doxygen:
	-(srcdir=`cd ${top_srcdir}; pwd`; \
	  outdir=`pwd`; \
	  /bin/sh ${srcdir}/docs/doxygen/run_doxygen \
	            --mode=user $${srcdir} $${outdir})

doxygen-maint:
	-(srcdir=`cd ${top_srcdir}; pwd`; \
	  outdir=`pwd`; \
	  ${srcdir}/docs/doxygen/run_doxygen --mode=maint $${srcdir} $${outdir})

.PHONY: doxygen doxygen-maint

# Multilib support.
MAKEOVERRIDES=

# Multilib variables.
MULTISRCTOP =
MULTIBUILDTOP =
MULTIDIRS =
MULTISUBDIR =
MULTIDO = true
MULTICLEAN = true

# Multilib Makefile bits.
.PHONY: all-multi mostlyclean-multi clean-multi distclean-multi \
	maintainer-clean-multi

all-am: all-multi
install-am: install-multi
mostlyclean-am: mostlyclean-multi
clean-am: clean-multi
distclean-am: distclean-multi
maintainer-clean-am: maintainer-clean-multi

all-multi:
	$(MULTIDO) $(AM_MAKEFLAGS) DO=all multi-do
install-multi:
	$(MULTIDO) $(AM_MAKEFLAGS) DO=install multi-do
mostlyclean-multi:
	$(MULTICLEAN) $(AM_MAKEFLAGS) DO=mostlyclean multi-clean
clean-multi:
	$(MULTICLEAN) $(AM_MAKEFLAGS) DO=clean multi-clean
distclean-multi:
	$(MULTICLEAN) $(AM_MAKEFLAGS) DO=distclean multi-clean
maintainer-clean-multi:
	$(MULTICLEAN) $(AM_MAKEFLAGS) DO=maintainer-clean multi-clean

# All the machinations with string instantiations messes up the
# automake-generated TAGS rule. Make a simple one here.
TAGS: tags-recursive $(LISP)

# Work around what appears to be a GNU make bug handling MAKEFLAGS
# values defined in terms of make variables, as is the case for CC and
# friends when we are called from the top level Makefile.
AM_MAKEFLAGS = \
	"AR_FLAGS=$(AR_FLAGS)" \
	"CC_FOR_BUILD=$(CC_FOR_BUILD)" \
	"CC_FOR_TARGET=$(CC_FOR_TARGET)" \
	"CFLAGS=$(CFLAGS)" \
	"CXXFLAGS=$(CXXFLAGS)" \
	"CFLAGS_FOR_BUILD=$(CFLAGS_FOR_BUILD)" \
	"CFLAGS_FOR_TARGET=$(CFLAGS_FOR_TARGET)" \
	"INSTALL=$(INSTALL)" \
	"INSTALL_DATA=$(INSTALL_DATA)" \
	"INSTALL_PROGRAM=$(INSTALL_PROGRAM)" \
	"INSTALL_SCRIPT=$(INSTALL_SCRIPT)" \
	"LDFLAGS=$(LDFLAGS)" \
	"LIBCFLAGS=$(LIBCFLAGS)" \
	"LIBCFLAGS_FOR_TARGET=$(LIBCFLAGS_FOR_TARGET)" \
	"MAKE=$(MAKE)" \
	"MAKEINFO=$(MAKEINFO) $(MAKEINFOFLAGS)" \
	"PICFLAG=$(PICFLAG)" \
	"PICFLAG_FOR_TARGET=$(PICFLAG_FOR_TARGET)" \
	"SHELL=$(SHELL)" \
	"RUNTESTFLAGS=$(RUNTESTFLAGS)" \
	"exec_prefix=$(exec_prefix)" \
	"infodir=$(infodir)" \
	"libdir=$(libdir)" \
	"includedir=$(includedir)" \
	"prefix=$(prefix)" \
	"tooldir=$(tooldir)" \
	"AR=$(AR)" \
	"AS=$(AS)" \
	"LD=$(LD)" \
	"LIBCFLAGS=$(LIBCFLAGS)" \
	"PICFLAG=$(PICFLAG)" \
	"RANLIB=$(RANLIB)" \
	"NM=$(NM)" \
	"NM_FOR_BUILD=$(NM_FOR_BUILD)" \
	"NM_FOR_TARGET=$(NM_FOR_TARGET)" \
	"DESTDIR=$(DESTDIR)" \
	"WERROR=$(WERROR)" 


