#include "edge-task/edge-task.h"

#include <stdbool.h>

#include "platform/platform.h"
#include "systime/systime.h"

// Пороги измерения фронтов, вольты.
#define EDGE_RISE_V 2.0f
#define EDGE_FALL_V 1.3f
// Предельное время измерения.
#define EDGE_TIMEOUT_US 2000000

typedef enum
{
    EDGE_STATE_IDLE,
    EDGE_STATE_DISCHARGE, // подготовка: разрядить узел до нуля
    EDGE_STATE_RISING,    // заряд до 2 В — фронт вверх
    EDGE_STATE_CHARGING,  // дозаряд до 3 В перед измерением спада
    EDGE_STATE_FALLING,   // разряд до 1,3 В — фронт вниз
    EDGE_STATE_DONE,
} edge_state_t;

static edge_state_t state = EDGE_STATE_IDLE;
static uint64_t started_us = 0;
static edge_result_t result;
static edge_task_callback_t report = 0;

void edge_task_init(edge_task_callback_t callback)
{
    report = callback;
}

// Порог дозаряда перед измерением спада: близко к 3,3 В,
// чтобы фронт вниз шёл от полного заряда, как от полного разряда вверх.
#define EDGE_CHARGE_V 3.0f

bool edge_task_measure(bool open_drain)
{
    if (state != EDGE_STATE_IDLE)
    {
        return false;
    }

    result.open_drain = open_drain;
    platform_line_mode(open_drain ? PLATFORM_DRIVE_OD : PLATFORM_DRIVE_PP);
    platform_line_set(false);
    started_us = systime_us();
    state = EDGE_STATE_DISCHARGE;
    return true;
}

void edge_task_handle(void)
{
    if (state == EDGE_STATE_IDLE || state == EDGE_STATE_DONE)
    {
        return;
    }

    uint64_t now_us = systime_us();
    float voltage = platform_node_voltage_v();

    if (now_us - started_us > EDGE_TIMEOUT_US)
    {
        result.rise_ms = -1.0f;
        result.fall_ms = -1.0f;
        state = EDGE_STATE_DONE;
    }
    else if (state == EDGE_STATE_DISCHARGE)
    {
        // Ждём разряда узла до уровня шума перед измерением роста.
        if (voltage < 0.1f)
        {
            started_us = now_us;
            state = EDGE_STATE_RISING;
        }
    }
    else if (state == EDGE_STATE_RISING)
    {
        platform_line_set(true);
        if (voltage >= EDGE_RISE_V)
        {
            result.rise_ms = (float)(now_us - started_us) / 1000.0f;
            started_us = now_us;
            state = EDGE_STATE_CHARGING;
        }
    }
    else if (state == EDGE_STATE_CHARGING)
    {
        // Продолжаем заряд до 3 В, чтобы спад шёл от полного заряда.
        platform_line_set(true);
        if (voltage >= EDGE_CHARGE_V)
        {
            platform_line_set(false);
            started_us = now_us;
            state = EDGE_STATE_FALLING;
        }
    }
    else if (state == EDGE_STATE_FALLING)
    {
        if (voltage <= EDGE_FALL_V)
        {
            result.fall_ms = (float)(now_us - started_us) / 1000.0f;
            state = EDGE_STATE_DONE;
        }
    }

    if (state == EDGE_STATE_DONE && report != 0)
    {
        report(&result);
        state = EDGE_STATE_IDLE;
    }
}