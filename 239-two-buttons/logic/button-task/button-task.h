#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "platform/platform.h"

typedef void (*button_task_callback_t)(void);

// Заводит кнопку: on_press срабатывает на нажатие (у кнопки без долгого
// действия — сразу после подавления дребезга, у кнопки с ним — при
// отпускании), on_long_press — один раз при удержании дольше секунды.
// NULL вместо on_long_press означает, что у кнопки нет долгого действия.
void button_task_init(platform_button_t button,
                      button_task_callback_t on_press,
                      button_task_callback_t on_long_press);
void button_task_handle(void);
bool button_task_is_pressed(platform_button_t button);
uint32_t button_task_get_press_count(platform_button_t button);
