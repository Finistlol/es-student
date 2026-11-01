#include "platform/platform.h"

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "pico/stdio.h"
#include "pico/time.h"

static const uint32_t LED_PIN = 25;
static const uint32_t BUTTON_PINS[PLATFORM_BUTTON_COUNT] = {15};
static const uint32_t VOLTAGE_PIN = 26;
static const uint32_t CHARGE_PIN = 22;

// Канал 4 — встроенный датчик температуры кристалла.
#define ADC_CHANNEL_VOLTAGE 0
#define ADC_CHANNEL_TEMP 4

// АЦП 12-разрядный: 4096 уровней от нуля до опорного напряжения.
#define ADC_LEVELS 4096.0f
#define ADC_REF_V 3.3f

// Частота отсчётов потока и блока: 44,1 кГц из 48 МГц такта АЦП.
#define ADC_SAMPLE_HZ 44100.0f

// Вызывается из platform_init: разбирает причину прошлого сброса
// и восстанавливает счётчик перезапусков из scratch-регистра.
static void restart_reason_init(void);

void platform_init(void)
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    for (uint32_t i = 0; i < PLATFORM_BUTTON_COUNT; i++)
    {
        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[i]);
    }

    adc_init();
    adc_gpio_init(VOLTAGE_PIN);
    adc_set_temp_sensor_enabled(true);

    gpio_init(CHARGE_PIN);
    gpio_set_dir(CHARGE_PIN, GPIO_OUT);
    gpio_put(CHARGE_PIN, false);

    restart_reason_init();
}

void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(platform_button_t button)
{
    // Кнопка замыкает вывод на землю, вывод подтянут к питанию:
    // нажатой кнопке соответствует низкий уровень.
    return !gpio_get(BUTTON_PINS[button]);
}

float platform_get_voltage_v(void)
{
    adc_select_input(ADC_CHANNEL_VOLTAGE);
    uint16_t code = adc_read();
    return (float)code * ADC_REF_V / ADC_LEVELS;
}

float platform_get_temp_c(void)
{
    // Датчик температуры подключён к каналу 4, RP2040 Datasheet, 4.9.2.1.
    adc_select_input(ADC_CHANNEL_TEMP);
    uint16_t code = adc_read();
    float sensor_v = (float)code * ADC_REF_V / ADC_LEVELS;
    // T = 27 - (U - 0.706) / 0.001721, RP2040 Datasheet, раздел 4.9.5.
    return 27.0f - (sensor_v - 0.706f) / 0.001721f;
}

void platform_charge_set(bool on)
{
    gpio_put(CHARGE_PIN, on);
}

// --- Мигание по будильнику ---

// Обработчик будильника: короткий, без печати и ожиданий.
// Счётчик читает команда из суперцикла, поэтому volatile.
static volatile uint32_t blink_toggles = 0;
static repeating_timer_t blink_timer;
static bool blink_running = false;

static bool blink_handler(repeating_timer_t *timer)
{
    (void)timer;
    static bool level = false;
    level = !level;
    gpio_put(LED_PIN, level);
    blink_toggles = blink_toggles + 1;
    return true;
}

bool platform_timer_blink_start(uint32_t period_ms)
{
    if (blink_running)
    {
        return false;
    }

    blink_toggles = 0;
    if (!add_repeating_timer_ms(period_ms, blink_handler, NULL, &blink_timer))
    {
        return false;
    }

    blink_running = true;
    return true;
}

void platform_timer_blink_stop(void)
{
    cancel_repeating_timer(&blink_timer);
    blink_running = false;
}

uint32_t platform_timer_blink_toggles(void)
{
    return blink_toggles;
}

// --- 3.2.2: захват отсчётов по будильнику ---

static uint16_t capture_codes[PLATFORM_CAPTURE_MAX];
static uint32_t capture_times[PLATFORM_CAPTURE_MAX];
static volatile uint32_t capture_count = 0;
static uint32_t capture_total = 0;
static uint32_t capture_period_us = 0;
static uint32_t capture_handler_us = 0;
static repeating_timer_t capture_timer;
static volatile bool capture_running = false;

// Отсчёт снимается в момент срабатывания будильника. Отрицательный период
// ставит вызовы на ровную сетку: каждый следующий отсчитывается от
// заявленного времени, а не от фактического завершения прошлого.
static bool capture_handler(repeating_timer_t *timer)
{
    (void)timer;
    uint32_t entered_us = time_us_32();

    capture_times[capture_count] = entered_us;
    capture_codes[capture_count] = adc_read();
    capture_count = capture_count + 1;

    capture_handler_us += time_us_32() - entered_us;

    if (capture_count >= capture_total)
    {
        capture_running = false;
        return false;
    }

    return true;
}

bool platform_capture_start(uint32_t count, uint32_t period_us)
{
    if (count == 0 || count > PLATFORM_CAPTURE_MAX || period_us < 10)
    {
        return false;
    }

    capture_total = count;
    capture_count = 0;
    capture_period_us = period_us;
    capture_handler_us = 0;

    adc_select_input(ADC_CHANNEL_VOLTAGE);

    if (!add_repeating_timer_us(-(int64_t)period_us, capture_handler, NULL, &capture_timer))
    {
        return false;
    }

    capture_running = true;
    return true;
}

void platform_capture_get(platform_capture_info_t *info)
{
    info->count = capture_count;
    info->period_us = capture_period_us;
    info->codes = capture_codes;

    if (capture_count < 2)
    {
        info->interval_min_us = 0;
        info->interval_max_us = 0;
        info->handler_pct = 0.0f;
        return;
    }

    uint32_t interval_min = capture_times[1] - capture_times[0];
    uint32_t interval_max = interval_min;
    for (uint32_t i = 2; i < capture_count; i++)
    {
        uint32_t interval = capture_times[i] - capture_times[i - 1];
        if (interval < interval_min)
        {
            interval_min = interval;
        }
        if (interval > interval_max)
        {
            interval_max = interval;
        }
    }

    uint32_t elapsed_us = capture_times[capture_count - 1] - capture_times[0];
    info->interval_min_us = interval_min;
    info->interval_max_us = interval_max;
    info->handler_pct = elapsed_us > 0
                            ? (float)capture_handler_us * 100.0f / (float)elapsed_us
                            : 0.0f;
}

// --- 3.2.3: захват блока через DMA ---

static uint16_t dma_block[8192];
static volatile bool dma_block_done = false;
static uint32_t dma_block_samples = 0;
static uint32_t dma_block_started_us = 0;
static uint32_t dma_block_elapsed_us = 0;
static int dma_block_channel = -1;

// О конце захвата сообщает прерывание: прошивка нигде не ждёт.
static void __isr dma_block_handler(void)
{
    dma_hw->ints0 = 1u << dma_block_channel;
    dma_block_done = true;
    dma_block_elapsed_us = time_us_32() - dma_block_started_us;
    adc_run(false);
    adc_fifo_drain();
}

bool platform_dma_capture_start(uint32_t samples)
{
    if (samples == 0 || samples > 8192)
    {
        return false;
    }

    if (dma_block_channel < 0)
    {
        dma_block_channel = (int)dma_claim_unused_channel(false);
        irq_set_exclusive_handler(DMA_IRQ_0, dma_block_handler);
        irq_set_enabled(DMA_IRQ_0, true);
    }

    dma_channel_config config = dma_channel_get_default_config(dma_block_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, DREQ_ADC);

    dma_block_samples = samples;
    dma_block_done = false;
    dma_block_started_us = time_us_32();

    adc_fifo_setup(true, true, 1, false, false);
    // Шаг отсчётов держит делитель АЦП: 48 МГц / 44100 - 1.
    adc_set_clkdiv(48000000.0f / ADC_SAMPLE_HZ - 1.0f);

    dma_channel_configure(dma_block_channel, &config,
                          dma_block, &adc_hw->fifo, samples, false);

    dma_channel_set_irq0_enabled(dma_block_channel, true);
    dma_channel_acknowledge_irq0(dma_block_channel);

    adc_fifo_drain();
    dma_channel_start(dma_block_channel);
    adc_run(true);
    return true;
}

bool platform_dma_capture_done(void)
{
    return dma_block_done;
}

void platform_dma_capture_result(uint32_t *samples, uint32_t *elapsed_us, const uint16_t **codes)
{
    *samples = dma_block_samples;
    *elapsed_us = dma_block_elapsed_us;
    *codes = dma_block;
}

// --- 3.2.4: поток с двойной буферизацией ---

static uint16_t stream_buf[2][PLATFORM_STREAM_SAMPLES];
static volatile bool stream_ready[2] = {false, false};
static volatile uint32_t stream_overruns = 0;
static int stream_channels[2] = {-1, -1};
static bool stream_running = false;

// Оба канала сидят на одной линии DMA_IRQ_1: обработчик разбирает,
// какой канал закончил, и возвращает ему начало его буфера —
// канал перезапустится по цепочке от второго канала.
static void __isr stream_handler(void)
{
    uint32_t bits = dma_hw->intr;

    if (bits & (1u << stream_channels[0]))
    {
        dma_hw->ints1 = 1u << stream_channels[0];
        if (stream_ready[0])
        {
            stream_overruns = stream_overruns + 1;
        }
        stream_ready[0] = true;
        dma_channel_set_write_addr(stream_channels[0], stream_buf[0], false);
    }

    if (bits & (1u << stream_channels[1]))
    {
        dma_hw->ints1 = 1u << stream_channels[1];
        if (stream_ready[1])
        {
            stream_overruns = stream_overruns + 1;
        }
        stream_ready[1] = true;
        dma_channel_set_write_addr(stream_channels[1], stream_buf[1], false);
    }
}

bool platform_stream_start(void)
{
    if (stream_running)
    {
        return false;
    }

    if (stream_channels[0] < 0)
    {
        stream_channels[0] = (int)dma_claim_unused_channel(false);
        stream_channels[1] = (int)dma_claim_unused_channel(false);

        irq_set_exclusive_handler(DMA_IRQ_1, stream_handler);
        irq_set_enabled(DMA_IRQ_1, true);
    }

    stream_ready[0] = false;
    stream_ready[1] = false;
    stream_overruns = 0;

    // Оба канала по запросу АЦП, каждый передаёт запрос другому по цепочке.
    dma_channel_config config_a = dma_channel_get_default_config(stream_channels[0]);
    channel_config_set_transfer_data_size(&config_a, DMA_SIZE_16);
    channel_config_set_read_increment(&config_a, false);
    channel_config_set_write_increment(&config_a, true);
    channel_config_set_dreq(&config_a, DREQ_ADC);
    channel_config_set_chain_to(&config_a, stream_channels[1]);

    dma_channel_config config_b = dma_channel_get_default_config(stream_channels[1]);
    channel_config_set_transfer_data_size(&config_b, DMA_SIZE_16);
    channel_config_set_read_increment(&config_b, false);
    channel_config_set_write_increment(&config_b, true);
    channel_config_set_dreq(&config_b, DREQ_ADC);
    channel_config_set_chain_to(&config_b, stream_channels[0]);

    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(48000000.0f / ADC_SAMPLE_HZ - 1.0f);

    dma_channel_configure(stream_channels[0], &config_a,
                          stream_buf[0], &adc_hw->fifo, PLATFORM_STREAM_SAMPLES, false);
    dma_channel_configure(stream_channels[1], &config_b,
                          stream_buf[1], &adc_hw->fifo, PLATFORM_STREAM_SAMPLES, false);

    dma_channel_set_irq1_enabled(stream_channels[0], true);
    dma_channel_set_irq1_enabled(stream_channels[1], true);
    dma_channel_acknowledge_irq1(stream_channels[0]);
    dma_channel_acknowledge_irq1(stream_channels[1]);

    adc_fifo_drain();
    dma_channel_start(stream_channels[0]);
    adc_run(true);

    stream_running = true;
    return true;
}

void platform_stream_stop(void)
{
    if (!stream_running)
    {
        return;
    }

    dma_channel_abort(stream_channels[0]);
    dma_channel_abort(stream_channels[1]);
    adc_run(false);
    adc_fifo_drain();
    dma_channel_cleanup(stream_channels[0]);
    dma_channel_cleanup(stream_channels[1]);
    stream_running = false;
}

bool platform_stream_take(const uint16_t **samples)
{
    for (uint32_t i = 0; i < 2; i++)
    {
        if (stream_ready[i])
        {
            *samples = stream_buf[i];
            stream_ready[i] = false;
            return true;
        }
    }
    return false;
}

uint32_t platform_stream_overruns(void)
{
    return stream_overruns;
}

void platform_send_bytes(const uint8_t *data, uint16_t len)
{
    // Двоичные данные уходят без замены байта 0A парой 0D 0A.
    stdio_put_string((const char *)data, len, false, false);
}

// --- 3.2.7: сторожевой таймер ---

// SDK занимает scratch[4..7], счётчик перезапусков живёт в scratch[0].
static uint32_t restarts = 0;
static bool wdt_on = false;
static bool wdt_cause = false;

bool platform_watchdog_enable(uint32_t timeout_ms)
{
    if (timeout_ms < 1 || timeout_ms > 8388)
    {
        return false;
    }

    watchdog_enable(timeout_ms, true);
    wdt_on = true;
    return true;
}

void platform_watchdog_update(void)
{
    if (wdt_on)
    {
        watchdog_update();
    }
}

bool platform_restarted_by_watchdog(void)
{
    return wdt_cause;
}

uint32_t platform_restarts(void)
{
    return restarts;
}

static void restart_reason_init(void)
{
    wdt_cause = watchdog_enable_caused_reboot();

    if (wdt_cause)
    {
        restarts = watchdog_hw->scratch[0] + 1;
    }
    else
    {
        restarts = 0;
    }

    watchdog_hw->scratch[0] = restarts;
}