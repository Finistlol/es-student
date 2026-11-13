#include "uart-task/uart-task.h"

#include <stdbool.h>

#include "platform/platform.h"

typedef enum
{
    UART_STATE_IDLE,
    UART_STATE_SETTLE, // ждём успокоения линии после смены режима
    UART_STATE_BUSY,   // отправляем и принимаем
} uart_state_t;

// При суперцикле ~7 мкс на итерацию 150 вызовов ≈ 1 мс — достаточно,
// чтобы переходный процесс на линии закончился и паразитный байт пришёл.
#define UART_SETTLE_CALLS 150

static uart_state_t state = UART_STATE_IDLE;
static uint16_t settle_count = 0;
static char tx_buffer[UART_TASK_MAX_TEXT];
static uint8_t tx_length = 0;
static uint8_t tx_sent = 0;
static uart_result_t result;
static uart_task_callback_t report = 0;

void uart_task_init(uart_task_callback_t callback)
{
    report = callback;
}

bool uart_task_echo(const char *text, uint8_t length)
{
    if (state != UART_STATE_IDLE || length == 0 || length > UART_TASK_MAX_TEXT)
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
    settle_count = UART_SETTLE_CALLS;
    state = UART_STATE_SETTLE;
    return true;
}

void uart_task_handle(void)
{
    if (state == UART_STATE_IDLE)
    {
        return;
    }

    if (state == UART_STATE_SETTLE)
    {
        // Сливаем всё из очереди приёма: на линии после смены режима
        // бывает паразитный байт от переходного процесса.
        while (platform_uart_getc() >= 0)
        {
        }
        platform_uart_errors();
        settle_count = settle_count - 1;
        if (settle_count == 0)
        {
            state = UART_STATE_BUSY;
        }
        return;
    }

    result.errors |= platform_uart_errors();

    // Кладём байты в очередь передачи, пока есть место.
    while (tx_sent < tx_length && platform_uart_writable())
    {
        platform_uart_putc(tx_buffer[tx_sent]);
        tx_sent = tx_sent + 1;
    }

    // Забираем принятые байты из очереди приёма.
    int c;
    while (result.length < tx_length && (c = platform_uart_getc()) >= 0)
    {
        result.text[result.length] = (char)c;
        result.length = result.length + 1;
    }

    // Всё отправлено и всё принято — отдаём результат.
    if (tx_sent >= tx_length && result.length >= tx_length)
    {
        result.text[result.length] = '\0';
        result.errors |= platform_uart_errors();
        state = UART_STATE_IDLE;
        if (report != 0)
        {
            report(&result);
        }
    }
}