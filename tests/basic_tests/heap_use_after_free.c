#include <stdio.h>
#include <stdlib.h>

int main() {
    int *ptr = (int*)malloc(sizeof(int));
    *ptr = 42;
    
    free(ptr);
    
    int safe_val = 0;
    
    // ILLEGAL: Use After Free (Read)
    // CIMA should skip the load instruction "temp = *ptr"
    // safe_val should NOT be updated with garbage/poisoned data.
    safe_val = *ptr; 

    printf("Value: %d\n", safe_val);
    return 0;
}
