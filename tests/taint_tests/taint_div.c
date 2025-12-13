#include <stdio.h>
#include <stdlib.h>

int main() {
    int *array = (int*)malloc(5 * sizeof(int));
    
    // Initialize array
    for(int i=0; i<5; i++) array[i] = 0;

    printf("Starting Division Test...\n");

    // OOB read returns tainted divisor
    int divisor = array[10];

    int result = 100;
    result = result / divisor;

    printf("Result: %d\n", result);

    free(array);
    return 0;
}