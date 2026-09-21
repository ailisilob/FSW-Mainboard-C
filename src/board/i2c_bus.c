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
    /* Checking for non-existent bus and other errors*/
    if (bus == NULL || baud == 0 || id >= bus_count || id < 0) {return i2c_arg_err;}

    if (id == i2c_0) {
        bus->i2c = i2c0;
        bus->sda = 24;
        bus->scl = 25;
    } else {
        bus->i2c = i2c1;
        bus->sda = 46;
        bus->scl = 47;
    }

    gpio_set_function(bus->sda, GPIO_FUNC_I2C);
    gpio_set_function(bus->scl, GPIO_FUNC_I2C);
    gpio_pull_up(bus->sda);
    gpio_pull_up(bus->scl);

    bus->baudrate = i2c_init(bus->i2c, baud);
    bus->timeout = time;

    if (!mutex_is_initialized(&bus->lock)) {
        mutex_init(&bus->lock);
    }

    bus->init = true;
    return i2c_ok;
}

void i2c_bus_deinit(i2c_bus_t *bus) {
    /* Checking for non-existent bus */
    if (bus == NULL) {return;}
    /* Check for uinit bus */
    if (bus->init == false) {return;}

    /* Release pins */
    gpio_set_function(bus->sda, GPIO_FUNC_SIO);
    gpio_set_function(bus->scl, GPIO_FUNC_SIO);

    /* Deinit HW Block */
    i2c_deinit(bus->i2c);

    bus->init = false;
}

i2c_status_t i2c_bus_recover(i2c_bus_t *bus) {
    return i2c_busy;
}

i2c_status_t i2c_bus_lock(i2c_bus_t *bus) {
    /* Check for non-existent bus */
    if (bus == NULL) {return i2c_arg_err;}
    if (!bus->init) {return i2c_bus_not_init;}

    /* Try to get mutex lock */
    if (!mutex_enter_timeout_us(&bus->lock, bus->timeout)) {
        return i2c_busy;
    }

    return i2c_ok;
}

void i2c_bus_unlock(i2c_bus_t *bus) {
    /* Check for non-existent bus */
    if (bus == NULL) {return;}
    if (!bus->init) {return;}

    /* Try to release mutex */
    mutex_exit(&bus->lock);

    return;
}

/* Bus settings */
i2c_status_t i2c_set_baud(i2c_bus_t *bus, uint32_t baud) {
    /* Checking for non-existent bus */
    if (bus == NULL || baud == 0) {return i2c_arg_err;}
    if (!bus->init) {return i2c_bus_not_init;}

    bus->baudrate = i2c_set_baudrate(bus->i2c, baud);
    
    return i2c_ok;
}

i2c_status_t i2c_set_mode(i2c_device_t *dev, bool slave) {
    /* Checking for non-existent bus */
    if (dev == NULL || dev->bus == NULL) {return i2c_arg_err;}
    if (!dev->bus->init) {return i2c_bus_not_init;}

    i2c_set_slave_mode(dev->bus->i2c, slave, dev->addr);
    
    return i2c_ok;
}

/* Device operations */
i2c_status_t i2c_device_init(i2c_device_t *dev, i2c_bus_t *bus, uint8_t addr) {
    /* Checking for non-existent device/bus and other errors */
    if (dev == NULL || bus == NULL || addr < 0x08 || addr >= 0x78) {return i2c_arg_err;}
    /* Check for init bus */
    if (bus->init == false) {return i2c_bus_not_init;}

    dev->addr = addr;
    dev->bus = bus;

    return i2c_ok;
}

i2c_status_t i2c_read(i2c_device_t *dev, uint8_t *dst, size_t len) {
    /* Checking for non-existent device and other errors */
    if (dev == NULL || dst == NULL || len == 0) {return i2c_arg_err;}
    if (dev->bus == NULL) {return i2c_dev_not_init;}
    if (!dev->bus->init) {return i2c_bus_not_init;}

    /* Acquire lock on bus */
    i2c_status_t status = i2c_bus_lock(dev->bus);
    if (status != i2c_ok) {return status;}

    /* Read device */
    int bytes_read = i2c_read_timeout_us(dev->bus->i2c, dev->addr, dst, len, false, dev->bus->timeout);
    
    /* Unlock bus */
    i2c_bus_unlock(dev->bus);

    if (bytes_read == PICO_ERROR_GENERIC) {
        return i2c_nack_err;
    } else if (bytes_read == PICO_ERROR_TIMEOUT) {
        return i2c_timeout;
    } else {
        return i2c_ok;
    }
}

i2c_status_t i2c_write(i2c_device_t *dev, const uint8_t *src, size_t len) {
    /* Checking for non-existent device and other errors */
    if (dev == NULL || src == NULL || len == 0) {return i2c_arg_err;}
    if (dev->bus == NULL) {return i2c_dev_not_init;}
    if (!dev->bus->init) {return i2c_bus_not_init;}

    /* Acquire lock on bus */
    i2c_status_t status = i2c_bus_lock(dev->bus);
    if (status != i2c_ok) {return status;}

    /* Write to device */
    int bytes_write = i2c_write_timeout_us(dev->bus->i2c, dev->addr, src, len, false, dev->bus->timeout);
    
    /* Unlock bus */
    i2c_bus_unlock(dev->bus);

    if (bytes_write == PICO_ERROR_GENERIC) {
        return i2c_nack_err;
    } else if (bytes_write == PICO_ERROR_TIMEOUT) {
        return i2c_timeout;
    } else {
        return i2c_ok;
    }
}

i2c_status_t ic2_write_read(i2c_device_t *dev, const uint8_t *src, uint8_t *dst, size_t len) {
    /* Checking for non-existent device and other errors */
    if (dev == NULL || src == NULL || dst == NULL || len == 0) {return i2c_arg_err;}
    if (dev->bus == NULL) {return i2c_dev_not_init;}
    if (!dev->bus->init) {return i2c_bus_not_init;}

    /* Acquire lock on bus */
    i2c_status_t status = i2c_bus_lock(dev->bus);
    if (status != i2c_ok) {return status;}

    /* Write to device */
    int bytes_write = i2c_write_timeout_us(dev->bus->i2c, dev->addr, src, len, true, dev->bus->timeout);
    
    if (bytes_write == PICO_ERROR_GENERIC) {
        status = i2c_nack_err;
    } else if (bytes_write == PICO_ERROR_TIMEOUT) {
        status = i2c_timeout;
    }

    if (status == i2c_ok) {
        /* Read device */
        int bytes_read = i2c_read_timeout_us(dev->bus->i2c, dev->addr, dst, len, false, dev->bus->timeout);
    
        if (bytes_read == PICO_ERROR_GENERIC) {
            status = i2c_nack_err;
        } else if (bytes_read == PICO_ERROR_TIMEOUT) {
            status = i2c_timeout;
        }
    }

    /* Unlock bus */
    i2c_bus_unlock(dev->bus);

    return status;
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
    /* Check for non-existent bus and other errors */
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
            ret = i2c_read_blocking(bus->i2c, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");
    return i2c_ok;
}