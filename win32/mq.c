#include <rtthread.h>
#include "list.h"

#include <SDL/SDL.h>

rt_list_t _mq_list;
SDL_mutex* _mq_list_mutex;

void rt_mq_system_init()
{
	rt_list_init(&_mq_list);
	_mq_list_mutex = SDL_CreateMutex();
}

#define hmq ((struct host_mq*)(mq->host_mq))
struct host_mq
{
	SDL_sem *mutex;
	SDL_sem *msg;
};

struct rt_mq_message
{
	struct rt_mq_message* next;
};

rt_err_t rt_mq_init(rt_mq_t mq, const char* name, void *msgpool, rt_size_t msg_size, rt_size_t pool_size, rt_uint8_t flag)
{
	size_t index;
	struct rt_mq_message* head;

	/* parameter check */
	RT_ASSERT(mq != RT_NULL);

	/* set parent flag */
	mq->flag = flag;

	for (index = 0; index < RT_NAME_MAX; index ++)
	{
		mq->name[index] = name[index];
	}

	/* append to mq list */
	SDL_mutexP(_mq_list_mutex);
	rt_list_insert_after(&(_mq_list), &(mq->list));
	SDL_mutexV(_mq_list_mutex);

	/* set message pool */
	mq->msg_pool 	= msgpool;

	/* get correct message size */
	mq->msg_size	= RT_ALIGN(msg_size,  RT_ALIGN_SIZE);
	mq->max_msgs	= pool_size / (mq->msg_size + sizeof(struct rt_mq_message));

	/* init message list */
	mq->msg_queue_head = RT_NULL;
	mq->msg_queue_tail = RT_NULL;

	/* init message empty list */
	mq->msg_queue_free = RT_NULL;

	for (index = 0; index < mq->max_msgs; index ++)
	{
		head = (struct rt_mq_message*)((rt_uint8_t*)mq->msg_pool +
			index * (mq->msg_size + sizeof(struct rt_mq_message)));
		head->next = mq->msg_queue_free;
		mq->msg_queue_free = head;
	}

	/* the initial entry is zero */
	mq->entry		= 0;

	/* init mutex */
	mq->host_mq		= (void*) malloc(sizeof(struct host_mq));
	hmq->mutex		= SDL_CreateSemaphore(1);
	hmq->msg		= SDL_CreateSemaphore(0);

	return RT_EOK;
}

rt_err_t rt_mq_detach(rt_mq_t mq)
{
	/* parameter check */
	RT_ASSERT(mq != RT_NULL);

	SDL_DestroySemaphore(hmq->msg);
	SDL_DestroySemaphore(hmq->mutex);
	
	free(mq->host_mq);
	mq->host_mq = NULL;

	/* remove from list */
	SDL_mutexP(_mq_list_mutex);
	rt_list_remove(&(mq->list));
	SDL_mutexV(_mq_list_mutex);

	return RT_EOK;
}

rt_mq_t rt_mq_create (const char* name, rt_size_t msg_size, rt_size_t max_msgs, rt_uint8_t flag)
{
	size_t index;
	struct rt_messagequeue* mq;
	struct rt_mq_message* head;

	/* allocate object */
	mq = (rt_mq_t) rt_malloc(sizeof(struct rt_messagequeue));
	if (mq == RT_NULL) return mq;

	/* set flag */
	mq->flag = flag;

	for (index = 0; index < RT_NAME_MAX; index ++)
	{
		mq->name[index] = name[index];
	}

	/* append to mq list */
	SDL_mutexP(_mq_list_mutex);
	rt_list_insert_after(&(_mq_list), &(mq->list));
	SDL_mutexV(_mq_list_mutex);

	/* init message queue */

	/* get correct message size */
	mq->msg_size	= RT_ALIGN(msg_size, RT_ALIGN_SIZE);
	mq->max_msgs	= max_msgs;

	/* allocate message pool */
	mq->msg_pool = rt_malloc((mq->msg_size + sizeof(struct rt_mq_message))* mq->max_msgs);
	if (mq->msg_pool == RT_NULL)
	{
		rt_mq_delete(mq);
		return RT_NULL;
	}

	/* init message list */
	mq->msg_queue_head = RT_NULL;
	mq->msg_queue_tail = RT_NULL;

	/* init message empty list */
	mq->msg_queue_free = RT_NULL;

	for (index = 0; index < mq->max_msgs; index ++)
	{
		head = (struct rt_mq_message*)((rt_uint8_t*)mq->msg_pool +
			index * (mq->msg_size + sizeof(struct rt_mq_message)));
		head->next = mq->msg_queue_free;
		mq->msg_queue_free = head;
	}

	/* the initial entry is zero */
	mq->entry		= 0;

	/* init mutex */
	mq->host_mq		= (void*) malloc(sizeof(struct host_mq));
	hmq->mutex		= SDL_CreateSemaphore(1);
	hmq->msg		= SDL_CreateSemaphore(0);

	return mq;
}

rt_err_t rt_mq_delete (rt_mq_t mq)
{
	/* parameter check */
	RT_ASSERT(mq != RT_NULL);

	rt_mq_detach(mq);

	/* free mailbox pool */
	rt_free(mq->msg_pool);

	/* delete mailbox object */
	rt_free(mq);

	return RT_EOK;
}

rt_err_t rt_mq_send (rt_mq_t mq, void* buffer, rt_size_t size)
{
	struct rt_mq_message *msg;

	/* greater than one message size */
	if (size > mq->msg_size) return -RT_ERROR;

	SDL_SemWait(hmq->mutex);

	/* get a free list, there must be an empty item */
	msg = (struct rt_mq_message*)mq->msg_queue_free;

	/* message queue is full */
	if (msg == RT_NULL)
	{
		SDL_SemPost(hmq->mutex);
		return -RT_EFULL;
	}

	/* move free list pointer */
	mq->msg_queue_free = msg->next;

	/* copy buffer */
	rt_memcpy(msg + 1, buffer, size);

	/* link msg to message queue */
	if (mq->msg_queue_tail != RT_NULL)
	{
		/* if the tail exists, */
		((struct rt_mq_message*)mq->msg_queue_tail)->next = msg;
	}
	/* the msg is the new tail of list, the next shall be NULL */
	msg->next = RT_NULL;

	/* set new tail */
	mq->msg_queue_tail = msg;

	/* if the head is empty, set head */
	if (mq->msg_queue_head == RT_NULL)mq->msg_queue_head = msg;

	/* increase message entry */
	mq->entry ++;

	/* post one message */
	SDL_SemPost(hmq->msg);
	SDL_SemPost(hmq->mutex);

	return RT_EOK;
}

rt_err_t rt_mq_urgent(rt_mq_t mq, void* buffer, rt_size_t size)
{
	struct rt_mq_message *msg;

	/* greater than one message size */
	if (size > mq->msg_size) return -RT_ERROR;

	SDL_SemWait(hmq->mutex);

	/* get a free list, there must be an empty item */
	msg = (struct rt_mq_message*)mq->msg_queue_free;

	/* message queue is full */
	if (msg == RT_NULL)
	{
		SDL_SemPost(hmq->mutex);
		return -RT_EFULL;
	}

	/* move free list pointer */
	mq->msg_queue_free = msg->next;

	/* copy buffer */
	rt_memcpy(msg + 1, buffer, (unsigned short)size);

	/* link msg to the beginning of message queue */
	msg->next = mq->msg_queue_head;
	mq->msg_queue_head = msg;

	/* if there is no tail */
	if (mq->msg_queue_tail == RT_NULL) mq->msg_queue_tail = msg;

	/* increase message entry */
	mq->entry ++;

	/* post one message */
	SDL_SemPost(hmq->msg);
	SDL_SemPost(hmq->mutex);

	return RT_EOK;
}

rt_err_t rt_mq_recv (rt_mq_t mq, void* buffer, rt_size_t size, rt_int32_t timeout)
{
	rt_err_t r;
	struct rt_mq_message *msg;

	SDL_SemWait(hmq->mutex);

	/* mq is empty */
	if (mq->entry == 0)
	{
		SDL_SemPost(hmq->mutex);

		if (timeout == RT_WAITING_FOREVER)
		{
			r = SDL_SemWait(hmq->msg);
		}
		else
		{
			r = SDL_SemWaitTimeout(hmq->msg, timeout * 10);
		}

		if (r == SDL_MUTEX_TIMEDOUT) 
			return -RT_ETIMEOUT;
		if (r != 0) return -RT_ERROR;

		SDL_SemWait(hmq->mutex);
	}
	else
	{
		/* take one message */
		SDL_SemWait(hmq->msg);
	}

	/* get message from queue */
	msg = (struct rt_mq_message*) mq->msg_queue_head;

	/* move message queue head */
	mq->msg_queue_head = msg->next;

	/* reach queue tail, set to NULL */
	if (mq->msg_queue_tail == msg) mq->msg_queue_tail = RT_NULL;

	/* copy message */
	rt_memcpy(buffer, msg + 1,
		size > mq->msg_size? (unsigned short)(mq->msg_size) : (unsigned short)size);

	/* put message to free list */
	msg->next = (struct rt_mq_message*)mq->msg_queue_free;
	mq->msg_queue_free = msg;

	/* decrease message entry */
	mq->entry --;

	SDL_SemPost(hmq->mutex);

	return RT_EOK;
}

rt_err_t rt_mq_control(rt_mq_t mq, rt_uint8_t cmd, void* arg)
{
	return RT_EOK;
}
