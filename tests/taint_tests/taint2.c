#include <stdio.h>
#include <stdlib.h>

int main() {
    int *array = (int*)malloc(sizeof(int) * 2);
    
    // 1. OOB Read -> Tainted Load (Value 0, Taint True)
    // 2. Div Logic -> (1000 / 1) -> Result 1000 (Taint True)
    // 3. Printf -> Consumes the value directly.
    //    We do NOT assign to a variable here, so there is no StoreInst to block.
    
    printf("Risk Result: %d\n", 1000 / array[10]);
    
    free(array);
    return 0;
}
