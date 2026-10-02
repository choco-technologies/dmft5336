# dmft5336

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmft5336/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmft5336/actions/workflows/ci.yml)

dmft5336 DMOD library module.

## Description

TODO: describe what this module does.

## Building

### Using CMake

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout
instead of fetching `develop` from GitHub.

### Using Make

```bash
make DMOD_MODE=DMOD_MODULE DMOD_DIR=/path/to/dmod
```

## Testing

Tests are built automatically alongside the module (see `tests/`). Once built,
run them with `ctest`:

```bash
cd build
ctest --output-on-failure
```

`ctest` installs the test module's dependencies with `dmf-get` and then runs
it through `dmod_loader`. To run it manually instead:

```bash
export DMOD_DMF_DIR=$(pwd)/build/dmf
dmf-get install -d ${DMOD_DMF_DIR}/test_dmft5336-local.dmd -y
dmod_loader build/dmf/test_dmft5336.dmf
```

## Usage

<TBD>

This library module provides functions that can be used by other modules:

```c
#include "dmft5336.h"
```

## API

| Function | Description |
|----------|-------------|
| `dmft5336_create()` | Create a new `dmft5336_t` instance. |
| `dmft5336_destroy()` | Destroy an instance created by `_create()`. |
| `dmft5336_is_valid()` | Check whether a handle is a valid instance. |

See [include/dmft5336.h](include/dmft5336.h) for the full
declarations and [docs/api-reference.md](docs/api-reference.md) for the
complete reference.

## Documentation

See the `docs/` directory:

- **[api-reference.md](docs/api-reference.md)** - Complete API documentation

View documentation using `dmf-man dmft5336`.
## Project Structure

```
dmft5336/
├── docs/              # Documentation (markdown format)
├── include/           # Public headers
│   └── dmft5336.h
├── src/
│   └── dmft5336.c
├── tests/
│   ├── CMakeLists.txt
│   └── dmft5336_test.c
├── CMakeLists.txt
├── Makefile
├── dmft5336.dmr
└── manifest.dmm
```

## Author

Patryk Kubiak

## License

MIT
