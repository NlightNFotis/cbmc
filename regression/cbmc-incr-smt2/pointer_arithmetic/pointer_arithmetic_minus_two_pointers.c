#define NULL (void *)0

int main()
{
    int *x;
    int *y;
    int *z = x - y;
    __CPROVER_assume(x != NULL);
    __CPROVER_assume(y != NULL);
    __CPROVER_assume(x != y);

    __CPROVER_assert(z == x, "expected false after pointer manipulation");
    __CPROVER_assert(z == y, "expected false after pointer manipulation");
    __CPROVER_assert(y != x, "expected true");
}
