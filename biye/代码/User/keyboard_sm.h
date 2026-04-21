#ifndef __KEYBOARD_SM_H
#define __KEYBOARD_SM_H

#include "app_types.h"

void KeyboardSM_Init(void);
void KeyboardSM_Tick(void);
u8 KeyboardSM_IsBusy(void);
void KeyboardSM_ClearInput(void);
u8 KeyboardSM_GetInputCount(void);
u8 KeyboardSM_GetDigitBuffer(u8 *out6);

#endif
