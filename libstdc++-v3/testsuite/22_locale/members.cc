// 2001-01-19 Benjamin Kosnik <bkoz@redhat.com>

// Copyright (C) 2001 Free Software Foundation
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

// 22.2.3.1.1  numpunct members

#include <locale>
#include <string>
#include <debug_assert.h>

// creating unnamed locales 1 using new + combine
void
test01()
{
  using namespace std;
  bool test = true;
  const string name_c("C");
  const string str_unnamed("*");
  string str;

  // construct a locale object with the specialized facet.
  locale		loc_c = locale::classic();
  locale 		loc_1(locale::classic(), new numpunct<char>);

  // check names
  VERIFY( loc_c.name() == name_c );
  VERIFY( loc_1.name() == str_unnamed );

  // sanity check the constructed locale has the specialized facet.
  VERIFY( has_facet<numpunct<char> >(loc_1) );
  VERIFY( has_facet<numpunct<char> >(loc_c) );
  
  // extract facet
  const numpunct<char>&	f_nump_1 = use_facet<numpunct<char> >(loc_1); 
  const numpunct<char>&	f_nump_c = use_facet<numpunct<char> >(loc_c); 

  // attempt to re-synthesize classic locale
  locale		loc_2 = loc_1.combine<numpunct<char> >(loc_c);
  VERIFY( loc_2.name() == str_unnamed );
  VERIFY( loc_2 != loc_c );
}


void
test02()
{
  using namespace std;
  bool test = true;
  const string name_c("C");
  const string name_no("*");
  string str;

  // construct a locale object with the specialized facet.
  locale		loc_c = locale::classic();
  locale 		loc_1(locale::classic(), 
			      new numpunct_byname<char>("fr_FR"));
  locale		loc_fr("fr_FR");

  // check names
  VERIFY( loc_c.name() == name_c );
  VERIFY( loc_1.name() == name_no );

  // sanity check the constructed locale has the specialized facet.
  VERIFY( has_facet<numpunct<char> >(loc_1) );
  VERIFY( has_facet<numpunct<char> >(loc_c) );
  
  // attempt to re-synthesize classic locale
  locale		loc_2 = loc_1.combine<numpunct<char> >(loc_c);
  VERIFY( loc_2.name() == name_no );
  VERIFY( loc_2 != loc_c );

  // extract facet
  const numpunct<char>&	f_nump_1 = use_facet<numpunct<char> >(loc_1); 
  const numpunct<char>&	f_nump_2 = use_facet<numpunct<char> >(loc_2); 
  const numpunct<char>&	f_nump_c = use_facet<numpunct<char> >(loc_c); 
  const numpunct<char>&	f_nump_fr = use_facet<numpunct<char> >(loc_fr); 

  // sanity check the data is correct.
  char dp1 = f_nump_c.decimal_point();
  char th1 = f_nump_c.thousands_sep();
  string g1 = f_nump_c.grouping();
  string t1 = f_nump_c.truename();
  string f1 = f_nump_c.falsename();

  char dp2 = f_nump_1.decimal_point();
  char th2 = f_nump_1.thousands_sep();
  string g2 = f_nump_1.grouping();
  string t2 = f_nump_1.truename();
  string f2 = f_nump_1.falsename();

  char dp3 = f_nump_2.decimal_point();
  char th3 = f_nump_2.thousands_sep();
  string g3 = f_nump_2.grouping();
  string t3 = f_nump_2.truename();
  string f3 = f_nump_2.falsename();

  char dp4 = f_nump_fr.decimal_point();
  char th4 = f_nump_fr.thousands_sep();
  string g4 = f_nump_fr.grouping();
  string t4 = f_nump_fr.truename();
  string f4 = f_nump_fr.falsename();

#if 0
  // XXX these should not be the same if named locales are working correctly.
  VERIFY( dp1 != dp2 );
  VERIFY( th1 != th2 );
#endif

  VERIFY( dp1 == dp3 );
  VERIFY( th1 == th3 );
  VERIFY( t1 == t3 );
  VERIFY( f1 == f3 );
}


int main()
{
  test01();
  test02();
  return 0;
}
