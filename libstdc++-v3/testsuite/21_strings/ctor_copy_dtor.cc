// 1999-06-04 bkoz

// Copyright (C) 1999, 2000, 2001 Free Software Foundation, Inc.
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

// 21.3.1 basic_string constructors.

#include <new>
#include <string>
#include <stdexcept>
#include <debug_assert.h>

int test01(void)
{
  bool test = true;
  typedef std::string::size_type csize_type;
  typedef std::string::iterator citerator;
  csize_type npos = std::string::npos;
  csize_type csz01, csz02;

  const char str_lit01[] = "rodeo beach, marin";
  const std::string str01(str_lit01);
  const std::string str02("baker beach, san francisco");

  // basic_string(const string&, size_type pos = 0, siz_type n = npos, alloc)
  csz01 = str01.size();
  try {
    std::string str03(str01, csz01 + 1);
    VERIFY( false );
  }		 
  catch(std::out_of_range& fail) {
    VERIFY( true );
  }
  catch(...) {
    VERIFY( false );
  }

  try {
    std::string str03(str01, csz01);
    VERIFY( str03.size() == 0 );
    VERIFY( str03.size() <= str03.capacity() );
  }		 
  catch(...) {
    VERIFY( false );
  }

#if 0
  // XXX These tests have been temporarily disabled.
  //http://gcc.gnu.org/ml/libstdc++/2000-10/msg00033.html
  // basic_string(const char* s, size_type n, alloc)
  csz01 = str01.max_size();
  // NB: As strlen(str_lit01) != csz01, this test is undefined. It
  // should not crash, but what gets constructed is a bit arbitrary.
  try {
    std::string str03(str_lit01, csz01 + 1);
    VERIFY( true );
  }		 
  catch(std::length_error& fail) {
    VERIFY( true );
  }
  catch(...) {
    VERIFY( false );
  }

  // NB: As strlen(str_lit01) != csz01, this test is undefined. It
  // should not crash, but what gets constructed is a bit arbitrary.
  // The "maverick's" of all string objects.
  try {
    std::string str04(str_lit01, npos); 
    VERIFY( true );
  }		 
  catch(std::length_error& fail) {
    VERIFY( true );
  }
  catch(...) {
    VERIFY( false );
  }

  // Build a maxsize - 1 lengthed string consisting of all A's
  try {
    std::string str03(csz01 - 1, 'A');
    VERIFY( str03.size() == csz01 - 1 );
    VERIFY( str03.size() <= str03.capacity() );
  }		 
  // NB: bad_alloc is regrettable but entirely kosher for
  // out-of-memory situations.
  catch(std::bad_alloc& fail) {
    VERIFY( true );
  }
  catch(...) {
    VERIFY( false );
  }
#endif

  // basic_string(const char* s, const allocator& a = allocator())
  std::string str04(str_lit01);
  VERIFY( str01 == str04 );


  // basic_string(size_type n, char c, const allocator& a = allocator())
  csz01 = str01.max_size();
  try {
    std::string str03(csz01 + 1, 'z');
    VERIFY( false );
  }		 
  catch(std::length_error& fail) {
    VERIFY( true );
  }
  catch(...) {
    VERIFY( false );
  }

  try {
    std::string str04(npos, 'b'); // the "maverick's" of all string objects.
    VERIFY( false );
  }		 
  catch(std::length_error& fail) {
    VERIFY( true );
  }
  catch(...) {
    VERIFY( false );
  }

  try {
    std::string str03(csz01 - 1, 'z');
    VERIFY( str03.size() != 0 );
    VERIFY( str03.size() <= str03.capacity() );
  }		 
  // NB: bad_alloc is regrettable but entirely kosher for
  // out-of-memory situations.
  catch(std::bad_alloc& fail) {
    VERIFY( true );
  }
  catch(...) {
    VERIFY( false );
  }


  // template<typename _InputIter>
  //   basic_string(_InputIter begin, _InputIter end, const allocator& a)
  std::string str06(str01.begin(), str01.end());
  VERIFY( str06 == str01 );

#ifdef DEBUG_ASSERT
  assert(test);
#endif
  return test;
}

void test02()
{
  bool test = true;

  // template<typename _InputIter>
  //   basic_string(_InputIter begin, _InputIter end, const allocator& a)
  // where _InputIter is integral [21.3.1 para 15]
  std::string s(10,0);
  VERIFY( s.size() == 10 );
#ifdef DEBUG_ASSERT
  assert(test);
#endif
}

void test03()
{
  bool test = true;
  const char* with_nulls = "This contains \0 a zero byte.";

  // These are tests to see how basic_string handles data with NUL
  // bytes.  Obviously basic_string(char*) will halt at the first one, but
  // nothing else should.
  std::string s1 (with_nulls, 28);
  VERIFY( s1.size() == 28 );
  std::string s2 (s1);
  VERIFY( s2.size() == 28 );

#ifdef DEBUG_ASSERT
  assert(test);
#endif
}

int main()
{ 
  test01();
  test02();
  test03();
  return 0;
}
