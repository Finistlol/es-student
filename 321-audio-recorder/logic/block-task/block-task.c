#include "block-task/block-task.h"

#include <stdbool.h>

#include "platform/platform.h"

// Частота отсчётов захвата, Гц: шаг задаёт делитель АЦП в platform.
#define BLOCK_SAMPLE_HZ 44100

static block_task_callback_t report = 0;
static bool started = false;

void block_task_init(block_task_callback_t callback)
{
    report = callback;
}

bool block_task_start(uint32_t samples)
{
    if (started)
    {
        return false;
    }

    if (!platform_dma_capture_start(samples))
    {
        return false;
    }

    started = true;
    return true;
}

void block_task_handle(void)
{
    if (!started || !platform_dma_capture_done())
    {
        return;
    }

    uint32_t samples = 0;
    uint32_t elapsed_us = 0;
    const uint16_t *codes = 0;
    platform_dma_capture_result(&samples, &elapsed_us, &codes);

    block_result_t result;
    result.samples = samples;
    result.elapsed_us = elapsed_us;
    result.expected_us = (uint32_t)((uint64_t)samples * 1000000 / BLOCK_SAMPLE_HZ);

    uint32_t min = codes[0];
    uint32_t max = codes[0];
    uint64_t sum = 0;
    for (uint32_t i = 0; i < samples; i++)
    {
        uint32_t code = codes[i];
        if (code < min)
        {
            min = code;
        }
        if (code > max)
        {
            max = code;
        }
        sum += code;
    }

    result.min = min;
    result.max = max;
    result.mean = samples > 0 ? (float)sum / (float)samples : 0.0f;

    started = false;
    if (report != 0)
    {
        report(&result);
    }
}
