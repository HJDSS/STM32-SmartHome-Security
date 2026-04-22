#include "ff.h"

#if FF_FS_REENTRANT
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t s_ff_mutex = NULL;

int ff_mutex_create (int vol)
{
		(void)vol;
		if(s_ff_mutex==NULL) s_ff_mutex = xSemaphoreCreateMutex();
		return (s_ff_mutex!=NULL);
}

void ff_mutex_delete (int vol)
{
		(void)vol;
}

int ff_mutex_take (int vol)
{
		(void)vol;
		if(s_ff_mutex==NULL) return 0;
		return (xSemaphoreTake(s_ff_mutex,pdMS_TO_TICKS(FF_FS_TIMEOUT))==pdPASS);
}

void ff_mutex_give (int vol)
{
		(void)vol;
		if(s_ff_mutex==NULL) return;
		xSemaphoreGive(s_ff_mutex);
}
#endif

#if !FF_FS_READONLY && !FF_FS_NORTC
DWORD get_fattime (void)
{
		return 0;
}
#endif

