#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 10

typedef enum {
    ADC_NORMAL,
    ADC_ATTACK
} adc_mode_t;

int adc_read(int iteration, adc_mode_t mode) {
    int is_high_cycle = (iteration % 2 == 1);

    if (mode == ADC_NORMAL) {
        return is_high_cycle ? 9 : 2;
    } else {
        return is_high_cycle ? 11 : 4;
    }
}

typedef struct {
    double current_speed;     
    int last_pwm;         
    int consecutive_zeros;   
} fan_state_t;

void fan_speed(fan_state_t *fan, int pwm_value) {
    int logical_value;
    if (pwm_value <= 40) {
        logical_value = 0;
    } else if (pwm_value >= 60) {
        logical_value = 1;
    } else {
        logical_value = 0;  
    }

    const double ACCELERATION = 15.0; 
    const double DECELERATION = 8.0;  
    const double MAX_SPEED = 100.0;  
    const double MIN_SPEED = 0.0;  

    if (logical_value == 1) {
        fan->current_speed += ACCELERATION;
        if (fan->current_speed > MAX_SPEED) {
            fan->current_speed = MAX_SPEED;
        }
        fan->consecutive_zeros = 0;
    } else {
        fan->consecutive_zeros++;

        if (fan->consecutive_zeros > 2) {
            fan->current_speed -= DECELERATION;
            if (fan->current_speed < MIN_SPEED) {
                fan->current_speed = MIN_SPEED;
            }
        }
    }

    fan->last_pwm = pwm_value;

    printf("  Fan Speed: PWM=%d -> Logic=%d | Speed=%.1f%% | %s\n",
           pwm_value, logical_value, fan->current_speed,
           fan->current_speed > 5.0 ? "RUNNING" : "STOPPED");
}

int main() {
    int *control_array = (int*)malloc(ARRAY_SIZE * sizeof(int));

    for (int i = 0; i < 5; i++) {
        control_array[i] = 10 + (i * 5);      // 10, 15, 20, 25, 30
    }
    for (int i = 5; i < 10; i++) {
        control_array[i] = 70 + ((i-5) * 5);  // 70, 75, 80, 85, 90
    }

    fan_state_t fan = {0.0, 0, 0};

    printf("PHASE 1: Normal ADC Operation\n");
    for (int i = 0; i < 20; i++) {
        int adc_index = adc_read(i, ADC_NORMAL);
        int control_value = control_array[adc_index];
        printf("Cycle %d: ADC=%d, Control=%d\n", i, adc_index, control_value);
        fan_speed(&fan, control_value);
    }

    printf("\nPHASE 2: ADC Attack (1.5 Centered Mode)\n");
    for (int i = 0; i < 20; i++) {
        int adc_index = adc_read(i, ADC_ATTACK);
        int control_value = control_array[adc_index];
        printf("Cycle %d: ADC=%d, Control=%d\n", i, adc_index, control_value);
        fan_speed(&fan, control_value);
    }

    free(control_array);
    return 0;
}
