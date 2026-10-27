#include "led-task/led-task.h"

#include <stdbool.h>

#include "platform/platform.h"
#include "systime/systime.h"

static led_state_t state = LED_STATE_BLINK;
static uint32_t period_ms = 1000;
static uint64_t last_toggle_us = 0;
static bool level = false;

void led_task_init(void)
{
    led_task_set_state(LED_STATE_BLINK);
    period_ms = 1000;
}

void led_task_handle(void)
{
    switch (state)
    {
    case LED_STATE_BLINK:
        if (systime_period_elapsed(&last_toggle_us, (uint64_t)period_ms * 500))
        {
            level = !level;
            platform_led_set(level);
        }
        break;
    case LED_STATE_OFF:
    case LED_STATE_ON:
        break;
    }
}

void led_task_set_state(led_state_t next)
{
    state = next;

    switch (state)
    {
    case LED_STATE_ON:
        level = true;
        platform_led_set(level);
        break;
    case LED_STATE_BLINK:
        level = false;
        platform_led_set(level);
        last_toggle_us = systime_us();
        break;
    case LED_STATE_OFF:
    default:
        level = false;
        platform_led_set(level);
        break;
    }
}

led_state_t led_task_get_state(void)
{
    return state;
}

void led_task_set_period_ms(uint32_t value)
{
    period_ms = value;
}

uint32_t led_task_get_period_ms(void)
{
    return period_ms;
}

void led_task_next_state(void)
{
    switch (state)
    {
    case LED_STATE_OFF:
        led_task_set_state(LED_STATE_ON);
        break;
    case LED_STATE_ON:
        led_task_set_state(LED_STATE_BLINK);
        break;
    case LED_STATE_BLINK:
    default:
        led_task_set_state(LED_STATE_OFF);
        break;
    }
}