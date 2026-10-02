# dmft5336

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmft5336/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmft5336/actions/workflows/ci.yml)

DMOD driver for the FocalTech FT5336 capacitive touch controller.

## Description

`dmft5336` implements the [dmdrvi](https://github.com/choco-technologies/dmdrvi)
driver interface (2.0) for the FT5336 - the touch controller of e.g. the
STM32F746G-DISCO's 4.3" LCD. It has no hardware port: the chip is reached
through a [dmi2c](https://github.com/choco-technologies/dmi2c) bus node, so it
runs on any target dmi2c supports.

- `read()` of the node returns the current touch state (up to 5 points, in
  screen coordinates),
- `dmft5336_ioctl_cmd_wait_event` blocks until something changes - on the
  chip's INT pin (a dmgpio interrupt dispatched through dmhaman) or by
  polling the chip,
- swap/invert/clip transformation from panel to screen coordinates.

## Building

```bash
cmake -S . -B build -DDMOD_TOOLS_NAME=arch/armv7/cortex-m7
cmake --build build
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout.
The dmi2c headers are fetched from its per-family packages
(`DMOD_CPU_FAMILY`, default `stm32f7` - the headers are the same for every
family).

## Testing

Host tests (point decoding, configuration, ioctl contract) run through
`dmod_loader`:

```bash
cmake -S . -B build -DDMOD_TOOLS_NAME=arch/x86_64
cmake --build build --target dmft5336 test_dmft5336
export DMOD_DMF_DIR=$(pwd)/build/dmf
dmf-get install -d build/dmf/dmft5336.dmd -y
dmod_loader build/dmf/test_dmft5336.dmf
```

On a target, `touchtest` (see [tools/touchtest](tools/touchtest/README.md))
prints the chip info, the current state, or touches as they happen. In Renode
the STM32F7 Discovery platform has an FT5336 on I2C3; inject touches from the
monitor with `sysbus.i2c3.touchscreen MoveTo X Y`, `... Press`, `... Release`.

## Usage

Add dmi2c and the driver with its board configuration to the firmware, e.g.
in dmod-boot's `configs/board/stm32f746g-disco/flash.dmd`:

```
dmi2c
dmft5336 driver=board/stm32f746g-disco/touch.ini
touchtest
```

`touch.ini` configures the I2C pins, the dmi2c bus and the panel together;
the bus is handed to the driver as a friend (`friend_role=i2c_bus`), so no
bus path is configured by hand.

Then:

```c
#include "dmft5336.h"

void *touch = Dmod_FileOpen("/dev/touch", "r");
uint32_t timeout = 1000;
if (Dmod_Ioctl(touch, dmft5336_ioctl_cmd_wait_event, &timeout) == 0)
{
    dmft5336_state_t state;
    Dmod_FileRead(&state, 1, sizeof(state), touch);
    /* state.count points in state.points[] */
}
Dmod_FileClose(touch);
```

## Documentation

- **[docs/api-reference.md](docs/api-reference.md)** - device file, ioctl commands, types
- **[docs/configuration.md](docs/configuration.md)** - INI keys, INT pin, board example

View documentation using `dmf-man dmft5336`.

## Project Structure

```
dmft5336/
├── configs/board/          # Ready-made board configurations
├── docs/                   # Documentation (markdown format)
├── include/
│   ├── dmft5336.h          # Module API
│   └── dmft5336_types.h    # State, info, ioctl definitions
├── src/
│   ├── dmft5336.c          # dmdrvi driver, configuration, wait_event
│   ├── chip.c              # FT5336 register access over dmi2c, decoding
│   └── private.h
├── tests/                  # Host unit tests (dmod_loader)
├── tools/touchtest/        # On-target test application
├── CMakeLists.txt
├── Makefile
├── dmft5336.dmr
└── manifest.dmm
```

## Author

Patryk Kubiak

## License

MIT
