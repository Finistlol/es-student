#pragma once

#include <stdint.h>

#include "api/api.h"
#include "block-task/block-task.h"

typedef void (*api_callback_t)(const command_t *command);

// Запись таблицы команд прибора: имя, справка для help и обработчик.
typedef struct
{
    const char *name;
    const char *help;
    api_callback_t callback;
} api_command_t;

extern const api_command_t api_commands[];
extern const uint32_t api_command_count;
// Отчёт о готовом блоке DMA: печатает ответ команды dma_cap.
void api_report_block(const block_result_t *result);
