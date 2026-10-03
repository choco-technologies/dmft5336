# touchtest

Manual test tool for a configured input device - a `dmft5336` touch panel or
any other dmdrvi input driver. It only opens the device node and uses the
standard `DMDRVI_IOCTL_INPUT_*` commands - configuration is done by
`dmdevfs` from the board INI.

```
touchtest [-d DEVICE] COMMAND [ARGS]
  info                     print what the device reports (and the FT5336 chip)
  read                     print the current input state
  watch [EVENTS] [IDLE_MS] print touches as they happen (default 20 events,
                           stops after IDLE_MS without a touch, default 10000)
```

`DEVICE` defaults to `/dev/touch`. Example output of `watch`:

```
touchtest: watching for 20 events (idle timeout 10000 ms)
touch: 1/1 id=0 x=240 y=136 move
touch: 1/1 id=0 x=300 y=200 move
touch: released
```

`info` shows the standard device info (`device: FT5336 (touchscreen)`,
capabilities, screen size); for an FT5336 it adds `chip id: 0x51 (FT5336)`
once the chip answered on the bus. A mouse's motion and buttons are printed
as `pointer: dx=.. dy=.. wheel=.. buttons=0x..`.
