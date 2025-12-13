#include <stdio.h>

int main() {
     volatile int x = 0;
    for (int i = 0; i < 10; i++) {
        x = x + 1;
    }
    printf("Hello World: %d\n", x);
    return 0;
}
