#ifndef DMFT5336_TYPES_H
#define DMFT5336_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmdrvi_ioctl.h"

/** The FT5336 reports up to five simultaneous touch points. */
#define DMFT5336_MAX_POINTS     5

/** Value of the chip ID register (0xA8) of an FT5336. */
#define DMFT5336_CHIP_ID        0x51

/** Device name reported by DMDRVI_IOCTL_INPUT_GET_INFO. */
#define DMFT5336_DEVICE_NAME    "FT5336"

/**
 * @brief Coordinate transformation from the panel to the screen.
 *
 * Applied in this order: swap X and Y, then invert X against width - 1 and
 * Y against height - 1. Width and height are the screen size, used for the
 * inversion and to clip the reported coordinates (0 = no clipping).
 */
typedef struct
{
    uint16_t            width;      /**< Screen width in pixels */
    uint16_t            height;     /**< Screen height in pixels */
    bool                swap_xy;    /**< Panel X is the screen Y and vice versa */
    bool                invert_x;   /**< Mirror horizontally */
    bool                invert_y;   /**< Mirror vertically */
} dmft5336_transform_t;

/**
 * @brief Information returned by dmft5336_ioctl_cmd_get_chip_info.
 */
typedef struct
{
    uint8_t             chip_id;            /**< Chip ID register (0x51 for an FT5336), 0 until the chip was reached */
    uint8_t             firmware_id;        /**< Firmware version register */
    dmft5336_transform_t transform;         /**< Configured coordinate transformation */
} dmft5336_chip_info_t;

/**
 * @brief IOCTL commands specific to the dmft5336 device.
 *
 * The node is a standard dmdrvi input device: read() returns a
 * dmdrvi_input_state_t, DMDRVI_IOCTL_INPUT_GET_INFO / _GET_STATE /
 * _WAIT_EVENT work as for any other input driver. These commands only add
 * what is specific to the FT5336.
 */
typedef enum
{
    /* Private commands start at DMDRVI_IOCTL_CUSTOM_BASE: everything below
     * it is a standard dmdrvi command. Generic code must check that the node
     * is an FT5336 (DMDRVI_IOCTL_INPUT_GET_INFO name) before sending these -
     * another driver numbers its own commands from the same base. */
    dmft5336_ioctl_cmd_get_chip_info = DMDRVI_IOCTL_CUSTOM_BASE,   /**< arg: dmft5336_chip_info_t* */

    dmft5336_ioctl_cmd_max
} dmft5336_ioctl_cmd_t;

/**
 * @brief Opaque driver context type (forward declaration)
 *
 * The concrete definition is private to the dmft5336 driver.
 */
struct dmdrvi_context;
typedef struct dmdrvi_context *dmdrvi_context_t;

#endif /* DMFT5336_TYPES_H */
