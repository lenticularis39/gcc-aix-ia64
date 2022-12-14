// 2000-09-13 Benjamin Kosnik <bkoz@redhat.com>

// Copyright (C) 2000, 2001 Free Software Foundation
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

// 22.1.1.2 locale constructors and destructors [lib.locale.cons]

#include <cwchar> // for mbstate_t
#include <locale>
#include <stdexcept>
#include <debug_assert.h>

#if _GLIBCPP_USE_WCHAR_T
typedef std::codecvt<char, char, std::mbstate_t> 	      c_codecvt;
typedef std::codecvt_byname<char, char, std::mbstate_t>       c_codecvt_byname;
typedef std::codecvt<wchar_t, char, std::mbstate_t>	      w_codecvt;
typedef std::codecvt_byname<wchar_t, char, std::mbstate_t>    w_codecvt_byname;

class gnu_codecvt: public c_codecvt { }; 

class gnu_facet: public std::locale::facet
{
public:
  static std::locale::id id;
};

std::locale::id gnu_facet::id;

void test01()
{
  using namespace std;

  typedef unsigned short			unicode_t;
  typedef unicode_t				int_type;
  typedef char					ext_type;
  typedef __enc_traits				enc_type;
  typedef codecvt<int_type, ext_type, enc_type>	unicode_codecvt;

  bool test = true;
  string str1, str2;

  // construct a locale object with the C facet
  const locale& 	loc01 = locale::classic();

  // 1
  // template <class Facet> locale(const locale& other, Facet* f)
  // construct a locale object with the specialized facet.
  locale loc02(locale::classic(), new gnu_codecvt);
  VERIFY (loc01 != loc02);
  VERIFY (loc02.name() == "*");
  try
    {
      VERIFY (has_facet<gnu_codecvt>(loc02));
      VERIFY (has_facet<c_codecvt>(loc02));
      VERIFY (has_facet<w_codecvt>(loc02));
    }
  catch(...)
    { VERIFY( false ); }

  try 
    { use_facet<gnu_facet>(loc02); }
  catch(bad_cast& obj)
    { VERIFY( true ); }
  catch(...)
    { VERIFY( false ); }

  // unicode_codecvt
  locale loc13(locale::classic(), new unicode_codecvt);  
  VERIFY (loc01 != loc13);
  VERIFY (loc13.name() == "*");
  try 
    {
      VERIFY (has_facet<c_codecvt>(loc13));
      VERIFY (has_facet<w_codecvt>(loc13));
      VERIFY (has_facet<unicode_codecvt>(loc13));
    }
  catch(...)
    { VERIFY( false ); }

  try 
    { use_facet<gnu_facet>(loc13); }
  catch(bad_cast& obj)
    { VERIFY( true ); }
  catch(...)
    { VERIFY( false ); }

  // 2
  // locale() throw()
  locale loc03;
  VERIFY (loc03 == loc01);
  VERIFY (loc03.name() == "C");
  locale loc04 = locale::global(loc02);
  locale loc05;
  VERIFY (loc05 != loc03);
  VERIFY (loc05 == loc02);

  // 3
  // explicit locale(const char* std_name)
  locale loc06("fr_FR");
  VERIFY (loc06 != loc01);  
  VERIFY (loc06 != loc02);  
  VERIFY (loc06.name() == "fr_FR");
  locale loc07("");
  VERIFY (loc07 != loc01);  
  VERIFY (loc07 != loc02);  
  VERIFY (loc07.name() == "");
  try
    { locale loc08(static_cast<const char*>(NULL)); }
  catch(runtime_error& obj)
    { VERIFY (true); }
  catch(...)
    { VERIFY (false); }

  // 4
  // locale(const locale& other, const char* std_name, category)
  {
    // This is the same as 5 only use "C" for loc("C")
    locale loc09(loc06, "C", locale::ctype);
    VERIFY (loc09.name() != "fr_FR");
    VERIFY (loc09.name() != "C");
    VERIFY (loc09.name() != "*");
    VERIFY (loc09 != loc01);  
    VERIFY (loc09 != loc06);  

    locale loc10(loc02, "C", locale::ctype);
    VERIFY (loc10.name() == "*");
    VERIFY (loc10 != loc01);   // As not named, even tho facets same...
    VERIFY (loc10 != loc02);  

    locale loc11(loc01, "C", locale::ctype);
    VERIFY (loc11.name() == "C");
    VERIFY (loc11 == loc01);  

    try
      { locale loc12(loc01, static_cast<const char*>(NULL), locale::ctype); }
    catch(runtime_error& obj)
      { VERIFY (true); }
    catch(...)
      { VERIFY (false); }

    try
      { locale loc13(loc01, "localized by the wu-tang clan", locale::ctype); }
    catch(runtime_error& obj)
      { VERIFY (true); }
    catch(...)
      { VERIFY (false); }

    locale loc14(loc06, "C", locale::none);
    VERIFY (loc14.name() == "fr_FR");
    VERIFY (loc14 == loc06);  

    locale loc15(loc06, "C", locale::collate);
    VERIFY (loc15.name() != "fr_FR");
    VERIFY (loc15.name() != "C");
    VERIFY (loc15.name() != "*");
    VERIFY (loc15.name() != loc09.name());
    VERIFY (loc15 != loc01);  
    VERIFY (loc15 != loc06);  
    VERIFY (loc15 != loc09);  
  }

  // 5
  // locale(const locale& other, const locale& one, category)
  {
    // This is the exact same as 4, with locale("C") for "C"
    locale loc09(loc06, loc01, locale::ctype);
    VERIFY (loc09.name() != "fr_FR");
    VERIFY (loc09.name() != "C");
    VERIFY (loc09.name() != "*");
    VERIFY (loc09 != loc01);  
    VERIFY (loc09 != loc06);  

    locale loc10(loc02, loc01, locale::ctype);
    VERIFY (loc10.name() == "*");
    VERIFY (loc10 != loc01);   // As not named, even tho facets same...
    VERIFY (loc10 != loc02);  

    locale loc11(loc01, loc01, locale::ctype);
    VERIFY (loc11.name() == "C");
    VERIFY (loc11 == loc01);  

    try
      { locale loc12(loc01, static_cast<const char*>(NULL), locale::ctype); }
    catch(runtime_error& obj)
      { VERIFY (true); }
    catch(...)
      { VERIFY (false); }

    try
      { locale loc13(loc01, locale("wu-tang clan"), locale::ctype); }
    catch(runtime_error& obj)
      { VERIFY (true); }
    catch(...)
      { VERIFY (false); }

    locale loc14(loc06, loc01, locale::none);
    VERIFY (loc14.name() == "fr_FR");
    VERIFY (loc14 == loc06);  

    locale loc15(loc06, loc01, locale::collate);
    VERIFY (loc15.name() != "fr_FR");
    VERIFY (loc15.name() != "C");
    VERIFY (loc15.name() != "*");
    VERIFY (loc15.name() != loc09.name());
    VERIFY (loc15 != loc01);  
    VERIFY (loc15 != loc06);  
    VERIFY (loc15 != loc09);  
  }
}
#endif /* !defined(_GLIBCPP_USE_WCHAR_T) */

int main ()
{
#if _GLIBCPP_USE_WCHAR_T
  test01();
#endif 

  return 0;
}
