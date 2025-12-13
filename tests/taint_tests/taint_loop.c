#include <stdio.h>
#include <stdlib.h>

int main() {
    int arr[5] = {1, 1, 1, 1, 1};
    int safe_sum = 0;
    
    printf("Starting Loop Test...\n");

    // Loop goes OOB for the last 5 iterations
    for (int i = 0; i < 10; i++) {
        // OOB reads return 0 and are tainted
        int val = arr[i];
        safe_sum += val;
    }
    
    printf("Final Sum: %d\n", safe_sum);

    return 0;
}