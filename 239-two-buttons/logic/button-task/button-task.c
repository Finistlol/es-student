#include "button-task/button-task.h"

#include <stdbool.h>

#include "platform/platform.h"
#include "systime/systime.h"

// Новый уровень принимается, только если продержался столько времени.
#define BUTTON_DEBOUNCE_US 20000

// Удержание дольше секунды — долгое нажатие.
#define BUTTON_LONG_PRESS_US 1000000

// Машина состояний одной кнопки. Кнопок у прибора несколько, поэтому
// состояние каждой лежит своим элементом массива.
typedef enum
{
    BUTTON_STATE_UP,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_DOWN,
    BUTTON_STATE_RELEASE_WAIT,
    BUTTON_STATE_LONG_FIRED,
} button_state_t;

typedef struct
{
    button_state_t state;
    uint64_t since_us;
    uint32_t press_count;
    bool suppress_short;
    button_task_callback_t on_press;
    button_task_callback_t on_long_press;
} button_t;

static button_t buttons[PLATFORM_BUTTON_COUNT];

void button_task_init(platform_button_t button,
                      button_task_callback_t on_press,
                      button_task_callback_t on_long_press)
{
    buttons[button].state = BUTTON_STATE_UP;
    buttons[button].since_us = 0;
    buttons[button].press_count = 0;
    buttons[button].on_press = on_press;
    buttons[button].on_long_press = on_long_press;
}

static void handle_one(platform_button_t id)
{
    button_t *b = &buttons[id];
    bool pressed = platform_button_read(id);
    uint64_t now_us = systime_us();

    switch (b->state)
    {
    case BUTTON_STATE_UP:
        if (pressed)
        {
            b->state = BUTTON_STATE_PRESSED;
            b->since_us = now_us;
        }
        break;
    case BUTTON_STATE_PRESSED:
        if (!pressed)
        {
            b->state = BUTTON_STATE_UP;
        }
        else if (now_us - b->since_us >= BUTTON_DEBOUNCE_US)
        {
            b->press_count = b->press_count + 1;
            if (b->on_long_press != 0)
            {
                // Короткое или долгое решится при отпускании.
                b->state = BUTTON_STATE_DOWN;
            }
            else
            {
                b->state = BUTTON_STATE_DOWN;
                if (b->on_press != 0)
                {
                    b->on_press();
                }
            }
        }
        break;
    case BUTTON_STATE_DOWN:
        if (b->on_long_press != 0 && now_us - b->since_us >= BUTTON_LONG_PRESS_US)
        {
            b->state = BUTTON_STATE_LONG_FIRED;
            b->on_long_press();
        }
        else if (!pressed)
        {
            b->state = BUTTON_STATE_RELEASE_WAIT;
            b->since_us = now_us;
        }
        break;
    case BUTTON_STATE_RELEASE_WAIT:
        if (pressed)
        {
            b->state = BUTTON_STATE_DOWN;
        }
        else if (now_us - b->since_us >= BUTTON_DEBOUNCE_US)
        {
            b->state = BUTTON_STATE_UP;
            if (b->suppress_short)
            {
                b->suppress_short = false;
            }
            else if (b->on_long_press != 0 && b->on_press != 0)
            {
                // Отпущено до секунды — это было короткое нажатие.
                b->on_press();
            }
        }
        break;
    case BUTTON_STATE_LONG_FIRED:
        if (!pressed)
        {
            b->state = BUTTON_STATE_RELEASE_WAIT;
            b->since_us = now_us;
            // Долгое уже сработало: короткого при отпускании не будет.
            b->suppress_short = true;
        }
        break;
    }
}

void button_task_handle(void)
{
    for (uint32_t id = 0; id < PLATFORM_BUTTON_COUNT; id++)
    {
        handle_one((platform_button_t)id);
    }
}

bool button_task_is_pressed(platform_button_t button)
{
    return buttons[button].state == BUTTON_STATE_DOWN ||
           buttons[button].state == BUTTON_STATE_LONG_FIRED;
}

uint32_t button_task_get_press_count(platform_button_t button)
{
    return buttons[button].press_count;
}
