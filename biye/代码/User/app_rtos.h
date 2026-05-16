#ifndef __APP_RTOS_H
#define __APP_RTOS_H

#include "board_config.h"

#if USE_FREERTOS
void AppRTOS_Start(void);
#endif

#endif
