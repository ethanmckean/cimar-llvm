#include <stdio.h>

int main() {
    int sensor_data[5]; // Size 5
    int i;

    printf("Starting Loop\n");

    for (i = 0; i < 10; i++) {
        sensor_data[i] = i * 10;
    }

    printf("Loop Finished. Valid Data: %d, %d\n", sensor_data[0], sensor_data[4]);
    return 0;
}
