#include <stdint.h>

int main()
{
    int32_t *a;
    int32_t *z = a + 2 * sizeof(int32_t);

    __CPROVER_assert(a != z, "expected true");
    __CPROVER_assert(a == z, "expected false");
}

// 32

// 64
