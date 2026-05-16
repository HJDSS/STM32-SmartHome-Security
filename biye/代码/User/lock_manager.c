#include "lock_manager.h"
#include "flash_store.h"
#include "oled_view.h"
#include "linkage.h"
#include "board_config.h"
#include "gpio.h"
#include "oled.h"
#include "delay.h"
#include "syslog.h"
#include <stddef.h>

extern volatile u8 RELAY_TIME;

static lock_state_t s_lock;

static void reset_pwd_chg(void)
{
    s_lock.pwd_chg_state = 0u;
}

static u8 pwd_equal_6(const u8 *a, const u8 *b)
{
    u8 i;
    for(i = 0; i < 6; i++)
    {
        if(a[i] != b[i])
            return 0;
    }
    return 1;
}

void LockManager_Init(void)
{
    s_lock.user_level = USER_NONE;
    s_lock.armed = 0;
    s_lock.relay_on = 0;
    s_lock.pwd_failed_count = 0;
    s_lock.pwd_locked = 0;
    s_lock.pwd_lock_until_ms = 0u;
    s_lock.admin_until_ms = 0u;
    s_lock.pwd_chg_state = 0u;
    s_lock.enroll_pending = 0u;
}

const lock_state_t* LockManager_GetState(void)
{
    return &s_lock;
}

void LockManager_OnConfirm(const keypad_input_t *in)
{
    u8 user_pwd[6];
    u8 admin_pwd[6];

    if(in == NULL || in->count != 6)
        return;

    FlashStore_ReadPasswords(user_pwd, admin_pwd);

    if(pwd_equal_6(in->digits, user_pwd))
    {
        s_lock.user_level = USER_NORMAL;
        s_lock.relay_on = 1;
        s_lock.pwd_failed_count = 0;
        s_lock.admin_until_ms = 0u;
        RELAY = 0;
        RELAY_TIME = 15;
        OLED_ShowString(0, 16, "UNLOCK OK       ", 16);
        Linkage_OnUnlock(UNLOCK_SRC_PWD);
    }
    else if(pwd_equal_6(in->digits, admin_pwd))
    {
        s_lock.user_level = USER_ADMIN;
        s_lock.relay_on = 1;
        s_lock.pwd_failed_count = 0;
        s_lock.admin_until_ms = Bare_GetTickMs() + APP_ADMIN_SESSION_MS;
        RELAY = 0;
        RELAY_TIME = 15;
        OLED_ShowString(0, 16, "ADMIN MODE      ", 16);
        Linkage_OnUnlock(UNLOCK_SRC_PWD);
    }
    else
    {
        s_lock.pwd_failed_count++;
        OLED_ShowString(0, 16, "PWD ERROR       ", 16);
        if(s_lock.pwd_failed_count >= APP_LOCK_MAX_FAIL)
        {
            s_lock.pwd_locked = 1u;
            s_lock.pwd_lock_until_ms = Bare_GetTickMs() + APP_LOCK_LOCKOUT_MS;
            OLED_ShowString(0, 32, "KEYPAD LOCKED   ", 16);
        }
    }
}

void LockManager_OnFunctionKey(int key)
{
    (void)key;
}

void LockManager_Tick(void)
{
    u32 now;

    now = Bare_GetTickMs();

    if(s_lock.pwd_locked)
    {
        if(now < s_lock.pwd_lock_until_ms)
        {
            RELAY = 1;
            return;
        }
        s_lock.pwd_locked = 0u;
        s_lock.pwd_failed_count = 0u;
    }

    if(s_lock.user_level == USER_ADMIN && s_lock.admin_until_ms != 0u
       && !s_lock.enroll_pending && now >= s_lock.admin_until_ms)
    {
        s_lock.admin_until_ms = 0u;
        if(s_lock.relay_on != 0u)
            s_lock.user_level = USER_NORMAL;
        else
            s_lock.user_level = USER_NONE;
        if(s_lock.pwd_chg_state != 0u)
        {
            reset_pwd_chg();
            SysLog_Add(LOG_EVT_ALARM, "PWD_CHG_ABORT_BY_WIN");
        }
    }

    if(RELAY_TIME == 0u)
    {
        RELAY = 1;
        if(s_lock.relay_on != 0u)
        {
            s_lock.relay_on = 0u;
            if(s_lock.user_level == USER_ADMIN || s_lock.admin_until_ms != 0u)
            {
                s_lock.user_level = USER_NONE;
                s_lock.admin_until_ms = 0u;
            }
        }
    }
}
