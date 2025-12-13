#include <stdio.h>
#include <stdlib.h>

int main() {
    int *array = (int*)malloc(sizeof(int) * 2);
    array[0] = 10;
    array[1] = 20;

    int val = -1;

    // OOB read returns tainted value, store should be skipped
    val = array[5];

    printf("Recovered Value: %d\n", val);
    
    free(array);
    return 0;
}
