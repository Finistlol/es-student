#include "api/api-commands.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device/device.h"
#include "platform/platform.h"
#include "firmware.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "telemetry-task/telemetry-task.h"
#include "block-task/block-task.h"
#include "stream-task/stream-task.h"
#include "profiling/profiling.h"
#include "systime/systime.h"

// Числовой аргумент: десятичная запись из одних цифр от 0 до 4294967295.
static bool parse_u32(const char *text, uint32_t *value)
{
    if (text[0] == '\0')
    {
        return false;
    }

    for (const char *p = text; *p != '\0'; p++)
    {
        if (*p < '0' || *p > '9')
        {
            return false;
        }
    }

    errno = 0;
    unsigned long parsed = strtoul(text, NULL, 10);
    if (errno != 0 || parsed > 4294967295UL)
    {
        return false;
    }

    *value = (uint32_t)parsed;
    return true;
}

static const char *led_state_name(led_state_t state)
{
    switch (state)
    {
    case LED_STATE_ON:
        return "on";
    case LED_STATE_BLINK:
        return "blink";
    case LED_STATE_OFF:
    default:
        return "off";
    }
}

static void print_led_state(void)
{
    printf("led: %s, period %lu ms\n",
           led_state_name(led_task_get_state()),
           (unsigned long)led_task_get_period_ms());
}

static void cmd_info(const command_t *command)
{
    (void)command;

    device_info_t info;
    device_get_info(&info);

    printf("name: %s\n", FIRMWARE_NAME);
    printf("version: %s\n", FIRMWARE_VERSION);
    printf("project: %s\n", FIRMWARE_PROJECT);
    printf("repo: %s\n", FIRMWARE_REPO);
    printf("board: %s\n", info.board);
    printf("serial: %s\n", info.serial);
    printf("chip: manufacturer 0x%03lx, part 0x%04lx, revision %lu\n",
           (unsigned long)info.chip_manufacturer,
           (unsigned long)info.chip_part,
           (unsigned long)info.chip_revision);
    printf("pico-sdk: %s\n", info.sdk_version);
}

static void cmd_uptime(const command_t *command)
{
    (void)command;
    printf("uptime: %llu ms\n", (unsigned long long)systime_ms());
}

static void cmd_led_enable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_ON);
    print_led_state();
}

static void cmd_led_disable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_OFF);
    print_led_state();
}

static void cmd_led_blink(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_BLINK);
    print_led_state();
}

static void cmd_led_period(const command_t *command)
{
    uint32_t period_ms;

    if (command->argc != 1 || !parse_u32(command->argv[0], &period_ms))
    {
        printf("error: usage led_period <period_ms>\n");
        return;
    }

    if (period_ms == 0)
    {
        printf("error: period_ms must be greater than 0\n");
        return;
    }

    led_task_set_period_ms(period_ms);
    print_led_state();
}

static void cmd_button(const command_t *command)
{
    (void)command;
    printf("button: %s, presses %lu\n",
           button_task_is_pressed() ? "pressed" : "released",
           (unsigned long)button_task_get_press_count());
}

static void cmd_get_voltage(const command_t *command)
{
    (void)command;
    printf("voltage: %.3f V\n", platform_get_voltage_v());
}

static void cmd_get_temp(const command_t *command)
{
    (void)command;
    printf("temp: %.1f C\n", platform_get_temp_c());
}

static void cmd_tm_start(const command_t *command)
{
    uint32_t period_ms = 100;

    if (command->argc > 1)
    {
        printf("error: usage tm_start [<period_ms>]\n");
        return;
    }

    if (command->argc == 1)
    {
        if (!parse_u32(command->argv[0], &period_ms))
        {
            printf("error: usage tm_start [<period_ms>]\n");
            return;
        }

        if (period_ms < 10)
        {
            printf("error: period_ms must be at least 10\n");
            return;
        }
    }

    if (!telemetry_task_start(period_ms))
    {
        printf("error: period_ms must be at least 10\n");
        return;
    }

    printf("telemetry: started, period %lu ms\n", (unsigned long)period_ms);
}

static void cmd_tm_stop(const command_t *command)
{
    (void)command;
    telemetry_task_stop();
    printf("telemetry: stopped\n");
}

static void cmd_charge_on(const command_t *command)
{
    (void)command;
    platform_charge_set(true);
    printf("charge: on\n");
}

static void cmd_charge_off(const command_t *command)
{
    (void)command;
    platform_charge_set(false);
    printf("charge: off\n");
}

static void cmd_timer_blink_start(const command_t *command)
{
    uint32_t period_ms;

    if (command->argc != 1 || !parse_u32(command->argv[0], &period_ms))
    {
        printf("error: usage timer_blink_start <period_ms>\n");
        return;
    }

    if (period_ms < 10 || period_ms > 60000)
    {
        printf("error: period_ms must be 10..60000\n");
        return;
    }

    if (!platform_timer_blink_start(period_ms))
    {
        printf("error: blink is already running\n");
        return;
    }

    printf("timer_blink: started, period %lu ms\n", (unsigned long)period_ms);
}

static void cmd_timer_blink_stop(const command_t *command)
{
    (void)command;
    platform_timer_blink_stop();
    printf("timer_blink: stopped, %lu toggles\n",
           (unsigned long)platform_timer_blink_toggles());
}

// Отчёт о готовом блоке DMA: печатает его функция, переданная задаче блока.
void api_report_block(const block_result_t *result)
{
    printf("dma_cap: %lu samples in %lu us, expected %lu us, min %lu max %lu mean %.1f\n",
           (unsigned long)result->samples,
           (unsigned long)result->elapsed_us,
           (unsigned long)result->expected_us,
           (unsigned long)result->min,
           (unsigned long)result->max,
           result->mean);
}

static void cmd_cap_start(const command_t *command)
{
    uint32_t count = 0;
    uint32_t period_us = 0;

    if (command->argc != 2 || !parse_u32(command->argv[0], &count) ||
        !parse_u32(command->argv[1], &period_us))
    {
        printf("error: usage cap_start <N> <period_us>\n");
        return;
    }

    if (count < 1 || count > 2000 || period_us < 10)
    {
        printf("error: N must be 1..2000, period_us at least 10\n");
        return;
    }

    if (!platform_capture_start(count, period_us))
    {
        printf("error: N must be 1..2000, period_us at least 10\n");
        return;
    }

    printf("capture: started, %lu samples, period %lu us\n",
           (unsigned long)count, (unsigned long)period_us);
}

static void cmd_cap_dump(const command_t *command)
{
    (void)command;

    platform_capture_info_t info;
    platform_capture_get(&info);

    printf("capture: %lu samples, period %lu us, interval %lu..%lu us, handler %.1f %%\n",
           (unsigned long)info.count,
           (unsigned long)info.period_us,
           (unsigned long)info.interval_min_us,
           (unsigned long)info.interval_max_us,
           info.handler_pct);

    for (uint32_t i = 0; i < info.count; i++)
    {
        if (i % 16 == 0)
        {
            printf("cap:");
        }
        printf(" %lu", (unsigned long)info.codes[i]);
        if (i % 16 == 15 || i + 1 == info.count)
        {
            printf("\n");
        }
    }
}

static void cmd_dma_cap(const command_t *command)
{
    uint32_t samples = 0;

    if (command->argc != 1 || !parse_u32(command->argv[0], &samples))
    {
        printf("error: usage dma_cap <N>\n");
        return;
    }

    if (samples < 1 || samples > 8192)
    {
        printf("error: N must be 1..8192\n");
        return;
    }

    if (!block_task_start(samples))
    {
        printf("error: N must be 1..8192\n");
        return;
    }
}

static void cmd_stream_start(const command_t *command)
{
    bool eight_bit = false;

    if (command->argc > 1 ||
        (command->argc == 1 && strcmp(command->argv[0], "8") != 0))
    {
        printf("error: usage stream_start [8]\n");
        return;
    }

    if (command->argc == 1)
    {
        eight_bit = true;
    }

    if (!stream_task_start(eight_bit))
    {
        printf("error: stream is already running\n");
        return;
    }

    printf("stream: started, 44100 Hz, %u bit\n", eight_bit ? 8u : 16u);
}

static void cmd_stream_stop(const command_t *command)
{
    (void)command;

    uint32_t packets = 0;
    uint32_t samples = 0;
    uint32_t overruns = 0;
    stream_task_stop(&packets, &samples, &overruns);

    printf("stream: stopped, %lu frames, %lu samples, %lu overruns\n",
           (unsigned long)packets,
           (unsigned long)samples,
           (unsigned long)overruns);
}

static void cmd_wdt_enable(const command_t *command)
{
    uint32_t timeout_ms = 0;

    if (command->argc != 1 || !parse_u32(command->argv[0], &timeout_ms))
    {
        printf("error: usage wdt_enable <timeout_ms>\n");
        return;
    }

    if (!platform_watchdog_enable(timeout_ms))
    {
        printf("error: timeout_ms must be 1..8388\n");
        return;
    }

    printf("wdt: enabled, timeout %lu ms\n", (unsigned long)timeout_ms);
}

static void cmd_hang(const command_t *command)
{
    (void)command;
    printf("hang: superloop stopped\n");

    // Намеренный вечный цикл: суперцикл больше не обновляет сторожевой
    // таймер, и плата перезапускается им.
    for (;;)
    {
    }
}

static void cmd_reset_reason(const command_t *command)
{
    (void)command;
    printf("reset: %s, restarts %lu\n",
           platform_restarted_by_watchdog() ? "watchdog" : "other",
           (unsigned long)platform_restarts());
}

static void cmd_generate_tone(const command_t *command)
{
    uint32_t freq_hz = 0;

    if (command->argc != 1 || !parse_u32(command->argv[0], &freq_hz))
    {
        printf("error: usage generate_tone <Hz>\n");
        return;
    }

    if (!platform_tone_set(freq_hz))
    {
        printf("error: Hz must be 0 or 20..20000\n");
        return;
    }

    if (freq_hz == 0)
    {
        printf("tone: off\n");
    }
    else
    {
        printf("tone: %lu Hz\n", (unsigned long)freq_hz);
    }
}

// Отчёты микрофона: блок снимает задача блока, а здесь его коды
// переводятся в смещение и уровень сигнала за окно.
static void mic_report_bias(const block_result_t *result)
{
    // Смещение — среднее кодов блока в вольтах.
    float bias_v = result->mean * 3.3f / 4096.0f;
    printf("mic: bias %.3f V\n", bias_v);
}

static void mic_report_level(const block_result_t *result)
{
    // Отклонения от среднего: среднеквадратичное и пик, в милливольтах.
    float mean = result->mean;
    float sum_sq = 0.0f;
    float peak = 0.0f;
    for (uint32_t i = 0; i < result->samples; i++)
    {
        float dev = (float)result->codes[i] - mean;
        sum_sq += dev * dev;
        if (dev < 0)
        {
            dev = -dev;
        }
        if (dev > peak)
        {
            peak = dev;
        }
    }
    float rms = result->samples > 0 ? sum_sq / result->samples : 0.0f;
    // Корень без math.h: два шага Ньютона от грубой оценки.
    rms = rms > 0 ? rms : 0.0f;
    float r = rms > 1.0f ? rms : 1.0f;
    for (int i = 0; i < 8; i++)
    {
        r = (r + rms / r) / 2.0f;
    }
    float to_mv = 3300.0f / 4096.0f;
    printf("mic: level %.1f mV rms, peak %.1f mV, %lu ms\n",
           r * to_mv, peak * to_mv,
           (unsigned long)(result->samples * 1000UL / 44100UL));
}

static void cmd_mic_bias(const command_t *command)
{
    (void)command;
    if (!block_task_start_cb(2000, mic_report_bias))
    {
        printf("error: block task is busy\n");
    }
}

static void cmd_mic_level(const command_t *command)
{
    uint32_t ms = 100;

    if (command->argc > 1)
    {
        printf("error: usage mic_level [<ms>]\n");
        return;
    }

    if (command->argc == 1 && !parse_u32(command->argv[0], &ms))
    {
        printf("error: usage mic_level [<ms>]\n");
        return;
    }

    if (ms < 1 || ms > 185)
    {
        printf("error: ms must be 1..185\n");
        return;
    }

    uint32_t samples = (uint32_t)((uint64_t)ms * 44100 / 1000);
    if (!block_task_start_cb(samples, mic_report_level))
    {
        printf("error: block task is busy\n");
    }
}

static void cmd_pi_start(const command_t *command)
{
    uint32_t terms = 1000000;

    if (command->argc > 1)
    {
        printf("error: usage pi_start [<terms>]\n");
        return;
    }

    if (command->argc == 1)
    {
        if (!parse_u32(command->argv[0], &terms))
        {
            printf("error: usage pi_start [<terms>]\n");
            return;
        }

        if (terms == 0)
        {
            printf("error: terms must be greater than 0\n");
            return;
        }
    }

    if (!pi_task_start(terms))
    {
        printf("error: pi is busy\n");
        return;
    }

    printf("pi: started, %lu terms\n", (unsigned long)terms);
}

static void cmd_pi(const command_t *command)
{
    (void)command;

    pi_task_result_t result;
    pi_task_get_result(&result);

    switch (result.state)
    {
    case PI_TASK_RUNNING:
        printf("pi: running\n");
        break;
    case PI_TASK_DONE:
        printf("pi: %.9f in %llu ms\n", result.value, (unsigned long long)result.time_ms);
        break;
    case PI_TASK_NOT_STARTED:
    default:
        printf("pi: not started\n");
        break;
    }
}

static void cmd_profiling(const command_t *command)
{
    (void)command;

#if PROFILING_ENABLED
    profiling_result_t result;
    for (uint8_t id = 0; id < PROFILING_MAX_STOPWATCHES; id++)
    {
        if (!profiling_get(id, &result))
        {
            continue;
        }
        printf("%-10s count %-10lu mean %.2f us  max %lu us\n",
               result.name,
               (unsigned long)result.count,
               result.mean_us,
               (unsigned long)result.max_us);
    }
#else
    printf("profiling is disabled in this build\n");
#endif
}

static void cmd_profiling_reset(const command_t *command)
{
    (void)command;
    profiling_reset();
    printf("profiling reset\n");
}

const api_command_t api_commands[] = {
    { "info", "device passport", cmd_info },
    { "uptime", "time since reset", cmd_uptime },
    { "led_enable", "turn LED on", cmd_led_enable },
    { "led_disable", "turn LED off", cmd_led_disable },
    { "led_blink", "blink LED", cmd_led_blink },
    { "led_period", "set blink period, ms", cmd_led_period },
    { "button", "button state and press count", cmd_button },
    { "pi_start", "start pi calculation", cmd_pi_start },
    { "pi", "last pi result", cmd_pi },
    { "profiling", "stopwatch results", cmd_profiling },
    { "profiling_reset", "reset stopwatches", cmd_profiling_reset },
    { "get_voltage", "voltage on GP26, V", cmd_get_voltage },
    { "get_temp", "chip temperature, C", cmd_get_temp },
    { "tm_start", "start telemetry stream", cmd_tm_start },
    { "tm_stop", "stop telemetry stream", cmd_tm_stop },
    { "charge_on", "charge RC circuit", cmd_charge_on },
    { "timer_blink_start", "blink LED by timer, ms", cmd_timer_blink_start },
    { "timer_blink_stop", "stop timer blink", cmd_timer_blink_stop },
    { "cap_start", "capture N samples by timer", cmd_cap_start },
    { "cap_dump", "print last capture", cmd_cap_dump },
    { "dma_cap", "capture block by DMA", cmd_dma_cap },
    { "stream_start", "start sample stream", cmd_stream_start },
    { "stream_stop", "stop sample stream", cmd_stream_stop },
    { "wdt_enable", "enable watchdog, ms", cmd_wdt_enable },
    { "hang", "stop superloop forever", cmd_hang },
    { "reset_reason", "why board restarted", cmd_reset_reason },
    { "generate_tone", "tone on GP20, Hz", cmd_generate_tone },
    { "mic_bias", "microphone bias, V", cmd_mic_bias },
    { "mic_level", "microphone level, mV", cmd_mic_level },
    { "charge_off", "discharge RC circuit", cmd_charge_off },
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);