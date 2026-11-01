#pragma once

#include <stdbool.h>
#include <stdint.h>

// Кнопки платы. Сколько их и каким выводом каждая сидит, знает platform.
typedef enum
{
    PLATFORM_BUTTON_1,
    PLATFORM_BUTTON_COUNT
} platform_button_t;

void platform_init(void);
void platform_led_set(bool on);
bool platform_button_read(platform_button_t button);float platform_get_voltage_v(void);
float platform_get_temp_c(void);
void platform_charge_set(bool on);

// Мигание светодиода по будильнику SDK. period_ms от 10 до 60000.
// Возвращает false, если мигание уже идёт или период неверен.
bool platform_timer_blink_start(uint32_t period_ms);
void platform_timer_blink_stop(void);
uint32_t platform_timer_blink_toggles(void);
