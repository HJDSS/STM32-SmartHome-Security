#include "lock_manager.h"
#include "flash_store.h"
#include "oled_view.h"
#include "linkage.h"
#include "board_config.h"
#include "app_params.h"
#include "gpio.h"
#include "oled.h"
#include "delay.h"
#include "syslog.h"
#include "capture_task.h"
#include "as608.h"
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
    s_lock.brute_alarm = 0u;
    s_lock.brute_alarm_sent = 0u;
}

u8 LockManager_IsBruteAlarm(void)
{
    return s_lock.brute_alarm;
}

void LockManager_ClearBruteAlarm(void)
{
    s_lock.brute_alarm = 0u;
    s_lock.brute_alarm_sent = 0u;
    s_lock.pwd_failed_count = 0u;
}

u8 LockManager_IsPwdLocked(void)
{
    return s_lock.pwd_locked;
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
        s_lock.pwd_locked = 0u;
        s_lock.admin_until_ms = 0u;
        s_lock.brute_alarm = 0u;
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
        s_lock.pwd_locked = 0u;
        s_lock.admin_until_ms = Bare_GetTickMs() + APP_ADMIN_SESSION_MS;
        s_lock.brute_alarm = 0u;
        RELAY = 0;
        RELAY_TIME = 15;
        OLED_ShowString(0, 16, "ADMIN MODE      ", 16);
        Linkage_OnUnlock(UNLOCK_SRC_PWD);
    }
    else
    {
        s_lock.pwd_failed_count++;
        OLED_ShowString(0, 16, "PWD ERROR       ", 16);
        if(s_lock.pwd_failed_count >= APP_LOCK_BRUTE_MAX)
        {
            s_lock.brute_alarm = 1u;
            if(!s_lock.brute_alarm_sent)
            {
                s_lock.brute_alarm_sent = 1u;
                SysLog_Add(LOG_EVT_ALARM, "BRUTE_FORCE");
            }
            OLED_ShowString(0, 32, "BRUTE ALARM     ", 16);
        }
        else if(s_lock.pwd_failed_count >= APP_LOCK_MAX_FAIL)
        {
            s_lock.pwd_locked = 1u;
            s_lock.pwd_lock_until_ms = Bare_GetTickMs() + APP_LOCK_LOCKOUT_MS;
            OLED_ShowString(0, 32, "KEYPAD LOCKED   ", 16);
        }
    }
}

extern arm_mode_t arm_mode;
extern void Security_Set_ArmMode(arm_mode_t mode);

static void admin_extend_session(void)
{
    s_lock.admin_until_ms = Bare_GetTickMs() + (u32)APP_ADMIN_SESSION_MAX_MS;
}

void LockManager_OnFunctionKey(int key)
{
    static u8 s_log_page = 0u;
    static u32 s_log_show_ms = 0u;
    u32 now = Bare_GetTickMs();

    if(s_lock.user_level == USER_ADMIN)
    {
        /* ========== 管理员态功能键 ========== */
        switch(key)
        {
        case 'A':
            /* A=录入指纹：延长窗口到60s，触发录入流程 */
            s_lock.enroll_pending = 1u;
            admin_extend_session();
            OLED_ShowString(0, 16, "ENROLL START    ", 16);
            OLED_ShowString(0, 32, "PUT FINGER      ", 16);
            SysLog_Add(LOG_EVT_CONFIG, "FP_ENROLL_START");
            break;

        case 'B':
            /* B=清库：擦除全部指纹 */
            FINGERPRINT_Cmd_Delete_All_Model();
            OLED_ShowString(0, 16, "FP DB CLEAR     ", 16);
            OLED_ShowString(0, 32, "ALL FP ERASED   ", 16);
            SysLog_Add(LOG_EVT_CONFIG, "FP_CLEAR_ALL");
            break;

        case 'C':
            /* C=退出管理员 */
            s_lock.user_level = USER_NONE;
            s_lock.admin_until_ms = 0u;
            s_lock.enroll_pending = 0u;
            s_lock.pwd_chg_state = 0u;
            OLED_ShowString(0, 16, "ADMIN EXIT      ", 16);
            OLED_ShowString(0, 32, "INPUT PASSWORD  ", 16);
            SysLog_Add(LOG_EVT_CONFIG, "ADMIN_EXIT");
            break;

        case 'D':
            /* D=改密：进入改密流程 */
            s_lock.pwd_chg_state = 1u;
            admin_extend_session();
            OLED_ShowString(0, 16, "CHANGE PWD      ", 16);
            OLED_ShowString(0, 32, "INPUT NEW 6-DGT ", 16);
            SysLog_Add(LOG_EVT_CONFIG, "PWD_CHG_START");
            break;

        default:
            break;
        }
    }
    else
    {
        /* ========== 普通态功能键（快捷布防/日志/紧急） ========== */
        switch(key)
        {
        case 'A':
            if(arm_mode == ARM_MODE_DISARM)
            {
                Security_Set_ArmMode(ARM_MODE_AWAY);
                OLED_ShowString(0, 16, "ARMED AWAY      ", 16);
                SysLog_Add(LOG_EVT_CONFIG, "ARM_AWAY_KEY");
            }
            else
            {
                Security_Set_ArmMode(ARM_MODE_DISARM);
                LockManager_ClearBruteAlarm();
                OLED_ShowString(0, 16, "DISARMED        ", 16);
                SysLog_Add(LOG_EVT_CONFIG, "DISARM_KEY");
            }
            s_log_show_ms = 0u;
            break;

        case 'B':
            if(arm_mode == ARM_MODE_DISARM)
            {
                Security_Set_ArmMode(ARM_MODE_HOME);
                OLED_ShowString(0, 16, "ARMED HOME      ", 16);
                SysLog_Add(LOG_EVT_CONFIG, "ARM_HOME_KEY");
            }
            else if(arm_mode == ARM_MODE_HOME)
            {
                Security_Set_ArmMode(ARM_MODE_DISARM);
                LockManager_ClearBruteAlarm();
                OLED_ShowString(0, 16, "DISARMED        ", 16);
                SysLog_Add(LOG_EVT_CONFIG, "DISARM_KEY");
            }
            else
            {
                Security_Set_ArmMode(ARM_MODE_HOME);
                OLED_ShowString(0, 16, "ARMED HOME      ", 16);
                SysLog_Add(LOG_EVT_CONFIG, "ARM_HOME_KEY");
            }
            s_log_show_ms = 0u;
            break;

        case 'C':
            {
                u16 total = SysLog_Count();
                u16 max_page = (total > 0u) ? (u16)((total - 1u) / 3u) : 0u;
                if(s_log_show_ms == 0u || (u32)(now - s_log_show_ms) > 5000u)
                    s_log_page = 0u;
                else if(s_log_page < max_page)
                    s_log_page++;
                else
                    s_log_page = 0u;
                s_log_show_ms = now;
                SysLog_OLED_ShowPage(s_log_page, LOG_FILTER_ALL);
            }
            break;

        case 'D':
            Security_Set_ArmMode(ARM_MODE_AWAY);
            BEEP_SoundOn();
            SysLog_Add(LOG_EVT_ALARM, "PANIC_KEY");
#if EN_OV7670_LOCAL
            Capture_Request(CAP_EVT_INTRUSION);
#endif
            OLED_ShowString(0, 16, "PANIC ALARM     ", 16);
            s_log_show_ms = 0u;
            break;

        default:
            break;
        }
    }
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
        s_lock.user_level = USER_NONE;
        OLED_ShowString(0, 16, "ADMIN TIMEOUT   ", 16);
        if(s_lock.pwd_chg_state != 0u)
        {
            reset_pwd_chg();
            SysLog_Add(LOG_EVT_CONFIG, "PWD_CHG_ABORT_BY_WIN");
        }
        else
        {
            SysLog_Add(LOG_EVT_CONFIG, "ADMIN_TIMEOUT");
        }
    }

    if(RELAY_TIME == 0u)
    {
        RELAY = 1;
        if(s_lock.relay_on != 0u)
        {
            s_lock.relay_on = 0u;
            if(s_lock.user_level != USER_NONE)
            {
                s_lock.user_level = USER_NONE;
                s_lock.admin_until_ms = 0u;
                s_lock.pwd_chg_state = 0u;
                OLED_ShowString(0, 16, "INPUT PASSWORD  ", 16);
            }
        }
    }
}
