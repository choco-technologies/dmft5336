#define DMOD_ENABLE_REGISTRATION ON
#define ENABLE_DIF_REGISTRATIONS ON
#include "dmod_test.h"
#include "dmft5336.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <errno.h>
#include <string.h>

/* There is no I2C bus on the host: the bus path below never exists, so the
 * driver can be created but every access to the chip reports -ENODEV. */
#define TEST_INI \
    "[touch]\n" \
    "driver_name=dmft5336\n" \
    "i2c_bus=/dev/dmi2cx/no_such_bus\n" \
    "address=56\n" \
    "width=480\nheight=272\nswap_xy=on\n"

typedef struct
{
    dmod_dmdrvi_create_t    create;
    dmod_dmdrvi_free_t      free;
    dmod_dmdrvi_open_t      open;
    dmod_dmdrvi_close_t     close;
    dmod_dmdrvi_read_t      read;
    dmod_dmdrvi_write_t     write;
    dmod_dmdrvi_ioctl_t     ioctl;
} driver_t;

typedef struct
{
    driver_t            drv;
    dmini_context_t     ini;
    dmdrvi_context_t    ctx;
    dmdrvi_dev_num_t    dev_num;
    void*               handle;
} device_t;

static dmft5336_transform_t g_transform;

void dmod_test_setup(void)
{
    memset(&g_transform, 0, sizeof(g_transform));
}

void dmod_test_teardown(void)
{
}

/* ---- Point decoding ---- */

/* XH: event in bits 7:6, X[11:8] in 3:0; YH: ID in 7:4, Y[11:8] in 3:0 */
static void make_raw(uint8_t raw[6], uint8_t event, uint16_t x, uint16_t y, uint8_t id)
{
    raw[0] = (uint8_t)((event << 6) | ((x >> 8) & 0x0F));
    raw[1] = (uint8_t)x;
    raw[2] = (uint8_t)((id << 4) | ((y >> 8) & 0x0F));
    raw[3] = (uint8_t)y;
    raw[4] = 0x20;
    raw[5] = 0x30;
}

DMOD_TEST_STEP(dmft5336_decode_point_plain)
{
    uint8_t raw[6];
    dmft5336_point_t p;
    make_raw(raw, dmft5336_event_contact, 0x1A5, 0x0C3, 2);

    DMOD_TEST_EXPECT_TRUE(dmft5336_decode_point(raw, &g_transform, &p));
    DMOD_TEST_EXPECT_EQ(p.x, 0x1A5);
    DMOD_TEST_EXPECT_EQ(p.y, 0x0C3);
    DMOD_TEST_EXPECT_EQ(p.id, 2);
    DMOD_TEST_EXPECT_EQ(p.event, dmft5336_event_contact);
    DMOD_TEST_EXPECT_EQ(p.weight, 0x20);
    DMOD_TEST_EXPECT_EQ(p.area, 0x3);
}

DMOD_TEST_STEP(dmft5336_decode_point_transforms)
{
    uint8_t raw[6];
    dmft5336_point_t p;
    /* Panel X=100, Y=300 on a panel mounted with swapped axes */
    make_raw(raw, dmft5336_event_down, 100, 300, 0);

    g_transform.swap_xy = true;
    DMOD_TEST_EXPECT_TRUE(dmft5336_decode_point(raw, &g_transform, &p));
    DMOD_TEST_EXPECT_EQ(p.x, 300);
    DMOD_TEST_EXPECT_EQ(p.y, 100);

    g_transform.width = 480;
    g_transform.height = 272;
    g_transform.invert_x = true;
    g_transform.invert_y = true;
    DMOD_TEST_EXPECT_TRUE(dmft5336_decode_point(raw, &g_transform, &p));
    DMOD_TEST_EXPECT_EQ(p.x, 479 - 300);
    DMOD_TEST_EXPECT_EQ(p.y, 271 - 100);
}

DMOD_TEST_STEP(dmft5336_decode_point_clips_and_rejects)
{
    uint8_t raw[6];
    dmft5336_point_t p;

    make_raw(raw, dmft5336_event_contact, 4000, 1000, 0);
    g_transform.width = 480;
    g_transform.height = 272;
    DMOD_TEST_EXPECT_TRUE(dmft5336_decode_point(raw, &g_transform, &p));
    DMOD_TEST_EXPECT_EQ(p.x, 479);
    DMOD_TEST_EXPECT_EQ(p.y, 271);

    make_raw(raw, dmft5336_event_none, 10, 10, 0);
    DMOD_TEST_EXPECT_FALSE(dmft5336_decode_point(raw, &g_transform, &p));
    DMOD_TEST_EXPECT_FALSE(dmft5336_decode_point(NULL, &g_transform, &p));
}

/* ---- Through the dmdrvi DIF, the way dmdevfs calls the driver ---- */

static bool get_driver(driver_t* drv)
{
    Dmod_Context_t* module = Dmod_GetModuleContext("dmft5336");
    if (module == NULL)
    {
        return false;
    }
    drv->create = Dmod_GetDifFunction(module, dmod_dmdrvi_create_sig);
    drv->free   = Dmod_GetDifFunction(module, dmod_dmdrvi_free_sig);
    drv->open   = Dmod_GetDifFunction(module, dmod_dmdrvi_open_sig);
    drv->close  = Dmod_GetDifFunction(module, dmod_dmdrvi_close_sig);
    drv->read   = Dmod_GetDifFunction(module, dmod_dmdrvi_read_sig);
    drv->write  = Dmod_GetDifFunction(module, dmod_dmdrvi_write_sig);
    drv->ioctl  = Dmod_GetDifFunction(module, dmod_dmdrvi_ioctl_sig);
    return drv->create != NULL && drv->free != NULL && drv->open != NULL && drv->close != NULL &&
           drv->read != NULL && drv->write != NULL && drv->ioctl != NULL;
}

static bool device_open(device_t* dev, const char* ini_text)
{
    memset(dev, 0, sizeof(*dev));
    if (!get_driver(&dev->drv))
    {
        return false;
    }
    dev->ini = dmini_create();
    dmini_parse_string(dev->ini, ini_text);
    dmini_set_active_section(dev->ini, "touch", 0);

    dev->ctx = dev->drv.create(dev->ini, &dev->dev_num);
    dev->handle = (dev->ctx != NULL) ? dev->drv.open(dev->ctx, DMDRVI_O_RDONLY, &dev->dev_num) : NULL;
    return dev->handle != NULL;
}

static void device_close(device_t* dev)
{
    if (dev->handle != NULL)
    {
        dev->drv.close(dev->ctx, dev->handle);
    }
    if (dev->ctx != NULL)
    {
        dev->drv.free(dev->ctx);
    }
    if (dev->ini != NULL)
    {
        dmini_destroy(dev->ini);
    }
}

DMOD_TEST_STEP(dmft5336_create_names_node_after_section)
{
    device_t dev;
    DMOD_TEST_EXPECT_TRUE(device_open(&dev, TEST_INI));
    DMOD_TEST_EXPECT_EQ(dev.dev_num.flags, DMDRVI_NUM_ALT_NAME);
    DMOD_TEST_EXPECT_EQ(strcmp(dev.dev_num.alt_name, "touch"), 0);
    device_close(&dev);
}

DMOD_TEST_STEP(dmft5336_create_rejects_invalid_config)
{
    device_t dev;
    DMOD_TEST_EXPECT_FALSE(device_open(&dev, "[touch]\ndriver_name=dmft5336\n"));
    DMOD_TEST_EXPECT_NULL(dev.ctx);
    device_close(&dev);

    DMOD_TEST_EXPECT_FALSE(device_open(&dev, "[touch]\ni2c_bus=/dev/x\naddress=200\n"));
    DMOD_TEST_EXPECT_NULL(dev.ctx);
    device_close(&dev);

    DMOD_TEST_EXPECT_FALSE(device_open(&dev, "[touch]\ni2c_bus=/dev/x\npoll_interval_ms=0\n"));
    DMOD_TEST_EXPECT_NULL(dev.ctx);
    device_close(&dev);
}

DMOD_TEST_STEP(dmft5336_reports_missing_bus)
{
    device_t dev;
    DMOD_TEST_EXPECT_TRUE(device_open(&dev, TEST_INI));
    if (dev.handle != NULL)
    {
        dmft5336_state_t state;
        dmft5336_info_t info;
        uint32_t timeout = 10;

        DMOD_TEST_EXPECT_EQ(dev.drv.read(dev.ctx, dev.handle, &state, sizeof(state), 0), -ENODEV);
        DMOD_TEST_EXPECT_EQ(dev.drv.read(dev.ctx, dev.handle, &state, 4, 0), -EINVAL);
        DMOD_TEST_EXPECT_EQ(dev.drv.ioctl(dev.ctx, dev.handle, dmft5336_ioctl_cmd_wait_event, &timeout), -ENODEV);

        DMOD_TEST_EXPECT_EQ(dev.drv.ioctl(dev.ctx, dev.handle, dmft5336_ioctl_cmd_get_info, &info), 0);
        DMOD_TEST_EXPECT_EQ(info.chip_id, 0);
        DMOD_TEST_EXPECT_EQ(info.max_points, DMFT5336_MAX_POINTS);
        DMOD_TEST_EXPECT_FALSE(info.interrupt_driven);
        DMOD_TEST_EXPECT_TRUE(info.transform.swap_xy);
        DMOD_TEST_EXPECT_EQ(info.transform.width, 480);
    }
    device_close(&dev);
}

DMOD_TEST_STEP(dmft5336_ioctl_answers_only_its_own_command_range)
{
    device_t dev;
    DMOD_TEST_EXPECT_TRUE(device_open(&dev, TEST_INI));
    if (dev.handle != NULL)
    {
        uint32_t probe[16] = { 0 };

        DMOD_TEST_EXPECT_EQ(dev.drv.ioctl(dev.ctx, dev.handle, DMDRVI_IOCTL_BLOCK_GET_INFO, probe), -ENOTTY);
        DMOD_TEST_EXPECT_EQ(dev.drv.ioctl(dev.ctx, dev.handle, DMDRVI_IOCTL_MONITOR_GET_POLICY, probe), -ENOTTY);
        DMOD_TEST_EXPECT_EQ(dev.drv.ioctl(dev.ctx, dev.handle, 0, probe), -ENOTTY);
        DMOD_TEST_EXPECT_EQ(dev.drv.ioctl(dev.ctx, dev.handle, dmft5336_ioctl_cmd_max, probe), -ENOTTY);

        DMOD_TEST_EXPECT_EQ((int)dmft5336_ioctl_cmd_get_info, DMDRVI_IOCTL_CUSTOM_BASE);
        DMOD_TEST_EXPECT_EQ(dev.drv.ioctl(dev.ctx, dev.handle, dmft5336_ioctl_cmd_get_info, NULL), -EINVAL);
        DMOD_TEST_EXPECT_EQ(dev.drv.write(dev.ctx, dev.handle, probe, 4, 0), -ENOTSUP);
    }
    device_close(&dev);
}
