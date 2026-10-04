#include "clock.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "log.h"

// Пониженная частота ядра: половина штатной 125 МГц.
const uint32_t CLK_SYS_LOW_KHZ = 62500;

static void clk_row(const char *name, uint32_t set_khz, uint32_t measured_khz)
{
    printf("%-10s %8u %11u\n", name, (unsigned)set_khz, (unsigned)measured_khz);
}

void clk_info(void)
{
    printf("signal     set_khz measured_khz\n");
    clk_row("clk_ref", clock_get_hz(clk_ref) / 1000,
            frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_REF));
    clk_row("clk_sys", clock_get_hz(clk_sys) / 1000,
            frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS));
    clk_row("clk_peri", clock_get_hz(clk_peri) / 1000,
            frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI));
    clk_row("clk_usb", clock_get_hz(clk_usb) / 1000,
            frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB));
    clk_row("clk_adc", clock_get_hz(clk_adc) / 1000,
            frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC));
    printf("%-10s %8s %11u\n", "rosc", "-",
           frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC));
}

void uptime(void)
{
    printf("uptime: %llu ms\n", time_us_64() / 1000);
}

// set_sys_clock_khz() подбирает множители PLL под нужную частоту и переводит
// clk_peri на PLL_USB, чтобы UART и прочая периферия продолжила работать.
static void clk_sys_set(uint32_t khz)
{
    if (set_sys_clock_khz(khz, false))
    {
        LOG_INF("clk_sys %u kHz\n", (unsigned)khz);
    }
    else
    {
        LOG_ERR("clk_sys %u kHz is not set\n", (unsigned)khz);
    }
}

void clk_sys_low(void)
{
    clk_sys_set(CLK_SYS_LOW_KHZ);
}

void clk_sys_default(void)
{
    clk_sys_set(SYS_CLK_KHZ);
}