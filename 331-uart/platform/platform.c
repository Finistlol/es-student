#include "platform/platform.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"

#define LED_PIN 25
#define BUTTON_PIN 15
#define LINE_PIN 22
#define NODE_ADC_PIN 26
#define NODE_ADC_CHANNEL 0

// АЦП 12-разрядный: 4096 уровней от нуля до опорного напряжения.
#define ADC_LEVELS 4096.0f
#define ADC_REF_V 3.3f

static platform_drive_mode_t line_mode = PLATFORM_DRIVE_PP;

static void platform_uart_setup(void);

void platform_init(void)
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // Линия GP22: драйвер RC-цепи.
    gpio_init(LINE_PIN);
    gpio_set_dir(LINE_PIN, GPIO_OUT);
    gpio_put(LINE_PIN, false);

    // Узел RC-цепи: вход АЦП на GP26.
    adc_init();
    adc_gpio_init(NODE_ADC_PIN);
    adc_select_input(NODE_ADC_CHANNEL);

    platform_uart_setup();
    platform_uart1_setup();
}

void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(platform_button_t button)
{
    (void)button;
    return !gpio_get(BUTTON_PIN);
}

void platform_line_mode(platform_drive_mode_t mode)
{
    line_mode = mode;

    if (mode == PLATFORM_DRIVE_OD)
    {
        // Открытый сток из двухтактного драйвера: в регистре выхода ноль,
        // подтяжка вверх включена, направление переключается.
        gpio_put(LINE_PIN, false);
        gpio_pull_up(LINE_PIN);
        gpio_set_dir(LINE_PIN, GPIO_IN); // отпущено = подтяжка тянет вверх
    }
    else
    {
        // Двухтактный: обычный выход.
        gpio_disable_pulls(LINE_PIN);
        gpio_set_dir(LINE_PIN, GPIO_OUT);
        gpio_put(LINE_PIN, false);
    }
}

void platform_line_set(bool high)
{
    if (line_mode == PLATFORM_DRIVE_PP)
    {
        gpio_put(LINE_PIN, high);
    }
    else
    {
        // Открытый сток: high = отпустить (вход, подтяжка тянет вверх),
        // low = притянуть к земле (выход с нулём в регистре).
        if (high)
        {
            gpio_set_dir(LINE_PIN, GPIO_IN);
        }
        else
        {
            gpio_set_dir(LINE_PIN, GPIO_OUT); // в регистре уже ноль
        }
    }
}

float platform_node_voltage_v(void)
{
    adc_select_input(NODE_ADC_CHANNEL);
    uint16_t code = adc_read();
    return (float)code * ADC_REF_V / ADC_LEVELS;
}
// --- UART0 ---

#include "hardware/uart.h"
#include <string.h>

#define UART_ID uart0
#define UART_BAUDRATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

static void platform_uart_setup(void)
{
    uart_init(uart0, 115200);
    // По умолчанию — внутренняя петля, выводы модулю не отданы.
    platform_uart_loop_internal(true);
}

void platform_uart_loop_internal(bool enable)
{
    if (enable)
    {
        // Петля внутри модуля: выводы не нужны модулю.
        gpio_set_function(UART_TX_PIN, GPIO_FUNC_NULL);
        gpio_set_function(UART_RX_PIN, GPIO_FUNC_NULL);
        hw_set_bits(&uart_get_hw(UART_ID)->cr, UART_UARTCR_LBE_BITS);
    }
    else
    {
        // Петля через выводы: GP0 и GP1 отданы модулю UART.
        hw_clear_bits(&uart_get_hw(UART_ID)->cr, UART_UARTCR_LBE_BITS);
        gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    }

    // Смена режима оставляет в очередях приёма мусор от переходного
    // процесса на линии: очереди сбрасываются.
    uart_get_hw(UART_ID)->cr &= ~UART_UARTCR_UARTEN_BITS;
    uart_get_hw(UART_ID)->lcr_h &= ~UART_UARTLCR_H_FEN_BITS;
    uart_get_hw(UART_ID)->lcr_h |= UART_UARTLCR_H_FEN_BITS;
    uart_get_hw(UART_ID)->cr |= UART_UARTCR_UARTEN_BITS;
    // Признаки ошибок от переходного процесса тоже сбрасываются.
    uart_get_hw(UART_ID)->rsr = 0;
}

bool platform_uart_writable(void)
{
    return uart_is_writable(UART_ID);
}

bool platform_uart_putc(char c)
{
    if (!uart_is_writable(UART_ID))
    {
        return false;
    }
    uart_get_hw(UART_ID)->dr = (uint8_t)c;
    return true;
}

int platform_uart_getc(void)
{
    if (!uart_is_readable(UART_ID))
    {
        return -1;
    }
    return (int)(uart_get_hw(UART_ID)->dr & 0xFF);
}

uint32_t platform_uart_errors(void)
{
    // UARTRSR и UARTECR — один адрес: чтение даёт признаки,
    // запись сбрасывает их.
    uint32_t rsr = uart_get_hw(UART_ID)->rsr;
    uart_get_hw(UART_ID)->rsr = 0;
    return rsr;
}

// --- UART1: второй модуль на GP8 (TX) и GP9 (RX) ---

#define UART1_ID uart1
#define UART1_TX_PIN 8
#define UART1_RX_PIN 9

void platform_uart1_setup(void)
{
    uart_init(uart1, 115200);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
}

uint32_t platform_uart1_baud_shift(int32_t percent)
{
    if (percent < -50 || percent > 50)
    {
        return 0;
    }
    uint32_t target = (uint32_t)(115200UL * (100L + percent) / 100L);
    return uart_set_baudrate(uart1, target);
}

void platform_uart0_parity(const char *mode)
{
    uart_parity_t p = UART_PARITY_NONE;
    if (strcmp(mode, "even") == 0) p = UART_PARITY_EVEN;
    else if (strcmp(mode, "odd") == 0) p = UART_PARITY_ODD;
    uart_set_format(uart0, 8, 1, p);
}

void platform_uart1_parity(const char *mode)
{
    uart_parity_t p = UART_PARITY_NONE;
    if (strcmp(mode, "even") == 0) p = UART_PARITY_EVEN;
    else if (strcmp(mode, "odd") == 0) p = UART_PARITY_ODD;
    uart_set_format(uart1, 8, 1, p);
}

bool platform_uart1_writable(void)
{
    return uart_is_writable(uart1);
}

bool platform_uart1_putc(char c)
{
    if (!uart_is_writable(uart1))
    {
        return false;
    }
    uart_get_hw(uart1)->dr = (uint8_t)c;
    return true;
}

int platform_uart1_getc(void)
{
    if (!uart_is_readable(uart1))
    {
        return -1;
    }
    return (int)(uart_get_hw(uart1)->dr & 0xFF);
}

uint32_t platform_uart1_errors(void)
{
    uint32_t rsr = uart_get_hw(uart1)->rsr;
    uart_get_hw(uart1)->rsr = 0;
    return rsr;
}

void platform_uart1_flush_rx(void)
{
    while (uart_is_readable(uart1))
    {
        (void)uart_get_hw(uart1)->dr;
    }
    uart_get_hw(uart1)->rsr = 0;
}

// --- Блочный приём на UART0 с прерыванием (3.3.4) ---

volatile uint8_t platform_block_count = 0;
static uint8_t block_buffer[PLATFORM_BLOCK_SIZE];

// Обработчик прерывания приёма UART0: забирает байты из очереди
// в буфер, пока передача идёт. Без печати и ожиданий.
static void __isr uart0_rx_handler(void)
{
    uart_get_hw(uart0)->icr = UART_UARTICR_RXIC_BITS;
    while (uart_is_readable(uart0) && platform_block_count < PLATFORM_BLOCK_SIZE)
    {
        block_buffer[platform_block_count] = (uint8_t)(uart_get_hw(uart0)->dr & 0xFF);
        platform_block_count = platform_block_count + 1;
    }
}

void platform_uart0_irq_enable(bool enable)
{
    if (enable)
    {
        irq_set_exclusive_handler(UART0_IRQ, uart0_rx_handler);
        uart_get_hw(uart0)->imsc |= UART_UARTIMSC_RXIM_BITS;
        irq_set_enabled(UART0_IRQ, true);
    }
    else
    {
        uart_get_hw(uart0)->imsc &= ~UART_UARTIMSC_RXIM_BITS;
        irq_set_enabled(UART0_IRQ, false);
    }
}

const uint8_t *platform_block_data(void)
{
    return block_buffer;
}

void platform_uart0_drain_rx(void)
{
    while (uart_is_readable(uart0))
    {
        (void)uart_get_hw(uart0)->dr;
    }
    uart_get_hw(uart0)->rsr = 0;
}

// --- 3.3.5: DMA-захват на пределе АЦП ---

#include "hardware/dma.h"

static uint16_t scope_buf[SCOPE_BLOCK_SIZE];
static volatile bool scope_done = false;
static uint32_t scope_samples = 0;
static int scope_channel = -1;

static void __isr scope_dma_handler(void)
{
    dma_hw->ints0 = 1u << scope_channel;
    scope_done = true;
    adc_run(false);
    adc_fifo_drain();
}

bool platform_scope_capture(uint32_t samples)
{
    if (scope_channel < 0)
    {
        scope_channel = (int)dma_claim_unused_channel(false);
        irq_set_exclusive_handler(DMA_IRQ_0, scope_dma_handler);
        irq_set_enabled(DMA_IRQ_0, true);
    }

    if (samples == 0 || samples > SCOPE_BLOCK_SIZE)
    {
        return false;
    }

    scope_samples = samples;
    scope_done = false;

    // АЦП на пределе: делитель 0, преобразования идут подряд.
    adc_select_input(NODE_ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(0);

    dma_channel_config config = dma_channel_get_default_config(scope_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, DREQ_ADC);

    dma_channel_configure(scope_channel, &config,
                          scope_buf, &adc_hw->fifo, samples, false);
    dma_channel_set_irq0_enabled(scope_channel, true);
    dma_channel_acknowledge_irq0(scope_channel);

    adc_fifo_drain();
    dma_channel_start(scope_channel);
    adc_run(true);
    return true;
}

bool platform_scope_ready(void)
{
    return scope_done;
}

void platform_scope_result(uint32_t *samples, const uint16_t **codes)
{
    *samples = scope_samples;
    *codes = scope_buf;
}

void platform_scope_set_uart_baud(uint32_t baud)
{
    uart_set_baudrate(uart0, baud);
    uart_set_baudrate(uart1, baud);
}
