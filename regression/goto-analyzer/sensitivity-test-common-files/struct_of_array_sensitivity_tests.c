#include <assert.h>

int main(int argc, char *argv[])
{
  // Test how well we can represent structs
  struct int_array_float_array
  {
  	int a[3];
  	float b[3];
  };
  struct int_array_float_array x={.a:{0, 1, 2}, .b:{3.0f, 4.0f, 5.0f}};
  assert(x.a[0]==0);
  assert(*(x.a+0)==0);
  assert(*(0+x.a)==0);
  assert(0[x.a]==0);

  // Test merging when there is only one value on both paths
  if(argc>2)
  {
    x.a[0]=0;
  }
  assert(x.a[0]==0);
  assert(x.a[1]==1);
  assert(x.b[0]==3.0f);

  // Test merging when there is one value for a and two values for b, to test if
  // we are representing them separately
  if(argc>3)
  {
    x.a[0]=0;
    x.b[2]=15.0f;
  }
  assert(x.a[0]==0);
  assert(x.a[1]==1);
  assert(x.b[2]>0.0f);
  assert(x.b[2]==15.0f);
  assert(x.b[2]==1.0f);
  assert(x.b[0]==3.0f);

  // Test merging when there are two values for a and b
  if(argc>4)
  {
    x.a[0]=11;
    x.b[2]=25.0f;
  }
  assert(x.a[0]<12);
  assert(x.a[0]>2);
  assert(x.a[0]==0);
  assert(x.a[1]==1);

  return 0;
}
