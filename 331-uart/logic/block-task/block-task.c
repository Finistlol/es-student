#include "block-task/block-task.h"

#include <stdbool.h>

#include "platform/platform.h"

static block_task_callback_t report = 0;
static bool started = false;
static char text_buf[5];
static uint8_t text_len = 0;
static uint32_t baud = 0;
static uint8_t text_sent = 0;
static scope_result_t result;

void block_task_init(block_task_callback_t callback)
{
    report = callback;
}

bool block_task_start(uint32_t baud_, const char *text, uint8_t len)
{
    if (started || len == 0 || len > 4)
    {
        return false;
    }

    baud = baud_;
    text_len = len;
    for (uint8_t i = 0; i < len; i++)
    {
        text_buf[i] = text[i];
    }
    text_buf[len] = '\0';

    // Выводы GP0/GP1 должны быть отданы модулю UART0 для передачи.
    platform_uart_loop_internal(false);
    // Захват стартует раньше передачи: АЦП видит линию до старт-бита.
    platform_scope_capture(SCOPE_BLOCK_SIZE);
    platform_scope_set_uart_baud(baud);
    // Смена скорости оставляет в UART1 мусор и признаки — сбрасываем.
    platform_uart1_flush_rx();
    platform_uart1_errors();
    text_sent = 0;
    started = true;
    return true;
}

void block_task_handle(void)
{
    if (!started)
    {
        return;
    }

    // Отправляем текст через UART0 (TX на GP0, подключён к GP26).
    while (text_sent < text_len && platform_uart_writable())
    {
        platform_uart_putc(text_buf[text_sent]);
        text_sent = text_sent + 1;
    }

    // Ждём готовности блока и приёма эха на UART1.
    if (text_sent >= text_len && platform_scope_ready())
    {
        result.samples = SCOPE_BLOCK_SIZE;
        result.rate_hz = 500000;
        result.baud = baud;
        result.text = text_buf;
        result.text_len = text_len;
        result.uart_errors = platform_uart1_errors();
        platform_scope_result(&result.samples, &result.codes);

        // Забираем эхо из UART1.
        while (platform_uart1_getc() >= 0)
        {
        }

        started = false;
        if (report != 0)
        {
            report(&result);
        }
    }
}