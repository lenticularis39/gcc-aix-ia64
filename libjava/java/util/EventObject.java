/* EventObject.java - Represent events fired by objects.
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


package java.util;

import java.io.Serializable;

/**
 * Represents Events fired by Objects.
 *
 * @see EventListener
 */
public class EventObject implements Serializable
{
  private static final long serialVersionUID = 5516075349620653480L;
  protected transient Object source;

  /**
   * Constructs an EventObject with the specified source.
   */
  public EventObject(Object source)
  {
    this.source = source;
  }

  /**
   * @return The source of the Event.
   */
  public Object getSource()
  {
    return source;
  }

  /**
   * @return String representation of the Event.
   * @override toString in class Object
   */
  public String toString()
  {
    return this.getClass() + "[source=" + source.toString() + "]";
  }
}
