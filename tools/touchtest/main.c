#include "dmod.h"
#include "dmft5336.h"
#include <string.h>

/**
 * @brief Manual dmft5336 device test tool.
 *
 * Works on an already-configured touch panel node - it does not configure
 * anything itself, it only uses the device file (read/ioctl).
 */

#define DEFAULT_DEVICE      "/dev/touch"
#define DEFAULT_EVENTS      20U
#define DEFAULT_IDLE_MS     10000U

static void print_usage(const char *name)
{
    Dmod_Printf("Usage: %s [-d DEVICE] COMMAND [ARGS]\n", name);
    Dmod_Printf("  info                     print the chip and driver configuration\n");
    Dmod_Printf("  read                     print the current touch state\n");
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
        case dmft5336_event_down:    return "down";
        case dmft5336_event_up:      return "up";
        case dmft5336_event_contact: return "contact";
        default:                     return "none";
    }
}

static void print_state(const dmft5336_state_t *state)
{
    if (state->count == 0)
    {
        Dmod_Printf("touch: released\n");
        return;
    }
    for (uint8_t i = 0; i < state->count && i < DMFT5336_MAX_POINTS; i++)
    {
        const dmft5336_point_t *p = &state->points[i];
        Dmod_Printf("touch: %u/%u id=%u x=%u y=%u %s\n", i + 1U, state->count, p->id, p->x, p->y, event_name(p->event));
    }
}

static int read_state(void *fp, dmft5336_state_t *state)
{
    size_t n = Dmod_FileRead(state, 1, sizeof(*state), fp);
    if (n != sizeof(*state))
    {
        DMOD_LOG_ERROR("touchtest: reading the touch state failed\n");
        return -1;
    }
    return 0;
}

static int cmd_info(void *fp)
{
    dmft5336_info_t info;
    if (Dmod_Ioctl(fp, dmft5336_ioctl_cmd_get_info, &info) != 0)
        return -1;

    Dmod_Printf("chip id:      0x%02X%s\n", info.chip_id,
                (info.chip_id == DMFT5336_CHIP_ID) ? " (FT5336)" : (info.chip_id == 0 ? " (not reached)" : ""));
    Dmod_Printf("firmware id:  0x%02X\n", info.firmware_id);
    Dmod_Printf("max points:   %u\n", info.max_points);
    Dmod_Printf("events:       %s\n", info.interrupt_driven ? "INT pin" : "polling");
    Dmod_Printf("screen:       %ux%u swap_xy=%u invert_x=%u invert_y=%u\n",
                info.transform.width, info.transform.height, info.transform.swap_xy,
                info.transform.invert_x, info.transform.invert_y);
    return 0;
}

static int cmd_read(void *fp)
{
    dmft5336_state_t state;
    if (read_state(fp, &state) != 0)
        return -1;
    print_state(&state);
    return 0;
}

/* Waits for touch reports and prints every state change. */
static int cmd_watch(void *fp, uint32_t events, uint32_t idle_ms)
{
    dmft5336_state_t last;
    memset(&last, 0, sizeof(last));

    Dmod_Printf("touchtest: watching for %u events (idle timeout %u ms)\n", events, idle_ms);
    for (uint32_t seen = 0; seen < events;)
    {
        int ret = Dmod_Ioctl(fp, dmft5336_ioctl_cmd_wait_event, &idle_ms);
        if (ret != 0)
        {
            Dmod_Printf("touchtest: no touch for %u ms, stopping (%d)\n", idle_ms, ret);
            return 0;
        }

        dmft5336_state_t state;
        if (read_state(fp, &state) != 0)
            return -1;
        if (dmft5336_states_equal(&state, &last))
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
