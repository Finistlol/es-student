#pragma once

#include <stdbool.h>
#include <stdint.h>

// Результат осциллограммы: заголовок и отсчёты АЦП.
typedef struct
{
    uint32_t samples;
    uint32_t rate_hz;
    uint32_t baud;
    const char *text;
    uint8_t text_len;
    uint32_t uart_errors;
    const uint16_t *codes;
} scope_result_t;

typedef void (*block_task_callback_t)(const scope_result_t *result);

void block_task_init(block_task_callback_t callback);
bool block_task_start(uint32_t baud, const char *text, uint8_t len);
void block_task_handle(void);