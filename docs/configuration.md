# DMFT5336 Configuration Guide

`dmft5336` is configured by `dmdevfs` from an INI section with
`driver_name=dmft5336`. The section name becomes the node name
(`[touch]` -> `/dev/touch`).

## Parameters

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `i2c_bus` | path | (required) | dmi2c node of the bus the chip is on, e.g. `/dev/dmi2cx/touch_i2c` |
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

## Order and dependencies

The bus must exist before the touch panel is used: give the touch section a
`driver_order` after the dmi2c bus (dmi2c's board files use 10 for the pins
and 11 for the bus). The bus node is only opened on first use, so the order
matters for when the panel becomes usable, not for creating it.

## INT pin (optional)

To have `wait_event` sleep on the chip's interrupt instead of polling,
configure the INT pin as a `dmgpio` input with an interrupt handler name and
give the touch section the same name:

```ini
[touch_int]
driver_name=dmgpio
driver_order=12
pin=PI13
mode=input
pull=up
interrupt_trigger=falling_edge
interrupt_handler=touch_int

[touch]
driver_name=dmft5336
driver_order=12
i2c_bus=/dev/dmi2cx/touch_i2c
interrupt_handler=touch_int
```

Two things to check on a board before using it:

- **EXTI line sharing (STM32):** an EXTI line serves one port only. On the
  STM32F746G-DISCO the INT pin PI13 and the microSD card detect PC13 both
  need EXTI13 - which is why that board's configuration polls.
- **Interrupt priority:** the handler posts a dmosi semaphore from the GPIO
  interrupt, which FreeRTOS only allows at or below its maximum syscall
  priority. dmgpio currently leaves the EXTI interrupt at the default
  (highest) NVIC priority, which trips FreeRTOS' `configASSERT` in
  `vPortValidateInterruptPriority()`.

## Example (STM32F746G-DISCO)

See [`configs/board/stm32f746g-disco/touch.ini`](../configs/board/stm32f746g-disco/touch.ini)
- together with dmi2c's `configs/board/stm32f746g-disco/i2c3.ini`:

```ini
[touch]
driver_name=dmft5336
driver_order=12
i2c_bus=/dev/dmi2cx/touch_i2c
address=56
width=480
height=272
swap_xy=on
poll_interval_ms=20
```
