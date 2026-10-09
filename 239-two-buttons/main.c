#include "firmware.h"

#include "logging/log.h"

#include "platform/platform.h"
#include "stdio-text-protocol/stdio-text-protocol.h"
#include "profiling/profiling.h"

#include "api/api.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"

// Номера секундомеров прибора: итерация суперцикла, приём и выполнение
// команды, обработчики трёх задач.
typedef enum
{
    SW_LOOP,
    SW_COMMAND,
    SW_LED,
    SW_BUTTON,
    SW_PI,
    SW_COUNT
} stopwatch_id_t;

int main(void)
{
    platform_init();
    stdio_text_protocol_init();

    profiling_stopwatch_init(SW_LOOP, "loop");
    profiling_stopwatch_init(SW_COMMAND, "command");
    profiling_stopwatch_init(SW_LED, "led");
    profiling_stopwatch_init(SW_BUTTON, "button");
    profiling_stopwatch_init(SW_PI, "pi");

    led_task_init();
    button_task_init(PLATFORM_BUTTON_1, led_task_next_state, led_task_blink);
    button_task_init(PLATFORM_BUTTON_2, led_task_next_period, NULL);

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

        profiling_stop(SW_LOOP);
    }
}