#pragma once

#include <stdbool.h>
#include <stdint.h>

// Запись телеметрии: время с момента сброса, напряжение и температура.
typedef struct
{
    uint64_t us;
    float voltage_v;
    float temp_c;
} telemetry_sample_t;

// Функция, которой задача отдаёт каждую запись.
typedef void (*telemetry_task_callback_t)(const telemetry_sample_t *sample);

void telemetry_task_init(telemetry_task_callback_t callback);
void telemetry_task_handle(void);

// Запускает поток с периодом period_ms; false — период слишком мал.
bool telemetry_task_start(uint32_t period_ms);
void telemetry_task_stop(void);
