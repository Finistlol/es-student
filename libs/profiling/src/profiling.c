#include "profiling/profiling.h"

#if PROFILING_ENABLED

#include "pico/time.h"

typedef struct
{
    const char *name;
    uint32_t count;
    uint64_t sum_us;
    uint32_t max_us;
    uint32_t started_us;
    bool running;
} stopwatch_t;

static stopwatch_t stopwatches[PROFILING_MAX_STOPWATCHES];

void profiling_stopwatch_init(uint8_t id, const char *name)
{
    if (id >= PROFILING_MAX_STOPWATCHES)
    {
        return;
    }

    stopwatches[id].name = name;
    stopwatches[id].count = 0;
    stopwatches[id].sum_us = 0;
    stopwatches[id].max_us = 0;
    stopwatches[id].running = false;
}

void profiling_start(uint8_t id)
{
    if (id >= PROFILING_MAX_STOPWATCHES)
    {
        return;
    }

    stopwatches[id].started_us = time_us_32();
    stopwatches[id].running = true;
}

void profiling_stop(uint8_t id)
{
    if (id >= PROFILING_MAX_STOPWATCHES || !stopwatches[id].running)
    {
        return;
    }

    uint32_t now_us = time_us_32();
    uint32_t spent_us = now_us - stopwatches[id].started_us;

    stopwatches[id].running = false;
    stopwatches[id].count = stopwatches[id].count + 1;
    stopwatches[id].sum_us = stopwatches[id].sum_us + spent_us;

    if (spent_us > stopwatches[id].max_us)
    {
        stopwatches[id].max_us = spent_us;
    }
}

bool profiling_get(uint8_t id, profiling_result_t *result)
{
    if (id >= PROFILING_MAX_STOPWATCHES || stopwatches[id].name == NULL)
    {
        return false;
    }

    result->name = stopwatches[id].name;
    result->count = stopwatches[id].count;
    result->mean_us = stopwatches[id].count > 0
                          ? (float)stopwatches[id].sum_us / stopwatches[id].count
                          : 0.0f;
    result->max_us = stopwatches[id].max_us;
    return true;
}

void profiling_reset(void)
{
    for (uint8_t id = 0; id < PROFILING_MAX_STOPWATCHES; id++)
    {
        stopwatches[id].count = 0;
        stopwatches[id].sum_us = 0;
        stopwatches[id].max_us = 0;
    }
}

#else

// Пустой файл сборки без профилирования: все вызовы заменены макросами.
typedef int profiling_disabled_dummy_t;

#endif