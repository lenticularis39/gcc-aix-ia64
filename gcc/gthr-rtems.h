/* RTEMS threads compatibily routines for libgcc2 and libobjc.
   by: Rosimildo da Silva( rdasilva@connecttel.com ) */
/* Compile this one with gcc.  */
/* Copyright (C) 1997, 1999, 2000 Free Software Foundation, Inc.

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

/* As a special exception, if you link this library with other files,
   some of which are compiled with GCC, to produce an executable,
   this library does not by itself cause the resulting executable
   to be covered by the GNU General Public License.
   This exception does not however invalidate any other reasons why
   the executable file might be covered by the GNU General Public License.  */

#ifndef __gthr_rtems_h
#define __gthr_rtems_h


#define __GTHREADS 1

#define __GTHREAD_ONCE_INIT  0
#define __GTHREAD_MUTEX_INIT_FUNCTION  rtems_gxx_mutex_init

/* avoid depedency on rtems specific headers */
typedef void *__gthread_key_t;
typedef int   __gthread_once_t;
typedef void *__gthread_mutex_t;

/*
 * External functions provided by RTEMS. They are very similar to their POSIX 
 * counterparts. A "Wrapper API" is being use to avoid dependency on any RTEMS
 * header files.
 */

/* generic per task variables */
extern int rtems_gxx_once (__gthread_once_t *once, void (*func) ());
extern int rtems_gxx_key_create (__gthread_key_t *key, void (*dtor) (void *));
extern int rtems_gxx_key_dtor (__gthread_key_t key, void *ptr);
extern int rtems_gxx_key_delete (__gthread_key_t key);
extern void *rtems_gxx_getspecific (__gthread_key_t key);
extern int rtems_gxx_setspecific (__gthread_key_t key, const void *ptr);

/* mutex support */
extern void rtems_gxx_mutex_init (__gthread_mutex_t *mutex);
extern int rtems_gxx_mutex_lock (__gthread_mutex_t *mutex);
extern int rtems_gxx_mutex_trylock (__gthread_mutex_t *mutex);
extern int rtems_gxx_mutex_unlock (__gthread_mutex_t *mutex);


/* RTEMS threading is always active */
static inline int
__gthread_active_p (void)
{
  return 1;
}

/* Wrapper calls */
static inline int
__gthread_once (__gthread_once_t *once, void (*func) ())
{
   return rtems_gxx_once( once, func );
}

static inline int
__gthread_key_create (__gthread_key_t *key, void (*dtor) (void *))
{
  return rtems_gxx_key_create( key, dtor );
}

static inline int
__gthread_key_dtor (__gthread_key_t key, void *ptr)
{
   return rtems_gxx_key_dtor(key, ptr);
}

static inline int
__gthread_key_delete (__gthread_key_t key)
{
  return rtems_gxx_key_delete (key);
}

static inline void *
__gthread_getspecific (__gthread_key_t key)
{
  return rtems_gxx_getspecific (key);
}

static inline int
__gthread_setspecific (__gthread_key_t key, const void *ptr)
{
  return rtems_gxx_setspecific (key, ptr);
}

static inline int
__gthread_mutex_lock (__gthread_mutex_t *mutex)
{
    return rtems_gxx_mutex_lock (mutex);
}

static inline int
__gthread_mutex_trylock (__gthread_mutex_t *mutex)
{
    return rtems_gxx_mutex_trylock (mutex);
}

static inline int
__gthread_mutex_unlock (__gthread_mutex_t *mutex)
{
    return rtems_gxx_mutex_unlock( mutex );
}

#endif /* __gthr_rtems_h */
