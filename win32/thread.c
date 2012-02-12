#include <rtthread.h>
#include "list.h"

#include <SDL/SDL.h>

#ifdef THREAD_DEBUG
#define DBG_MSG(x)	rt_kprintf x
#else
#define DBG_MSG(x)
#endif

rt_list_t _thread_list;
SDL_mutex *_thread_list_mutex;

void rt_thread_system_init()
{
	rt_list_init(&_thread_list);

	_thread_list_mutex = SDL_CreateMutex();
}

rt_err_t rt_thread_init(struct rt_thread* thread,
	const char* name,
	void (*entry)(void* parameter), void* parameter,
	void* stack_start, rt_uint32_t stack_size,
	rt_uint8_t priority, rt_uint32_t tick)
{
	int index;

	RT_ASSERT(thread != RT_NULL);

	for (index = 0; index < RT_NAME_MAX; index ++)
	{
		thread->name[index] = name[index];
	}

	thread->entry = entry;
	thread->parameter = parameter;
	thread->tid = thread;
	thread->stat = RT_THREAD_INIT;
	thread->host_thread = 0;
	thread->user_data = 0;

	rt_list_init(&(thread->tlist));

	/* add to list */
	SDL_mutexP(_thread_list_mutex);
	rt_list_insert_after(&_thread_list, &(thread->tlist));
	SDL_mutexV(_thread_list_mutex);

	return RT_EOK;
}

rt_err_t rt_thread_detach(rt_thread_t thread)
{
	/* kill thread */
	if (thread->stat == RT_THREAD_READY)
	{
		// SDL_KillThread((SDL_Thread *)(thread->host_thread));
	}

	thread->stat = RT_THREAD_INIT;

	SDL_mutexP(_thread_list_mutex);
	rt_list_remove(&(thread->tlist));
	SDL_mutexV(_thread_list_mutex);

	return RT_EOK;
}

rt_thread_t rt_thread_create (const char* name,
	void (*entry)(void* parameter), void* parameter,
	rt_uint32_t stack_size,
	rt_uint8_t priority, rt_uint32_t tick)
{
	rt_thread_t thread;

	thread = (rt_thread_t)rt_malloc(sizeof(struct rt_thread));
	if (thread != RT_NULL)
	{
		rt_thread_init(thread, name, entry, parameter, NULL, stack_size, priority, tick);
	}

	return thread;
}

rt_thread_t rt_thread_self(void)
{
	Uint32 tid = SDL_ThreadID();

	struct rt_list_node* node;
	rt_thread_t thread;

	SDL_mutexP(_thread_list_mutex);
	rt_list_foreach(node, &(_thread_list), next)
	{
		thread = rt_list_entry(node, struct rt_thread, tlist);

		if (thread->host_thread == tid)
		{
			SDL_mutexV(_thread_list_mutex);

			DBG_MSG(("thread self:%s, sdl_tid: %d, tid: 0x%p\n",
                thread->name,
                tid,
                thread->tid));

			return thread->tid;
		}
	}

	SDL_mutexV(_thread_list_mutex);
	return RT_NULL;
}

int host_thread_entry(void* parameter)
{
	rt_thread_t thread = (rt_thread_t) parameter;

	/* get host thread id */
	thread->host_thread = SDL_ThreadID();
	thread->stat = RT_THREAD_READY;

	/* execute thread entry */
	thread->entry(thread->parameter);

	DBG_MSG(("create thread:%s, sdl_tid: %d, tid: 0x%p\n",
		thread->name,
		thread->host_thread,
		thread->tid));

	return 0;
}

rt_err_t rt_thread_startup(rt_thread_t thread)
{
	if (thread->stat == RT_THREAD_READY)
	{
		return -RT_ERROR;
	}

	SDL_CreateThread(host_thread_entry, thread);

	return RT_EOK;
}

rt_err_t rt_thread_delete(rt_thread_t thread)
{
	rt_thread_detach(thread);

	free(thread);
	return RT_EOK;
}

rt_err_t rt_thread_yield(void)
{
	return RT_EOK;
}

rt_err_t rt_thread_delay(rt_tick_t tick)
{
	SDL_Delay(tick * 10);

	return RT_EOK;
}

rt_err_t rt_thread_control(rt_thread_t thread, rt_uint8_t cmd, void* arg)
{
	return RT_EOK;
}

rt_err_t rt_thread_suspend(rt_thread_t thread)
{
	return RT_EOK;
}

rt_err_t rt_thread_resume(rt_thread_t thread)
{
	return RT_EOK;
}

/* note: this function is not located in RT-Thread RTOS */
rt_thread_t rt_thread_find(char* name)
{
	rt_thread_t thread;
	struct rt_list_node* node;

	SDL_mutexP(_thread_list_mutex);
	rt_list_foreach(node, &(_thread_list), next)
	{
		thread = rt_list_entry(node, struct rt_thread, tlist);
		if (strncmp(thread->name, name, RT_NAME_MAX) == 0)
		{
			SDL_mutexV(_thread_list_mutex);
			return thread->tid;
		}
	}

	SDL_mutexV(_thread_list_mutex);
	return RT_NULL;
}
