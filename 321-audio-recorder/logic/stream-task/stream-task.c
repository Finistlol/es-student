#include "stream-task/stream-task.h"

#include <stdbool.h>

#include "platform/platform.h"

static stream_task_callback_t send = 0;
static uint8_t packet[STREAM_PACKET_MAX];
static bool running = false;
static uint32_t packet_count = 0;

void stream_task_init(stream_task_callback_t callback)
{
    send = callback;
}

bool stream_task_start(bool eight_bit)
{
    if (running)
    {
        return false;
    }

    if (!platform_stream_start(eight_bit))
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

    uint32_t per_packet = platform_stream_bits() == 8 ? STREAM_SAMPLES_8 : STREAM_SAMPLES_16;
    *packets = packet_count;
    *samples = packet_count * per_packet;
    *overruns = platform_stream_overruns();
}

static void put_u16(uint8_t *at, uint16_t value)
{
    at[0] = (uint8_t)(value & 0xff);
    at[1] = (uint8_t)(value >> 8);
}

void stream_task_handle(void)
{
    if (!running)
    {
        return;
    }

    const uint8_t *data = 0;
    uint16_t samples = 0;
    if (!platform_stream_take(&data, &samples))
    {
        return;
    }

    put_u16(&packet[0], STREAM_MARKER);
    put_u16(&packet[2], (uint16_t)(packet_count + 1));
    put_u16(&packet[4], samples);

    uint16_t data_bytes = platform_stream_bits() == 8 ? samples : samples * 2;
    for (uint16_t i = 0; i < data_bytes; i++)
    {
        packet[STREAM_HEADER_BYTES + i] = data[i];
    }

    packet_count = packet_count + 1;

    if (send != 0)
    {
        send(packet, STREAM_HEADER_BYTES + data_bytes);
    }
}
