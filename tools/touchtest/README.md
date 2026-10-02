# touchtest

Manual test tool for a configured `dmft5336` touch panel. It only opens the
device node - configuration is done by `dmdevfs` from the board INI.

```
touchtest [-d DEVICE] COMMAND [ARGS]
  info                     print the chip and driver configuration
  read                     print the current touch state
  watch [EVENTS] [IDLE_MS] print touches as they happen (default 20 events,
                           stops after IDLE_MS without a touch, default 10000)
```

`DEVICE` defaults to `/dev/touch`. Example output of `watch`:

```
touchtest: watching for 20 events (idle timeout 10000 ms)
touch: 1/1 id=0 x=240 y=136 contact
touch: 1/1 id=0 x=300 y=200 contact
touch: released
```

`info` shows `chip id: 0x51 (FT5336)` once the chip answered on the bus.
