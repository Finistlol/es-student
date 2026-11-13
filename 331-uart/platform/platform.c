#include "platform/platform.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"

#define LED_PIN 25
#define BUTTON_PIN 15
#define LINE_PIN 22
#define NODE_ADC_PIN 26
#define NODE_ADC_CHANNEL 0

// АЦП 12-разрядный: 4096 уровней от нуля до опорного напряжения.
#define ADC_LEVELS 4096.0f
#define ADC_REF_V 3.3f

static platform_drive_mode_t line_mode = PLATFORM_DRIVE_PP;

void platform_init(void)
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // Линия GP22: драйвер RC-цепи.
    gpio_init(LINE_PIN);
    gpio_set_dir(LINE_PIN, GPIO_OUT);
    gpio_put(LINE_PIN, false);

    // Узел RC-цепи: вход АЦП на GP26.
    adc_init();
    adc_gpio_init(NODE_ADC_PIN);
    adc_select_input(NODE_ADC_CHANNEL);
}

void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(platform_button_t button)
{
    (void)button;
    return !gpio_get(BUTTON_PIN);
}

void platform_line_mode(platform_drive_mode_t mode)
{
    line_mode = mode;

    if (mode == PLATFORM_DRIVE_OD)
    {
        // Открытый сток из двухтактного драйвера: в регистре выхода ноль,
        // подтяжка вверх включена, направление переключается.
        gpio_put(LINE_PIN, false);
        gpio_pull_up(LINE_PIN);
        gpio_set_dir(LINE_PIN, GPIO_IN); // отпущено = подтяжка тянет вверх
    }
    else
    {
        // Двухтактный: обычный выход.
        gpio_disable_pulls(LINE_PIN);
        gpio_set_dir(LINE_PIN, GPIO_OUT);
        gpio_put(LINE_PIN, false);
    }
}

void platform_line_set(bool high)
{
    if (line_mode == PLATFORM_DRIVE_PP)
    {
        gpio_put(LINE_PIN, high);
    }
    else
    {
        // Открытый сток: high = отпустить (вход, подтяжка тянет вверх),
        // low = притянуть к земле (выход с нулём в регистре).
        if (high)
        {
            gpio_set_dir(LINE_PIN, GPIO_IN);
        }
        else
        {
            gpio_set_dir(LINE_PIN, GPIO_OUT); // в регистре уже ноль
        }
    }
}

float platform_node_voltage_v(void)
{
    adc_select_input(NODE_ADC_CHANNEL);
    uint16_t code = adc_read();
    return (float)code * ADC_REF_V / ADC_LEVELS;
}