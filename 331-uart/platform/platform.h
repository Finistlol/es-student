#pragma once

#include <stdbool.h>
#include <stdint.h>

// Кнопки платы. Сколько их и каким выводом каждая сидит, знает platform.
typedef enum
{
    PLATFORM_BUTTON_1,
    PLATFORM_BUTTON_COUNT
} platform_button_t;

// Режим драйвера линии GP22.
typedef enum
{
    PLATFORM_DRIVE_PP, // двухтактный: вывод как выход, 0 или 3,3 В
    PLATFORM_DRIVE_OD  // открытый сток: вывод как вход с подтяжкой
} platform_drive_mode_t;

void platform_init(void);
void platform_led_set(bool on);
bool platform_button_read(platform_button_t button);

// Настройка драйвера линии GP22.
void platform_line_mode(platform_drive_mode_t mode);
// В PP: high = 3,3 В, low = 0 В.
// В OD: high = отпустить линию (вход), low = притянуть к земле (выход с нулём).
void platform_line_set(bool high);
// Напряжение узла RC-цепи через АЦП на GP26.
float platform_node_voltage_v(void);
// --- UART0: петля и обмен ---

// Внутренняя петля (LBE): true = бит LBE стоит, выводы не отданы модулю.
// Через перемычку: false = LBE снят, GP0 и GP1 отданы модулю UART.
void platform_uart_loop_internal(bool enable);
bool platform_uart_writable(void);
bool platform_uart_putc(char c);
int platform_uart_getc(void);
uint32_t platform_uart_errors(void);
