#ifndef DMFT5336_PRIVATE_H
#define DMFT5336_PRIVATE_H

#include "dmod.h"
#include "dmosi.h"
#include "dmft5336.h"

/* Magic set to "FT53" */
#define DMFT5336_CONTEXT_MAGIC      0x46543533

/* FT5336 registers (FocalTech FT5x36 register map, as used by ST's BSP). */
#define FT5336_REG_TD_STATUS        0x02    /* Number of touch points, bits 3:0 */
#define FT5336_REG_P1_XH            0x03    /* First point record */
#define FT5336_POINT_RECORD_SIZE    6       /* XH XL YH YL WEIGHT MISC */
#define FT5336_REG_G_MODE           0xA4    /* Interrupt mode */
#define FT5336_REG_FIRMWARE_ID      0xA6
#define FT5336_REG_CHIP_ID          0xA8

#define FT5336_G_MODE_TRIGGER       0x01    /* INT pulses on every new report */

/* Event flag of a point record (XH bits 7:6) */
#define FT5336_EVENT_DOWN           0
#define FT5336_EVENT_UP             1
#define FT5336_EVENT_CONTACT        2
#define FT5336_EVENT_NONE           3

/* friend_role of the dmi2c bus the chip is on (same friends_group) */
#define DMFT5336_BUS_FRIEND_ROLE    "i2c_bus"

/**
 * @brief DMDRVI context structure
 */
struct dmdrvi_context
{
    uint32_t                magic;              /**< Magic number for validation */
    char                   *bus_path;           /**< dmi2c node of the bus (friend_role=i2c_bus), NULL until reported */
    uint16_t                address;            /**< Unshifted 7-bit I2C address */
    dmft5336_transform_t    transform;          /**< Panel -> screen coordinates */
    char                   *interrupt_handler;  /**< dmhaman name fired by the INT pin (NULL = poll) */
    uint32_t                poll_interval_ms;   /**< wait_event polling period without INT, while active */
    uint32_t                idle_poll_interval_ms; /**< ... while nothing touched for active_ms */
    uint32_t                active_ms;          /**< How long after a change the panel counts as active */
    uint32_t                last_change_ms;     /**< When the state last changed (tick count) */
    void                   *bus;                /**< Open bus node, NULL until the chip was reached */
    uint8_t                 chip_id;            /**< Read once the chip is reached */
    uint8_t                 firmware_id;
    dmosi_mutex_t           lock;               /**< Serializes bus access and state */
    dmosi_semaphore_t       event_sem;          /**< Posted from the INT handler */
    dmdrvi_input_state_t    last_state;         /**< Last state handed out (poll change detection) */
};

/* chip.c - everything that talks to the FT5336 over dmi2c. The caller holds
 * context->lock. */
int  chip_connect(dmdrvi_context_t context);
void chip_disconnect(dmdrvi_context_t context);
int  chip_read_state(dmdrvi_context_t context, dmdrvi_input_state_t *state);

#endif // DMFT5336_PRIVATE_H
