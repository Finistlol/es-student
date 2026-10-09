#pragma once

#include <stdbool.h>
#include <stdint.h>

// Профилирование выключается сборкой: строка в CMakeLists.txt проекта
// задаёт PROFILING_ENABLED=0, и вызовы секундомеров уходят из образа.
#ifndef PROFILING_ENABLED
#define PROFILING_ENABLED 1
#endif

#define PROFILING_MAX_STOPWATCHES 8

// Результаты одного секундомера с момента сброса.
typedef struct
{
    const char *name;
    uint32_t count;
    float mean_us;
    uint32_t max_us;
} profiling_result_t;

#if PROFILING_ENABLED

// Заводит секундомер с именем, под которым он попадёт в отчёт.
void profiling_stopwatch_init(uint8_t id, const char *name);

// Замеряет участок между вызовами start и stop.
void profiling_start(uint8_t id);
void profiling_stop(uint8_t id);

// Отдаёт результаты секундомера; false — секундомер не заведён.
bool profiling_get(uint8_t id, profiling_result_t *result);

// Сбрасывает результаты всех заведённых секундомеров.
void profiling_reset(void);

#else

// В выключенной сборке вызовы секундомеров уходят из образа целиком.
#define profiling_stopwatch_init(id, name) ((void)0)
#define profiling_start(id) ((void)0)
#define profiling_stop(id) ((void)0)
#define profiling_get(id, result) ((void)(result), false)
#define profiling_reset() ((void)0)

#endif