/* java.beans.beancontext.BeanContextMembershipEvent
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


package java.beans.beancontext;

import java.util.Collection;
import java.util.Arrays;
import java.util.Iterator;

/**
 * Event fired when children are added to or removed from a <code>BeanContext</code>.
 * Whether they were added or removed depends entirely on which method
 * of the listener interface was called.
 *
 * @author John Keiser
 * @since JDK1.2
 * @see java.beans.beancontext.BeanContextMembershipListener
 */

public class BeanContextMembershipEvent extends BeanContextEvent {
	/**
	 * The children that were added or removed.
	 */
	protected Collection children;

	/**
	 * Create a new membership event.
	 * @param context the event source.
	 * @param children the children added to or removed from the source.
	 */
	public BeanContextMembershipEvent(BeanContext context, Collection children) {
		super(context);
		this.children = children;
	}

	/**
	 * Create a new membership event.
	 * @param context the event source.
	 * @param children the children added to or removed from the source.
	 */
	public BeanContextMembershipEvent(BeanContext context, Object[] children) {
		super(context);
		this.children = Arrays.asList(children);
	}

	/**
	 * The number of children removed or added.
	 * @return the number of children removed or added.
	 */
	public int size() {
		return children.size();
	}

	/**
	 * An iterator that will step through all the children.
	 * @return an iterator over all the children.
	 */
	public Iterator iterator() {
		return children.iterator();
	}

	/**
	 * An array of the children.
	 * @return an array of the children.
	 */
	public Object[] toArray() {
		return children.toArray();
	}

	/**
	 * Tell whether the <code>Object</code> is one of the children added or removed.
	 * @param child the child to check.
	 * @return whether the <code>Object</code> is added or removed.
	 */
	public boolean contains(Object child) {
		return children.contains(child);
	}
}
