// Copyright (C) 2001 Free Software Foundation, Inc.
// Contributed by Nathan Sidwell 31 Jul 2001 <nathan@codesourcery.com>

// Bug 3820. We were bit copying empty bases including the
// padding. Which clobbers whatever they overlay.

struct Empty {};

struct Inter : Empty {};

int now = 0;

struct NonPod
{
  int m;

  NonPod () {m = 0x12345678;}
  NonPod (int m_) {m = m_;}
  NonPod &operator= (NonPod const &src) {now = m; m = src.m;}
  NonPod (NonPod const &src) {m = src.m;}
};

struct A : Inter
{
  A (int c) {m = c;}
  
  NonPod m;
};

struct B
{
  Inter empty;
  NonPod m;

  B (int c) {m = c;}
};

struct C : NonPod, Inter
{
  C (int c) : NonPod (c), Inter () {}
};

int main ()
{
  A a (0x12131415);
  
  int was = a.m.m;
  
  a = 0x22232425;

  if (was != now)
    return 1;	// we copied the empty base which clobbered a.m.m's
		// original value.
  
  A b (0x32333435);
  *(Inter *)&a = *(Inter *)&b;
  
  if (a.m.m != 0x22232425)
    return 2;	// we copied padding, which clobbered a.m.m

  A b2 (0x32333435);
  (Inter &)b2 = Inter ();
  if (b2.m.m != 0x32333435)
    return 2;	// we copied padding, which clobbered b2.m.m
  
  B c (0x12131415);
  was = c.m.m;
  c = 0x22232425;
  if (was != now)
    return 3;
  
  B d (0x32333435);
  c.empty = d.empty;

  if (c.m.m != 0x22232425)
    return 4;

  C e (0x32333435);

  if (e.m != 0x32333435)
    return 2;	// we copied padding
  
  return 0;
}
