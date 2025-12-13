#include <stdio.h>

int main() {
    // 1. Declare an array (aggregate type)
    // We initialize it to avoid undefined behavior during the load.
    int arr[10] = {0}; 

    // 2. A loop to generate repeated Load/Store patterns
    for (int i = 0; i < 10; i++) {
        // This line is the key target for your pass:
        // It performs a LOAD from arr[i]
        // It performs an arithmetic operation
        // It performs a STORE back to arr[i]
        arr[i] = arr[i] + 1;
    }

    // 3. Verify the result to ensure instrumentation didn't break logic
    printf("Result at index 9: %d\n", arr[9]);
    
    return 0;
}
