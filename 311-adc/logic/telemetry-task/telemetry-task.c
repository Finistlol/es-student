#include "telemetry-task/telemetry-task.h"

#include <stdbool.h>

#include "platform/platform.h"
#include "systime/systime.h"

// Меньший период телеметрия не отдаёт: строка потока не успевает уйти.
#define TELEMETRY_MIN_PERIOD_MS 10

typedef enum
{
    TELEMETRY_STATE_IDLE,
    TELEMETRY_STATE_RUN,
} telemetry_state_t;

static telemetry_state_t state = TELEMETRY_STATE_IDLE;
static uint64_t period_us = 100000;
static uint64_t last_sample_us = 0;
static telemetry_task_callback_t emit = 0;

void telemetry_task_init(telemetry_task_callback_t callback)
{
    emit = callback;
    state = TELEMETRY_STATE_IDLE;
}

bool telemetry_task_start(uint32_t period_ms)
{
    if (period_ms < TELEMETRY_MIN_PERIOD_MS)
    {
        return false;
    }

    period_us = (uint64_t)period_ms * 1000;
    last_sample_us = systime_us();
    state = TELEMETRY_STATE_RUN;
    return true;
}

void telemetry_task_stop(void)
{
    state = TELEMETRY_STATE_IDLE;
}

void telemetry_task_handle(void)
{
    if (state != TELEMETRY_STATE_RUN)
    {
        return;
    }

    if (!systime_period_elapsed(&last_sample_us, period_us))
    {
        return;
    }

    telemetry_sample_t sample;
    sample.us = systime_us();
    sample.voltage_v = platform_get_voltage_v();
    sample.temp_c = platform_get_temp_c();

    if (emit != 0)
    {
        emit(&sample);
    }
}
