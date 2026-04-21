#include "lock_manager.h"
#include "flash_store.h"
#include "oled_view.h"
#include "linkage.h"
#include "board_config.h"

static lock_state_t s_lock;

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
    s_lock.pwd_lock_until_ms = 0;
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
        RELAY = 0;
        RELAY_TIME = 15;
        OLED_ShowString(0, 16, "ADMIN MODE      ", 16);
        Linkage_OnUnlock(UNLOCK_SRC_PWD);
    }
    else
    {
        s_lock.pwd_failed_count++;
        OLED_ShowString(0, 16, "PWD ERROR       ", 16);
        if(s_lock.pwd_failed_count >= 3u)
        {
            s_lock.pwd_locked = 1u;
            s_lock.pwd_lock_until_ms = Bare_GetTickMs() + 30000u;
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
    if(s_lock.pwd_locked)
    {
        if((u32)(Bare_GetTickMs() - s_lock.pwd_lock_until_ms) < 3000000000u)
        {
            RELAY = 1;
            return;
        }
        s_lock.pwd_locked = 0u;
        s_lock.pwd_failed_count = 0u;
    }

    if(RELAY_TIME == 0)
        RELAY = 1;
}
