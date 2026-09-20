#ifndef ARGUS_DRIVERS_ADM1176_H
#define ARGUS_DRIVERS_ADM1176_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "argus/eps_types.h"
#include "argus/status.h"

#define ADM1176_DEFAULT_SENSE_RESISTOR_OHM 0.01f
#define ADM1176_MIN_I2C_ADDRESS 0x40u
#define ADM1176_MAX_I2C_ADDRESS 0x4fu
#define ADM1176_TYPICAL_CONVERSION_TIME_US 150u

typedef enum {
    ADM1176_VOLTAGE_RANGE_26V35 = 0,
    ADM1176_VOLTAGE_RANGE_6V65 = 1,
} adm1176_voltage_range_t;

typedef enum {
    ADM1176_STATUS_ADC_OVERCURRENT = 1u << 0,
    ADM1176_STATUS_ADC_ALERT = 1u << 1,
    ADM1176_STATUS_HOT_SWAP_OVERCURRENT = 1u << 2,
    ADM1176_STATUS_HOT_SWAP_ALERT = 1u << 3,
    ADM1176_STATUS_OFF = 1u << 4,
    ADM1176_STATUS_OFF_ALERT = 1u << 5,
} adm1176_status_bit_t;

typedef struct {
    bool adc_overcurrent;
    bool adc_alert;
    bool hot_swap_overcurrent;
    bool hot_swap_alert;
    bool output_off;
    bool output_off_alert;
    uint8_t raw;
} adm1176_status_flags_t;

/*
 * Hardware-independent transport used by the driver. The board I2C layer
 * supplies these two operations; the ADM1176 driver does not call Pico SDK
 * functions directly.
 */
typedef argus_status_t (*adm1176_transport_write_fn)(
    void *context,
    uint8_t address,
    const uint8_t *data,
    size_t length
);

typedef argus_status_t (*adm1176_transport_read_fn)(
    void *context,
    uint8_t address,
    uint8_t *data,
    size_t length
);

typedef struct {
    void *context;
    adm1176_transport_write_fn write;
    adm1176_transport_read_fn read;
} adm1176_transport_t;

typedef struct {
    const adm1176_transport_t *transport;
    float sense_resistor_ohm;
    uint8_t address;
    uint8_t command;
    uint8_t alert_enable;
    uint8_t control;
    uint8_t overcurrent_threshold;
    adm1176_voltage_range_t voltage_range;
    bool initialized;
} adm1176_t;

/* Configure continuous voltage and current conversion. */
argus_status_t adm1176_init(
    adm1176_t *device,
    const adm1176_transport_t *transport,
    uint8_t address,
    float sense_resistor_ohm,
    adm1176_voltage_range_t voltage_range
);

/* Read and convert the latest three-byte voltage/current result. */
argus_status_t adm1176_read(adm1176_t *device, argus_power_sample_t *sample);

/* Convert the ADM1176's three raw measurement bytes into engineering units. */
argus_status_t adm1176_decode_measurement(
    const uint8_t buffer[3],
    float sense_resistor_ohm,
    adm1176_voltage_range_t voltage_range,
    argus_power_sample_t *sample
);

argus_status_t adm1176_read_status(adm1176_t *device, uint8_t *status);
argus_status_t adm1176_read_status_flags(
    adm1176_t *device,
    adm1176_status_flags_t *status
);
argus_status_t adm1176_is_enabled(adm1176_t *device, bool *enabled);
argus_status_t adm1176_set_enabled(adm1176_t *device, bool enabled);

/* Threshold is the upper eight bits of the 12-bit current ADC result. */
argus_status_t adm1176_set_overcurrent_threshold(adm1176_t *device, uint8_t threshold);
argus_status_t adm1176_set_overcurrent_threshold_amps(adm1176_t *device, float current_a);
argus_status_t adm1176_get_overcurrent_threshold_amps(
    const adm1176_t *device,
    float *current_a
);
argus_status_t adm1176_clear_alerts(adm1176_t *device);

/* Marks the software object unavailable; it does not switch off the load. */
void adm1176_deinit(adm1176_t *device);

#endif
