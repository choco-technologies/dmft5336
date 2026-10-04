# dmft5336 API Reference

`dmft5336` is a [dmdrvi](https://github.com/choco-technologies/dmdrvi) driver
(DIF version 2.0) for the FocalTech FT5336 capacitive touch controller. The
chip is reached through a [dmi2c](https://github.com/choco-technologies/dmi2c)
bus node - reported to the driver as a friend (`friend_role=i2c_bus`, see
[configuration.md](configuration.md)); the driver has no hardware port of its
own.

`dmdevfs` names the node after the configuration section - `[touch]` becomes
`/dev/touch` (`/dev/dmft53360` when the section name cannot be used).

The node is a standard dmdrvi **input device**: it implements the
`DMDRVI_IOCTL_INPUT_*` commands and returns `dmdrvi_input_state_t` from
`read()` (`dmdrvi_ioctl.h`, see dmdrvi's "Input Ioctl Commands"). Code
written against that interface - a GUI input driver, `touchtest` - works
with this driver and with any other touch panel or mouse driver alike.

## Device file

| Operation | Behavior |
|-----------|----------|
| `open` / `close` | No per-handle state. The bus is opened and the chip probed on first use. |
| `read` | Returns the current touch state as a `dmdrvi_input_state_t`. The buffer must be at least that large (`-EINVAL` otherwise); the offset is ignored. `-ENODEV` while the bus cannot be opened, a negative dmi2c error when the chip does not answer. |
| `write` | Not supported (`-ENOTSUP`; a zero-length write returns 0). |
| `stat` | Size 0, mode 0444. |

## IOCTL commands

| Command | `arg` | Description |
|---------|-------|-------------|
| `DMDRVI_IOCTL_INPUT_GET_INFO` | `dmdrvi_input_info_t*` | Name `FT5336`, type `DMDRVI_INPUT_TYPE_TOUCHSCREEN`, capabilities `CONTACTS`, `PRESSURE`, `CONTACT_SIZE` (+ `INTERRUPT` with an INT pin), `width`/`height` from the configuration, `max_contacts` 5. Answered without reaching the chip |
| `DMDRVI_IOCTL_INPUT_GET_STATE` | `dmdrvi_input_state_t*` | Same as `read()` |
| `DMDRVI_IOCTL_INPUT_WAIT_EVENT` | `const uint32_t*` timeout in ms, or `NULL` to wait forever | Block until something new happens. `-ETIMEDOUT` when nothing did |
| `dmft5336_ioctl_cmd_get_chip_info` | `dmft5336_chip_info_t*` | Driver specific (`DMDRVI_IOCTL_CUSTOM_BASE`): chip and firmware ID (0 until the chip was reached), configured transformation |

Any other command, including the standard dmdrvi network/block/monitor/
graphics commands, returns `-ENOTTY`. Send `dmft5336_ioctl_cmd_get_chip_info`
only to a node whose `GET_INFO` name is `FT5336` - other drivers number
their own commands from the same base.

`WAIT_EVENT` behaves according to the configuration:

- **INT pin** (`interrupt_handler` set): sleeps until the chip's INT edge
  fires the named dmhaman handler. At most one pending report is remembered,
  so a touch between two waits is not lost.
- **Polling** (no `interrupt_handler`): reads the chip until the state
  differs from the last one handed out by `read()` or `GET_STATE`, or the
  timeout ends - it never sleeps past the timeout, so a caller waiting until
  its next frame is back on time. While the panel is touched, and for
  `active_ms` after a change, it reads every `poll_interval_ms`; otherwise
  every `idle_poll_interval_ms` - fast while someone uses it, few bus
  transfers while nobody does.

The state the chip reports maps onto `dmdrvi_input_state_t` as:

| `dmdrvi_input_contact_t` | FT5336 |
|--------------------------|--------|
| `x`, `y` | Point coordinates after swap/inversion/clipping |
| `id` | Touch ID (YH bits 7:4) |
| `event` | Event flag: press down -> `DOWN`, contact -> `MOVE`, lift up -> `UP` |
| `pressure` | WEIGHT register (0 when the chip does not report it) |
| `size` | MISC bits 7:4 - touch area (0 when not reported) |

`buttons`, `dx`, `dy` and `wheel` are always 0.

A typical loop:

```c
#include "dmdrvi_ioctl.h"

void *touch = Dmod_FileOpen("/dev/touch", "r");
for (;;)
{
    uint32_t timeout = 1000;
    if (Dmod_Ioctl(touch, DMDRVI_IOCTL_INPUT_WAIT_EVENT, &timeout) != 0)
        continue;                               /* nothing for a second */

    dmdrvi_input_state_t state;
    Dmod_FileRead(&state, 1, sizeof(state), touch);
    for (uint8_t i = 0; i < state.contact_count; i++)
        handle_touch(state.contacts[i].x, state.contacts[i].y);
    if (state.contact_count == 0)
        handle_release();
}
```

## Types (`dmft5336_types.h`)

The state and info types are dmdrvi's (`dmdrvi_input_state_t`,
`dmdrvi_input_contact_t`, `dmdrvi_input_info_t`). The driver adds:

```c
#define DMFT5336_MAX_POINTS   5
#define DMFT5336_CHIP_ID      0x51
#define DMFT5336_DEVICE_NAME  "FT5336"

typedef struct {
    uint16_t width, height;   /* screen size, for inversion and clipping (0 = none) */
    bool swap_xy, invert_x, invert_y;
} dmft5336_transform_t;

typedef struct {
    uint8_t chip_id, firmware_id;
    dmft5336_transform_t transform;
} dmft5336_chip_info_t;
```

## Module API (`dmft5336.h`)

| Function | Description |
|----------|-------------|
| `bool dmft5336_decode_point(const uint8_t raw[6], const dmft5336_transform_t*, dmdrvi_input_contact_t*)` | Decode one point record (XH XL YH YL WEIGHT MISC) into a contact in screen coordinates; false for an empty record |

## Chip access

Every access is one dmi2c transfer: a register write followed by a read
(repeated START). The driver reads `TD_STATUS` (0x02), then each active point
record (0x03 + 6*n) separately rather than one burst from 0x00 - the chip
accepts both and some models of it (Renode's FT5336) only answer
per-register requests. On first use it checks the chip ID (0xA8, 0x51 for an
FT5336), reads the firmware ID (0xA6) and sets `G_MODE` (0xA4) to trigger
mode, so INT pulses on every report.
