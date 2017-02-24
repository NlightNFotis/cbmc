#include <assert.h>

int main(int argc, char *argv[])
{
  // Test if we can represent constant ints, and also that the transformers are
  // working correctly.
  int x=0;
  assert(x==0);
  assert(x==1);
  assert(x==argc);

  assert(x<1);
  assert(x<-1);
  assert(x<argc);
  
  assert(x>-1);
  assert(x>1);
  assert(x>argc);
  
  assert(x!=1);
  assert(x!=0);
  assert(x!=argc);
  
  assert(!(x==1));
  assert(!(x==0));
  assert(!(x==argc));
  
  return 0;
}
