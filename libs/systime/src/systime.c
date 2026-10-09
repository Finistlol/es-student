#include "systime/systime.h"

#include "pico/time.h"

uint64_t systime_us(void)
{
    return time_us_64();
}

uint64_t systime_ms(void)
{
    return time_us_64() / 1000;
}

bool systime_period_elapsed(uint64_t *last_us, uint64_t period_us)
{
    uint64_t now_us = time_us_64();

    if (now_us - *last_us < period_us)
    {
        return false;
    }

    *last_us += period_us;

    if (now_us - *last_us >= period_us)
    {
        // Отстали больше чем на период: начинаем сетку заново.
        *last_us = now_us;
    }

    return true;
}