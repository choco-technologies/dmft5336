# DMFT5336 Configuration Files

Ready-made touch panel configurations for development boards.

```
configs/
└── board/
    └── stm32f746g-disco/
        └── touch.ini      # FT5336 of the 4.3" LCD, on I2C3
```

Each file is a complete configuration: the I2C pins (`dmgpio`), the bus
(`dmi2c`, `friend_role=i2c_bus`) and the panel, in one `friends_group` -
dmdevfs hands the bus to the driver. Load only dmi2c's module next to it,
not its own board file for the same controller. See
[docs/configuration.md](../docs/configuration.md) for every key.

## Boards

| Board | File | Bus | Notes |
|-------|------|-----|-------|
| 32F746G-DISCOVERY | `board/stm32f746g-disco/touch.ini` | I2C3 on PH7/PH8, 100 kHz (`/dev/dmi2cx/touch_i2c`), address 0x38 | Axes swapped relative to the LCD (`swap_xy=on`, 480x272), as in ST's BSP. Polled: the INT pin PI13 shares EXTI13 with the microSD card detect PC13. Verified on the board (chip ID 0x51, firmware 0x12) and in Renode (injected touches) |
