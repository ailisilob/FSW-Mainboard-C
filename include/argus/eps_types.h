#ifndef ARGUS_EPS_TYPES_H
#define ARGUS_EPS_TYPES_H

#include <stdint.h>

typedef struct {
    uint16_t raw_voltage;
    uint16_t raw_current;
    float voltage_v;
    float current_a;
} argus_power_sample_t;

#endif
