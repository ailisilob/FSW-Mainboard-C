/**
 * @authors Ailisi Bao, John Buttles, Jacob Rohozen
 * @file i2c_bus.h
 * 
 * @brief This file defines the implementation for the I2C bus on the Argus mainboard.
 * This implementation wraps the Raspberry Pi Pico (RP2350) hardware API "hardware_i2c".
 * There is little functional difference between the SDK and this implementation. The only
 * added functionality is the ability to store devices/buses and their parameters in structs.
 */

#include "argus/i2c_bus.h"
#include "hardware/gpio.h"

/* Bus bring-up & operations */
i2c_status_t i2c_bus_init(i2c_bus_t *bus, i2c_bus_id_t id, uint32_t baud, uint32_t time) {
    /* Check for non-existent bus */
    if (bus == NULL || id != i2c_1 || id != i2c_0) {return i2c_arg_err;}
    if (id == i2c_0) {
        bus->i2c = i2c0;
        bus->sda = I2C_SDA_0;
        bus->scl = I2C_SCL_0;
    } else {
        bus->i2c = i2c1;
        bus->sda = I2C_SDA_1;
        bus->scl = I2C_SCL_1;
    }

    bus->baudrate = i2c_init(&bus->i2c, baud);
    bus->timeout = time;

    /* Set gpio pins to i2c */
    gpio_set_function(bus->sda, GPIO_FUNC_I2C);
    gpio_set_function(bus->scl, GPIO_FUNC_I2C);

    /* Enable pull up resistors */
    gpio_pull_up(bus->sda);
    gpio_pull_up(bus->scl);

    if (!mutex_is_initialized(&bus->lock)) {
        mutex_init(&bus->lock);
    }
    bus->init = true;
    return i2c_ok;
}

void i2c_bus_deinit(i2c_bus_t *bus) {
    /* Check for non-existent bus */
    if (bus == NULL || !bus->init) {return;}

    i2c_deinit(bus->i2c);
    gpio_set_function(bus->sda, GPIO_FUNC_SIO);
    gpio_set_function(bus->scl, GPIO_FUNC_SIO);
 
    bus->init = false;
}

i2c_status_t i2c_bus_recover(i2c_bus_t *bus) {
    
}

i2c_status_t i2c_bus_lock(i2c_bus_t *bus) {

}

void i2c_bus_unlock(i2c_bus_t *bus) {

}

/* Bus settings */
i2c_status_t i2c_set_baud(i2c_bus_t *bus) {

}

i2c_status_t i2c_set_mode(i2c_bus_t *bus) {

}

/* Device operations */
i2c_status_t i2c_device_init(i2c_device_t *dev, i2c_bus_t *bus, uint8_t addr) {
    if (dev == NULL || bus == NULL || addr >= 0x78 || addr >= 0x07) {return i2c_arg_err;}

    dev->bus = bus;
    dev->addr = addr;
    return i2c_ok;
}

i2c_status_t i2c_read(i2c_device_t *dev, uint8_t *dst, size_t len) {
    if (dev == NULL || dst == NULL || len == 0) {return i2c_arg_err;}
}

i2c_status_t i2c_write(i2c_device_t *dev, const uint8_t *src, size_t len) {
    if (dev == NULL || src == NULL || len == 0) {return i2c_arg_err;}
}

i2c_status_t ic2_write_read(i2c_device_t *dev, const uint8_t *src, uint8_t *dst, size_t len) {

}

/* Helper fxn for scan_bus */
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

/** Helper fxn for scanning devices on bus.
 *  @brief The following code was copied from
 *  the SDK I2C example under hardware_i2c.
*/
i2c_status_t scan_bus(i2c_bus_t *bus) {
    if (bus == NULL) {return i2c_arg_err;}
    if (!bus->init) {return i2c_bus_not_init;}
    // Enable UART so we can print status output
    stdio_init_all();
    
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    return i2c_ok;
}