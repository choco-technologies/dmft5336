#include "dmod.h"
#include "dmft5336.h"
#include <string.h>

/**
 * @brief Manual input device test tool.
 *
 * Works on an already-configured input node - it does not configure
 * anything itself, it only uses the device file (read and the standard
 * DMDRVI_IOCTL_INPUT_* commands), so it works with any dmdrvi input driver.
 * For an FT5336 it also prints the chip registers.
 */

#define DEFAULT_DEVICE      "/dev/touch"
#define DEFAULT_EVENTS      20U
#define DEFAULT_IDLE_MS     10000U

static void print_usage(const char *name)
{
    Dmod_Printf("Usage: %s [-d DEVICE] COMMAND [ARGS]\n", name);
    Dmod_Printf("  info                     print what the device reports (and the FT5336 chip)\n");
    Dmod_Printf("  read                     print the current input state\n");
    Dmod_Printf("  watch [EVENTS] [IDLE_MS] print touches as they happen (default %u events,\n", DEFAULT_EVENTS);
    Dmod_Printf("                           stops after IDLE_MS without a touch, default %u)\n", DEFAULT_IDLE_MS);
    Dmod_Printf("DEVICE defaults to %s\n", DEFAULT_DEVICE);
}

static uint32_t parse_count(const char *s, uint32_t default_value)
{
    uint32_t value = 0;
    if (s == NULL)
        return default_value;
    for (; *s >= '0' && *s <= '9'; s++)
        value = value * 10U + (uint32_t)(*s - '0');
    return (value != 0U) ? value : default_value;
}

/* A switch, not a table of name pointers: the dmod loader does not relocate
 * pointers stored in initialized data. */
static const char *event_name(uint8_t event)
{
    switch (event)
    {
        case DMDRVI_INPUT_CONTACT_DOWN: return "down";
        case DMDRVI_INPUT_CONTACT_MOVE: return "move";
        case DMDRVI_INPUT_CONTACT_UP:   return "up";
        default:                        return "?";
    }
}

static const char *type_name(dmdrvi_input_type_t type)
{
    switch (type)
    {
        case DMDRVI_INPUT_TYPE_TOUCHSCREEN: return "touchscreen";
        case DMDRVI_INPUT_TYPE_MOUSE:       return "mouse";
        case DMDRVI_INPUT_TYPE_BUTTONS:     return "buttons";
        default:                            return "unknown";
    }
}

static void print_state(const dmdrvi_input_state_t *state)
{
    if (state->buttons != 0U || state->dx != 0 || state->dy != 0 || state->wheel != 0)
        Dmod_Printf("pointer: dx=%d dy=%d wheel=%d buttons=0x%X\n",
                    state->dx, state->dy, state->wheel, (unsigned)state->buttons);
    if (state->contact_count == 0)
    {
        Dmod_Printf("touch: released\n");
        return;
    }
    for (uint8_t i = 0; i < state->contact_count && i < DMDRVI_INPUT_MAX_CONTACTS; i++)
    {
        const dmdrvi_input_contact_t *c = &state->contacts[i];
        Dmod_Printf("touch: %u/%u id=%u x=%u y=%u %s\n", i + 1U, state->contact_count, c->id, c->x, c->y, event_name(c->event));
    }
}

static int read_state(void *fp, dmdrvi_input_state_t *state)
{
    size_t n = Dmod_FileRead(state, 1, sizeof(*state), fp);
    if (n != sizeof(*state))
    {
        DMOD_LOG_ERROR("touchtest: reading the input state failed\n");
        return -1;
    }
    return 0;
}

/* FT5336 extras - only for a node that reported itself as one: another
 * driver numbers its own commands from the same base. */
static void print_chip_info(void *fp)
{
    dmft5336_chip_info_t chip;
    if (Dmod_Ioctl(fp, dmft5336_ioctl_cmd_get_chip_info, &chip) != 0)
        return;

    Dmod_Printf("chip id:      0x%02X%s\n", chip.chip_id,
                (chip.chip_id == DMFT5336_CHIP_ID) ? " (FT5336)" : (chip.chip_id == 0 ? " (not reached)" : ""));
    Dmod_Printf("firmware id:  0x%02X\n", chip.firmware_id);
    Dmod_Printf("transform:    swap_xy=%u invert_x=%u invert_y=%u\n",
                chip.transform.swap_xy, chip.transform.invert_x, chip.transform.invert_y);
}

static int cmd_info(void *fp)
{
    dmdrvi_input_info_t info;
    if (Dmod_Ioctl(fp, DMDRVI_IOCTL_INPUT_GET_INFO, &info) != 0)
    {
        DMOD_LOG_ERROR("touchtest: not an input device\n");
        return -1;
    }
    info.name[DMDRVI_INPUT_NAME_MAX - 1U] = '\0';

    Dmod_Printf("device:       %s (%s)\n", info.name, type_name(info.type));
    Dmod_Printf("capabilities: 0x%02X\n", (unsigned)info.capabilities);
    Dmod_Printf("max points:   %u\n", info.max_contacts);
    Dmod_Printf("buttons:      %u\n", info.button_count);
    Dmod_Printf("events:       %s\n", (info.capabilities & DMDRVI_INPUT_CAP_INTERRUPT) ? "interrupt" : "polling");
    Dmod_Printf("screen:       %ux%u\n", info.width, info.height);
    if (strcmp(info.name, DMFT5336_DEVICE_NAME) == 0)
        print_chip_info(fp);
    return 0;
}

static int cmd_read(void *fp)
{
    dmdrvi_input_state_t state;
    if (read_state(fp, &state) != 0)
        return -1;
    print_state(&state);
    return 0;
}

/* Waits for input events and prints every state change. */
static int cmd_watch(void *fp, uint32_t events, uint32_t idle_ms)
{
    dmdrvi_input_state_t last;
    memset(&last, 0, sizeof(last));

    Dmod_Printf("touchtest: watching for %u events (idle timeout %u ms)\n", events, idle_ms);
    for (uint32_t seen = 0; seen < events;)
    {
        int ret = Dmod_Ioctl(fp, DMDRVI_IOCTL_INPUT_WAIT_EVENT, &idle_ms);
        if (ret != 0)
        {
            Dmod_Printf("touchtest: no touch for %u ms, stopping (%d)\n", idle_ms, ret);
            return 0;
        }

        dmdrvi_input_state_t state;
        if (read_state(fp, &state) != 0)
            return -1;
        if (dmdrvi_input_state_equal(&state, &last))
            continue;
        print_state(&state);
        last = state;
        seen++;
    }
    return 0;
}

static int run_command(void *fp, int argc, char *argv[])
{
    const char *cmd = (argc > 0) ? argv[0] : "read";

    if (strcmp(cmd, "info") == 0)  return cmd_info(fp);
    if (strcmp(cmd, "read") == 0)  return cmd_read(fp);
    if (strcmp(cmd, "watch") == 0)
        return cmd_watch(fp, parse_count((argc > 1) ? argv[1] : NULL, DEFAULT_EVENTS),
                         parse_count((argc > 2) ? argv[2] : NULL, DEFAULT_IDLE_MS));

    DMOD_LOG_ERROR("touchtest: unknown command '%s'\n", cmd);
    return -1;
}

int main(int argc, char *argv[])
{
    const char *device = DEFAULT_DEVICE;
    int first = 1;

    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        print_usage(argv[0]);
        return 0;
    }
    if (argc > 2 && strcmp(argv[1], "-d") == 0)
    {
        device = argv[2];
        first = 3;
    }

    void *fp = Dmod_FileOpen(device, "r");
    if (fp == NULL)
    {
        DMOD_LOG_ERROR("touchtest: failed to open '%s'\n", device);
        return 1;
    }

    int ret = run_command(fp, argc - first, &argv[first]);
    Dmod_FileClose(fp);
    return (ret == 0) ? 0 : 1;
}
