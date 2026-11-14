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
// Дополнения к platform.h для занятий 3.3.3 и 3.3.4.
// К existing declarations добавляем UART1 и блочный приём.

// --- UART1: второй модуль на GP8 (TX) и GP9 (RX) ---
void platform_uart1_setup(void);
uint32_t platform_uart1_baud_shift(int32_t percent);
void platform_uart0_parity(const char *mode);
void platform_uart1_parity(const char *mode);
bool platform_uart1_writable(void);
bool platform_uart1_putc(char c);
int platform_uart1_getc(void);
uint32_t platform_uart1_errors(void);
void platform_uart1_flush_rx(void);

// --- Блочный приём на UART0 с прерыванием (3.3.4) ---
#define PLATFORM_BLOCK_SIZE 64
void platform_uart0_irq_enable(bool enable);
extern volatile uint8_t platform_block_count;
const uint8_t *platform_block_data(void);
void platform_uart0_drain_rx(void);
// --- 3.3.5: DMA-захват линии UART на GP26 (500 000 отсчётов/с) ---

#define SCOPE_BLOCK_SIZE 4096

bool platform_scope_capture(uint32_t samples);
bool platform_scope_ready(void);
void platform_scope_result(uint32_t *samples, const uint16_t **codes);
void platform_scope_set_uart_baud(uint32_t baud);
