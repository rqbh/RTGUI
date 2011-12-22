#include "rtthread.h"
#include <SDL/SDL.h>

struct _host_timer
{
	SDL_TimerID id;
	Uint32 interval;
};

Uint32 _host_timer_callback(Uint32 interval, void *param)
{
	struct _host_timer* ht;
	struct rt_timer* timer = (struct rt_timer*) param;

	ht = (struct _host_timer*)timer->host_timer;

	/* call timer callback */
	timer->timeout_func(timer->parameter);

	if (timer->flag & RT_TIMER_FLAG_PERIODIC)
	{
		/* convert RT-Thread OS tick to milliseconds, in default, 1 os tick = 10 milliseconds */
		interval = ht->interval * 10;

		return interval;
	}

	return 0;
}

void rt_timer_init(rt_timer_t timer,
	const char* name,
	void (*timeout)(void* parameter), void* parameter,
	rt_tick_t time, rt_uint8_t flag)
{
	int index;
	struct _host_timer* ht;

	RT_ASSERT(timer != RT_NULL);

	for (index = 0; index < RT_NAME_MAX; index ++)
	{
		timer->name[index] = name[index];
	}

	timer->timeout_func = timeout;
	timer->parameter = parameter;
	timer->flag = flag;

	ht = (struct _host_timer*) rt_malloc(sizeof(struct _host_timer));
	timer->host_timer = ht;

	ht->interval = time;
}

rt_err_t rt_timer_detach(rt_timer_t timer)
{
	rt_timer_stop(timer);
	rt_free(timer->host_timer);
	timer->host_timer = RT_NULL;

	return RT_EOK;
}

rt_timer_t rt_timer_create(const char* name,
	void (*timeout)(void* parameter), void* parameter,
	rt_tick_t time, rt_uint8_t flag)
{
	rt_timer_t timer;

	timer = (rt_timer_t)rt_malloc(sizeof(struct rt_timer));
	if (timer != RT_NULL) 
		rt_timer_init(timer, name, timeout, parameter, time, flag);

	return timer;
}

rt_err_t rt_timer_delete(rt_timer_t timer)
{
	rt_timer_detach(timer);
	rt_free(timer);

	return RT_EOK;
}

rt_err_t rt_timer_start(rt_timer_t timer)
{
	struct _host_timer* ht;
	Uint32 interval;

	ht = (struct _host_timer*)timer->host_timer;
	if (ht->id != RT_NULL) SDL_RemoveTimer(ht->id);

	/* convert RT-Thread OS tick to milliseconds, in default, 1 os tick = 10 milliseconds */
	interval = ht->interval * 10;

	ht->id = SDL_AddTimer(interval, _host_timer_callback, timer);
	if (ht->id != RT_NULL)
	{
		return RT_EOK;
	}

	return -RT_ERROR;
}

rt_err_t rt_timer_stop(rt_timer_t timer)
{
	struct _host_timer* ht;
	
	ht = (struct _host_timer*)timer->host_timer;
	SDL_RemoveTimer(ht->id);
	ht->id = RT_NULL;

	return RT_EOK;
}

rt_err_t rt_timer_control(rt_timer_t timer, rt_uint8_t cmd, void* arg)
{
	return -RT_ERROR;
}
