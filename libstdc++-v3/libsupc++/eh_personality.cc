// -*- C++ -*- The GNU C++ exception personality routine.
// Copyright (C) 2001 Free Software Foundation, Inc.
//
// This file is part of GNU CC.
//
// GNU CC is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// GNU CC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with GNU CC; see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.


#include <bits/c++config.h>
#include <cstdlib>
#include "unwind-cxx.h"

using namespace __cxxabiv1;

#include "unwind-pe.h"


struct lsda_header_info
{
  _Unwind_Ptr Start;
  _Unwind_Ptr LPStart;
  _Unwind_Ptr ttype_base;
  const unsigned char *TType;
  const unsigned char *action_table;
  unsigned char ttype_encoding;
  unsigned char call_site_encoding;
};

static const unsigned char *
parse_lsda_header (_Unwind_Context *context, const unsigned char *p,
		   lsda_header_info *info)
{
  _Unwind_Ptr tmp;
  unsigned char lpstart_encoding;

  info->Start = (context ? _Unwind_GetRegionStart (context) : 0);

  // Find @LPStart, the base to which landing pad offsets are relative.
  lpstart_encoding = *p++;
  if (lpstart_encoding != DW_EH_PE_omit)
    p = read_encoded_value (context, lpstart_encoding, p, &info->LPStart);
  else
    info->LPStart = info->Start;

  // Find @TType, the base of the handler and exception spec type data.
  info->ttype_encoding = *p++;
  if (info->ttype_encoding != DW_EH_PE_omit)
    {
      p = read_uleb128 (p, &tmp);
      info->TType = p + tmp;
    }
  else
    info->TType = 0;

  // The encoding and length of the call-site table; the action table
  // immediately follows.
  info->call_site_encoding = *p++;
  p = read_uleb128 (p, &tmp);
  info->action_table = p + tmp;

  return p;
}

static const std::type_info *
get_ttype_entry (lsda_header_info *info, long i)
{
  _Unwind_Ptr ptr;

  i *= size_of_encoded_value (info->ttype_encoding);
  read_encoded_value_with_base (info->ttype_encoding, info->ttype_base,
				info->TType - i, &ptr);

  return reinterpret_cast<const std::type_info *>(ptr);
}

static bool
check_exception_spec (lsda_header_info *info, const std::type_info *throw_type,
		      long filter_value)
{
  const unsigned char *e = info->TType - filter_value - 1;

  while (1)
    {
      const std::type_info *catch_type;
      _Unwind_Ptr tmp;
      void *dummy;

      e = read_uleb128 (e, &tmp);

      // Zero signals the end of the list.  If we've not found
      // a match by now, then we've failed the specification.
      if (tmp == 0)
        return false;

      // Match a ttype entry.
      catch_type = get_ttype_entry (info, tmp);
      if (catch_type->__do_catch (throw_type, &dummy, 1))
	return true;
    }
}

// Using a different personality function name causes link failures
// when trying to mix code using different exception handling models.
#ifdef _GLIBCPP_SJLJ_EXCEPTIONS
#define PERSONALITY_FUNCTION	__gxx_personality_sj0
#define __builtin_eh_return_data_regno(x) x
#else
#define PERSONALITY_FUNCTION	__gxx_personality_v0
#endif

extern "C" _Unwind_Reason_Code
PERSONALITY_FUNCTION (int version,
		      _Unwind_Action actions,
		      _Unwind_Exception_Class exception_class,
		      struct _Unwind_Exception *ue_header,
		      struct _Unwind_Context *context)
{
  __cxa_exception *xh = __get_exception_header_from_ue (ue_header);

  enum found_handler_type
  {
    found_nothing,
    found_terminate,
    found_cleanup,
    found_handler
  } found_type;

  lsda_header_info info;
  const unsigned char *language_specific_data;
  const unsigned char *action_record;
  const unsigned char *p;
  _Unwind_Ptr landing_pad, ip;
  int handler_switch_value;
  void *adjusted_ptr = xh + 1;

  // Interface version check.
  if (version != 1)
    return _URC_FATAL_PHASE1_ERROR;

  // Shortcut for phase 2 found handler for domestic exception.
  if (actions == (_UA_CLEANUP_PHASE | _UA_HANDLER_FRAME)
      && exception_class == __gxx_exception_class)
    {
      handler_switch_value = xh->handlerSwitchValue;
      landing_pad = (_Unwind_Ptr) xh->catchTemp;
      found_type = (landing_pad == 0 ? found_terminate : found_handler);
      goto install_context;
    }

  language_specific_data = (const unsigned char *)
    _Unwind_GetLanguageSpecificData (context);

  // If no LSDA, then there are no handlers or cleanups.
  if (! language_specific_data)
    return _URC_CONTINUE_UNWIND;

  // Parse the LSDA header.
  p = parse_lsda_header (context, language_specific_data, &info);
  info.ttype_base = base_of_encoded_value (info.ttype_encoding, context);
  ip = _Unwind_GetIP (context) - 1;
  landing_pad = 0;
  action_record = 0;
  handler_switch_value = 0;

#ifdef _GLIBCPP_SJLJ_EXCEPTIONS
  // The given "IP" is an index into the call-site table, with two
  // exceptions -- -1 means no-action, and 0 means terminate.  But
  // since we're using uleb128 values, we've not got random access
  // to the array.
  if ((int) ip < 0)
    return _URC_CONTINUE_UNWIND;
  else if (ip == 0)
    {
      // Fall through to set found_terminate.
    }
  else
    {
      _Unwind_Ptr cs_lp, cs_action;
      do
	{
	  p = read_uleb128 (p, &cs_lp);
	  p = read_uleb128 (p, &cs_action);
	}
      while (--ip);

      // Can never have null landing pad for sjlj -- that would have
      // been indicated by a -1 call site index.
      landing_pad = cs_lp + 1;
      if (cs_action)
	action_record = info.action_table + cs_action - 1;
      goto found_something;
    }
#else
  // Search the call-site table for the action associated with this IP.
  while (p < info.action_table)
    {
      _Unwind_Ptr cs_start, cs_len, cs_lp, cs_action;

      // Note that all call-site encodings are "absolute" displacements.
      p = read_encoded_value (0, info.call_site_encoding, p, &cs_start);
      p = read_encoded_value (0, info.call_site_encoding, p, &cs_len);
      p = read_encoded_value (0, info.call_site_encoding, p, &cs_lp);
      p = read_uleb128 (p, &cs_action);

      // The table is sorted, so if we've passed the ip, stop.
      if (ip < info.Start + cs_start)
	p = info.action_table;
      else if (ip < info.Start + cs_start + cs_len)
	{
	  if (cs_lp)
	    landing_pad = info.LPStart + cs_lp;
	  if (cs_action)
	    action_record = info.action_table + cs_action - 1;
	  goto found_something;
	}
    }
#endif // _GLIBCPP_SJLJ_EXCEPTIONS

  // If ip is not present in the table, call terminate.  This is for
  // a destructor inside a cleanup, or a library routine the compiler
  // was not expecting to throw.
  found_type = (actions & _UA_FORCE_UNWIND ? found_nothing : found_terminate);
  goto do_something;

 found_something:
  if (landing_pad == 0)
    {
      // If ip is present, and has a null landing pad, there are
      // no cleanups or handlers to be run.
      found_type = found_nothing;
    }
  else if (action_record == 0)
    {
      // If ip is present, has a non-null landing pad, and a null
      // action table offset, then there are only cleanups present.
      // Cleanups use a zero switch value, as set above.
      found_type = found_cleanup;
    }
  else
    {
      // Otherwise we have a catch handler or exception specification.

      signed long ar_filter, ar_disp;
      const std::type_info *throw_type, *catch_type;
      bool saw_cleanup = false;
      bool saw_handler = false;

      // During forced unwinding, we only run cleanups.  With a foreign
      // exception class, there's no exception type.
      // ??? What to do about GNU Java and GNU Ada exceptions.

      if ((actions & _UA_FORCE_UNWIND)
	  || exception_class != __gxx_exception_class)
	throw_type = 0;
      else
	throw_type = xh->exceptionType;

      while (1)
	{
	  _Unwind_Ptr tmp;

	  p = action_record;
	  p = read_sleb128 (p, &tmp); ar_filter = tmp;
	  read_sleb128 (p, &tmp); ar_disp = tmp;

	  if (ar_filter == 0)
	    {
	      // Zero filter values are cleanups.
	      saw_cleanup = true;
	    }
	  else if (ar_filter > 0)
	    {
	      // Positive filter values are handlers.
	      catch_type = get_ttype_entry (&info, ar_filter);
	      adjusted_ptr = xh + 1;

	      // Null catch type is a catch-all handler.  We can catch
	      // foreign exceptions with this.
	      if (! catch_type)
		{
		  if (!(actions & _UA_FORCE_UNWIND))
		    {
		      saw_handler = true;
		      break;
		    }
		}
	      else if (throw_type)
		{
		  // Pointer types need to adjust the actual pointer, not
		  // the pointer to pointer that is the exception object.
		  // This also has the effect of passing pointer types
		  // "by value" through the __cxa_begin_catch return value.
		  if (throw_type->__is_pointer_p ())
		    adjusted_ptr = *(void **) adjusted_ptr;

		  if (catch_type->__do_catch (throw_type, &adjusted_ptr, 1))
		    {
		      saw_handler = true;
		      break;
		    }
		}
	    }
	  else
	    {
	      // Negative filter values are exception specifications.
	      // ??? How do foreign exceptions fit in?  As far as I can
	      // see we can't match because there's no __cxa_exception
	      // object to stuff bits in for __cxa_call_unexpected to use.
	      if (throw_type
		  && ! check_exception_spec (&info, throw_type, ar_filter))
		{
		  saw_handler = true;
		  break;
		}
	    }

	  if (ar_disp == 0)
	    break;
	  action_record = p + ar_disp;
	}

      if (saw_handler)
	{
	  handler_switch_value = ar_filter;
	  found_type = found_handler;
	}
      else
	found_type = (saw_cleanup ? found_cleanup : found_nothing);
    }

 do_something:
   if (found_type == found_nothing)
     return _URC_CONTINUE_UNWIND;

  if (actions & _UA_SEARCH_PHASE)
    {
      if (found_type == found_cleanup)
	return _URC_CONTINUE_UNWIND;

      // For domestic exceptions, we cache data from phase 1 for phase 2.
      if (exception_class == __gxx_exception_class)
        {
          xh->handlerSwitchValue = handler_switch_value;
          xh->actionRecord = action_record;
          xh->languageSpecificData = language_specific_data;
          xh->adjustedPtr = adjusted_ptr;

          // ??? Completely unknown what this field is supposed to be for.
          // ??? Need to cache TType encoding base for call_unexpected.
          xh->catchTemp = (void *) (_Unwind_Ptr) landing_pad;
	}
      return _URC_HANDLER_FOUND;
    }

 install_context:
  if (found_type == found_terminate)
    {
      __cxa_begin_catch (&xh->unwindHeader);
      __terminate (xh->terminateHandler);
    }

  // Cache the TType base value for __cxa_call_unexpected, as we won't
  // have an _Unwind_Context then.
  if (handler_switch_value < 0)
    {
      parse_lsda_header (context, xh->languageSpecificData, &info);
      xh->catchTemp = (void *) base_of_encoded_value (info.ttype_encoding,
						      context);
    }

  _Unwind_SetGR (context, __builtin_eh_return_data_regno (0),
		 (_Unwind_Ptr) &xh->unwindHeader);
  _Unwind_SetGR (context, __builtin_eh_return_data_regno (1),
		 handler_switch_value);
  _Unwind_SetIP (context, landing_pad);
  return _URC_INSTALL_CONTEXT;
}

extern "C" void
__cxa_call_unexpected (_Unwind_Exception *exc_obj)
{
  __cxa_begin_catch (exc_obj);

  // This function is a handler for our exception argument.  If we exit
  // by throwing a different exception, we'll need the original cleaned up.
  struct end_catch_protect
  {
    end_catch_protect() { }
    ~end_catch_protect() { __cxa_end_catch(); }
  } end_catch_protect_obj;

  __cxa_exception *xh = __get_exception_header_from_ue (exc_obj);

  try {
    __unexpected (xh->unexpectedHandler);
  } catch (...) {
    // Get the exception thrown from unexpected.
    // ??? Foreign exceptions can't be stacked this way.

    __cxa_eh_globals *globals = __cxa_get_globals_fast ();
    __cxa_exception *new_xh = globals->caughtExceptions;

    // We don't quite have enough stuff cached; re-parse the LSDA.
    lsda_header_info info;
    parse_lsda_header (0, xh->languageSpecificData, &info);
    info.ttype_base = (_Unwind_Ptr) xh->catchTemp;

    // If this new exception meets the exception spec, allow it.
    if (check_exception_spec (&info, new_xh->exceptionType,
			      xh->handlerSwitchValue))
      throw;

    // If the exception spec allows std::bad_exception, throw that.
    const std::type_info &bad_exc = typeid (std::bad_exception);
    if (check_exception_spec (&info, &bad_exc, xh->handlerSwitchValue))
      throw std::bad_exception ();

    // Otherwise, die.
    __terminate(xh->terminateHandler);
  }
}
