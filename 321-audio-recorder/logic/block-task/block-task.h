#pragma once

#include <stdbool.h>
#include <stdint.h>

// Итог захвата блока: сколько отсчётов, за какое время по факту и по расчёту,
// и статистика кодов АЦП.
typedef struct
{
    uint32_t samples;
    uint32_t elapsed_us;
    uint32_t expected_us;
    uint32_t min;
    uint32_t max;
    float mean;
    const uint16_t *codes;
} block_result_t;

// Функция, которой задача отдаёт готовый блок.
typedef void (*block_task_callback_t)(const block_result_t *result);

void block_task_init(block_task_callback_t callback);
bool block_task_start(uint32_t samples);
bool block_task_start_cb(uint32_t samples, block_task_callback_t callback);
void block_task_handle(void);
