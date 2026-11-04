#pragma once

#include <stdbool.h>
#include <stdint.h>

// Кнопки платы. Сколько их и каким выводом каждая сидит, знает platform.
typedef enum
{
    PLATFORM_BUTTON_1,
    PLATFORM_BUTTON_COUNT
} platform_button_t;

void platform_init(void);
void platform_led_set(bool on);
bool platform_button_read(platform_button_t button);
float platform_get_voltage_v(void);
float platform_get_temp_c(void);
void platform_charge_set(bool on);

// Мигание светодиода по будильнику SDK. period_ms от 10 до 60000.
// Возвращает false, если мигание уже идёт или период неверен.
bool platform_timer_blink_start(uint32_t period_ms);
void platform_timer_blink_stop(void);
uint32_t platform_timer_blink_toggles(void);

// --- 3.2.2: захват отсчётов по будильнику ---

#define PLATFORM_CAPTURE_MAX 2000

typedef struct
{
    uint32_t count;
    uint32_t period_us;
    uint32_t interval_min_us;
    uint32_t interval_max_us;
    float handler_pct;
    const uint16_t *codes;
} platform_capture_info_t;

bool platform_capture_start(uint32_t count, uint32_t period_us);
void platform_capture_get(platform_capture_info_t *info);

// --- 3.2.3: захват блока через DMA ---

bool platform_dma_capture_start(uint32_t samples);
bool platform_dma_capture_done(void);
void platform_dma_capture_result(uint32_t *samples, uint32_t *elapsed_us, const uint16_t **codes);

// --- 3.2.4: поток с двойной буферизацией ---

#define PLATFORM_STREAM_SAMPLES_16 512
#define PLATFORM_STREAM_SAMPLES_8 1024

// eight_bit: false — поток 16 бит по 512 отсчётов, true — 8 бит по 1024.
bool platform_stream_start(bool eight_bit);
void platform_stream_stop(void);
bool platform_stream_take(const uint8_t **bytes, uint16_t *samples);
uint8_t platform_stream_bits(void);
uint32_t platform_stream_overruns(void);
void platform_send_bytes(const uint8_t *data, uint16_t len);

// Тон на GP20: меандр частоты freq_hz от модуля ШИМ, ядро в нём не участвует;
// 0 — тон выключен, на выводе снова ноль; false — частота вне 20..20000 Гц.
bool platform_tone_set(uint32_t freq_hz);

// --- 3.2.7: сторожевой таймер ---

bool platform_watchdog_enable(uint32_t timeout_ms);
void platform_watchdog_update(void);
bool platform_restarted_by_watchdog(void);
uint32_t platform_restarts(void);
