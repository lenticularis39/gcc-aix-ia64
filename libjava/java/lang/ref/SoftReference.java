/* java.lang.ref.SoftReference
   Copyright (C) 1999 Free Software Foundation, Inc.

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


package java.lang.ref;

/**
 * A soft reference will be cleared, if the object is only softly
 * reachable and the garbage collection needs more memory.  The garbage
 * collection will use an intelligent strategy to determine which soft
 * references it should clear.  This makes a soft reference ideal for
 * caches.<br>
 *
 * @author Jochen Hoenicke 
 */
public class SoftReference 
  extends Reference
{
  /**
   * Create a new soft reference, that is not registered to any queue.
   * @param referent the object we refer to.
   */
  public SoftReference(Object referent)
  {
    super(referent);
  }

  /**
   * Create a new soft reference.
   * @param referent the object we refer to.
   * @param q the reference queue to register on.
   * @exception NullPointerException if q is null.
   */
  public SoftReference(Object referent, ReferenceQueue q)
  {
    super(referent, q);
  }
  
  /**
   * Returns the object, this reference refers to.
   * @return the object, this reference refers to, or null if the 
   * reference was cleared.
   */
  public Object get()
  {
    /* Why is this overloaded??? 
     * Maybe for a kind of LRU strategy. */
    return super.get();
  }
}
