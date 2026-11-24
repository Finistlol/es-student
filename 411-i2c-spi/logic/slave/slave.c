#include "logic/slave/slave.h"
#include "platform/platform.h"

enum { BLINK_PERIOD_MS = 500 };

static uint32_t last_change;
static bool led_on;

void slave_device_init(void)
{
    platform_slave_init();
    last_change = platform_time_ms();
}

void slave_device_handle(void)
{
    if (platform_time_ms() - last_change >= BLINK_PERIOD_MS)
    {
        last_change += BLINK_PERIOD_MS;
        led_on = !led_on;
        platform_led_set(led_on);
    }
}
