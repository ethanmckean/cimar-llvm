#include <stdio.h>

int main() {
    int sensor_data[5]; // Size 5
    int i;
    for (i = 0; i < 5; i++) {
        sensor_data[i] = i;
    }

    int x = 0;
    for (i = 0; i < 10; i++) {
        x = sensor_data[i];
        x = x + 3;
    }

    printf("Final x: %d, Valid Data: %d, %d\n", x, sensor_data[0], sensor_data[4]);
    return 0;
}
