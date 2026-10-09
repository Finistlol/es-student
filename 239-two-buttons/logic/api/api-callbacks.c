#include "api/api-commands.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "device/device.h"
#include "platform/platform.h"
#include "firmware.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
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
    for (uint32_t id = 0; id < PLATFORM_BUTTON_COUNT; id++)
    {
        printf("button %lu: %s, presses %lu\n",
               (unsigned long)(id + 1),
               button_task_is_pressed((platform_button_t)id) ? "pressed" : "released",
               (unsigned long)button_task_get_press_count((platform_button_t)id));
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
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);