# GCC 3 for AIX/ia64

This repository contains an attempt to resurrect GCC for AIX5L IA64.
Currently it consists of GCC 3.0.1 patched with Timothy Wall's patch from
2001, see [here](https://gcc.gnu.org/legacy-ml/gcc-patches/2001-07/msg00360.html).

This is a part of a wider effort to revive AIX/ia64 started on NCommander's
Discord. For more information, see the related [wiki page](https://restless.systems/wiki/AIX_on_Itanium).

## Building

A cross compiler can be built the usual way. For x86-64 hosts a 32-bit build
using --build and --host set to i686-unknown-linux-gnu is required, since GCC
3.0.1 pre-dates x86-64 support in GCC.

For example, a full cross compiler can be build like this, assuming ia64-ibm-aix
bintools are in PATH, PREFIX is set to the cross toolchain prefix, and SYSTEM is
set to the location of an AIX/ia64 system root:

```
$ mkdir build && cd build
$ CC='gcc -m32' ../configure --prefix=$PREFIX --with-sysroot=$SYSTEM --enable-languages=c,c++ --build=i686-unknown-linux-gnu --host=i686-unknown-linux-gnu --target=ia64-ibm-aix
$ make
$ make install
```

## Issues

There are a few issues preventing a successfull build:
- cc1 crashes with a segmentation fault on a simple source file

Also see Issues in GitHub repository.
