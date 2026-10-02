# DMFT5336 Configuration Files

Ready-made touch panel configurations for development boards.

```
configs/
└── board/
    └── stm32f746g-disco/
        └── touch.ini      # FT5336 of the 4.3" LCD, on I2C3
```

The I2C bus is not part of these files - it belongs to dmi2c's own board
configuration (a bus is often shared, e.g. with an audio codec), so load
both. See [docs/configuration.md](../docs/configuration.md) for every key.

## Boards

| Board | File | Bus | Notes |
|-------|------|-----|-------|
| 32F746G-DISCOVERY | `board/stm32f746g-disco/touch.ini` | dmi2c `i2c3.ini` (`/dev/dmi2cx/touch_i2c`), address 0x38 | Axes swapped relative to the LCD (`swap_xy=on`, 480x272), as in ST's BSP. Polled: the INT pin PI13 shares EXTI13 with the microSD card detect PC13. Verified on the board (chip ID 0x51, firmware 0x12) and in Renode (injected touches) |
