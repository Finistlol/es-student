#include <stdio.h>

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

typedef enum
{
    SW_LOOP,
    SW_COMMAND,
    SW_LED,
    SW_BUTTON,
    SW_PI,
    SW_EDGE,
    SW_UART,
    SW_COUNT
} stopwatch_id_t;

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

static void uart_report(const uart_result_t *result)
{
    printf("rx: %s err: %s\n",
           result->text,
           result->errors == 0 ? "none" : "overrun");
}

static void uart_block_report(const uart_block_result_t *result)
{
    const char *err = "none";
    if (result->errors & 8) err = "OE";
    else if (result->errors & 2) err = "PE";
    else if (result->errors & 1) err = "FE";
    printf("block %s: %d/%d match, err: %s\n",
           result->irq_mode ? "irq" : "poll",
           result->match_count, result->total, err);
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
    profiling_stopwatch_init(SW_UART, "uart");

    led_task_init();
    button_task_init(led_task_next_state);
    edge_task_init(edge_report);
    uart_task_init(uart_report, uart_block_report);

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

        led_task_handle();
        button_task_handle();
        pi_task_handle();
        edge_task_handle();
        uart_task_handle();

        profiling_stop(SW_LOOP);
    }
}