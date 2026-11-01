#include "pi-task/pi-task.h"

#include <stdbool.h>

#include "systime/systime.h"

// Порция членов ряда за один вызов обработчика. Подобрана так, чтобы вызов
// укладывался в требование Н1: одна порция занимает меньше миллисекунды.
#define PI_TERMS_PER_CALL 150

static pi_task_state_t state = PI_TASK_NOT_STARTED;
static uint32_t terms_total = 0;
static uint32_t terms_done = 0;
static uint32_t term_index = 0;
static double sum = 0.0;
static double value = 0.0;
static uint64_t started_us = 0;
static uint64_t time_ms = 0;

bool pi_task_start(uint32_t terms)
{
    if (state == PI_TASK_RUNNING)
    {
        return false;
    }

    state = PI_TASK_RUNNING;
    terms_total = terms;
    terms_done = 0;
    term_index = 0;
    sum = 0.0;
    started_us = systime_us();
    return true;
}

void pi_task_handle(void)
{
    if (state != PI_TASK_RUNNING)
    {
        return;
    }

    uint32_t portion = terms_total - terms_done;
    if (portion > PI_TERMS_PER_CALL)
    {
        portion = PI_TERMS_PER_CALL;
    }

    for (uint32_t i = 0; i < portion; i++)
    {
        double term = term_index % 2 == 0 ? 1.0 : -1.0;
        sum += term / (2.0 * (double)term_index + 1.0);
        term_index = term_index + 1;
    }

    terms_done = terms_done + portion;

    if (terms_done >= terms_total)
    {
        value = sum * 4.0;
        time_ms = (systime_us() - started_us) / 1000;
        state = PI_TASK_DONE;
    }
}

void pi_task_get_result(pi_task_result_t *result)
{
    result->state = state;
    result->value = value;
    result->time_ms = time_ms;
}