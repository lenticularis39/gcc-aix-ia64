/* Copyright (C) 2000  Free Software Foundation

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */
 
package javax.naming;

import java.lang.Exception;

public class InvalidSearchFilterException extends NamingException
{
  public InvalidSearchFilterException ()
  {
    super ();
  }

  public InvalidSearchFilterException (String msg)
  {
    super (msg);
  }
}
