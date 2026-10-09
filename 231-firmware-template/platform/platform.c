#include "platform/platform.h"

#include "hardware/gpio.h"
#include "pico/stdio.h"

static const uint32_t LED_PIN = 25;
static const uint32_t BUTTON_PINS[PLATFORM_BUTTON_COUNT] = {15};

void platform_init(void)
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    for (uint32_t i = 0; i < PLATFORM_BUTTON_COUNT; i++)
    {
        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[i]);
    }
}

void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(platform_button_t button)
{
    // Кнопка замыкает вывод на землю, вывод подтянут к питанию:
    // нажатой кнопке соответствует низкий уровень.
    return !gpio_get(BUTTON_PINS[button]);
}
