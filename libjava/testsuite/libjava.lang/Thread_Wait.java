// Test basic thread creation and wait/notify functionality.
// Origin: Bryce McKinlay <bryce@albatross.co.nz>

public class Thread_Wait implements Runnable
{
  public static void main(String args[])
  {
    new Thread_Wait();
  }
  
  public Thread_Wait()
  {
    System.out.println("creating thread");
    Thread t = new Thread(this);
    t.start();
    
    try
    {
      Thread.sleep(100);
    }
    catch (Exception x)
    {
      System.out.println("exception occured: " + x);
    }

    synchronized (this)
    {
      System.out.println("notifying other thread");
      notify();
    }
  }
  
  public void run()
  {
    System.out.println ("new thread running");
    synchronized (this)
    {
      try
      {
	wait();
      }
      catch (Exception x)
      {
        System.out.println("exception occured: " + x);
      }
    }    
    System.out.println ("thread notified okay");
  }
}
