// 2000-09-07 bgarcia@laurelnetworks.com

// Copyright (C) 2000, 2001 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// 23.3.4 template class multiset

#include <set>
#include <string>
make 
// { dg-do compile }
// { dg-excess-errors "" }

// libstdc++/86: map & set iterator comparisons are not type-safe
int main(void)
{
  bool test = true;

  std::set<unsigned int> setByIndex;
  std::set<std::string> setByName;
  
  std::set<unsigned int>::iterator itr(setByIndex.begin());
  

  // NB: it's not setByIndex!!
  test &= itr != setByName.end(); 
  // { dg-error "no match for" "" { xfail *-*-* } 41 }
  test &= itr == setByName.end(); 
  // { dg-error "no match for" "" { xfail *-*-* } 43 }
  return 0;
}
