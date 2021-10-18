#include <assert.h>
#include <stdlib.h>

int main() {
  char *a = malloc(1);

  // lots of errors with `--pointer-check` enabled
  assert(*a == *a);

  // no errors even with `--pointer-check` enabled
  assert(
    __CPROVER_forall {
      int i ; (0 <= i && i < 1) ==> *(a+i) == *(a+i)
    }
  );
}
