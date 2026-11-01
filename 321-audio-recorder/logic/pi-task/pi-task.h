#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PI_TASK_NOT_STARTED,
    PI_TASK_RUNNING,
    PI_TASK_DONE,
} pi_task_state_t;

typedef struct
{
    pi_task_state_t state;
    double value;
    uint64_t time_ms;
} pi_task_result_t;

// Запускает расчёт; false — предыдущий расчёт ещё идёт.
bool pi_task_start(uint32_t terms);

// Считает очередную порцию ряда и возвращается: суперцикл не блокируется.
void pi_task_handle(void);

void pi_task_get_result(pi_task_result_t *result);