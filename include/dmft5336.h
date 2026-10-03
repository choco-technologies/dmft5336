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
dmod_dmft5336_api(2.0, bool, _decode_point, ( const uint8_t raw[6], const dmft5336_transform_t* transform, dmdrvi_input_contact_t* contact ));

#endif // DMFT5336_H
