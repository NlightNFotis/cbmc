#include <assert.h>

int main(int argc, char *argv[])
{
  // Test how well we can represent pointers
  // Basic use of addresses
  int a=0;
  int b=0;
  int *x=&a;
  int *x2=&a;
  int *y=&b;
  assert(x==&a);
  assert(x==&b);
  assert(x==x2);
  assert(x==y);

  // Reading from a dereferenced pointer
  assert(*x==0);
  assert(*x==1);
  a=1;
  assert(*x==1);
  assert(*x==0);

  // Writing to a dereferenced pointer
  *x=2;
  assert(a==2);
  assert(a==0);

  // Reassign the pointer
  x=&b;

  // Basic use of addresses, after reassigning the pointer
  assert(x==&b);
  assert(x==&a);
  assert(x==y);
  assert(x==x2);

  // Reading from a dereferenced pointer, after reassigning the pointer
  assert(*x==0);
  assert(*x==1);
  b=1;
  assert(*x==1);
  assert(*x==0);

  // Writing to a dereferenced pointer, after reassigning the pointer
  *x=2;
  assert(b==2);
  assert(b==0);

  return 0;
}
