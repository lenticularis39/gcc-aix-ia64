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

Currently, only a C and Fortran compiler can be built. This can be done like this,
assuming `$PREFIX` is where binutils 2.12 targeting ia64-ibm-aix are:

```
$ ./fix-timestamps.sh
$ mkdir build && cd build
$ ../configure --prefix=$PREFIX --enable-languages=c,f77 -target=ia64-ibm-aix
$ make all-gcc
$ make install-gcc
```

To be able to actually use your compiler to compile and link code, you also need some files
from an AIX installation, namely those in directories /usr/include, /usr/lib/ia64l64, and
/usr/ccs/lib/ia64l64. Include files should be put in `$PREFIX/ia64-ibm-aix/include`, library
files in `$PREFIX/ia64-ibm-aix/lib` (no need to create a `ia64l64` subfolder there).

GCC 3 cannot recognize x86_64 systems, a workaround is running configure to target i686,
provided you have a multilib system with corresponding development packages installed:
```
$ CC='gcc -m32' ../configure ... --build=i686-unknown-linux-gnu --host=i686-unknown-linux-gnu
```

## Usage

After the compiler is built, installed and the includes and libraries are properly set up
(see above), you can start using the compiler. Here is an example of building Hello world,
assuming `$PREFIX/bin` is in PATH:

```
$ ia64-ibm-aix-gcc hello.c -o hello-gcc
$ file hello-gcc
hello-gcc: ELF 64-bit LSB executable, IA-64, version 1 (SYSV), dynamically linked, interpreter /usr/lib/ia64l64/libc.so.1, not stripped
$ ls -l hello-gcc
-rwxr-xr-x. 1 tglozar tglozar 6317 Oct 26 18:50 hello-gcc
```

## Issues

See Issues in GitHub repository.
