/* Inflater.java - Decompress a data stream
   Copyright (C) 1999, 2000 Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.
 
GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA.

As a special exception, if you link this library with other files to
produce an executable, this library does not by itself cause the
resulting executable to be covered by the GNU General Public License.
This exception does not however invalidate any other reasons why the
executable file might be covered by the GNU General Public License. */

package java.util.zip;

import gnu.gcj.RawData;

/**
 * @author Tom Tromey
 * @date May 17, 1999
 */

/* Written using on-line Java Platform 1.2 API Specification
 * and JCL book.
 * Believed complete and correct.
 */

public class Inflater
{
  public native void end ();

  protected void finalize ()
  {
    end ();
  }

  public synchronized boolean finished ()
  {
    return is_finished;
  }

  public native int getAdler ();
  public native int getRemaining ();
  public native int getTotalIn ();
  public native int getTotalOut ();

  public int inflate (byte[] buf) throws DataFormatException
  {
    return inflate (buf, 0, buf.length);
  }

  public native int inflate (byte[] buf, int off, int len)
    throws DataFormatException;

  private native void init (boolean noHeader);

  public Inflater ()
  {
    this (false);
  }

  public Inflater (boolean noHeader)
  {
    init (noHeader);
  }

  public synchronized boolean needsDictionary ()
  {
    return dict_needed;
  }

  public synchronized boolean needsInput ()
  {
    return getRemaining () == 0;
  }

  public native void reset ();

  public void setDictionary (byte[] buf)
  {
    setDictionary (buf, 0, buf.length);
  }

  public native void setDictionary (byte[] buf, int off, int len);

  public void setInput (byte[] buf)
  {
    setInput (buf, 0, buf.length);
  }

  public native void setInput (byte[] buf, int off, int len);

  // The zlib stream.
  private RawData zstream;

  // True if finished.
  private boolean is_finished;

  // True if dictionary needed.
  private boolean dict_needed;
}
