// 2000-08-17 Benjamin Kosnik <bkoz@cygnus.com>

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

// 22.2.1.5 - Template class codecvt [lib.locale.codecvt]

#include <locale>
#include <debug_assert.h>

// Required instantiation, degenerate conversion.
// codecvt<char, char, mbstate_t>
void test01()
{
  using namespace std;
  typedef codecvt_base::result			result;
  typedef codecvt<char, char, mbstate_t> 	c_codecvt;

  bool 			test = true;
  const char* 		c_lit = "black pearl jasmine tea";
  const char* 	        from_next;
  int 			size = 25;
  char* 		c_arr = new char[size];
  char*			to_next;

  locale 		loc;
  c_codecvt::state_type state;
  const c_codecvt* 	cvt = &use_facet<c_codecvt>(loc); 

  // in
  result r1 = cvt->in(state, c_lit, c_lit + size, from_next, 
		      c_arr, c_arr + size, to_next);
  VERIFY( r1 == codecvt_base::noconv );
  VERIFY( !strcmp(c_arr, c_lit) ); 
  VERIFY( from_next == c_lit );
  VERIFY( to_next == c_arr );

  // out
  result r2 = cvt->out(state, c_lit, c_lit + size, from_next, 
		       c_arr, c_arr + size, to_next);
  VERIFY( r2 == codecvt_base::noconv );
  VERIFY( !strcmp(c_arr, c_lit) ); 
  VERIFY( from_next == c_lit );
  VERIFY( to_next == c_arr );

  // unshift
  strcpy(c_arr, c_lit);
  result r3 = cvt->unshift(state, c_arr, c_arr + size, to_next);
  VERIFY( r3 == codecvt_base::noconv );
  VERIFY( !strcmp(c_arr, c_lit) ); 
  VERIFY( to_next == c_arr );

  int i = cvt->encoding();
  VERIFY( i == 1 );

  VERIFY( cvt->always_noconv() );

  int j = cvt->length(state, c_lit, c_lit + size, 5);
  VERIFY( j == 5 );

  int k = cvt->max_length();
  VERIFY( k == 1 );

  delete [] c_arr;
}

int main ()
{
  test01();

  return 0;
}
