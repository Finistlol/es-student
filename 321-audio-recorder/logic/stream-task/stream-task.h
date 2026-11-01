#pragma once

#include <stdbool.h>
#include <stdint.h>

// Метка пакета потока: байты a5 5a, в памяти младшим байтом вперёд.
#define STREAM_MARKER 0x5aa5
#define STREAM_SAMPLES_16 512
#define STREAM_SAMPLES_8 1024

// Пакет: метка, номер пакета, число отсчётов — по два байта младшим
// вперёд, дальше сами отсчёты: байт в 8-битном потоке, полуслово в 16-битном.
#define STREAM_HEADER_BYTES 6
#define STREAM_PACKET_MAX (STREAM_HEADER_BYTES + STREAM_SAMPLES_8)

// Функция, которой задача отдаёт собранный пакет байтами как есть.
typedef void (*stream_task_callback_t)(const uint8_t *packet, uint16_t len);

void stream_task_init(stream_task_callback_t callback);
bool stream_task_start(bool eight_bit);
void stream_task_stop(uint32_t *packets, uint32_t *samples, uint32_t *overruns);
void stream_task_handle(void);
