#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int a; // Offset 0
    int b; // Offset 4
} Point;

int main() {
    int oob_src[1] = {0};
    Point p = {100, 200};
    
    printf("Testing struct field granularity\n");

    // Taint Source
    int tainted = oob_src[5];

    // Corrupt Field A
    // This store should be skipped, p.a remains 100
    p.a = tainted;

    // Valid Store to Field B
    // This store should succeed
    int valid_val = 50;
    p.b = valid_val;

    // Load & Check
    // Ensure tainted field A doesn't affect field B
    int check_val = 0;
    check_val = p.b;

    int final_sink = 0;
    final_sink = check_val;

    printf("p.a: %d, p.b: %d, final_sink: %d\n", p.a, p.b, final_sink);

    return 0;
}