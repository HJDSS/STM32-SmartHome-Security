#include "osal_evt.h"

static osal_task_rec_t s_tasks[OSAL_TASK_MAX];
static osal_task_rec_t *s_head;

static void osal_lock(u32 *primask)
{
	*primask = __get_PRIMASK();
	__disable_irq();
}

static void osal_unlock(u32 primask)
{
	if((primask & 0x1u) == 0u)
		__enable_irq();
}

void osal_init(void)
{
	u8 i;
	s_head = 0;
	for(i = 0; i < OSAL_TASK_MAX; i++)
	{
		s_tasks[i].next = 0;
		s_tasks[i].handler = 0;
		s_tasks[i].task_id = i;
		s_tasks[i].priority = 0;
		s_tasks[i].events = 0;
		s_tasks[i].used = 0;
	}
}

s8 osal_add_task(osal_task_handler_t handler, u8 priority)
{
	u8 i;
	osal_task_rec_t *node;
	osal_task_rec_t **pp;
	u32 pm;

	if(handler == 0)
		return -1;
	for(i = 0; i < OSAL_TASK_MAX; i++)
	{
		if(!s_tasks[i].used)
		{
			node = &s_tasks[i];
			node->used = 1;
			node->handler = handler;
			node->priority = priority;
			node->events = 0;
			node->next = 0;

			osal_lock(&pm);
			pp = &s_head;
			while((*pp) != 0 && (*pp)->priority <= priority)
				pp = &((*pp)->next);
			node->next = *pp;
			*pp = node;
			osal_unlock(pm);
			return (s8)node->task_id;
		}
	}
	return -1;
}

u8 osal_set_event(u8 task_id, osal_evt_t event_flag)
{
	u32 pm;
	if(task_id >= OSAL_TASK_MAX || !s_tasks[task_id].used)
		return 0;
	osal_lock(&pm);
	s_tasks[task_id].events |= event_flag;
	osal_unlock(pm);
	return 1;
}

u8 osal_clear_event(u8 task_id, osal_evt_t event_flag)
{
	u32 pm;
	if(task_id >= OSAL_TASK_MAX || !s_tasks[task_id].used)
		return 0;
	osal_lock(&pm);
	s_tasks[task_id].events &= (osal_evt_t)(~event_flag);
	osal_unlock(pm);
	return 1;
}

osal_task_rec_t *osal_next_active_task(void)
{
	osal_task_rec_t *p = s_head;
	while(p)
	{
		if(p->events != 0)
			return p;
		p = p->next;
	}
	return 0;
}

u8 osal_dispatch_once(void)
{
	osal_task_rec_t *t;
	osal_evt_t ev;
	osal_evt_t ret;
	u32 pm;

	t = osal_next_active_task();
	if(t == 0)
		return 0;

	osal_lock(&pm);
	ev = t->events;
	t->events = 0;
	osal_unlock(pm);

	if(ev == 0 || t->handler == 0)
		return 1;

	ret = t->handler(t->task_id, ev);
	if(ret)
	{
		osal_lock(&pm);
		t->events |= ret;
		osal_unlock(pm);
	}
	return 1;
}
