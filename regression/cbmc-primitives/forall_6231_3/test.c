#include <assert.h>
#include <stdlib.h>

// This is essentially the same file as in test `forall_6231_1`, with the difference
// being that the forall statement contains a bigger bound, so that we are to have
// more concrete instantiations of the bound variable.

// clang-format off
int main() {
  char *a = malloc(10);

  // lots of errors with `--pointer-check` enabled
  assert(*a == *a);

  // no errors even with `--pointer-check` enabled
  assert(
    __CPROVER_forall {
      int i ; (0 <= i && i < 10) ==> *(a+i) == *(a+i)
    }
  );
}
// clang-format on
