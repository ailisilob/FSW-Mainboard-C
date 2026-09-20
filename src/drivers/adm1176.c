#include "argus/drivers/adm1176.h"

#include <stddef.h>

enum {
    ADM1176_COMMAND_V_CONT = 1u << 0,
    ADM1176_COMMAND_I_CONT = 1u << 2,
    ADM1176_COMMAND_VRANGE = 1u << 4,
    ADM1176_COMMAND_STATUS_READ = 1u << 6,

    ADM1176_REGISTER_ALERT_ENABLE = 0x81,
    ADM1176_REGISTER_ALERT_THRESHOLD = 0x82,
    ADM1176_REGISTER_CONTROL = 0x83,

    ADM1176_ALERT_ENABLE_ADC_OC4 = 1u << 1,
    ADM1176_ALERT_ENABLE_HOT_SWAP = 1u << 2,
    ADM1176_ALERT_CLEAR = 1u << 4,

    ADM1176_CONTROL_SWITCH_OFF = 1u << 0,
};

static const float ADM1176_CURRENT_FULL_SCALE_V = 0.10584f;
static const float ADM1176_VOLTAGE_FULL_SCALE_HIGH_V = 26.35f;
static const float ADM1176_VOLTAGE_FULL_SCALE_LOW_V = 6.65f;
static const float ADM1176_ADC_STEPS = 4096.0f;
static const float ADM1176_ALERT_STEPS = 256.0f;

static bool voltage_range_is_valid(adm1176_voltage_range_t voltage_range)
{
    return voltage_range == ADM1176_VOLTAGE_RANGE_26V35 ||
           voltage_range == ADM1176_VOLTAGE_RANGE_6V65;
}

static bool transport_is_valid(const adm1176_transport_t *transport)
{
    return transport != NULL && transport->write != NULL && transport->read != NULL;
}

static argus_status_t require_initialized(const adm1176_t *device)
{
    if (device == NULL) {
        return ARGUS_STATUS_INVALID_ARGUMENT;
    }
    if (!device->initialized || !transport_is_valid(device->transport)) {
        return ARGUS_STATUS_NOT_INITIALIZED;
    }
    return ARGUS_STATUS_OK;
}

static argus_status_t write_bytes(adm1176_t *device, const uint8_t *data, size_t length)
{
    return device->transport->write(
        device->transport->context,
        device->address,
        data,
        length
    );
}

static argus_status_t read_bytes(adm1176_t *device, uint8_t *data, size_t length)
{
    return device->transport->read(
        device->transport->context,
        device->address,
        data,
        length
    );
}

static argus_status_t write_extended_register(adm1176_t *device, uint8_t reg, uint8_t value)
{
    const uint8_t message[2] = {reg, value};
    return write_bytes(device, message, sizeof(message));
}

argus_status_t adm1176_init(
    adm1176_t *device,
    const adm1176_transport_t *transport,
    uint8_t address,
    float sense_resistor_ohm,
    adm1176_voltage_range_t voltage_range
)
{
    if (device == NULL || !transport_is_valid(transport) ||
        address < ADM1176_MIN_I2C_ADDRESS || address > ADM1176_MAX_I2C_ADDRESS ||
        sense_resistor_ohm <= 0.0f || !voltage_range_is_valid(voltage_range)) {
        return ARGUS_STATUS_INVALID_ARGUMENT;
    }

    device->transport = transport;
    device->sense_resistor_ohm = sense_resistor_ohm;
    device->address = address;
    device->command = ADM1176_COMMAND_V_CONT | ADM1176_COMMAND_I_CONT;
    if (voltage_range == ADM1176_VOLTAGE_RANGE_6V65) {
        device->command |= ADM1176_COMMAND_VRANGE;
    }
    device->alert_enable = ADM1176_ALERT_ENABLE_HOT_SWAP;
    device->control = 0;
    device->overcurrent_threshold = 0xff;
    device->voltage_range = voltage_range;
    device->initialized = false;

    const argus_status_t status = transport->write(
        transport->context,
        address,
        &device->command,
        1
    );
    if (status != ARGUS_STATUS_OK) {
        return status;
    }

    device->initialized = true;
    return ARGUS_STATUS_OK;
}

argus_status_t adm1176_decode_measurement(
    const uint8_t buffer[3],
    float sense_resistor_ohm,
    adm1176_voltage_range_t voltage_range,
    argus_power_sample_t *sample
)
{
    if (buffer == NULL || sample == NULL || sense_resistor_ohm <= 0.0f ||
        !voltage_range_is_valid(voltage_range)) {
        return ARGUS_STATUS_INVALID_ARGUMENT;
    }

    sample->raw_voltage = ((uint16_t)buffer[0] << 4) | ((uint16_t)buffer[2] >> 4);
    sample->raw_current = ((uint16_t)buffer[1] << 4) | (buffer[2] & 0x0fu);

    const float voltage_full_scale =
        voltage_range == ADM1176_VOLTAGE_RANGE_6V65
            ? ADM1176_VOLTAGE_FULL_SCALE_LOW_V
            : ADM1176_VOLTAGE_FULL_SCALE_HIGH_V;

    sample->voltage_v = voltage_full_scale * (float)sample->raw_voltage / ADM1176_ADC_STEPS;
    sample->current_a = ADM1176_CURRENT_FULL_SCALE_V * (float)sample->raw_current /
                        (ADM1176_ADC_STEPS * sense_resistor_ohm);
    return ARGUS_STATUS_OK;
}

argus_status_t adm1176_read(adm1176_t *device, argus_power_sample_t *sample)
{
    const argus_status_t initialized = require_initialized(device);
    if (initialized != ARGUS_STATUS_OK) {
        return initialized;
    }
    if (sample == NULL) {
        return ARGUS_STATUS_INVALID_ARGUMENT;
    }

    uint8_t buffer[3] = {0};
    const argus_status_t status = read_bytes(device, buffer, sizeof(buffer));
    if (status != ARGUS_STATUS_OK) {
        return status;
    }

    return adm1176_decode_measurement(
        buffer,
        device->sense_resistor_ohm,
        device->voltage_range,
        sample
    );
}

argus_status_t adm1176_read_status(adm1176_t *device, uint8_t *status_byte)
{
    const argus_status_t initialized = require_initialized(device);
    if (initialized != ARGUS_STATUS_OK) {
        return initialized;
    }
    if (status_byte == NULL) {
        return ARGUS_STATUS_INVALID_ARGUMENT;
    }

    const uint8_t status_command = device->command | ADM1176_COMMAND_STATUS_READ;
    argus_status_t status = write_bytes(device, &status_command, 1);
    if (status != ARGUS_STATUS_OK) {
        return status;
    }

    const argus_status_t read_status = read_bytes(device, status_byte, 1);
    const argus_status_t restore_status = write_bytes(device, &device->command, 1);

    if (read_status != ARGUS_STATUS_OK) {
        return read_status;
    }
    return restore_status;
}

argus_status_t adm1176_is_enabled(adm1176_t *device, bool *enabled)
{
    if (enabled == NULL) {
        return ARGUS_STATUS_INVALID_ARGUMENT;
    }

    uint8_t status_byte = 0;
    const argus_status_t status = adm1176_read_status(device, &status_byte);
    if (status != ARGUS_STATUS_OK) {
        return status;
    }

    *enabled = (status_byte & ADM1176_STATUS_OFF) == 0;
    return ARGUS_STATUS_OK;
}

argus_status_t adm1176_read_status_flags(
    adm1176_t *device,
    adm1176_status_flags_t *status
)
{
    if (status == NULL) {
        return ARGUS_STATUS_INVALID_ARGUMENT;
    }

    uint8_t raw = 0;
    const argus_status_t result = adm1176_read_status(device, &raw);
    if (result != ARGUS_STATUS_OK) {
        return result;
    }

    status->raw = raw;
    status->adc_overcurrent = (raw & ADM1176_STATUS_ADC_OVERCURRENT) != 0;
    status->adc_alert = (raw & ADM1176_STATUS_ADC_ALERT) != 0;
    status->hot_swap_overcurrent = (raw & ADM1176_STATUS_HOT_SWAP_OVERCURRENT) != 0;
    status->hot_swap_alert = (raw & ADM1176_STATUS_HOT_SWAP_ALERT) != 0;
    status->output_off = (raw & ADM1176_STATUS_OFF) != 0;
    status->output_off_alert = (raw & ADM1176_STATUS_OFF_ALERT) != 0;
    return ARGUS_STATUS_OK;
}

argus_status_t adm1176_set_enabled(adm1176_t *device, bool enabled)
{
    const argus_status_t initialized = require_initialized(device);
    if (initialized != ARGUS_STATUS_OK) {
        return initialized;
    }

    const uint8_t new_control = enabled ? 0 : ADM1176_CONTROL_SWITCH_OFF;
    argus_status_t status = write_extended_register(
        device,
        ADM1176_REGISTER_CONTROL,
        new_control
    );
    if (status != ARGUS_STATUS_OK || !enabled) {
        if (status == ARGUS_STATUS_OK) {
            device->control = new_control;
        }
        return status;
    }
    device->control = new_control;

    /* Match the previous flight driver: resume continuous ADC conversions. */
    return write_bytes(device, &device->command, 1);
}

argus_status_t adm1176_set_overcurrent_threshold(adm1176_t *device, uint8_t threshold)
{
    const argus_status_t initialized = require_initialized(device);
    if (initialized != ARGUS_STATUS_OK) {
        return initialized;
    }

    const uint8_t new_alert_enable = device->alert_enable | ADM1176_ALERT_ENABLE_ADC_OC4;
    argus_status_t status = write_extended_register(
        device,
        ADM1176_REGISTER_ALERT_ENABLE,
        new_alert_enable
    );
    if (status != ARGUS_STATUS_OK) {
        return status;
    }
    device->alert_enable = new_alert_enable;

    status = write_extended_register(device, ADM1176_REGISTER_ALERT_THRESHOLD, threshold);
    if (status != ARGUS_STATUS_OK) {
        return status;
    }
    device->overcurrent_threshold = threshold;
    return ARGUS_STATUS_OK;
}

argus_status_t adm1176_set_overcurrent_threshold_amps(adm1176_t *device, float current_a)
{
    const argus_status_t initialized = require_initialized(device);
    if (initialized != ARGUS_STATUS_OK) {
        return initialized;
    }

    const float full_scale_current_a =
        ADM1176_CURRENT_FULL_SCALE_V / device->sense_resistor_ohm;
    if (current_a < 0.0f || current_a > full_scale_current_a) {
        return ARGUS_STATUS_OUT_OF_RANGE;
    }

    unsigned int threshold = (unsigned int)(
        (current_a * ADM1176_ALERT_STEPS / full_scale_current_a) + 0.5f
    );
    if (threshold > 0xffu) {
        threshold = 0xffu;
    }
    return adm1176_set_overcurrent_threshold(device, (uint8_t)threshold);
}

argus_status_t adm1176_get_overcurrent_threshold_amps(
    const adm1176_t *device,
    float *current_a
)
{
    const argus_status_t initialized = require_initialized(device);
    if (initialized != ARGUS_STATUS_OK) {
        return initialized;
    }
    if (current_a == NULL) {
        return ARGUS_STATUS_INVALID_ARGUMENT;
    }

    const float full_scale_current_a =
        ADM1176_CURRENT_FULL_SCALE_V / device->sense_resistor_ohm;
    *current_a = full_scale_current_a * (float)device->overcurrent_threshold /
                 ADM1176_ALERT_STEPS;
    return ARGUS_STATUS_OK;
}

argus_status_t adm1176_clear_alerts(adm1176_t *device)
{
    const argus_status_t initialized = require_initialized(device);
    if (initialized != ARGUS_STATUS_OK) {
        return initialized;
    }

    return write_extended_register(
        device,
        ADM1176_REGISTER_ALERT_ENABLE,
        device->alert_enable | ADM1176_ALERT_CLEAR
    );
}

void adm1176_deinit(adm1176_t *device)
{
    if (device != NULL) {
        device->initialized = false;
    }
}
