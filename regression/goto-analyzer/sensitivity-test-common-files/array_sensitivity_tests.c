#include <assert.h>

int main(int argc, char *argv[])
{
  // The variables i, j and k will be used as indexes into arrays of size 3.
  // They can each take two different values. For i both of these values are
  // valid indexes, for j one is and one isn't, and for k neither of them are.
  int i, j, k;
  if(argc==2)
  {
    i = 1;
    j = 1;
    k = 99;
  }
  else
  {
    i = 2;
    j = 100;
    k = 100;
  }

  // Test if we can represent uniformly constant arrays
  int x[3]={0, 0, 0};
  assert(x[1]==0);
  assert(x[1]==1);
  assert(x[i]==0);
  assert(x[i]==1);
  assert(x[j]==0);
  assert(x[j]==1);
  assert(x[100]==0);
  assert(x[k]==0);


  // Test if we can represent constant arrays which aren't uniform
  int x[3]={0, 0, 1};
  assert(x[1]==0);
  assert(x[1]==1);
  assert(x[i]==0);
  assert(x[i]==1);
  assert(x[j]==0);
  assert(x[j]==1);
  assert(x[100]==0);
  assert(x[k]==0);

  // Test if we can detect that we are writing to an array element that may be
  // out of bounds, or is definitely out of bounds
  int x[3]={0, 0, 0};
  x[i]=1;
  assert(x[0]==0);
  assert(x[0]==1);
  assert(x[1]==1);
  assert(x[1]==0);

  int x[3]={0, 0, 0};
  x[j]=1;
  assert(x[0]==0);
  assert(x[0]==1);
  assert(x[1]==1);
  assert(x[1]==0);

  int x[3]={0, 0, 0};
  x[k]=1;
  assert(x[0]==0);
  assert(x[0]==1);
  assert(x[1]==1);
  assert(x[1]==0);

  return 0;
}
