# i2c_bus unit tests

Host-side Unity tests for `src/board/i2c_bus.c`. They run on your laptop, not on
the RP2350: the Pico SDK calls the driver makes are satisfied by fakes in
`fakes/`, which record every call so the tests can assert on the driver's side
of the hardware boundary.

## Layout

```
tests/i2c_bus/
├── CMakeLists.txt        standalone host build (no Pico SDK, no cross toolchain)
├── test_i2c_bus.c        the suite — test functions only, no main()
├── cmake/
│   └── DiscoverUnityTests.cmake   registers one CTest entry per Unity test
└── fakes/
    ├── pico_fake.h/.c    the call recorder — `pico_fake` + `pico_fake_reset()`
    ├── hardware/i2c.h    signatures copied from pico-sdk 2.3.1
    ├── hardware/gpio.h
    ├── pico/stdlib.h
    ├── pico/sync.h       mutex subset
    └── pico/binary_info.h
```

The runner (`main()` plus the `RUN_TEST` calls) is generated into the build
directory by Unity's Ruby generator and is not checked in.

`fakes/` is first on the include path, so `#include "hardware/i2c.h"` inside
`i2c_bus.c` resolves to the stub. Nothing in `src/` or `include/` was changed.

The fake headers deliberately copy the real SDK signatures character for
character. That is what makes a mismatch in the driver show up as a compiler
diagnostic here instead of only on hardware.

## Running

Needs the Unity submodule at `<repo>/unity` — i.e. the `unity` branch PR merged
(or `git submodule update --init --recursive` once it is).

```sh
cmake -S tests/i2c_bus -B build/tests
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

Every Unity test is registered as its own CTest entry, named
`i2c_bus.<test function>`, so `ctest` reports them individually:

```sh
ctest --test-dir build/tests -R test_bus_init_stores_timeout   # one test
ctest --test-dir build/tests -R test_device_init               # one group
ctest --test-dir build/tests --rerun-failed                    # just the reds
```

Or run the binary directly for the full Unity output:
`./build/tests/test_i2c_bus`. It also takes `-l` to list tests and
`-n <name>` to run one.

(`ctest --test-dir` needs CMake 3.20+. On an older CMake, `cd build/tests &&
ctest` does the same thing.)

`-Werror` is intentionally off. `i2c_bus.c` currently emits real warnings that
the tests are meant to surface, and `-Werror` would hide them behind a build
break.

## Current state

```
52 tests   19 passed   12 failed   21 skipped
```

That is the expected red phase. Failures are behaviours the header promises but
the implementation does not deliver yet; the skips are functions with empty
bodies, where calling them would be undefined behaviour and could take the whole
runner down — each one carries a `TEST_IGNORE_MESSAGE` naming what to implement,
with the real assertions written underneath, ready to go live once you delete
the ignore line.

(Unity exits 0 for an ignored test, which CTest would otherwise count as a
pass, so the discovery script gives each test a `SKIP_REGULAR_EXPRESSION` of
`:IGNORE`. That is why they show as *Skipped* rather than *Passed*.)

### Failing tests, and what each one is pointing at

**`i2c_bus_init` never succeeds** (8 failures) — `i2c_bus.c:17`:

```c
if (bus == NULL || id != i2c_1 || id != i2c_0) {return i2c_arg_err;}
```

`id` cannot equal both, so one of the two `!=` is always true and every call
returns `i2c_arg_err`. Wants `&&`, or a range check like `id >= bus_count`.

**`i2c_init` gets the wrong pointer level** — `i2c_bus.c:28`:

```c
bus->baudrate = i2c_init(&bus->i2c, baud);
```

`bus->i2c` is already the `i2c_inst_t *`; `&bus->i2c` is an `i2c_inst_t **`. The
compiler flags it (`-Wincompatible-pointer-types`). On hardware this pokes the
struct's own address as if it were an I2C peripheral. Drop the `&`.

**`i2c_device_init` rejects the wrong addresses** (3 failures) — `i2c_bus.c:80`:

```c
if (dev == NULL || bus == NULL || addr >= 0x78 || addr >= 0x07) {return i2c_arg_err;}
```

The second bound reads `>=` where it should read `<`. As written, `0x00`–`0x06`
are accepted (they are reserved) and everything from `0x07` up is rejected
(including the whole usable `0x08`–`0x77` range, e.g. `0x36` for the MAX17205).
Wants `addr < 0x08 || addr > 0x77`.

**`i2c_device_init` does not check `bus->init`** — a `i2c_bus_not_init` code
exists in the enum, so binding a device to a bus that was never brought up
should report it rather than return `i2c_ok`.

### Fixed

- **`scan_bus` probed `i2c_default` instead of `bus->i2c`** (the SDK example it
  was copied from hardcodes the default instance), so scanning bus 1 silently
  scanned bus 0. Fixed at `i2c_bus.c:133`;
  `test_scan_bus_probes_the_bus_it_was_given` now passes and guards it.

### Two tests encode contract decisions, not existing behaviour

These are proposals — push back on them if you disagree and I'll drop them:

- `test_bus_init_rejects_zero_baud` — a baud of 0 is an argument error.
  (Currently passes for the wrong reason: *every* init returns `i2c_arg_err`.)
- `test_device_init_rejects_uninitialized_bus` — returns `i2c_bus_not_init`.

### Things the tests can't check, worth a look anyway

- **`include/argus/i2c_bus.h:22` — the include guard closes too early.** The
  `#endif` sits right after the `#include` block, so every macro, typedef and
  prototype below it is outside the guard. Two translation units including the
  header, or one including it twice, gets redefinition errors. Move `#endif` to
  the end of the file. Not testable at runtime, and the suite includes the
  header once so it doesn't trip over it.
- **`ic2_write_read` is a typo** for `i2c_write_read`, in both the header and
  the `.c`. The tests use the declared spelling so they compile today; rename in
  both places and update `test_write_read_*`.
- **`i2c_set_baud(i2c_bus_t *bus)` / `i2c_set_mode(i2c_bus_t *bus)` take no
  value.** `set_baud` can read `bus->baudrate` and push it down, which is what
  the ignored test assumes. `set_mode` has no mode argument and no mode field to
  read, so its contract is genuinely undefined — the ignored test only covers
  argument validation.
- **`scan_bus` calls `stdio_init_all()`.** A scan helper re-initialising stdio
  is a surprising side effect for callers; consider hoisting it to startup.
- **`reserved_addr()` is non-static** and not declared in the header, so it
  leaks into the link namespace. Mark it `static`.
- **`bus->timeout` is stored but never used.** Nothing calls the
  `i2c_*_timeout_us` variants, so the timeout field has no effect today;
  `test_read_honours_the_bus_timeout` covers that once reads are implemented.

## Writing more tests

Add a function. That is the whole registration step:

```c
void test_something_specific(void) {
    ...
}
```

Two constraints come from the generator:

- **Test functions must not be `static`.** The generator's regex only matches
  functions declared starting at `void`, and the runner is a separate
  translation unit that links against them. Helpers stay `static`, which is
  also what keeps them from being mistaken for tests.
- **Names must start with `test`** (`test|spec|should` is the default prefix).

### How the plumbing works

1. `generate_test_runner.rb` scans `test_i2c_bus.c` and writes
   `build/tests/test_i2c_bus_Runner.c`, containing `main()` and the `RUN_TEST`
   calls. It runs with `--cmdline_args=1`, which is what gives the binary its
   `-l` and `-n` flags; Unity is compiled with `UNITY_USE_COMMAND_LINE_ARGS` to
   match.
2. After every link, `cmake/DiscoverUnityTests.cmake` runs the binary with
   `-l` and writes one `add_test` per test into
   `build/tests/test_i2c_bus_tests.cmake`, which CTest picks up via the
   directory's `TEST_INCLUDE_FILES` property.

Discovery happens at build time rather than configure time on purpose: the
CTest list is read out of the binary itself, so it cannot drift from what the
runner actually contains. Renaming or commenting out a test is reflected on the
next build with no `cmake` re-run.

One wrinkle inherited from Make: discovery is a POST_BUILD step, so it only
runs when the target relinks. Make compares timestamps at one-second
granularity, so two builds inside the same second can skip the relink and leave
a stale test list. Building again (or `touch`ing the test file) sorts it out.

### The fakes

`setUp()` calls `pico_fake_reset()`, so each test starts from a clean recorder.

Two helpers keep tests independent of functions that are still broken:

- `make_ready_bus(id)` builds an `i2c_bus_t` in the state a correct
  `i2c_bus_init()` would leave it in, so the deinit / device / transfer / scan
  tests stay meaningful while `i2c_bus_init()` is being fixed.
- `make_device(bus, addr)` does the same for `i2c_device_t`.

To simulate a device answering, call `pico_fake_set_ack(0x36, true)`; transfers
to addresses that don't ACK return `PICO_ERROR_GENERIC`, like real hardware.
`pico_fake.force_ret` / `forced_ret` override that for timeout and error-path
tests. Every transfer lands in `pico_fake.xfer[]` with the instance, address,
buffer, length and `nostop` flag.
