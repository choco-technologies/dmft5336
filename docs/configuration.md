# DMFT5336 Configuration Guide

`dmft5336` is configured by `dmdevfs` from an INI section with
`driver_name=dmft5336`. The section name becomes the node name
(`[touch]` -> `/dev/touch`).

## Parameters

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `address` | decimal | 56 (0x38) | Unshifted 7-bit I2C address, 8..119 |
| `width` | decimal | 0 | Screen width - inversion and clipping of X (0 = none) |
| `height` | decimal | 0 | Screen height - inversion and clipping of Y (0 = none) |
| `swap_xy` | `on`/`off` | `off` | The panel's X axis is the screen's Y axis |
| `invert_x` | `on`/`off` | `off` | Mirror X (needs `width`) |
| `invert_y` | `on`/`off` | `off` | Mirror Y (needs `height`) |
| `interrupt_handler` | name | (none) | dmhaman handler fired by the INT pin; without it `wait_event` polls |
| `poll_interval_ms` | decimal | 20 | Polling period of `wait_event` without an INT pin, 1..1000 |

Integers are decimal (`dmini_get_int`). The transformation is applied as:
swap X/Y, then invert X against `width - 1` and Y against `height - 1`.

## I2C bus (friend)

The bus is not configured as a path. Configure it in the same file - the
I2C pins (`dmgpio`) and the bus (`dmi2c`) - and put all sections in one
`friends_group`; give the bus section `friend_role=i2c_bus`. dmdevfs reports
the bus node's path to dmft5336 through `dmdrvi_friend_changed()` once it
exists. Until then (or if the bus goes away) every access to the chip
returns `-ENODEV`.

The driver only stores the path in that notification; the bus is opened and
the chip probed (chip ID, firmware ID, INT trigger mode) on first use - not
inside a dmdevfs callback.

Use the usual ordering: pins, then the bus, then the panel (e.g.
`driver_order` 10, 11 and 12). A bus shared with other devices (e.g. an
audio codec on the same controller) is configured once - other drivers on
it join the same friends group.

## INT pin (optional)

To have `wait_event` sleep on the chip's interrupt instead of polling,
configure the INT pin as a `dmgpio` input with an interrupt handler name and
give the touch section the same name:

```ini
[touch_int]
driver_name=dmgpio
driver_order=12
friends_group=touch
pin=PI13
mode=input
pull=up
interrupt_trigger=falling_edge
interrupt_handler=touch_int

[touch]
driver_name=dmft5336
driver_order=12
friends_group=touch
interrupt_handler=touch_int
```

Two things to check on a board before using it:

- **EXTI line sharing (STM32):** an EXTI line serves one port only. On the
  STM32F746G-DISCO the INT pin PI13 and the microSD card detect PC13 both
  need EXTI13 - which is why that board's configuration polls.
- **Interrupt priority:** the handler posts a dmosi semaphore from the GPIO
  interrupt, which FreeRTOS only allows at or below its maximum syscall
  priority. dmgpio releases without the EXTI priority fix leave the EXTI
  interrupt at the default (highest) NVIC priority, which trips FreeRTOS'
  `configASSERT` in `vPortValidateInterruptPriority()`.

## Example (STM32F746G-DISCO)

[`configs/board/stm32f746g-disco/touch.ini`](../configs/board/stm32f746g-disco/touch.ini)
is the complete configuration - the I2C3 pins, the bus and the panel:

```ini
[touch_i2c_scl]
driver_name=dmgpio
driver_order=10
friends_group=touch
pin=PH7
mode=alternate
alternate_function=4
output_circuit=open_drain

; ... [touch_i2c_sda] on PH8 likewise ...

[touch_i2c]
driver_name=dmi2c
driver_order=11
friends_group=touch
friend_role=i2c_bus
instance=3
baudrate=100000

[touch]
driver_name=dmft5336
driver_order=12
friends_group=touch
address=56
width=480
height=272
swap_xy=on
poll_interval_ms=20
```

Do not load dmi2c's own `i2c3.ini` next to it - it configures the same
controller.
