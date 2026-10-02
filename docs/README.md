# dmft5336 Documentation

`dmft5336` is a dmdrvi driver for the FocalTech FT5336 capacitive touch
controller, reached through a dmi2c bus. It exposes the panel as a device
node (e.g. `/dev/touch`).

## Contents

- **[api-reference.md](api-reference.md)** - device file, ioctl commands, types, module API
- **[configuration.md](configuration.md)** - INI keys, INT pin, board example

## Quick Reference

```c
#include "dmft5336.h"

void *touch = Dmod_FileOpen("/dev/touch", "r");
dmft5336_state_t state;
Dmod_FileRead(&state, 1, sizeof(state), touch);   /* state.count points */
Dmod_FileClose(touch);
```

View documentation using `dmf-man`:

```bash
dmf-man dmft5336          # Main documentation
dmf-man dmft5336 api      # API reference
```
