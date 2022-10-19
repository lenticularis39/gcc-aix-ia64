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

Currently only an incomplete `xgcc` compiler build works due to issues preventing
C++ and libgcc build. This can be done like this, assuming `$PREFIX` is where binutils
2.12 targeting ia64-ibm-aix are:

```
$ ./fix-timestamps.sh
$ mkdir build && cd build
$ ../configure --prefix=$PREFIX --enable-languages=c -target=ia64-ibm-aix
$ make all-gcc
```

GCC 3 cannot recognize x86_64 systems, a workaround is running configure to target i686,
provided you have a multilib system with corresponding development packages installed:
```
$ CC='gcc -m32' ../configure ... --build=i686-unknown-linux-gnu --host=i686-unknown-linux-gnu
```

## Issues

See Issues in GitHub repository.
