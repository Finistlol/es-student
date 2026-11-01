#include "platform/platform.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"
#include "pico/time.h"

static const uint32_t LED_PIN = 25;
static const uint32_t BUTTON_PINS[PLATFORM_BUTTON_COUNT] = {15};
static const uint32_t VOLTAGE_PIN = 26;
static const uint32_t CHARGE_PIN = 22;

// Канал 4 — встроенный датчик температуры кристалла.
#define ADC_CHANNEL_VOLTAGE 0
#define ADC_CHANNEL_TEMP 4

// АЦП 12-разрядный: 4096 уровней от нуля до опорного напряжения.
#define ADC_LEVELS 4096.0f
#define ADC_REF_V 3.3f

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

    adc_init();
    // Отдаёт вывод аналоговому входу: цифровая часть и подтяжки отключаются,
    // иначе они искажают измеряемое напряжение.
    adc_gpio_init(VOLTAGE_PIN);
    adc_set_temp_sensor_enabled(true);

    gpio_init(CHARGE_PIN);
    gpio_set_dir(CHARGE_PIN, GPIO_OUT);
    gpio_put(CHARGE_PIN, false);
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

float platform_get_voltage_v(void)
{
    adc_select_input(ADC_CHANNEL_VOLTAGE);
    uint16_t code = adc_read();
    return (float)code * ADC_REF_V / ADC_LEVELS;
}

float platform_get_temp_c(void)
{
    // Датчик температуры подключён к каналу 4, RP2040 Datasheet, 4.9.2.1.
    adc_select_input(ADC_CHANNEL_TEMP);
    uint16_t code = adc_read();
    float sensor_v = (float)code * ADC_REF_V / ADC_LEVELS;
    // T = 27 - (U - 0.706) / 0.001721, RP2040 Datasheet, раздел 4.9.5.
    return 27.0f - (sensor_v - 0.706f) / 0.001721f;
}

void platform_charge_set(bool on)
{
    gpio_put(CHARGE_PIN, on);
}

// --- Мигание по будильнику ---

// Обработчик будильника: короткий, без печати и ожиданий.
// Счётчик читает команда из суперцикла, поэтому volatile.
static volatile uint32_t blink_toggles = 0;
static repeating_timer_t blink_timer;
static bool blink_running = false;

static bool blink_handler(repeating_timer_t *timer)
{
    (void)timer;
    static bool level = false;
    level = !level;
    gpio_put(LED_PIN, level);
    blink_toggles = blink_toggles + 1;
    return true;
}

bool platform_timer_blink_start(uint32_t period_ms)
{
    if (blink_running)
    {
        return false;
    }

    blink_toggles = 0;
    if (!add_repeating_timer_ms(period_ms, blink_handler, NULL, &blink_timer))
    {
        return false;
    }

    blink_running = true;
    return true;
}

void platform_timer_blink_stop(void)
{
    cancel_repeating_timer(&blink_timer);
    blink_running = false;
}

uint32_t platform_timer_blink_toggles(void)
{
    return blink_toggles;
}
