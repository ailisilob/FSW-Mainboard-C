/**
 * @file i2c_bus_fake.c
 * @brief Implementation of the fake i2c_bus layer. See i2c_bus_fake.h.
 */

#include "i2c_bus_fake.h"

#include <string.h>

i2c_fake_t i2c_fake;

static i2c_bus_t fake_bus;

/* ---------------------------------------------------------------- setup -- */

void i2c_fake_reset(void) {
    memset(&i2c_fake, 0, sizeof(i2c_fake));
    i2c_fake.forced = i2c_ok;
    i2c_fake.read_fill = 0xA5;

    memset(&fake_bus, 0, sizeof(fake_bus));
    fake_bus.i2c = i2c0;
    fake_bus.sda = I2C_SDA_0;
    fake_bus.scl = I2C_SCL_0;
    fake_bus.baudrate = I2C_baud;
    fake_bus.timeout = I2C_timeout;
    fake_bus.init = true;
}

i2c_bus_t *i2c_fake_ready_bus(void) {
    return &fake_bus;
}

/** The device at `addr`, or NULL. */
static i2c_fake_device_t *find_device(uint8_t addr) {
    for (unsigned i = 0; i < I2C_FAKE_MAX_DEVICES; i++) {
        if (i2c_fake.dev[i].used && i2c_fake.dev[i].addr == addr) {
            return &i2c_fake.dev[i];
        }
    }
    return NULL;
}

/** The device at `addr`, creating it if there is a free slot. */
static i2c_fake_device_t *get_device(uint8_t addr) {
    i2c_fake_device_t *d = find_device(addr);
    if (d != NULL) {
        return d;
    }
    for (unsigned i = 0; i < I2C_FAKE_MAX_DEVICES; i++) {
        if (!i2c_fake.dev[i].used) {
            d = &i2c_fake.dev[i];
            memset(d, 0, sizeof(*d));
            d->used = true;
            d->addr = addr;
            return d;
        }
    }
    /* Out of slots. Raise I2C_FAKE_MAX_DEVICES if a test needs more. */
    return NULL;
}

void i2c_fake_set_present(uint8_t addr, bool present) {
    if (present) {
        (void)get_device(addr);
        return;
    }
    i2c_fake_device_t *d = find_device(addr);
    if (d != NULL) {
        memset(d, 0, sizeof(*d));
    }
}

void i2c_fake_set_reg(uint8_t addr, uint8_t reg, uint16_t value) {
    i2c_fake_device_t *d = get_device(addr);
    if (d != NULL) {
        d->reg[reg] = value;
    }
}

uint16_t i2c_fake_get_reg(uint8_t addr, uint8_t reg) {
    i2c_fake_device_t *d = find_device(addr);
    return d != NULL ? d->reg[reg] : 0;
}

void i2c_fake_fail_all(i2c_status_t st) {
    i2c_fake.forced = st;
}

void i2c_fake_fail_after(unsigned n_ok, i2c_status_t st) {
    i2c_fake.fail_after_armed = true;
    i2c_fake.fail_after_remaining = n_ok;
    i2c_fake.fail_after_status = st;
}

void i2c_fake_fail_reg(uint8_t addr, uint8_t reg, i2c_status_t st) {
    i2c_fake_device_t *d = get_device(addr);
    if (d != NULL) {
        d->reg_status[reg] = st;
    }
}

/* ------------------------------------------------------------ inspection -- */

const i2c_fake_xfer_t *i2c_fake_last_xfer(void) {
    if (i2c_fake.xfer_calls == 0) {
        return NULL;
    }
    unsigned n = i2c_fake.xfer_calls;
    if (n > I2C_FAKE_MAX_EVENTS) {
        n = I2C_FAKE_MAX_EVENTS;
    }
    return &i2c_fake.xfer[n - 1];
}

/** How many transfers are actually in the log. */
static unsigned logged(void) {
    return i2c_fake.xfer_calls < I2C_FAKE_MAX_EVENTS ? i2c_fake.xfer_calls : I2C_FAKE_MAX_EVENTS;
}

int i2c_fake_find_reg_read(uint8_t addr, uint8_t reg) {
    for (unsigned i = 0; i < logged(); i++) {
        const i2c_fake_xfer_t *x = &i2c_fake.xfer[i];
        if (x->op == i2c_fake_op_write || x->addr != addr || !x->reg_valid || x->r_len == 0) {
            continue;
        }
        /* A read of r_len bytes covers ceil(r_len/2) registers from x->reg. */
        unsigned span = (unsigned)((x->r_len + 1) / 2);
        for (unsigned k = 0; k < span; k++) {
            if ((uint8_t)(x->reg + k) == reg) {
                return (int)i;
            }
        }
    }
    return -1;
}

int i2c_fake_find_reg_write(uint8_t addr, uint8_t reg, uint16_t value) {
    for (unsigned i = 0; i < logged(); i++) {
        const i2c_fake_xfer_t *x = &i2c_fake.xfer[i];
        if (x->op == i2c_fake_op_read || x->addr != addr || x->w_len < 3) {
            continue;
        }
        unsigned pairs = (unsigned)((x->w_len - 1) / 2);
        for (unsigned k = 0; k < pairs && (1 + 2 * k + 1) < I2C_FAKE_MAX_BYTES; k++) {
            uint16_t v = (uint16_t)(x->w[1 + 2 * k] | ((uint16_t)x->w[1 + 2 * k + 1] << 8));
            if ((uint8_t)(x->w[0] + k) == reg && v == value) {
                return (int)i;
            }
        }
    }
    return -1;
}

/* -------------------------------------------------------------- transfers -- */

/** Status for one transfer, honouring the injected failures in precedence order. */
static i2c_status_t xfer_status(uint8_t addr, uint8_t reg, bool reg_valid) {
    if (i2c_fake.forced != i2c_ok) {
        return i2c_fake.forced;
    }
    if (i2c_fake.fail_after_armed) {
        if (i2c_fake.fail_after_remaining > 0) {
            i2c_fake.fail_after_remaining--;
        } else {
            return i2c_fake.fail_after_status;
        }
    }
    i2c_fake_device_t *d = find_device(addr);
    if (d == NULL) {
        return i2c_nack_err; /* nobody ACKed the address */
    }
    if (reg_valid && d->reg_status[reg] != i2c_ok) {
        return d->reg_status[reg];
    }
    return i2c_ok;
}

static void record(i2c_fake_op_t op, i2c_device_t *dev, uint8_t reg, bool reg_valid,
                   const uint8_t *src, size_t w_len, const uint8_t *dst, size_t r_len,
                   i2c_status_t ret) {
    if (i2c_fake.xfer_calls < I2C_FAKE_MAX_EVENTS) {
        i2c_fake_xfer_t *x = &i2c_fake.xfer[i2c_fake.xfer_calls];
        memset(x, 0, sizeof(*x));
        x->op = op;
        x->bus = dev->bus;
        x->addr = dev->addr;
        x->reg = reg;
        x->reg_valid = reg_valid;
        x->w_len = w_len;
        x->r_len = r_len;
        x->ret = ret;
        if (src != NULL) {
            memcpy(x->w, src, w_len < I2C_FAKE_MAX_BYTES ? w_len : I2C_FAKE_MAX_BYTES);
        }
        if (dst != NULL) {
            memcpy(x->r, dst, r_len < I2C_FAKE_MAX_BYTES ? r_len : I2C_FAKE_MAX_BYTES);
        }
    }
    i2c_fake.xfer_calls++;
}

/**
 * Apply a write payload to the register map: src[0] is the register pointer,
 * any bytes after it are little-endian 16-bit values written to successive
 * registers. A dangling odd byte is recorded but not applied -- the MAX17205
 * has no half-register write.
 */
static void apply_write(i2c_fake_device_t *d, const uint8_t *src, size_t len) {
    if (d == NULL || len == 0) {
        return;
    }
    uint8_t reg = src[0];
    d->ptr = reg;
    d->ptr_valid = true;
    for (size_t i = 1; i + 1 < len; i += 2) {
        d->reg[reg] = (uint16_t)(src[i] | ((uint16_t)src[i + 1] << 8));
        reg++;
        d->ptr = reg; /* the pointer advances past what was written, as on hardware */
    }
}

/** Fill `dst` from the register map starting at `reg`, little-endian. */
static void apply_read(i2c_fake_device_t *d, uint8_t reg, uint8_t *dst, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint16_t v = d != NULL ? d->reg[(uint8_t)(reg + i / 2)] : i2c_fake.read_fill;
        dst[i] = (i % 2 == 0) ? (uint8_t)(v & 0xFF) : (uint8_t)(v >> 8);
    }
    if (d != NULL) {
        d->ptr = (uint8_t)(reg + (len + 1) / 2);
        d->ptr_valid = true;
    }
}

/** The NULL/init checks every device operation in i2c_bus.c performs. */
static i2c_status_t check_device(i2c_device_t *dev) {
    if (dev->bus == NULL) {
        return i2c_dev_not_init;
    }
    if (!dev->bus->init) {
        return i2c_bus_not_init;
    }
    return i2c_ok;
}

/* ======================================================================= */
/* argus/i2c_bus.h                                                          */
/* ======================================================================= */

i2c_status_t i2c_bus_init(i2c_bus_t *bus, i2c_bus_id_t id, uint32_t baud, uint32_t time) {
    i2c_fake.bus_init_calls++;
    if (bus == NULL || baud == 0 || id >= bus_count) {
        return i2c_arg_err;
    }
    bus->i2c = (id == i2c_0) ? i2c0 : i2c1;
    bus->sda = (id == i2c_0) ? I2C_SDA_0 : I2C_SDA_1;
    bus->scl = (id == i2c_0) ? I2C_SCL_0 : I2C_SCL_1;
    bus->baudrate = baud;
    bus->timeout = time;
    bus->init = true;
    return i2c_ok;
}

void i2c_bus_deinit(i2c_bus_t *bus) {
    i2c_fake.bus_deinit_calls++;
    if (bus == NULL || !bus->init) {
        return;
    }
    bus->init = false;
}

i2c_status_t i2c_bus_recover(i2c_bus_t *bus) {
    i2c_fake.bus_recover_calls++;
    if (bus == NULL) {
        return i2c_arg_err;
    }
    return i2c_ok;
}

i2c_status_t i2c_bus_lock(i2c_bus_t *bus) {
    i2c_fake.lock_calls++;
    if (bus == NULL) {
        return i2c_arg_err;
    }
    if (!bus->init) {
        return i2c_bus_not_init;
    }
    if (i2c_fake.lock_contended) {
        return i2c_busy;
    }
    i2c_fake.lock_depth++;
    return i2c_ok;
}

void i2c_bus_unlock(i2c_bus_t *bus) {
    i2c_fake.unlock_calls++;
    if (bus == NULL || !bus->init) {
        return;
    }
    i2c_fake.lock_depth--;
}

i2c_status_t i2c_set_baud(i2c_bus_t *bus, uint32_t baud) {
    i2c_fake.set_baud_calls++;
    if (bus == NULL || baud == 0) {
        return i2c_arg_err;
    }
    if (!bus->init) {
        return i2c_bus_not_init;
    }
    bus->baudrate = baud;
    return i2c_ok;
}

i2c_status_t i2c_set_mode(i2c_device_t *dev, bool slave) {
    i2c_fake.set_mode_calls++;
    i2c_fake.set_mode_slave = slave;
    if (dev == NULL || dev->bus == NULL) {
        return i2c_arg_err;
    }
    if (!dev->bus->init) {
        return i2c_bus_not_init;
    }
    return i2c_ok;
}

i2c_status_t i2c_device_init(i2c_device_t *dev, i2c_bus_t *bus, uint8_t addr) {
    if (i2c_fake.device_init_calls < I2C_FAKE_MAX_EVENTS) {
        i2c_fake.device_init_addr[i2c_fake.device_init_calls] = addr;
    }
    i2c_fake.device_init_calls++;

    if (dev == NULL || bus == NULL || addr < 0x08 || addr >= 0x78) {
        return i2c_arg_err;
    }
    if (!bus->init) {
        return i2c_bus_not_init;
    }
    dev->addr = addr;
    dev->bus = bus;
    return i2c_ok;
}

i2c_status_t i2c_read(i2c_device_t *dev, uint8_t *dst, size_t len) {
    if (dev == NULL || dst == NULL || len == 0) {
        return i2c_arg_err;
    }
    i2c_status_t status = check_device(dev);
    if (status != i2c_ok) {
        return status;
    }

    i2c_fake_device_t *d = find_device(dev->addr);
    /* A bare read takes whatever the pointer was left at by the last write. */
    uint8_t reg = (d != NULL) ? d->ptr : 0;
    bool reg_valid = (d != NULL) && d->ptr_valid;

    status = xfer_status(dev->addr, reg, reg_valid);
    if (status == i2c_ok) {
        apply_read(d, reg, dst, len);
    }

    i2c_fake.read_calls++;
    record(i2c_fake_op_read, dev, reg, reg_valid, NULL, 0, status == i2c_ok ? dst : NULL, len,
           status);
    return status;
}

i2c_status_t i2c_write(i2c_device_t *dev, const uint8_t *src, size_t len) {
    if (dev == NULL || src == NULL || len == 0) {
        return i2c_arg_err;
    }
    i2c_status_t status = check_device(dev);
    if (status != i2c_ok) {
        return status;
    }

    status = xfer_status(dev->addr, src[0], true);
    if (status == i2c_ok) {
        apply_write(find_device(dev->addr), src, len);
    }

    i2c_fake.write_calls++;
    record(i2c_fake_op_write, dev, src[0], true, src, len, NULL, 0, status);
    return status;
}

i2c_status_t ic2_write_read(i2c_device_t *dev, const uint8_t *src, uint8_t *dst, size_t w_len,
                            size_t r_len) {
    if (dev == NULL || src == NULL || dst == NULL || w_len == 0 || r_len == 0) {
        return i2c_arg_err;
    }
    i2c_status_t status = check_device(dev);
    if (status != i2c_ok) {
        return status;
    }

    uint8_t reg = src[0];
    status = xfer_status(dev->addr, reg, true);
    if (status == i2c_ok) {
        i2c_fake_device_t *d = find_device(dev->addr);
        apply_write(d, src, w_len);
        /* A repeated START reads back from the register the write named, not
         * from wherever the write left the pointer. */
        apply_read(d, reg, dst, r_len);
    }

    i2c_fake.write_read_calls++;
    record(i2c_fake_op_write_read, dev, reg, true, src, w_len, status == i2c_ok ? dst : NULL, r_len,
           status);
    return status;
}

i2c_status_t scan_bus(i2c_bus_t *bus) {
    i2c_fake.scan_bus_calls++;
    if (bus == NULL) {
        return i2c_arg_err;
    }
    if (!bus->init) {
        return i2c_bus_not_init;
    }
    return i2c_ok;
}
