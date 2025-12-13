#include <stdio.h>
#include <stdlib.h>

int main() {
    int *heap_arr = (int*)malloc(4 * sizeof(int));
    
    printf("Attempting Direct Write OOB...\n");

    // Direct OOB write
    heap_arr[100] = 9999;

    printf("Survived the write!\n");

    // Verify execution continues
    int y = 10 + 20;
    printf("Math works: %d\n", y);

    free(heap_arr);
    return 0;
}