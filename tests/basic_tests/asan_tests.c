// asan_tests.c
#include <stdio.h>
#include <stdlib.h>

int global_arr[4] = {0, 1, 2, 3};

void heap_overflow() {
    int *p = (int *)malloc(4 * sizeof(int));
    for (int i = 0; i <= 4; i++) { // OOB when i == 4
        p[i] = i;
    }
    free(p);
}

void stack_overflow() {
    int a[4];
    for (int i = 0; i <= 4; i++) { // OOB when i == 4
        a[i] = i;
    }
}

void global_overflow() {
    for (int i = 0; i <= 4; i++) { // OOB when i == 4
        global_arr[i] = i;
    }
}

void use_after_free() {
    int *p = (int *)malloc(sizeof(int));
    *p = 42;
    free(p);
    *p = 13;   // UAF write
}

__attribute__((noinline))
int *ret_stack_addr() {
    int local = 123;
    return &local;
}

void use_after_return() {
    int *p = ret_stack_addr();
    // Stack frame of ret_stack_addr is gone now
    *p = 5; // UAR
}

void use_after_scope() {
    int *p;
    {
        int local = 10;
        p = &local;
    }           // 'local' is out of scope here
    *p = 20;    // UAS
}

void double_free() {
    int *p = (int *)malloc(sizeof(int));
    free(p);
    free(p); // double free
}

void invalid_free_stack() {
    int x = 5;
    int *p = &x;
    free(p); // invalid free (not heap)
}

void invalid_free_middle() {
    int *p = (int *)malloc(10 * sizeof(int));
    int *q = p + 5;
    free(q); // not the original pointer
    free(p); // this will also be bad now
}

void leak() {
    int *p = (int *)malloc(100 * sizeof(int));
    p[0] = 1;
    // no free -> leak
}

int main(void) {
    heap_overflow();
    stack_overflow();
    global_overflow();
    use_after_free();
    use_after_return();
    use_after_scope();
    double_free();
    invalid_free_stack();
    invalid_free_middle();
    leak();

    printf("All ASAN tests completed.\n");
    return 0;
}