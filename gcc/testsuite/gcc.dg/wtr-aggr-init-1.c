/* Test for -Wtraditional warnings on automatic aggregate initialization.
   Note, gcc should omit these warnings in system header files.
   By Kaveh R. Ghazi <ghazi@caip.rutgers.edu> 8/22/2000.  */
/* { dg-do compile } */
/* { dg-options "-Wtraditional" } */

struct foo
{
  int i;
  long l;
};

struct foo f0 = { 0, 0 };
static struct foo f1 = { 0, 0 };

void
testfunc1 (void)
{
  struct foo f3 = { 0, 0 }; /* { dg-warning "traditional C rejects automatic" "automatic aggregate initialization" } */
  static struct foo f4 = { 0, 0 };
  
  f3 = f4;
}
  
# 26 "sys-header.h" 3
/* We are in system headers now, no -Wtraditional warnings should issue.  */

struct foo f5 = { 0, 0 };
static struct foo f6 = { 0, 0 };

void
testfunc2 (void)
{
  struct foo f7 = { 0, 0 };
  static struct foo f8 = { 0, 0 };
  
  f7 = f8;
}
