#ifndef __OSAL_EVT_H
#define __OSAL_EVT_H

#include "sys.h"

typedef u16 osal_evt_t;
typedef osal_evt_t (*osal_task_handler_t)(u8 task_id, osal_evt_t events);

typedef struct osal_task_rec
{
	struct osal_task_rec *next;
	osal_task_handler_t handler;
	u8 task_id;
	u8 priority;
	volatile osal_evt_t events;
	u8 used;
} osal_task_rec_t;

#ifndef OSAL_TASK_MAX
#define OSAL_TASK_MAX  8u
#endif

void osal_init(void);
s8 osal_add_task(osal_task_handler_t handler, u8 priority);
u8 osal_set_event(u8 task_id, osal_evt_t event_flag);
u8 osal_clear_event(u8 task_id, osal_evt_t event_flag);
osal_task_rec_t *osal_next_active_task(void);
/* ?? 1 ??????????0 ??????? */
u8 osal_dispatch_once(void);

#endif
