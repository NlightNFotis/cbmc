#include <assert.h>
#include <stdlib.h>

// clang-format off
int main() {
  char *a = malloc(1);

  assert(*a == *a);

  // BUG: no errors even with `--pointer-check` enabled -- now fixed.
  assert(
    __CPROVER_forall {
      int i ; (0 <= i && i < 1) ==> *(a+i) == *(a+i)
    }
  );
}
// clang-format on
