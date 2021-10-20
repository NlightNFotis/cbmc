#include <assert.h>
#include <stdlib.h>

// clang-format off
int main() {
  char *a = malloc(128);

  assert(
    __CPROVER_forall {
      int i ; (0 <= i && i < 1) ==> *(a+i) == *(a+i)
    }
  );

  assert(
  __CPROVER_forall {
    int j; !(0 <= j && j < 1) || (j == 0 && *(a+j) == *(a+j))
  });
}
// clang-format on
