#ifndef __AS608_H_
#define __AS608_H_

#include "sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board_config.h"

/* AS608 状态脚：由 board_config.h 统一配置 */
#define PS_Sta   GPIO_ReadInputDataBit(BOARD_AS608_PS_STA_PORT, BOARD_AS608_PS_STA_PIN)

/* 是否使用 WAK 触摸门控：1=使用 PA0 触摸信号；0=忽略触摸，始终尝试识别 */
#define AS608_USE_WAKE_PIN        0
/* WAK 有效电平：1=高有效，0=低有效 */
#define AS608_WAKE_ACTIVE_HIGH    1

#if AS608_USE_WAKE_PIN
#if AS608_WAKE_ACTIVE_HIGH
#define AS608_TOUCH_DETECTED()    (PS_Sta == 1)
#else
#define AS608_TOUCH_DETECTED()    (PS_Sta == 0)
#endif
#else
#define AS608_TOUCH_DETECTED()    (1)
#endif

void PS_StaGPIO_Init(void);
unsigned char FINGERPRINT_Cmd_Delete_All_Model(void);
unsigned short AS608_Find_Fingerprint(void);
unsigned short AS608_Add_Fingerprint(unsigned short ID);
void FINGERPRINT_Cmd_Delete_Model(unsigned short uiID_temp);

#endif
