// Copyright (C) 2000 Free Software Foundation, Inc.
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

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

// Written by Benjamin Kosnik <bkoz@cygnus.com>

#include <bits/std_locale.h>

namespace std {

#ifdef _GLIBCPP_USE_WCHAR_T
  // Definitions for static const data members of __enc_traits.
  const int __enc_traits::_S_max_size;
#endif /* _GLIBCPP_USE_WCHAR_T */

  codecvt<char, char, mbstate_t>::
  codecvt(size_t __refs)
  : __codecvt_abstract_base<char, char, mbstate_t>(__refs)
  { }

  codecvt<char, char, mbstate_t>::
  ~codecvt() { }
  
  codecvt_base::result
  codecvt<char, char, mbstate_t>::
  do_out(state_type& /*__state*/, const intern_type* __from, 
	 const intern_type* __from_end, const intern_type*& __from_next,
	 extern_type* __to, extern_type* __to_end, 
	 extern_type*& __to_next) const
  { 
    size_t __len = min(__from_end - __from, __to_end - __to);
    memcpy(__to, __from, __len);
    __from_next = __from; 
    __to_next = __to;
    return noconv;  
  }
  
  codecvt_base::result
  codecvt<char, char, mbstate_t>::
  do_unshift(state_type& /*__state*/, extern_type* __to,
             extern_type* /*__to_end*/, extern_type*& __to_next) const
  { 
    __to_next = __to; 
    return noconv; 
  }
  
  codecvt_base::result
  codecvt<char, char, mbstate_t>::
  do_in(state_type& /*__state*/, const extern_type* __from, 
	const extern_type* __from_end, const extern_type*& __from_next,
	intern_type* __to, intern_type* __to_end, 
	intern_type*& __to_next) const
  { 
    size_t __len = min(__from_end - __from, __to_end - __to);
    memcpy(__to, __from, __len);
    __from_next = __from; 
    __to_next = __to;
    return noconv;  
  }

  int 
  codecvt<char, char, mbstate_t>::
  do_encoding() const throw() 
  { return 1; }
  
  bool 
  codecvt<char, char, mbstate_t>::
  do_always_noconv() const throw() 
  { return true; }
  
  int 
  codecvt<char, char, mbstate_t>::
  do_length (const state_type& /*__state*/, const extern_type* __from,
	     const extern_type* __end, size_t __max) const
  { return min(__max, static_cast<size_t>(__end - __from)); }
  
  int 
  codecvt<char, char, mbstate_t>::
  do_max_length() const throw() 
  { return 1; }
  
#ifdef _GLIBCPP_USE_WCHAR_T
  // codecvt<wchar_t, char, mbstate_t> required specialization
  codecvt<wchar_t, char, mbstate_t>::
  codecvt(size_t __refs)
  : __codecvt_abstract_base<wchar_t, char, mbstate_t>(__refs) { }

  codecvt<wchar_t, char, mbstate_t>::
  ~codecvt() { }
  
  codecvt_base::result
  codecvt<wchar_t, char, mbstate_t>::
  do_out(state_type& __state, const intern_type* __from, 
	 const intern_type* __from_end, const intern_type*& __from_next,
	 extern_type* __to, extern_type* __to_end,
	 extern_type*& __to_next) const
  {
    result __ret = error;
    size_t __len = min(__from_end - __from, __to_end - __to);
    size_t __conv = wcsrtombs(__to, &__from, __len, &__state);

    if (__conv == __len)
      {
	__from_next = __from;
	__to_next = __to + __conv;
	__ret = ok;
      }
    else if (__conv > 0 && __conv < __len)
      {
	__from_next = __from;
	__to_next = __to + __conv;
	__ret = partial;
      }
    else
      __ret = error;
	
    return __ret; 
  }
  
  codecvt_base::result
  codecvt<wchar_t, char, mbstate_t>::
  do_unshift(state_type& /*__state*/, extern_type* __to,
	     extern_type* /*__to_end*/, extern_type*& __to_next) const
  {
    __to_next = __to;
    return noconv;
  }
  
  codecvt_base::result
  codecvt<wchar_t, char, mbstate_t>::
  do_in(state_type& __state, const extern_type* __from, 
	const extern_type* __from_end, const extern_type*& __from_next,
	intern_type* __to, intern_type* __to_end,
	intern_type*& __to_next) const
  {
    result __ret = error;
    size_t __len = min(__from_end - __from, __to_end - __to);
    size_t __conv = mbsrtowcs(__to, &__from, __len, &__state);

    if (__conv == __len)
      {
	__from_next = __from;
	__to_next = __to + __conv;
	__ret = ok;
      }
    else if (__conv > 0 && __conv < __len)
      {
	__from_next = __from;
	__to_next = __to + __conv;
	__ret = partial;
      }
    else
      __ret = error;
	
    return __ret; 
  }
  
  int 
  codecvt<wchar_t, char, mbstate_t>::
  do_encoding() const throw()
  { return 0; }
  
  bool 
  codecvt<wchar_t, char, mbstate_t>::
  do_always_noconv() const throw()
  { return false; }
  
  int 
  codecvt<wchar_t, char, mbstate_t>::
  do_length(const state_type& /*__state*/, const extern_type* __from,
	    const extern_type* __end, size_t __max) const
  { return min(__max, static_cast<size_t>(__end - __from)); }

  int 
  codecvt<wchar_t, char, mbstate_t>::
  do_max_length() const throw()
  { return 1; }
#endif //  _GLIBCPP_USE_WCHAR_T

} // namespace std


