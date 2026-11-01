#include "stream-task/stream-task.h"

#include <stdbool.h>

#include "platform/platform.h"

static stream_task_callback_t send = 0;
static stream_packet_t packet;
static bool running = false;
static uint32_t packet_count = 0;

void stream_task_init(stream_task_callback_t callback)
{
    send = callback;
}

bool stream_task_start(void)
{
    if (running)
    {
        return false;
    }

    if (!platform_stream_start())
    {
        return false;
    }

    packet_count = 0;
    running = true;
    return true;
}

void stream_task_stop(uint32_t *packets, uint32_t *samples, uint32_t *overruns)
{
    platform_stream_stop();
    running = false;

    *packets = packet_count;
    *samples = packet_count * STREAM_SAMPLES;
    *overruns = platform_stream_overruns();
}

void stream_task_handle(void)
{
    if (!running)
    {
        return;
    }

    const uint16_t *samples = 0;
    if (!platform_stream_take(&samples))
    {
        return;
    }

    packet.marker = STREAM_MARKER;
    packet.number = (uint16_t)(packet_count + 1);
    packet.samples = STREAM_SAMPLES;
    for (uint32_t i = 0; i < STREAM_SAMPLES; i++)
    {
        packet.data[i] = samples[i];
    }

    packet_count = packet_count + 1;

    if (send != 0)
    {
        send(&packet, sizeof(packet));
    }
}
