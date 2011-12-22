#include <rtthread.h>
#include <SDL/SDL.h>

rt_err_t rt_sem_init (rt_sem_t sem, const char* name, rt_uint32_t value, rt_uint8_t flag)
{
	if (sem != RT_NULL)
	{
		int index;

		memset(sem->name, 0, sizeof(sem->name));
		for (index = 0; *name && index < RT_NAME_MAX; index ++)
		{
			sem->name[index] = *name;
			if (*name) name ++;
		}

		sem->host_sem = SDL_CreateSemaphore(value);
	}

	return RT_EOK;
}

rt_err_t rt_sem_detach (rt_sem_t sem)
{
	SDL_DestroySemaphore((SDL_sem *)(sem->host_sem));

	return RT_EOK;
}

rt_sem_t rt_sem_create (const char* name, rt_uint32_t value, rt_uint8_t flag)
{
	rt_sem_t sem;

	sem = (rt_sem_t)rt_malloc(sizeof(struct rt_semaphore));
	if (sem != RT_NULL)
	{
	    rt_sem_init(sem, name, value, flag);
		return sem;
	}

	return RT_NULL;
}

rt_err_t rt_sem_delete (rt_sem_t sem)
{
	rt_sem_detach(sem);

	rt_free(sem);

	return RT_EOK;
}

rt_err_t rt_sem_take (rt_sem_t sem, rt_int32_t time)
{
	int r;

	r = SDL_SemWaitTimeout((SDL_sem *)(sem->host_sem), time);

	if (r == 0) return RT_EOK;
	else if (r == SDL_MUTEX_TIMEDOUT) return -RT_ETIMEOUT;

	return -RT_ERROR;
}

rt_err_t rt_sem_trytake(rt_sem_t sem)
{
	if (SDL_SemTryWait((SDL_sem *)(sem->host_sem)) == 0)
	{
		return RT_EOK;
	}

	return -RT_ETIMEOUT;
}

rt_err_t rt_sem_release(rt_sem_t sem)
{
	if (SDL_SemPost((SDL_sem *)(sem->host_sem)) == 0)
	{
		return RT_EOK;
	}

	return -RT_ERROR;
}

rt_err_t rt_sem_control(rt_sem_t sem, rt_uint8_t cmd, void* arg)
{
	return RT_EOK;
}
