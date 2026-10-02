#ifndef DMFT5336_H
#define DMFT5336_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmod_types.h"
#include "dmft5336_defs.h"

/**
 * Public API for the dmft5336 module.
 *
 * Functions are declared with the dmod_dmft5336_api(...) macro - dmod's
 * standard pattern for functions callable from other modules (or from this
 * module's own tests/), resolved dynamically by the loader rather than
 * through normal static linkage. See dm_sw_ring/include/dm_sw_ring.h for a
 * fully worked real-world example of the same shape.
 *
 * Definitions in src/dmft5336.c use the matching
 * dmod_dmft5336_api_declaration(...) macro - a plain C function
 * definition here will NOT satisfy these declarations at link time.
 *
 * This is an example interface using the usual "opaque handle" pattern -
 * replace the handle, functions, and struct definition in
 * src/dmft5336.c with your module's real API.
 */

/* Opaque handle - the real struct is defined in src/dmft5336.c */
typedef struct dmft5336* dmft5336_t;

/**
 * Create a new dmft5336 instance.
 *
 * @return A valid handle on success, or NULL on allocation failure.
 */
dmod_dmft5336_api(1.0, dmft5336_t, _create, ( void ));

/**
 * Destroy an instance created by dmft5336_create(). Safe to call with
 * NULL.
 */
dmod_dmft5336_api(1.0, void, _destroy, ( dmft5336_t handle ));

/**
 * Example accessor - replace with your module's real API.
 *
 * @return true if handle is a valid, non-NULL instance.
 */
dmod_dmft5336_api(1.0, bool, _is_valid, ( dmft5336_t handle ));

#endif // DMFT5336_H
