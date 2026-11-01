#pragma once

#include <stdbool.h>
#include <stdint.h>

// Метка пакета потока: байты a5 5a, в памяти младшим байтом вперёд.
#define STREAM_MARKER 0x5aa5
#define STREAM_SAMPLES 512

// Пакет: метка, номер пакета, число отсчётов, сами отсчёты — 16 бит каждый.
typedef struct
{
    uint16_t marker;
    uint16_t number;
    uint16_t samples;
    uint16_t data[STREAM_SAMPLES];
} stream_packet_t;

// Функция, которой задача отдаёт собранный пакет.
typedef void (*stream_task_callback_t)(const stream_packet_t *packet, uint16_t len);

void stream_task_init(stream_task_callback_t callback);
bool stream_task_start(void);
void stream_task_stop(uint32_t *packets, uint32_t *samples, uint32_t *overruns);
void stream_task_handle(void);
