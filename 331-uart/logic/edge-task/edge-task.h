#pragma once

#include <stdbool.h>
#include <stdint.h>

// Результат измерения фронтов RC-цепи.
typedef struct
{
    bool open_drain; // true = od, false = pp
    float rise_ms;   // время роста узла до 2,0 В
    float fall_ms;   // время спада узла до 1,3 В
} edge_result_t;

// Функция, которой задача отдаёт результат измерения.
typedef void (*edge_task_callback_t)(const edge_result_t *result);

void edge_task_init(edge_task_callback_t callback);
bool edge_task_measure(bool open_drain);
void edge_task_handle(void);