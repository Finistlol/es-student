#include "button-task/button-task.h"

#include <stdbool.h>

#include "platform/platform.h"
#include "systime/systime.h"

// Новый уровень принимается, только если продержался столько времени.
#define BUTTON_DEBOUNCE_US 20000

// Машина состояний подавления дребезга: уровень меняется только после
// того, как продержится DEBOUNCE_US. Переход UP -> DOWN даёт нажатие.
typedef enum
{
    BUTTON_STATE_UP,
    BUTTON_STATE_DOWN_PENDING,
    BUTTON_STATE_DOWN,
    BUTTON_STATE_UP_PENDING,
} button_state_t;

static button_state_t state = BUTTON_STATE_UP;
static uint64_t since_us = 0;
static uint32_t press_count = 0;
static button_task_callback_t on_press = 0;

void button_task_init(button_task_callback_t callback)
{
    on_press = callback;
    state = BUTTON_STATE_UP;
    since_us = systime_us();
}

void button_task_handle(void)
{
    bool pressed = platform_button_read(PLATFORM_BUTTON_1);
    uint64_t now_us = systime_us();

    switch (state)
    {
    case BUTTON_STATE_UP:
        if (pressed)
        {
            state = BUTTON_STATE_DOWN_PENDING;
            since_us = now_us;
        }
        break;
    case BUTTON_STATE_DOWN_PENDING:
        if (!pressed)
        {
            state = BUTTON_STATE_UP;
        }
        else if (now_us - since_us >= BUTTON_DEBOUNCE_US)
        {
            state = BUTTON_STATE_DOWN;
            press_count = press_count + 1;
            if (on_press != 0)
            {
                on_press();
            }
        }
        break;
    case BUTTON_STATE_DOWN:
        if (!pressed)
        {
            state = BUTTON_STATE_UP_PENDING;
            since_us = now_us;
        }
        break;
    case BUTTON_STATE_UP_PENDING:
        if (pressed)
        {
            state = BUTTON_STATE_DOWN;
        }
        else if (now_us - since_us >= BUTTON_DEBOUNCE_US)
        {
            state = BUTTON_STATE_UP;
        }
        break;
    }
}

bool button_task_is_pressed(void)
{
    return state == BUTTON_STATE_DOWN;
}

uint32_t button_task_get_press_count(void)
{
    return press_count;
}