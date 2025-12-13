#include <stdio.h>
#include <stdlib.h>

int main() {
    int *arr = (int*)malloc(5 * sizeof(int));

    // Initialize array with known values
    for (int i = 0; i < 5; i++) {
        arr[i] = (i + 1) * 10;
    }

    int value = arr[10];
    printf("OOB access at arr[10] returned: %d\n", value);
    printf("Program continued successfully!\n");

    free(arr);
    return 0;
}
