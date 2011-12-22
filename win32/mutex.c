#include <rtthread.h>

#include <SDL/SDL.h>

rt_err_t rt_mutex_init (rt_mutex_t mutex, const char* name, rt_uint8_t flag)
{
	if (mutex != RT_NULL)
	{
		int index;
		for (index = 0; *name && index < RT_NAME_MAX; index ++)
		{
			mutex->name[index] = *name;
			if (*name) name ++;
		}

		mutex->host_mutex = SDL_CreateMutex();
	}

	return RT_EOK;
}

rt_err_t rt_mutex_detach (rt_mutex_t mutex)
{
	SDL_DestroyMutex((SDL_mutex *)(mutex->host_mutex));

	return RT_EOK;
}

rt_mutex_t rt_mutex_create (const char* name, rt_uint8_t flag)
{
	rt_mutex_t mutex;

	mutex = (rt_mutex_t)rt_malloc(sizeof(struct rt_mutex));
	if (mutex != RT_NULL)
	{
	    rt_mutex_init(mutex, name, flag);
		return mutex;
	}

	return RT_NULL;
}

rt_err_t rt_mutex_delete (rt_mutex_t mutex)
{
	rt_mutex_detach(mutex);

	rt_free(mutex);

	return RT_EOK;
}

rt_err_t rt_mutex_take (rt_mutex_t mutex, rt_int32_t time)
{
	if (SDL_mutexP((SDL_mutex *)(mutex->host_mutex)) == 0)
	{
		return RT_EOK;
	}

	return -RT_ERROR;
}

rt_err_t rt_mutex_release(rt_mutex_t mutex)
{
	if (SDL_mutexV((SDL_mutex *)(mutex->host_mutex)) == 0)
	{
		return RT_EOK;
	}

	return -RT_ERROR;
}

rt_err_t rt_mutex_control(rt_mutex_t mutex, rt_uint8_t cmd, void* arg)
{
	return RT_EOK;
}
