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
bool platform_button_read(platform_button_t button);