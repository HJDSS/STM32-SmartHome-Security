#ifndef __LOCK_MANAGER_H
#define __LOCK_MANAGER_H

#include "app_types.h"

void LockManager_Init(void);
void LockManager_Tick(void);
void LockManager_OnConfirm(const keypad_input_t *in);
void LockManager_OnFunctionKey(int key);
const lock_state_t* LockManager_GetState(void);
u8 LockManager_IsBruteAlarm(void);
void LockManager_ClearBruteAlarm(void);
u8 LockManager_IsPwdLocked(void);

#endif
