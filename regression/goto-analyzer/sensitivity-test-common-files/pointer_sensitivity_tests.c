#include <assert.h>

int main(int argc, char *argv[])
{
  // Test if we can represent constant pointers
  int y=0;
  int z=0;
  int *x=&y;
  assert(x==&y);
  assert(x==&z);
  assert(*x==0);
  assert(*x==1);
  return 0;
}
