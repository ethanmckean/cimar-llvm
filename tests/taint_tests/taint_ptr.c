#include <stdio.h>
#include <stdlib.h>

int main() {
    int unsafe_source[5] = {0, 0, 0, 0, 0};
    int target_array[5] = {10, 20, 30, 40, 50};
    
    printf("--- Test 1: Tainted Pointer Arithmetic ---\n");

    // OOB read returns tainted offset
    int tainted_offset = unsafe_source[6];

    printf("Tainted Offset: %d\n", tainted_offset);

    // Pointer arithmetic with tainted offset
    int *ptr = &target_array[tainted_offset];

    // Store to tainted pointer should be skipped
    *ptr = 999;

    if (target_array[0] == 10) {
        printf("SUCCESS: Store to tainted address was skipped.\n");
    } else if (target_array[0] == 999) {
        printf("FAILURE: Store occurred! target_array[0] overwritten.\n");
    } else {
        printf("UNKNOWN: target_array[0] = %d\n", target_array[0]);
    }

    return 0;
}