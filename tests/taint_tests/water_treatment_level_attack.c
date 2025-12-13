#include <stdio.h>

#define N_STEPS      20
#define DT           1.0
#define TARGET_LEVEL 5.0
#define MAX_LEVEL    6.0
#define DRAIN_RATE   1  // slower outflow, more realistic for treatment tanks
#define ATTACK_STEP  7
double level = 0.0;

int main(void) {
    double fill_table[6] = {0.0, 0.9, 1.8, 2.7, 3.6, 4.5};
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

        if (t >= ATTACK_STEP && t <= ATTACK_STEP + 2) {
            idx += 6;   // force idx well past the end of fill_table
        }

        // BUG: no upper bound check — unsafe when idx >= 6.
        double new_fill = fill_table[idx];
        fill = new_fill;

        // Update water level: inflow minus drain.
        level -= DRAIN_RATE;
        level += (fill) * DT;
        if (level <0){
            level = 0;
        }

        printf("t=%2d  sensor=%5.2f  idx=%2d  fill=%4.2f  level=%5.2f",
               t, sensor, idx, fill, level);
        if (level > MAX_LEVEL) {
            printf("  *** ABOVE MAX_LEVEL (%.2f) ***", MAX_LEVEL);
        }
        printf("\n");
    }

    return 0;
}