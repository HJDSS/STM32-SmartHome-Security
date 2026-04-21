#include "keyboard_sm.h"
#include "key4_4.h"
#include "oled_view.h"
#include "lock_manager.h"
#include "board_config.h"
#include "delay.h"
#include <stddef.h>

static keypad_input_t s_input;
static u32 s_last_key_ms = 0;
static u32 s_last_input_ms = 0;

#ifndef KEYBOARD_INPUT_TIMEOUT_MS
#define KEYBOARD_INPUT_TIMEOUT_MS  12000u
#endif
#ifndef KEYBOARD_KEY_MIN_INTERVAL_MS
#define KEYBOARD_KEY_MIN_INTERVAL_MS  45u
#endif

void KeyboardSM_Init(void)
{
    u8 i;
    for(i = 0; i < 6; i++)
        s_input.digits[i] = 0;
    s_input.count = 0;
    s_input.active = 0;
    s_last_key_ms = 0;
    s_last_input_ms = 0;
}

u8 KeyboardSM_IsBusy(void)
{
    return s_input.active;
}

void KeyboardSM_ClearInput(void)
{
    u8 i;
    for(i = 0; i < 6; i++)
        s_input.digits[i] = 0;
    s_input.count = 0;
    s_input.active = 0;
    s_last_key_ms = 0;
    s_last_input_ms = 0;
}

u8 KeyboardSM_GetInputCount(void)
{
    return s_input.count;
}

u8 KeyboardSM_GetDigitBuffer(u8 *out6)
{
    u8 i;
    if(out6 == NULL)
        return 0;
    for(i = 0; i < 6; i++)
        out6[i] = s_input.digits[i];
    return 1;
}

static u8 key_accept(int key)
{
    u32 now = Bare_GetTickMs();
    if(key < 0)
        return 0;
    if((s_last_key_ms != 0u) && ((u32)(now - s_last_key_ms) < (u32)KEYBOARD_KEY_MIN_INTERVAL_MS))
        return 0;
    s_last_key_ms = now;
    s_last_input_ms = now;
    return 1;
}

void KeyboardSM_Tick(void)
{
    int key;
    u32 now;

    key = Key_Scan();
    if(key != -1)
    {
        if(!key_accept(key))
            return;

        s_input.active = 1;

        if(key >= 0 && key <= 9)
        {
            if(s_input.count < 6)
            {
                s_input.digits[s_input.count++] = (u8)key;
                OLED_View_OnPasswordDigit(s_input.count, key);
            }
        }
        else if(key == '*')
        {
            KeyboardSM_ClearInput();
            OLED_View_OnPasswordCancel();
        }
        else if(key == '#')
        {
            LockManager_OnConfirm(&s_input);
            KeyboardSM_ClearInput();
        }
        else
        {
            LockManager_OnFunctionKey(key);
        }
    }

    now = Bare_GetTickMs();
    if((s_input.count > 0u) && ((u32)(now - s_last_input_ms) >= (u32)KEYBOARD_INPUT_TIMEOUT_MS))
    {
        KeyboardSM_ClearInput();
        OLED_View_OnPasswordTimeout();
    }

    if(s_input.count == 0u)
        s_input.active = 0;
}
