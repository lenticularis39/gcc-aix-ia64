#include <cassert>
#include <iostream>

int bar ()
{
  throw 100;
}

int main ()
{
  int i = 0;
  try
    {
      i = bar ();
    }
  catch (...)
    {
    }

//  cout << "i = " << i << endl;
  assert (i == 0) ; 
}
