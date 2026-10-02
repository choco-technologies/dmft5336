#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmft5336.h"

static dmft5336_t g_handle = NULL;

void dmod_test_setup(void)
{
    g_handle = dmft5336_create();
}

void dmod_test_teardown(void)
{
    dmft5336_destroy(g_handle);
    g_handle = NULL;
}

DMOD_TEST_STEP(dmft5336_create)
{
    DMOD_TEST_EXPECT_NOT_NULL(g_handle);
}

DMOD_TEST_STEP(dmft5336_is_valid)
{
    DMOD_TEST_EXPECT_TRUE(dmft5336_is_valid(g_handle));
}

DMOD_TEST_STEP(dmft5336_destroy_null)
{
    /* Destroying NULL must not crash. */
    dmft5336_destroy(NULL);
}
