#include <stdio.h>
#include <stdlib.h>

int main() {
    int stack_arr[5] = {1, 2, 3, 4, 5};
    int accumulator = 1000;

    printf("Starting Indirect Propagation...\n");

    // OOB read returns tainted value
    int val = stack_arr[6];

    // Taint propagates through arithmetic operations
    int intermediate = val + 50;
    int final_calc = intermediate * 2;

    // Store with tainted value should be skipped
    accumulator = final_calc;

    if (accumulator == 1000) {
        printf("SUCCESS: Taint tracked through math. Accumulator protected.\n");
    } else {
        printf("FAILURE: Accumulator updated to %d (Expected 1000)\n", accumulator);
    }

    return 0;
}