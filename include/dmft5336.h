#ifndef DMFT5336_H
#define DMFT5336_H

#include "dmft5336_defs.h"
#include "dmft5336_types.h"

/**
 * @brief Decode one touch point record as read from the chip.
 *
 * @p raw holds the point's registers starting at its XH register:
 * XH, XL, YH, YL, WEIGHT, MISC (raw[4]/raw[5] may be zero). The panel
 * coordinates are converted to screen coordinates with @p transform.
 *
 * @return true if the record describes a touch (event flag other than
 *         "none"), false otherwise or on NULL arguments.
 */
dmod_dmft5336_api(1.0, bool, _decode_point, ( const uint8_t raw[6], const dmft5336_transform_t* transform, dmft5336_point_t* point ));

/**
 * @brief Compare two touch states field by field (padding is ignored).
 *
 * @return true if both report the same points, false otherwise or on NULL.
 */
dmod_dmft5336_api(1.0, bool, _states_equal, ( const dmft5336_state_t* a, const dmft5336_state_t* b ));

#endif // DMFT5336_H
