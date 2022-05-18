#define NULL (void *)0

int main()
{
    int *x;
    unsigned int z;
    __CPROVER_assume(z < 3);
    __CPROVER_assume(z > 1);
    int *y = x - z;
    __CPROVER_assume(x != NULL);
    __CPROVER_assume(y != NULL);

    __CPROVER_assert(y == x, "expected false after pointer manipulation");
    __CPROVER_assert(y != x, "expected true");
}
