// 2000-09-11 Benjamin Kosnik <bkoz@redhat.com>

// Copyright (C) 2000 Free Software Foundation
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

// 22.1.1.4 locale operators [lib.locale.operators]

#include <cwchar> // for mbstate_t
#include <locale>
#include <debug_assert.h>

typedef std::codecvt<char, char, std::mbstate_t> ccodecvt;
class gnu_codecvt: public ccodecvt { }; 

void test01()
{
  using namespace std;

  bool test = true;
  string str1, str2;

  // construct a locale object with the C facet
  const locale& 	cloc = locale::classic();
  // construct a locale object with the specialized facet.
  locale                loc(locale::classic(), new gnu_codecvt);
  VERIFY ( cloc != loc );
  VERIFY ( !(cloc == loc) );

  str1 = cloc.name();
  str2 = loc.name();  
  VERIFY( loc(str1, str2) == false );
}

int main ()
{
  test01();

  return 0;
}
