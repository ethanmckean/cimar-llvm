#include <stdio.h>
#include <stdlib.h>

int main() {
    int stack_arr[5] = {10, 20, 30, 40, 50};
    int accumulator = 100;

    printf("Starting Control Flow Taint Test (Diamond Merge)...\n");

    // Tainted source - OOB read
    int index_trigger = stack_arr[15];

    int calculated_val = 0;

    // Diamond control flow based on tainted trigger
    if (index_trigger > 50) {
        calculated_val = 5 * 5;
    } else {
        calculated_val = 3 * 3;
    }

    // Control flow paths merge here, 'calculated_val' inherits taint
    accumulator = calculated_val;
    if (accumulator == 100) {
        printf("SUCCESS: Diamond merge taint tracked. Accumulator protected.\n");
    } else {
        printf("FAILURE: Accumulator updated to %d. Merge taint logic failed.\n", accumulator);
    }

    return 0;
}