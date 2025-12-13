#include <stdio.h>
#include <stdlib.h>

int main() {
    int *buffer = (int*)malloc(10 * sizeof(int));

    int crucial_flag = 777;

    printf("Original Flag: %d\n", crucial_flag);

    // OOB read returns tainted value
    int tainted_val = buffer[20];

    // Store with tainted value should be skipped
    crucial_flag = tainted_val;

    printf("Final Flag: %d\n", crucial_flag);

    free(buffer);
    return 0;
}