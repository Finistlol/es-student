#include "api/api-commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device/device.h"
#include "platform/platform.h"
#include "firmware.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "edge-task/edge-task.h"
#include "uart-task/uart-task.h"
#include "block-task/block-task.h"
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

    unsigned long parsed = strtoul(text, NULL, 10);
    if (parsed > 4294967295UL)
    {
        return false;
    }

    *value = (uint32_t)parsed;
    return true;
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
    printf("led: on, period %lu ms\n", (unsigned long)led_task_get_period_ms());
}

static void cmd_led_disable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_OFF);
    printf("led: off, period %lu ms\n", (unsigned long)led_task_get_period_ms());
}

static void cmd_led_blink(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_BLINK);
    printf("led: blink, period %lu ms\n", (unsigned long)led_task_get_period_ms());
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
    printf("led: %s, period %lu ms\n",
           led_task_get_state() == LED_STATE_ON ? "on" : "off",
           (unsigned long)led_task_get_period_ms());
}

static void cmd_button(const command_t *command)
{
    (void)command;
    printf("button: %s, presses %lu\n",
           button_task_is_pressed() ? "pressed" : "released",
           (unsigned long)button_task_get_press_count());
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
    default:
        printf("pi: not started\n");
        break;
    }
}

static void cmd_edge(const command_t *command)
{
    bool open_drain;

    if (command->argc != 1)
    {
        printf("error: usage edge pp|od\n");
        return;
    }

    if (strcmp(command->argv[0], "pp") == 0)
    {
        open_drain = false;
    }
    else if (strcmp(command->argv[0], "od") == 0)
    {
        open_drain = true;
    }
    else
    {
        printf("error: usage edge pp|od\n");
        return;
    }

    if (!edge_task_measure(open_drain))
    {
        printf("error: edge is busy\n");
        return;
    }
}

static void cmd_loop(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage loop internal|pins\n");
        return;
    }

    if (strcmp(command->argv[0], "internal") == 0)
    {
        platform_uart_loop_internal(true);
        printf("loop: internal\n");
    }
    else if (strcmp(command->argv[0], "pins") == 0)
    {
        platform_uart_loop_internal(false);
        printf("loop: pins\n");
    }
    else
    {
        printf("error: usage loop internal|pins\n");
    }
}

static void cmd_echo(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage echo <text>\n");
        return;
    }

    size_t len = strlen(command->argv[0]);
    if (len > 32)
    {
        printf("error: text longer than 32\n");
        return;
    }

    if (!uart_task_echo(command->argv[0], (uint8_t)len))
    {
        printf("error: uart is busy\n");
    }
}

static void cmd_link(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage link <text>\n");
        return;
    }

    size_t len = strlen(command->argv[0]);
    if (len > 32)
    {
        printf("error: text longer than 32\n");
        return;
    }

    if (!uart_task_link(command->argv[0], (uint8_t)len))
    {
        printf("error: uart is busy\n");
    }
}

static void cmd_baud_shift(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage baud_shift <percent>\n");
        return;
    }

    // Процент может быть отрицательным: разбираем знак вручную.
    const char *p = command->argv[0];
    bool negative = false;
    if (p[0] == '-')
    {
        negative = true;
        p = p + 1;
    }

    uint32_t abs_value = 0;
    if (!parse_u32(p, &abs_value) || abs_value > 50)
    {
        printf("error: percent must be -50..50\n");
        return;
    }

    int32_t percent = negative ? -(int32_t)abs_value : (int32_t)abs_value;
    uint32_t actual = platform_uart1_baud_shift(percent);
    if (actual == 0)
    {
        printf("error: percent must be -50..50\n");
        return;
    }

    printf("baud_shift: uart1 %lu baud\n", (unsigned long)actual);
}

static void cmd_text(const command_t *command)
{
    (void)command;
    if (!uart_task_text())
    {
        printf("error: uart is busy\n");
    }
}

static void cmd_parity(const command_t *command)
{
    if (command->argc != 2)
    {
        printf("error: usage parity <rx> <tx>\n");
        return;
    }

    for (int i = 0; i < 2; i++)
    {
        const char *mode = command->argv[i];
        if (strcmp(mode, "none") != 0 && strcmp(mode, "even") != 0 &&
            strcmp(mode, "odd") != 0)
        {
            printf("error: usage parity <rx> <tx>\n");
            return;
        }
    }

    platform_uart0_parity(command->argv[0]);
    platform_uart1_parity(command->argv[1]);
    printf("parity: rx %s, tx %s\n", command->argv[0], command->argv[1]);
}

static void cmd_block(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage block irq|poll\n");
        return;
    }

    bool irq_mode;
    if (strcmp(command->argv[0], "irq") == 0)
    {
        irq_mode = true;
    }
    else if (strcmp(command->argv[0], "poll") == 0)
    {
        irq_mode = false;
    }
    else
    {
        printf("error: usage block irq|poll\n");
        return;
    }

    if (!uart_task_block(irq_mode))
    {
        printf("error: uart is busy\n");
    }
}

static void cmd_scope(const command_t *command)
{
    uint32_t baud = 0;

    if (command->argc != 2 || !parse_u32(command->argv[0], &baud))
    {
        printf("error: usage scope <baud> <text>\n");
        return;
    }

    if (baud < 9600 || baud > 115200)
    {
        printf("error: usage scope <baud> <text>\n");
        return;
    }

    const char *text = command->argv[1];
    size_t len = strlen(text);
    if (len == 0 || len > 4)
    {
        printf("error: usage scope <baud> <text>\n");
        return;
    }

    if (!block_task_start(baud, text, (uint8_t)len))
    {
        printf("error: scope is busy\n");
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
    { "edge", "measure RC edges, pp|od", cmd_edge },
    { "loop", "UART loopback mode", cmd_loop },
    { "echo", "send text through UART", cmd_echo },
    { "link", "send text both ways", cmd_link },
    { "baud_shift", "shift uart1 baud, %", cmd_baud_shift },
    { "text", "send 32 chars uart1->0", cmd_text },
    { "parity", "set parity rx tx", cmd_parity },
    { "block", "64-byte block, irq|poll", cmd_block },
    { "scope", "capture UART waveform", cmd_scope },
    { "profiling", "stopwatch results", cmd_profiling },
    { "profiling_reset", "reset stopwatches", cmd_profiling_reset },
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);