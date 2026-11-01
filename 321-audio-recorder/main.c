#include "firmware.h"

#include "logging/log.h"

#include "platform/platform.h"
#include "stdio-text-protocol/stdio-text-protocol.h"
#include "profiling/profiling.h"

#include "api/api.h"
#include "api/api-commands.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "telemetry-task/telemetry-task.h"
#include "block-task/block-task.h"
#include "stream-task/stream-task.h"

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

// Строка потока телеметрии: задача измерения печать не касается,
// связь задачи с выводом задаёт main.
static void telemetry_line(const telemetry_sample_t *sample)
{
    printf("tm: %llu %.3f %.1f\n",
           (unsigned long long)sample->us,
           sample->voltage_v,
           sample->temp_c);
}

// Пакет потока уходит в порт байтами без замены: связь задачи с отправкой
// задаёт main, двоичные данные отправляет platform.
static void stream_send(const uint8_t *packet, uint16_t len)
{
    platform_send_bytes(packet, len);
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

    led_task_init();
    telemetry_task_init(telemetry_line);
    button_task_init(led_task_next_state);
    block_task_init(api_report_block);
    stream_task_init(stream_send);

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

        telemetry_task_handle();
        block_task_handle();
        stream_task_handle();

        profiling_stop(SW_LOOP);

        // Сторожевой таймер обновляет суперцикл — последним делом итерации:
        // если хоть одна задача зависнет, обновления прекратятся и плата
        // перезапустится.
        platform_watchdog_update();
    }
}