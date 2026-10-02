# dmft5336 API Reference

`dmft5336` is a [dmdrvi](https://github.com/choco-technologies/dmdrvi) driver
(DIF version 2.0) for the FocalTech FT5336 capacitive touch controller. The
chip is reached through a [dmi2c](https://github.com/choco-technologies/dmi2c)
bus node - reported to the driver as a friend (`friend_role=i2c_bus`, see
[configuration.md](configuration.md)); the driver has no hardware port of its
own.

`dmdevfs` names the node after the configuration section - `[touch]` becomes
`/dev/touch` (`/dev/dmft53360` when the section name cannot be used).

## Device file

| Operation | Behavior |
|-----------|----------|
| `open` / `close` | No per-handle state. The bus is opened and the chip probed on first use. |
| `read` | Returns the current touch state as a `dmft5336_state_t`. The buffer must be at least that large (`-EINVAL` otherwise); the offset is ignored. `-ENODEV` while the bus cannot be opened, a negative dmi2c error when the chip does not answer. |
| `write` | Not supported (`-ENOTSUP`; a zero-length write returns 0). |
| `stat` | Size 0, mode 0444. |

## IOCTL commands

Numbered from `DMDRVI_IOCTL_CUSTOM_BASE`. Any other command, including the
standard dmdrvi network/block/monitor commands, returns `-ENOTTY`.

| Command | `arg` | Description |
|---------|-------|-------------|
| `dmft5336_ioctl_cmd_get_info` | `dmft5336_info_t*` | Chip and firmware ID (0 until the chip was reached), maximum points, event source, configured transformation |
| `dmft5336_ioctl_cmd_get_state` | `dmft5336_state_t*` | Same as `read()` |
| `dmft5336_ioctl_cmd_wait_event` | `const uint32_t*` timeout in ms, or `NULL` to wait forever | Block until something new happens. `-ETIMEDOUT` when nothing did |

`wait_event` behaves according to the configuration:

- **INT pin** (`interrupt_handler` set): sleeps until the chip's INT edge
  fires the named dmhaman handler. At most one pending report is remembered,
  so a touch between two waits is not lost.
- **Polling** (no `interrupt_handler`): reads the chip every
  `poll_interval_ms` until the state differs from the last one handed out by
  `read()`, `get_state` or a previous `wait_event`.

A typical loop:

```c
#include "dmft5336.h"

void *touch = Dmod_FileOpen("/dev/touch", "r");
for (;;)
{
    uint32_t timeout = 1000;
    if (Dmod_Ioctl(touch, dmft5336_ioctl_cmd_wait_event, &timeout) != 0)
        continue;                               /* nothing for a second */

    dmft5336_state_t state;
    Dmod_FileRead(&state, 1, sizeof(state), touch);
    for (uint8_t i = 0; i < state.count; i++)
        handle_touch(state.points[i].x, state.points[i].y);
    if (state.count == 0)
        handle_release();
}
```

## Types (`dmft5336_types.h`)

```c
typedef enum {
    dmft5336_event_down, dmft5336_event_up,
    dmft5336_event_contact, dmft5336_event_none,
} dmft5336_event_t;

typedef struct {
    uint16_t x, y;        /* screen coordinates, after swap/inversion */
    uint8_t  id;          /* stays the same while the finger moves */
    uint8_t  event;       /* dmft5336_event_t */
    uint8_t  weight;      /* 0 when not reported */
    uint8_t  area;        /* 0 when not reported */
} dmft5336_point_t;

typedef struct {
    uint8_t          count;   /* 0 = nothing touches the panel */
    uint8_t          reserved[3];
    dmft5336_point_t points[DMFT5336_MAX_POINTS];   /* 5 */
} dmft5336_state_t;

typedef struct {
    uint16_t width, height;   /* screen size, for inversion and clipping (0 = none) */
    bool swap_xy, invert_x, invert_y;
} dmft5336_transform_t;

typedef struct {
    uint8_t chip_id, firmware_id, max_points;
    bool    interrupt_driven;
    dmft5336_transform_t transform;
} dmft5336_info_t;
```

## Module API (`dmft5336.h`)

| Function | Description |
|----------|-------------|
| `bool dmft5336_decode_point(const uint8_t raw[6], const dmft5336_transform_t*, dmft5336_point_t*)` | Decode one point record (XH XL YH YL WEIGHT MISC) into screen coordinates; false for an empty record |
| `bool dmft5336_states_equal(const dmft5336_state_t*, const dmft5336_state_t*)` | Compare two states field by field |

## Chip access

Every access is one dmi2c transfer: a register write followed by a read
(repeated START). The driver reads `TD_STATUS` (0x02), then each active point
record (0x03 + 6*n) separately rather than one burst from 0x00 - the chip
accepts both and some models of it (Renode's FT5336) only answer
per-register requests. On first use it checks the chip ID (0xA8, 0x51 for an
FT5336), reads the firmware ID (0xA6) and sets `G_MODE` (0xA4) to trigger
mode, so INT pulses on every report.
