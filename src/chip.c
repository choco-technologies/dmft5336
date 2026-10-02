#include "private.h"
#include "dmi2c_types.h"
#include <errno.h>
#include <string.h>

/* Every access is a register write followed by a read, in one dmi2c
 * transfer (repeated START). Points are read one record at a time instead of
 * one burst from register 0x00: the chip accepts both, and some models of it
 * (Renode's FT5336) only answer per-register requests. */

static int write_register(dmdrvi_context_t context, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    dmi2c_message_t message = { .address = context->address, .read = false, .data = data, .size = sizeof(data) };
    dmi2c_transfer_t transfer = { &message, 1 };

    return Dmod_Ioctl(context->bus, dmi2c_ioctl_cmd_transfer, &transfer);
}

static int read_registers(dmdrvi_context_t context, uint8_t reg, uint8_t *data, size_t size)
{
    dmi2c_message_t messages[2] = {
        { .address = context->address, .read = false, .data = &reg, .size = 1 },
        { .address = context->address, .read = true,  .data = data, .size = size },
    };
    dmi2c_transfer_t transfer = { messages, 2 };

    return Dmod_Ioctl(context->bus, dmi2c_ioctl_cmd_transfer, &transfer);
}

/* Reads the chip ID and switches INT to trigger mode (a pulse per report),
 * which is what wait_event's edge interrupt expects. */
static int probe(dmdrvi_context_t context)
{
    uint8_t chip_id = 0;
    int ret = read_registers(context, FT5336_REG_CHIP_ID, &chip_id, 1);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("No FT5336 at 0x%02X on %s (%d)\n", context->address, context->bus_path, ret);
        return ret;
    }
    if (chip_id != DMFT5336_CHIP_ID)
        DMOD_LOG_WARN("Unexpected chip ID 0x%02X at 0x%02X (FT5336 reports 0x%02X)\n",
                      chip_id, context->address, DMFT5336_CHIP_ID);

    /* The firmware ID is informational only - not every chip revision (or
     * emulator) implements it. */
    if (read_registers(context, FT5336_REG_FIRMWARE_ID, &context->firmware_id, 1) != 0)
        context->firmware_id = 0;

    ret = write_register(context, FT5336_REG_G_MODE, FT5336_G_MODE_TRIGGER);
    if (ret == 0)
        context->chip_id = chip_id;
    return ret;
}

int chip_connect(dmdrvi_context_t context)
{
    if (context->bus != NULL)
        return 0;
    if (context->bus_path == NULL)
        return -ENODEV;     /* The i2c_bus friend has not been reported (yet) */

    context->bus = Dmod_FileOpen(context->bus_path, "r+");
    if (context->bus == NULL)
    {
        DMOD_LOG_ERROR("Cannot open I2C bus %s\n", context->bus_path);
        return -ENODEV;
    }

    int ret = probe(context);
    if (ret != 0)
    {
        chip_disconnect(context);
        return ret;
    }

    DMOD_LOG_INFO("FT5336 found at 0x%02X on %s (chip ID 0x%02X, firmware 0x%02X)\n",
                  context->address, context->bus_path, context->chip_id, context->firmware_id);
    return 0;
}

void chip_disconnect(dmdrvi_context_t context)
{
    if (context->bus != NULL)
    {
        Dmod_FileClose(context->bus);
        context->bus = NULL;
    }
    context->chip_id     = 0;
    context->firmware_id = 0;
}

int chip_read_state(dmdrvi_context_t context, dmft5336_state_t *state)
{
    uint8_t status = 0;
    int ret = read_registers(context, FT5336_REG_TD_STATUS, &status, 1);

    memset(state, 0, sizeof(*state));
    /* Values above the maximum appear while the chip is not reporting. */
    uint8_t count = status & 0x0F;
    if (ret != 0 || count > DMFT5336_MAX_POINTS)
        return ret;

    for (uint8_t i = 0; i < count; i++)
    {
        uint8_t raw[FT5336_POINT_RECORD_SIZE] = { 0 };
        ret = read_registers(context, (uint8_t)(FT5336_REG_P1_XH + i * FT5336_POINT_RECORD_SIZE), raw, sizeof(raw));
        if (ret != 0)
            return ret;
        if (dmft5336_decode_point(raw, &context->transform, &state->points[state->count]))
            state->count++;
    }
    return 0;
}

/* ---- Decoding ---- */

static uint16_t transform_axis(uint16_t value, uint16_t size, bool invert)
{
    if (size == 0)
        return value;
    if (value >= size)
        value = (uint16_t)(size - 1U);
    return invert ? (uint16_t)(size - 1U - value) : value;
}

dmod_dmft5336_api_declaration(1.0, bool, _decode_point, ( const uint8_t raw[6], const dmft5336_transform_t* transform, dmft5336_point_t* point ))
{
    if (raw == NULL || transform == NULL || point == NULL)
        return false;

    uint8_t event = (uint8_t)(raw[0] >> 6);
    uint16_t panel_x = (uint16_t)(((raw[0] & 0x0FU) << 8) | raw[1]);
    uint16_t panel_y = (uint16_t)(((raw[2] & 0x0FU) << 8) | raw[3]);
    uint16_t x = transform->swap_xy ? panel_y : panel_x;
    uint16_t y = transform->swap_xy ? panel_x : panel_y;

    point->x      = transform_axis(x, transform->width, transform->invert_x);
    point->y      = transform_axis(y, transform->height, transform->invert_y);
    point->id     = (uint8_t)(raw[2] >> 4);
    point->event  = event;
    point->weight = raw[4];
    point->area   = (uint8_t)(raw[5] >> 4);
    return event != dmft5336_event_none;
}

static bool points_equal(const dmft5336_point_t *a, const dmft5336_point_t *b)
{
    return a->x == b->x && a->y == b->y && a->id == b->id && a->event == b->event &&
           a->weight == b->weight && a->area == b->area;
}

dmod_dmft5336_api_declaration(1.0, bool, _states_equal, ( const dmft5336_state_t* a, const dmft5336_state_t* b ))
{
    if (a == NULL || b == NULL || a->count != b->count || a->count > DMFT5336_MAX_POINTS)
        return false;
    for (uint8_t i = 0; i < a->count; i++)
    {
        if (!points_equal(&a->points[i], &b->points[i]))
            return false;
    }
    return true;
}
