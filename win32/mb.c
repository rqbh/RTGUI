#include <rtthread.h>
#include "list.h"

#include <SDL/SDL.h>

rt_list_t _mb_list;
SDL_mutex* _mb_list_mutex;

void rt_mb_system_init()
{
	rt_list_init(&_mb_list);
	_mb_list_mutex = SDL_CreateMutex();
}

#define hmb ((struct host_mb*)(mb->host_mb))
struct host_mb
{
	SDL_sem *mutex;
	SDL_sem *msg;
};

rt_err_t rt_mb_init(rt_mailbox_t mb, const char* name, void* msgpool, rt_size_t size, rt_uint8_t flag)
{
	int index;

	RT_ASSERT(mb != RT_NULL);

	mb->flag = flag;

	for (index = 0; index < RT_NAME_MAX; index ++)
	{
		mb->name[index] = name[index];
	}

	/* append to mq list */
	SDL_mutexP(_mb_list_mutex);
	rt_list_insert_after(&(_mb_list), &(mb->list));
	SDL_mutexV(_mb_list_mutex);

	/* init mailbox */
	mb->msg_pool	= msgpool;
	mb->size 		= size;

	mb->entry 	 	= 0;
	mb->in_offset 	= 0;
	mb->out_offset 	= 0;

	/* init mutex */
	mb->host_mb		= (void*) malloc(sizeof(struct host_mb));
	hmb->mutex		= SDL_CreateSemaphore(1);
	hmb->msg		= SDL_CreateSemaphore(0);

	return RT_EOK;
}

rt_err_t rt_mb_detach(rt_mailbox_t mb)
{
	/* parameter check */
	RT_ASSERT(mb != RT_NULL);

	SDL_DestroySemaphore(hmb->msg);
	SDL_DestroySemaphore(hmb->mutex);

	free(mb->host_mb);
	mb->host_mb = NULL;

	/* remove from list */
	SDL_mutexP(_mb_list_mutex);
	rt_list_remove(&(mb->list));
	SDL_mutexV(_mb_list_mutex);

	return RT_EOK;
}

rt_mailbox_t rt_mb_create (const char* name, rt_size_t size, rt_uint8_t flag)
{
	rt_mailbox_t mb = (rt_mailbox_t) rt_malloc (sizeof(struct rt_mailbox));
	mb->msg_pool = rt_malloc(size * sizeof(rt_uint32_t));
	if (mb->msg_pool == RT_NULL)
	{
		rt_free(mb);
		return RT_NULL;
	}

	rt_mb_init(mb, name, mb->msg_pool, size, flag);

	return mb;
}

rt_err_t rt_mb_delete (rt_mailbox_t mb)
{
	rt_mb_detach(mb);

	rt_free(mb);

	return RT_EOK;
}

rt_err_t rt_mb_send (rt_mailbox_t mb, rt_uint32_t value)
{
	/* parameter check */
	RT_ASSERT(mb != RT_NULL);

	SDL_SemWait(hmb->mutex);

	/* mailbox is full */
	if (mb->entry == mb->size)
	{
		SDL_SemPost(hmb->mutex);
		return -RT_EFULL;
	}

	/* set ptr */
	mb->msg_pool[mb->in_offset] = value;
	/* increase input offset */
	mb->in_offset = (++ mb->in_offset) % mb->size;
	/* increase message entry */
	mb->entry ++;

	/* post one message */
	SDL_SemPost(hmb->msg);
	SDL_SemPost(hmb->mutex);

	return RT_EOK;
}

rt_err_t rt_mb_recv (rt_mailbox_t mb, rt_uint32_t* value, rt_int32_t timeout)
{
	rt_err_t r;

	/* parameter check */
	RT_ASSERT(mb != RT_NULL);

	SDL_SemWait(hmb->mutex);

	/* mailbox is empty */
	if (mb->entry == 0)
	{
		SDL_SemPost(hmb->mutex);

		if (timeout == RT_WAITING_FOREVER)
		{
			r = SDL_SemWait(hmb->msg);
		}
		else
		{
			r = SDL_SemWaitTimeout(hmb->msg, timeout * 10);
		}

		if (r == SDL_MUTEX_TIMEDOUT) return -RT_ETIMEOUT;
		if (r != 0) return -RT_ERROR;

		SDL_SemWait(hmb->mutex);
	}
	else
	{
		/* take one message */
		SDL_SemWait(hmb->msg);
	}

	/* fill ptr */
	*value = mb->msg_pool[mb->out_offset];

	/* increase output offset */
	mb->out_offset = (++ mb->out_offset) % mb->size;
	/* decrease message entry */
	mb->entry --;

	SDL_SemPost(hmb->mutex);

	return RT_EOK;
}

rt_err_t rt_mb_control(rt_mailbox_t mb, rt_uint8_t cmd, void* arg)
{
	return RT_EOK;
}
