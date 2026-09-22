# max17205 unit tests

Host-side Unity tests for `src/drivers/max17205.c`. Same shape as
`tests/i2c_bus`, but the cut is one layer higher: the driver is linked against
a **fake `i2c_bus`**, not against `src/board/i2c_bus.c`. A failure here is
therefore a max17205 bug and never an i2c_bus bug — `i2c_bus.c` has its own
suite next door.

## Layout

```
tests/max17205/
├── CMakeLists.txt        standalone host build (no Pico SDK, no cross toolchain)
├── test_max17205.c       the suite — test functions only, no main()
└── fakes/
    ├── i2c_bus_fake.h/.c full implementation of argus/i2c_bus.h:
    │                     call recorder + 16-bit register-map device model
    └── pico/float.h      stub, so max17205.h's #include resolves off-target
```

The remaining SDK stubs (`hardware/i2c.h`, `pico/sync.h`, `pico/stdlib.h`,
`pico/binary_info.h`) and the CTest discovery script are reused from
`tests/i2c_bus/` rather than copied, so there is only one of each to maintain.
The runner (`main()` plus the `RUN_TEST` calls) is generated into the build
directory by Unity's Ruby generator and is not checked in.

## Running

Needs the Unity submodule at `<repo>/unity` and Ruby.

```sh
cmake -S tests/max17205 -B build/tests/max17205
cmake --build build/tests/max17205
ctest --test-dir build/tests/max17205 --output-on-failure
```

One test at a time:

```sh
ctest --test-dir build/tests/max17205 -R test_max17205_read_soc
```

## What the suite covers

18 tests, one per function in `max17205.h`: `init`, `deinit`, the thirteen
`read_*()`, `read_all` and `reset`. Each read test checks three things in one
place — that NULL arguments are rejected, that the call succeeds, and that the
raw register decodes to the right value.

Each test stages exactly one register (`read_all` stages all thirteen, each to
a distinct value). That is deliberate: every other register in the fake reads
back 0, so a driver that addresses the wrong register — or reads a shadow-RAM
temperature off the main I2C address — decodes 0 and the value assertion
fails. Checking the value checks the addressing for free.

`tearDown()` additionally asserts the driver did not return while holding the
bus lock, on every test.

## Units

The `expect_*()` functions at the top of `test_max17205.c` convert a raw
register value into what `max17205_readings_t` stores:

| field | unit | register LSB |
| --- | --- | --- |
| `soc` | percent | 1/256 % |
| `capacity` | mAh | 5.0 µVh / Rsense |
| `current` | mA, signed | 1.5625 µV / Rsense |
| `voltage` | mV | 1.25 mV (Batt) |
| `midvoltage` | mV | 78.125 µV (VCell) |
| `cycles`, `tte`, `ttf`, `time_pwrup`, `temperature*` | raw register value | — |

The staged raw values were picked so every conversion lands exactly — 50 %,
1500 mAh, 40 mA, 8000 mV, 3200 mV — so a failure means the decoding is wrong,
not that it rounded differently.

One inference to check: `read_capacity` is expected to read FullCapRep
(`0x10`), matching the `MAX17205_CAPACITY_ADDR` name. `MAX17205_REPCAP_ADDR`
(`0x05`, remaining capacity) is defined in the driver but unused — if
`capacity` means the remaining figure, change the register in that test.

## What the fake gives you

`fakes/i2c_bus_fake.h` is the reference; the short version:

**A device that behaves like the gauge.** 256 sixteen-bit registers per I2C
address, little-endian, with the usual write-the-pointer-then-read protocol —
so a test states the device state, not the byte stream.

```c
i2c_fake_set_present(0x36, true);           /* the gauge ACKs at this address */
i2c_fake_set_reg(0x36, 0x06, 0x3200);       /* RepSOC reads back 0x3200 */
uint16_t v = i2c_fake_get_reg(0x36, 0x60);  /* what the driver wrote */
```

Both idioms work: `ic2_write_read(dev, &reg, buf, 1, 2)` and a separate
`i2c_write(dev, &reg, 1)` + `i2c_read(dev, buf, 2)`. The register pointer
auto-increments across a multi-register read, as on hardware.

**A log of everything the driver put on the wire**, for assertions the value
checks do not already make:

```c
i2c_fake.xfer_calls, .read_calls, .write_calls, .write_read_calls
i2c_fake.xfer[i]        /* op, addr, reg, bytes written/read, status returned */
i2c_fake_last_xfer()
i2c_fake_find_reg_read(0x36, 0x06)            /* index of that read, or -1 */
i2c_fake_find_reg_write(0x36, 0x60, 0x000F)   /* index of that write, or -1 */
```

**Failure injection**, for the error paths:

```c
i2c_fake_set_present(0x0B, false);              /* nothing ACKs -> i2c_nack_err */
i2c_fake_fail_all(i2c_timeout);                 /* every transfer fails */
i2c_fake_fail_after(3, i2c_nack_err);           /* 3 succeed, the rest fail */
i2c_fake_fail_reg(0x36, 0x09, i2c_timeout);     /* only this register fails */
i2c_fake.lock_contended = true;                 /* i2c_bus_lock reports i2c_busy */
```

`i2c_fake_fail_after()` is the one for `max17205_read_all()`: let a few reads
land, fail the rest, then assert on both the returned status and which fields
of `max17205_readings_t` were left alone.

**Bring-up bookkeeping**, for init tests: `i2c_fake.device_init_calls` and
`device_init_addr[]` record the addresses the driver bound (expect `0x36` for
the main block and `0x0B` for shadow RAM), and `bus_init_calls`,
`set_baud_calls`, `set_mode_calls`, `scan_bus_calls` record the rest.

Argument validation mirrors `src/board/i2c_bus.c` exactly — the NULL checks,
the 0x08–0x77 address window, the bus/device init flags — so a driver that
passes a bad argument gets the same status code here as it would on hardware.

## Conventions

- `setUp()` calls `i2c_fake_reset()` and puts both halves of the gauge on the
  bus; `make_ready_dev()` returns a `max17205_t` in the state a correct
  `max17205_init()` would leave it in, so the read and reset tests stay
  meaningful while `max17205_init()` itself is still a stub.
- Test functions must be `void test_name(void)` and **not** `static` — Unity's
  generator only matches those. Helpers stay `static`, which is what keeps them
  out of the runner.

## Current state

The driver is a stub returning `max17205_bad` from every entry point, so this
is a test-first suite: **0 of 18 pass**.

The suite was checked against a throwaway reference implementation of
`max17205.c` written to the units above: 18/18 passed. So the failures are the
missing driver, not broken tests.

Not covered, by choice: a successful `max17205_init()` (nothing asserts that it
binds `0x36` and `0x0B`, or that it sets `dev.init`), bus errors mid-read,
`read_all` failing partway through, and signed decoding of a negative current
or a sub-zero temperature. The fake supports all of these — see the failure
injection above — if you want them later.
