#include <stdio.h>
#include <string.h>

int main() {
    char buffer[10];

    strcpy(buffer, "Safe");

    buffer[15] = 'X';

    printf("Status: Alive\n");
    return 0;
}
