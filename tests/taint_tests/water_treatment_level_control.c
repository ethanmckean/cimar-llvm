#include <stdio.h>

#define N_STEPS      20
#define DT           1.0
#define TARGET_LEVEL 5.0
#define MAX_LEVEL    6.0
#define DRAIN_RATE   0.1   // slower outflow, more realistic for treatment tanks
#define ATTACK_STEP  5 

int main(void) {
    double fill_table[6] = {0.0, 0.9, 1.8, 2.7, 3.6, 4.5};

    double level = 0.0;
    double fill  = 0.0;

    for (int t = 0; t < N_STEPS; t++) {
        // Sensor reading: how far below TARGET_LEVEL we are.
        double sensor = TARGET_LEVEL - level;

        // Convert to integer index.
        int idx = (int)sensor;

        // If idx is negative, clamp to 0 (at/above target).
        if (idx < 0) {
            idx = 0;
        }

        // BUG: no upper bound check — unsafe when idx >= 6.
        double new_fill = fill_table[idx];
        fill = new_fill;

        // Update water level: inflow minus drain.
        level += (fill - DRAIN_RATE) * DT;

        printf("t=%2d  sensor=%5.2f  idx=%2d  fill=%4.2f  level=%5.2f",
               t, sensor, idx, fill, level);
        if (level > MAX_LEVEL) {
            printf("  *** ABOVE MAX_LEVEL (%.2f) ***", MAX_LEVEL);
        }
        printf("\n");
    }

    return 0;
}