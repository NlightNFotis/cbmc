#include <assert.h>

int main(int argc, char *argv[])
{
  // Test if we can represent constant structs
  struct int_float
  {
  	int a;
  	float b;
  };
  struct int_float x = {0, 1.0};
  assert(x.a==0);
  assert(x.a==1);
  return 0;
}
