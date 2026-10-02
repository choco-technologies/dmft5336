#define DMOD_ENABLE_REGISTRATION    ON
#include "private.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmhaman.h"
#include <errno.h>
#include <string.h>

/* Valid range of an unshifted 7-bit I2C target address. */
#define DMFT5336_ADDRESS_MIN        0x08
#define DMFT5336_ADDRESS_MAX        0x77
#define DMFT5336_DEFAULT_ADDRESS    0x38

#define DMFT5336_DEFAULT_POLL_MS    20
#define DMFT5336_MAX_POLL_MS        1000

static bool is_valid_context(dmdrvi_context_t context)
{
    return (context != NULL && context->magic == DMFT5336_CONTEXT_MAGIC);
}

/* ---- INT pin ----
 *
 * The chip's INT output is a dmgpio input configured with
 * interrupt_handler=<name>; dmgpio calls that dmhaman handler from its ISR.
 * Registering under the same name here only wakes up wait_event. */

static int interrupt_handler(void *parameters, void *user_ctx)
{
    (void)parameters;
    dmdrvi_context_t context = (dmdrvi_context_t)user_ctx;

    if (is_valid_context(context))
        dmosi_semaphore_post(context->event_sem, 1);
    return 0;
}

/* ---- Configuration ---- */

static bool string_to_switch(const char *s)
{
    return s != NULL && strcmp(s, "on") == 0;
}

static char *dup_optional(dmini_context_t ini, const char *key)
{
    const char *value = dmini_get_string(ini, NULL, key, NULL);
    return (value != NULL && value[0] != '\0') ? Dmod_StrDup(value) : NULL;
}

static int check_config(dmdrvi_context_t context, int address, int poll_ms)
{
    if (context->bus_path == NULL)
    {
        DMOD_LOG_ERROR("i2c_bus not set in configuration (path of the dmi2c node)\n");
        return -EINVAL;
    }
    if (address < DMFT5336_ADDRESS_MIN || address > DMFT5336_ADDRESS_MAX)
    {
        DMOD_LOG_ERROR("Invalid address %d (expected %d..%d)\n", address, DMFT5336_ADDRESS_MIN, DMFT5336_ADDRESS_MAX);
        return -EINVAL;
    }
    if (poll_ms < 1 || poll_ms > DMFT5336_MAX_POLL_MS)
    {
        DMOD_LOG_ERROR("Invalid poll_interval_ms %d (expected 1..%d)\n", poll_ms, DMFT5336_MAX_POLL_MS);
        return -EINVAL;
    }
    return 0;
}

/**
 * @brief Parse this driver's .ini configuration.
 *
 * Passes NULL as the section to every dmini call: dmdevfs locks the ini
 * context to this driver's own section before calling dmdrvi_create().
 * Integers are decimal (dmini_get_int).
 */
static int read_config(dmdrvi_context_t context, dmini_context_t ini)
{
    dmft5336_transform_t *t = &context->transform;
    int address = dmini_get_int(ini, NULL, "address", DMFT5336_DEFAULT_ADDRESS);
    int poll_ms = dmini_get_int(ini, NULL, "poll_interval_ms", DMFT5336_DEFAULT_POLL_MS);
    int width   = dmini_get_int(ini, NULL, "width", 0);
    int height  = dmini_get_int(ini, NULL, "height", 0);

    context->bus_path          = dup_optional(ini, "i2c_bus");
    context->interrupt_handler = dup_optional(ini, "interrupt_handler");
    t->swap_xy  = string_to_switch(dmini_get_string(ini, NULL, "swap_xy", NULL));
    t->invert_x = string_to_switch(dmini_get_string(ini, NULL, "invert_x", NULL));
    t->invert_y = string_to_switch(dmini_get_string(ini, NULL, "invert_y", NULL));

    if (width < 0 || width > UINT16_MAX || height < 0 || height > UINT16_MAX)
    {
        DMOD_LOG_ERROR("Invalid width/height %dx%d\n", width, height);
        return -EINVAL;
    }
    t->width  = (uint16_t)width;
    t->height = (uint16_t)height;

    int ret = check_config(context, address, poll_ms);
    context->address          = (uint16_t)address;
    context->poll_interval_ms = (uint32_t)poll_ms;
    return ret;
}

/* ---- Context lifetime ---- */

static void destroy_context(dmdrvi_context_t context)
{
    if (context->interrupt_handler != NULL)
        dmhaman_unregister_handler(context->interrupt_handler, interrupt_handler);
    chip_disconnect(context);
    if (context->event_sem != NULL)
        dmosi_semaphore_destroy(context->event_sem);
    if (context->lock != NULL)
        dmosi_mutex_destroy(context->lock);
    Dmod_Free(context->bus_path);
    Dmod_Free(context->interrupt_handler);
    context->magic = 0;
    Dmod_Free(context);
}

static int start(dmdrvi_context_t context)
{
    context->lock      = dmosi_mutex_create(false);
    context->event_sem = dmosi_semaphore_create(0, 1);
    if (context->lock == NULL || context->event_sem == NULL)
        return -ENOMEM;

    if (context->interrupt_handler != NULL &&
        dmhaman_register_handler(context->interrupt_handler, interrupt_handler, context) != 0)
    {
        DMOD_LOG_ERROR("Cannot register interrupt handler '%s'\n", context->interrupt_handler);
        return -EINVAL;
    }
    return 0;
}

/* The section name becomes the node name (e.g. [touch] -> /dev/touch);
 * without a usable one the node is /dev/dmft5336<n>. */
static void fill_dev_num(dmini_context_t ini, dmdrvi_dev_num_t *dev_num)
{
    const char *section = dmini_section_name(ini, 0);

    memset(dev_num, 0, sizeof(*dev_num));
    if (section != NULL && section[0] != '\0' && strlen(section) <= DMDRVI_ALT_NAME_MAX_LEN)
    {
        dev_num->flags = DMDRVI_NUM_ALT_NAME;
        memcpy(dev_num->alt_name, section, strlen(section) + 1);
        return;
    }
    dev_num->flags = DMDRVI_NUM_MAJOR;
    dev_num->major = 0;
}

/* ---- Touch state ---- */

static int read_state(dmdrvi_context_t context, dmft5336_state_t *state)
{
    dmosi_mutex_lock(context->lock);
    int ret = chip_connect(context);
    if (ret == 0)
        ret = chip_read_state(context, state);
    if (ret == 0)
        context->last_state = *state;
    dmosi_mutex_unlock(context->lock);
    return ret;
}

static bool timed_out(uint32_t start_ms, int32_t timeout_ms)
{
    return timeout_ms >= 0 && (uint32_t)(dmosi_get_tick_count() - start_ms) >= (uint32_t)timeout_ms;
}

/* Without an INT pin: poll until the state differs from the one handed out
 * last (by read(), get_state or a previous wait). */
static int poll_for_change(dmdrvi_context_t context, int32_t timeout_ms)
{
    uint32_t start_ms = dmosi_get_tick_count();
    dmft5336_state_t seen = context->last_state;

    for (;;)
    {
        dmft5336_state_t now;
        dmosi_mutex_lock(context->lock);
        int ret = chip_connect(context);
        if (ret == 0)
            ret = chip_read_state(context, &now);
        dmosi_mutex_unlock(context->lock);

        if (ret != 0)
            return ret;
        if (!dmft5336_states_equal(&now, &seen))
            return 0;
        if (timed_out(start_ms, timeout_ms))
            return -ETIMEDOUT;
        dmosi_thread_sleep(context->poll_interval_ms);
    }
}

/* With an INT pin the semaphore counts at most one pending report, so an
 * event that came between two waits is not lost. */
static int wait_event(dmdrvi_context_t context, const uint32_t *timeout)
{
    int32_t timeout_ms = (timeout != NULL) ? (int32_t)*timeout : -1;

    if (context->interrupt_handler == NULL)
        return poll_for_change(context, timeout_ms);
    return (dmosi_semaphore_wait(context->event_sem, 1, timeout_ms) == 0) ? 0 : -ETIMEDOUT;
}

static void get_info(dmdrvi_context_t context, dmft5336_info_t *info)
{
    dmosi_mutex_lock(context->lock);
    (void)chip_connect(context);
    info->chip_id          = context->chip_id;
    info->firmware_id      = context->firmware_id;
    dmosi_mutex_unlock(context->lock);

    info->max_points       = DMFT5336_MAX_POINTS;
    info->interrupt_driven = context->interrupt_handler != NULL;
    info->transform        = context->transform;
}

/* ---- DMOD lifecycle ---- */

int dmod_init(const Dmod_Config_t *Config)
{
    DMOD_LOG_INFO("DMFT5336 touch driver module initialized\n");
    return 0;
}

int dmod_deinit(void)
{
    DMOD_LOG_INFO("DMFT5336 touch driver module deinitialized\n");
    return 0;
}

/* ---- DMDRVI interface ---- */

dmod_dmdrvi_dif_api_declaration(2.0, dmft5336, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    if (config == NULL || dev_num == NULL)
    {
        DMOD_LOG_ERROR("Invalid parameters to dmft5336_dmdrvi_create\n");
        return NULL;
    }

    dmdrvi_context_t context = Dmod_Malloc(sizeof(struct dmdrvi_context));
    if (context == NULL)
        return NULL;
    memset(context, 0, sizeof(*context));
    context->magic = DMFT5336_CONTEXT_MAGIC;

    if (read_config(context, config) != 0 || start(context) != 0)
    {
        DMOD_LOG_ERROR("Failed to create DMDRVI context with provided configuration\n");
        destroy_context(context);
        return NULL;
    }

    /* The bus node can only be opened once dmdevfs is mounted, so the chip
     * is reached on first use (read/ioctl). Not from _path_ready(): that runs
     * on dmdevfs' hotplug thread, whose stack is not sized for a nested file
     * open and I2C transfers (it overflowed on the STM32F746G-DISCO). */
    fill_dev_num(config, dev_num);
    return context;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmft5336, void, _free, ( dmdrvi_context_t context ))
{
    if (is_valid_context(context))
        destroy_context(context);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmft5336, void*, _open, ( dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t* dev_num ))
{
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmft5336_dmdrvi_open\n");
        return NULL;
    }
    return context;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmft5336, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    /* No per-handle state */
}

dmod_dmdrvi_dif_api_declaration(2.0, dmft5336, dmdrvi_ssize_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    if (!is_valid_context(context) || (buffer == NULL && size != 0) || offset < 0)
        return -EINVAL;
    if (size == 0)
        return 0;
    if (size < sizeof(dmft5336_state_t))
        return -EINVAL;

    /* offset is unused: every read returns the current touch state. */
    dmft5336_state_t state;
    int ret = read_state(context, &state);
    if (ret != 0)
        return ret;
    memcpy(buffer, &state, sizeof(state));
    return (dmdrvi_ssize_t)sizeof(state);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmft5336, dmdrvi_ssize_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    if (!is_valid_context(context))
        return -EINVAL;
    return (size == 0) ? 0 : -ENOTSUP;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmft5336, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmft5336_dmdrvi_ioctl\n");
        return -EINVAL;
    }

    switch (command)
    {
        case dmft5336_ioctl_cmd_get_info:
            if (arg == NULL) return -EINVAL;
            get_info(context, (dmft5336_info_t *)arg);
            return 0;
        case dmft5336_ioctl_cmd_get_state:
            if (arg == NULL) return -EINVAL;
            return read_state(context, (dmft5336_state_t *)arg);
        case dmft5336_ioctl_cmd_wait_event:
            return wait_event(context, (const uint32_t *)arg);
        default:
            /* Including the standard block/monitor/network commands dmdevfs
             * probes every node with. */
            return -ENOTTY;
    }
}

dmod_dmdrvi_dif_api_declaration(2.0, dmft5336, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    return is_valid_context(context) ? 0 : -EINVAL;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmft5336, int, _stat, ( dmdrvi_context_t context, const char* path, dmdrvi_stat_t* stat ))
{
    if (!is_valid_context(context) || stat == NULL)
        return -EINVAL;

    stat->size = (dmdrvi_size_t)0;  /* Not a byte store - every read returns the current state */
    stat->mode = 0444;
    return 0;
}
