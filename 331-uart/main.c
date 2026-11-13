#include "firmware.h"

#include "logging/log.h"

#include "platform/platform.h"
#include "stdio-text-protocol/stdio-text-protocol.h"
#include "profiling/profiling.h"

#include "api/api.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "edge-task/edge-task.h"
#include "uart-task/uart-task.h"

// Номера секундомеров прибора.
typedef enum
{
    SW_LOOP,
    SW_COMMAND,
    SW_LED,
    SW_BUTTON,
    SW_PI,
    SW_EDGE,
    SW_COUNT
} stopwatch_id_t;

// Результат фронтов печатает api, задача отдаёт данные.
static void edge_report(const edge_result_t *result)
{
    if (result->rise_ms < 0)
    {
        printf("edge: timeout\n");
        return;
    }
    printf("edge %s: rise %.1f ms, fall %.1f ms\n",
           result->open_drain ? "od" : "pp",
           result->rise_ms, result->fall_ms);
}

// Результат обмена UART печатает api, задача отдаёт данные.
static void uart_report(const uart_result_t *result)
{
    printf("rx: %s err: %s\n",
           result->text,
           result->errors == 0 ? "none" : "overrun");
}

int main(void)
{
    platform_init();
    stdio_text_protocol_init();

    profiling_stopwatch_init(SW_LOOP, "loop");
    profiling_stopwatch_init(SW_COMMAND, "command");
    profiling_stopwatch_init(SW_LED, "led");
    profiling_stopwatch_init(SW_BUTTON, "button");
    profiling_stopwatch_init(SW_PI, "pi");
    profiling_stopwatch_init(SW_EDGE, "edge");

    led_task_init();
    button_task_init(led_task_next_state);
    edge_task_init(edge_report);
    uart_task_init(uart_report);

    LOG_INF("%s %s\n", FIRMWARE_NAME, FIRMWARE_VERSION);

    while (1)
    {
        profiling_start(SW_LOOP);

        const command_t *command = stdio_text_protocol_handle();
        if (command != NULL)
        {
            profiling_start(SW_COMMAND);
            api_handle(command);
            profiling_stop(SW_COMMAND);
        }

        profiling_start(SW_LED);
        led_task_handle();
        profiling_stop(SW_LED);

        profiling_start(SW_BUTTON);
        button_task_handle();
        profiling_stop(SW_BUTTON);

        profiling_start(SW_PI);
        pi_task_handle();
        profiling_stop(SW_PI);

        profiling_start(SW_EDGE);
        edge_task_handle();
        profiling_stop(SW_EDGE);

        uart_task_handle();

        profiling_stop(SW_LOOP);
    }
}