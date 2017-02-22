#include <assert.h>

int main()
{
  int y=0;
  int z=0;
  int *x=&y;
  assert(x==&y);
  assert(x==&z);
  return 0;
}
