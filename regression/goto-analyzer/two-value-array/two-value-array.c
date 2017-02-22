#include <assert.h>

int main()
{
  int x[3]={0, 1, 2};
  assert(x[1]==1);
  assert(x[1]==2);
  return 0;
}
