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

/**
 * @brief What happened to a touch point (FT5336 event flag).
 */
typedef enum
{
    dmft5336_event_down = 0,    /**< The finger has just touched the panel */
    dmft5336_event_up,          /**< The finger has just been lifted */
    dmft5336_event_contact,     /**< The finger stays on the panel */
    dmft5336_event_none,        /**< No event */
} dmft5336_event_t;

/**
 * @brief One touch point, in screen coordinates (after the configured
 *        swap/inversion).
 */
typedef struct
{
    uint16_t            x;          /**< Column */
    uint16_t            y;          /**< Line */
    uint8_t             id;         /**< Touch ID - stays the same while the finger moves */
    uint8_t             event;      /**< dmft5336_event_t */
    uint8_t             weight;     /**< Touch pressure (0 when the chip does not report it) */
    uint8_t             area;       /**< Touch area (0 when the chip does not report it) */
} dmft5336_point_t;

/**
 * @brief Touch state - what read() of the device node returns.
 *
 * count == 0 means nothing touches the panel.
 */
typedef struct
{
    uint8_t             count;                          /**< Number of valid entries in points */
    uint8_t             reserved[3];
    dmft5336_point_t    points[DMFT5336_MAX_POINTS];    /**< Touch points */
} dmft5336_state_t;

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
 * @brief Information returned by dmft5336_ioctl_cmd_get_info.
 */
typedef struct
{
    uint8_t             chip_id;            /**< Chip ID register (0x51 for an FT5336), 0 until the chip was reached */
    uint8_t             firmware_id;        /**< Firmware version register */
    uint8_t             max_points;         /**< DMFT5336_MAX_POINTS */
    bool                interrupt_driven;   /**< wait_event sleeps on the INT pin (else it polls) */
    dmft5336_transform_t transform;         /**< Configured coordinate transformation */
} dmft5336_info_t;

/**
 * @brief IOCTL commands of the dmft5336 device.
 *
 * read() of the node returns a dmft5336_state_t (the buffer must be at least
 * that large). These commands cover the rest.
 */
typedef enum
{
    /* Private commands start at DMDRVI_IOCTL_CUSTOM_BASE: everything below
     * it is a standard dmdrvi command (network, block, monitor) that dmdevfs
     * and other generic code may send to any node - dmft5336 answers -ENOTTY. */
    dmft5336_ioctl_cmd_get_info = DMDRVI_IOCTL_CUSTOM_BASE, /**< arg: dmft5336_info_t* */
    dmft5336_ioctl_cmd_get_state,       /**< arg: dmft5336_state_t* - same as read() */
    dmft5336_ioctl_cmd_wait_event,      /**< arg: const uint32_t* timeout in ms, or NULL to wait forever */

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
