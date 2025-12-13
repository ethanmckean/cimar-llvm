#include "stdio.h"

int main(void){

    int arr[10] = {0};

    for(int i = 0; i <= 10; i++){
        arr[i] = i;
    }

    printf("arr[2] = %d\n", arr[2]);
    return 0;
}
