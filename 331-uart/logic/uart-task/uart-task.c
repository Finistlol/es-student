#include "uart-task/uart-task.h"

#include <stdbool.h>
#include <string.h>

#include "platform/platform.h"

typedef enum
{
    OP_IDLE,
    OP_SETTLE,     // успокоение линии после смены режима
    OP_ECHO_TX,    // отправка и приём в петле
    OP_LINK_0_1,   // UART0 → UART1
    OP_LINK_1_0,   // UART1 → UART0
    OP_TEXT_TX,    // UART1 → UART0 (32 символа)
    OP_BLOCK_TX,   // UART1 → UART0 (64 байта)
    OP_BLOCK_WAIT, // ожидание конца передачи для poll
    OP_BLOCK_RX,   // чтение из FIFO для poll
} op_state_t;

#define SETTLE_CALLS 150
#define LINE32 "0123456789ABCDEFGHIJKLMNOPQRSTUV"

static op_state_t state = OP_IDLE;
static uint16_t settle_count = 0;
static char tx_buffer[UART_TASK_MAX_TEXT];
static uint8_t tx_length = 0;
static uint8_t tx_sent = 0;
static uart_result_t result;
static uart_block_result_t block_result;
static uart_task_callback_t report = 0;
static uart_block_callback_t block_report = 0;
static bool link_second_half = false;
static bool text_mode = false;
static op_state_t settle_next = OP_ECHO_TX;
static bool block_irq_mode = false;
static uint8_t block_pattern[UART_TASK_BLOCK_SIZE];
static uint8_t block_rx_buffer[UART_TASK_BLOCK_SIZE];
static uint8_t block_rx_count = 0;

void uart_task_init(uart_task_callback_t callback,
                    uart_block_callback_t block_cb)
{
    report = callback;
    block_report = block_cb;
    for (uint8_t i = 0; i < UART_TASK_BLOCK_SIZE; i++)
    {
        block_pattern[i] = i;
    }
}

bool uart_task_echo(const char *text, uint8_t length)
{
    if (state != OP_IDLE || length == 0 || length > UART_TASK_MAX_TEXT)
    {
        return false;
    }
    for (uint8_t i = 0; i < length; i++)
    {
        tx_buffer[i] = text[i];
    }
    tx_length = length;
    tx_sent = 0;
    result.length = 0;
    result.errors = 0;
    settle_count = SETTLE_CALLS;
    state = OP_SETTLE;
    link_second_half = false;
    text_mode = false;
    return true;
}

bool uart_task_link(const char *text, uint8_t length)
{
    if (state != OP_IDLE || length == 0 || length > UART_TASK_MAX_TEXT)
    {
        return false;
    }
    // Обмен между модулями идёт через выводы — отключаем внутреннюю петлю.
    platform_uart_loop_internal(false);
    for (uint8_t i = 0; i < length; i++)
    {
        tx_buffer[i] = text[i];
    }
    tx_length = length;
    tx_sent = 0;
    result.length = 0;
    result.errors = 0;
    settle_count = SETTLE_CALLS;
    state = OP_SETTLE;
    link_second_half = true;
    text_mode = false;
    return true;
}

bool uart_task_text(void)
{
    if (state != OP_IDLE)
    {
        return false;
    }
    platform_uart_loop_internal(false);
    for (uint8_t i = 0; i < 32; i++)
    {
        tx_buffer[i] = LINE32[i];
    }
    tx_length = 32;
    tx_sent = 0;
    result.length = 0;
    result.errors = 0;
    settle_count = SETTLE_CALLS;
    state = OP_SETTLE;
    link_second_half = false; // сразу UART1 → UART0, без 0→1
    text_mode = true;
    return true;
}

bool uart_task_block(bool irq_mode)
{
    if (state != OP_IDLE)
    {
        return false;
    }
    platform_uart_loop_internal(false);
    block_irq_mode = irq_mode;
    block_result.match_count = 0;
    block_rx_count = 0;
    block_result.total = UART_TASK_BLOCK_SIZE;
    block_result.errors = 0;
    block_result.irq_mode = irq_mode;
    tx_length = UART_TASK_BLOCK_SIZE;
    tx_sent = 0;

    settle_count = SETTLE_CALLS;
    state = OP_SETTLE;
    // После успокоения начнём блок: фаза задаётся в start_echo_phase.
    block_irq_mode = irq_mode;
    text_mode = false;
    link_second_half = false;
    // Устанавливаем кастомную фазу: OP_SETTLE завершится в OP_BLOCK_TX.
    settle_next = OP_BLOCK_TX;
    return true;
}

static void start_echo_phase(void)
{
    // Блок: сразу в OP_BLOCK_TX, без echo/link/text логики.
    if (settle_next == OP_BLOCK_TX)
    {
        settle_next = OP_ECHO_TX;
        state = OP_BLOCK_TX;
        tx_sent = 0;
        return;
    }

    if (text_mode)
    {
        state = OP_LINK_1_0;
    }
    else
    {
        state = link_second_half ? OP_LINK_0_1 : OP_ECHO_TX;
    }
    tx_sent = 0;
    result.length = 0;
}

void uart_task_handle(void)
{
    if (state == OP_IDLE)
    {
        return;
    }

    if (state == OP_SETTLE)
    {
        while (platform_uart_getc() >= 0)
        {
        }
        while (platform_uart1_getc() >= 0)
        {
        }
        platform_uart_errors();
        platform_uart1_errors();
        settle_count = settle_count - 1;
        if (settle_count == 0)
        {
            start_echo_phase();
        }
        return;
    }

    if (state == OP_ECHO_TX)
    {
        result.errors |= platform_uart_errors();
        while (tx_sent < tx_length && platform_uart_writable())
        {
            platform_uart_putc(tx_buffer[tx_sent]);
            tx_sent = tx_sent + 1;
        }
        int c;
        while (result.length < tx_length && (c = platform_uart_getc()) >= 0)
        {
            result.text[result.length] = (char)c;
            result.length = result.length + 1;
        }
        if (tx_sent >= tx_length && result.length >= tx_length)
        {
            result.text[result.length] = '\0';
            result.errors |= platform_uart_errors();
            state = OP_IDLE;
            if (report != 0)
            {
                report(&result);
            }
        }
        return;
    }

    if (state == OP_LINK_0_1)
    {
        // UART0 TX → GP0 → GP9 → UART1 RX
        result.errors |= platform_uart1_errors();
        while (tx_sent < tx_length && platform_uart_writable())
        {
            platform_uart_putc(tx_buffer[tx_sent]);
            tx_sent = tx_sent + 1;
        }
        int c;
        while (result.length < tx_length && (c = platform_uart1_getc()) >= 0)
        {
            result.text[result.length] = (char)c;
            result.length = result.length + 1;
        }
        if (tx_sent >= tx_length && result.length >= tx_length)
        {
            result.text[result.length] = '\0';
            result.errors |= platform_uart1_errors();
            // Первая половина готова — отдаём через callback.
            result.direction = UART_DIR_0_1;
            if (report != 0)
            {
                report(&result);
            }
            tx_sent = 0;
            result.length = 0;
            result.errors = 0;
            state = OP_LINK_1_0;
        }
        return;
    }

    if (state == OP_LINK_1_0)
    {
        // UART1 TX → GP8 → GP1 → UART0 RX
        result.errors |= platform_uart_errors();
        while (tx_sent < tx_length && platform_uart1_writable())
        {
            platform_uart1_putc(tx_buffer[tx_sent]);
            tx_sent = tx_sent + 1;
        }
        int c;
        while (result.length < tx_length && (c = platform_uart_getc()) >= 0)
        {
            result.text[result.length] = (char)c;
            result.length = result.length + 1;
        }
        if (tx_sent >= tx_length && result.length >= tx_length)
        {
            result.text[result.length] = '\0';
            result.errors |= platform_uart_errors();
            result.direction = text_mode ? UART_DIR_TEXT : UART_DIR_1_0;
            text_mode = false;
            state = OP_IDLE;
            if (report != 0)
            {
                report(&result);
            }
        }
        return;
    }

    if (state == OP_BLOCK_TX)
    {
        // UART1 отправляет 64 байта в UART0.
        while (tx_sent < tx_length && platform_uart1_writable())
        {
            platform_uart1_putc((char)block_pattern[tx_sent]);
            tx_sent = tx_sent + 1;
        }

        if (block_irq_mode)
        {
            // Режим «с прерыванием»: суперцикл читает FIFO ВМОМЕНТ передачи.
            // Он быстрее, чем байты приходят (6,7 мкс против 87 мкс на байт),
            // поэтому переполнения не будет и все 64 байта дойдут.
            int c;
            while ((c = platform_uart_getc()) >= 0 &&
                   block_rx_count < UART_TASK_BLOCK_SIZE)
            {
                block_rx_buffer[block_rx_count] = (uint8_t)c;
                block_rx_count = block_rx_count + 1;
            }

            if (tx_sent >= tx_length && block_rx_count >= UART_TASK_BLOCK_SIZE)
            {
                block_result.errors = platform_uart_errors();
                for (uint8_t i = 0; i < UART_TASK_BLOCK_SIZE; i++)
                {
                    if (block_rx_buffer[i] == block_pattern[i])
                    {
                        block_result.match_count = block_result.match_count + 1;
                    }
                }
                state = OP_IDLE;
                if (block_report != 0)
                {
                    block_report(&block_result);
                }
            }
        }
        else if (tx_sent >= tx_length)
        {
            // Без прерывания: ждём окончания передачи, потом читаем FIFO.
            state = OP_BLOCK_WAIT;
            settle_count = 1200; // ~8 мс, достаточно для 64 байт при 115200
        }
        return;
    }

    if (state == OP_BLOCK_WAIT)
    {
        // Ждём, пока последний байт дойдёт от UART1 к UART0.
        settle_count = settle_count - 1;
        if (settle_count == 0)
        {
            state = OP_BLOCK_RX;
        }
        return;
    }

    if (state == OP_BLOCK_RX)
    {
        if (block_irq_mode)
        {
            // Результат из буфера прерывании: platform_block_data()/count.
            platform_uart0_irq_enable(false);
            block_result.errors = platform_uart_errors();
            const uint8_t *data = platform_block_data();
            uint8_t count = platform_block_count;
            for (uint8_t i = 0; i < count && i < UART_TASK_BLOCK_SIZE; i++)
            {
                if (data[i] == block_pattern[i])
                {
                    block_result.match_count = block_result.match_count + 1;
                }
            }
        }
        else
        {
            // Читаем всё, что накопилось в очереди приёма UART0 (максимум 32 байта).
            uint8_t fifo_buf[UART_TASK_BLOCK_SIZE];
            uint8_t fifo_count = 0;
            int c;
            while ((c = platform_uart_getc()) >= 0 && fifo_count < UART_TASK_BLOCK_SIZE)
            {
                fifo_buf[fifo_count] = (uint8_t)c;
                fifo_count = fifo_count + 1;
            }
            block_result.errors = platform_uart_errors();
            for (uint8_t i = 0; i < fifo_count; i++)
            {
                if (fifo_buf[i] == block_pattern[i])
                {
                    block_result.match_count = block_result.match_count + 1;
                }
            }
        }
        state = OP_IDLE;
        if (block_report != 0)
        {
            block_report(&block_result);
        }
        return;
    }
}