/* SerializablePermission.java -- Basic permissions related to serialization.
   Copyright (C) 1998, 2000 Free Software Foundation, Inc.

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


package java.io;

import java.security.BasicPermission;

/**
  * This class models permissions related to serialization.  As a subclass
  * of <code>BasicPermission</code>, this class has permissions that have
  * a name only.  There is no associated action list.
  * <p>
  * There are currently two allowable permission names for this class:
  * <ul>
  * <li><code>enableSubclassImplementation</code> - Allows a subclass to
  * override the default serialization behavior of objects.
  * <li><code>enableSubstitution</code> - Allows substitution of one object
  * for another during serialization or deserialization.
  * </ul>
  *
  * @see java.security.BasicPermission
  *
  * @version 0.0
  *
  * @author Aaron M. Renn (arenn@urbanophile.com)
  */
public final class SerializablePermission extends BasicPermission
{

/*
 * Class Variables
 */

private static final String[] legal_names = { "enableSubclassImplementation",
					      "enableSubstitution" };
/*************************************************************************/

/*
 * Constructors
 */

/**
  * This method initializes a new instance of <code>SerializablePermission</code>
  * that has the specified name.
  *
  * @param name The name of the permission.
  *
  * @exception IllegalArgumentException If the name is not valid for this class.
  */
public
SerializablePermission(String name)
{
  this(name, null);
}

/*************************************************************************/

/**
  * This method initializes a new instance of <code>SerializablePermission</code>
  * that has the specified name and action list.  Note that the action list
  * is unused in this class.
  *
  * @param name The name of the permission.
  * @param actions The action list (unused).
  *
  * @exception IllegalArgumentException If the name is not valid for this class.
  */
public
SerializablePermission(String name, String actions)
{
  super(name, actions);

  for (int i = 0; i < legal_names.length; i++)
    if (legal_names[i].equals(name))
      return;

  throw new IllegalArgumentException("Bad permission name:  " + name);
}


} // class SerializablePermission

