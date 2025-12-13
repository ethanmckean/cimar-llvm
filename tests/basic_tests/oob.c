#include <stdio.h>
#include <stdlib.h>

int main() {
    int *arr = (int*)malloc(5 * sizeof(int));

    arr[0] = 10;
    arr[10] = 999;

    printf("Execution continued past the violation\n");

    free(arr);
    return 0;
}
