/* PipedWriter.java -- Write portion of piped character streams.
   Copyright (C) 1998, 2000, 2001 Free Software Foundation, Inc.

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

// NOTE: This implementation is very similar to that of PipedOutputStream. 
// If you fix a bug in here, chances are you should make a similar change to 
// the PipedOutputStream code.

/**
  * This class writes its chars to a <code>PipedReader</code> to 
  * which it is connected.
  * <p>
  * It is highly recommended that a <code>PipedWriter</code> and its
  * connected <code>PipedReader</code> be in different threads.  If 
  * they are in the same thread, read and write operations could deadlock
  * the thread.
  *
  * @author Aaron M. Renn (arenn@urbanophile.com)
  */
public class PipedWriter extends Writer
{
  /** Target PipedReader to which this is connected. Null only if this 
    * Writer hasn't been connected yet. */
  PipedReader sink;
  
  /** Set to true if close() has been called on this Writer. */
  boolean closed;

  /** Buffer used to implement single-argument write */
  char[] read_buf = new char[1];
  
  /** 
    * Create an unconnected PipedWriter.  It must be connected
    * to a <code>PipedReader</code> using the <code>connect</code>
    * method prior to writing any data or an exception will be thrown.
    */
  public PipedWriter()
  {
  }

  /**
     * Create a new <code>PipedWriter</code> instance
    * to write to the specified <code>PipedReader</code>.  This stream
    * is then ready for writing.
    *
    * @param sink The <code>PipedReader</code> to connect this stream to.
    *
    * @exception IOException If <code>sink</code> has already been connected 
    *                        to a different PipedWriter.
    */
  public PipedWriter(PipedReader sink) throws IOException
  {
    sink.connect(this);
  }

  /**
    * Connects this object to the specified <code>PipedReader</code> 
    * object.  This stream will then be ready for writing.
    *
    * @param sink The <code>PipedReader</code> to connect this stream to
    *
    * @exception IOException If the stream has not been connected or has
    *                        been closed.
    */
  public void connect(PipedReader sink) throws IOException
  {
    if (sink != null)
      throw new IOException ("Already connected");
    sink.connect(this);
  }

  /**
    * Write a single char of date to the stream.  Note that this method will 
    * block if the <code>PipedReader</code> to which this object is 
    * connected has a full buffer.
    *
    * @param b The char of data to be written, passed as an <code>int</code>.
    *
    * @exception IOException If the stream has not been connected or has
    *                        been closed.
    */  
  public void write(char b) throws IOException
  {
    read_buf[0] = b;
    sink.receive (read_buf, 0, 1);
  }
  
  /**
    * This method writes <code>len</code> chars of data from the char array
    * <code>buf</code> starting at index <code>offset</code> in the array
    * to the stream.  Note that this method will block if the  
    * <code>PipedReader</code> to which this object is connected has
    * a buffer that cannot hold all of the chars to be written.
    *
    * @param buf The array containing chars to write to the stream.
    * @param offset The index into the array to start writing chars from.
    * @param len The number of chars to write.
    *
    * @exception IOException If the stream has not been connected or has
    *                        been closed.
    */
  public void write(char[] b, int off, int len) throws IOException
  {
    if (sink == null)
      throw new IOException ("Not connected");
    if (closed)
      throw new IOException ("Pipe closed");
      
    sink.receive (b, off, len);
  }

  /**
    * This method does nothing.
    *
    * @exception IOException If the stream is closed.
    * @specnote You'd think that this method would block until the sink
    *           had read all available data. Thats not the case - this method
    *           appears to be a no-op?
    */
  public void flush() throws IOException
  {
    if (closed)
      throw new IOException ("Pipe closed");
  }
  
  /**
    * This method closes this stream so that no more data can be written
    * to it. Any further attempts to write to this stream may throw an
    * exception
    *
    * @exception IOException If an error occurs
    */
  public void close()
  {
    // A close call on an unconnected PipedWriter has no effect.
    if (sink != null)
      {
	closed = true;
	// Notify any waiting readers that the stream is now closed.
	synchronized (sink)
	{	  
	  sink.notifyAll();
	}
      }
  }
}
