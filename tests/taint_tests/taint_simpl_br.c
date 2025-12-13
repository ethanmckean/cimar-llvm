#include <stdio.h>
#include <stdlib.h>

int main() {
    int stack_arr[5] = {1, 2, 3, 4, 5};
    // The "Sink": We want to protect this variable from tainted influence
    int sensitive_config = 0; 

    printf("Starting Control Flow Taint Test (Simple)...\n");

    // OOB read returns tainted condition
    int tainted_condition = stack_arr[10];

    printf("recovered from OOB read, proceeding to branch...\n");

    // Branch taken based on tainted condition
    int val_to_write = 10;

    if (tainted_condition) {
        val_to_write = 999;
    } else {
        val_to_write = 555;
    }

    // Store with control-flow tainted value should be skipped
    sensitive_config = val_to_write;

    printf("Final sensitive_config value: %d\n", sensitive_config);

    return 0;
}