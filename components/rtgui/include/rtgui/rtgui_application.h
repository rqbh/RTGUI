/*
 * File      : rtgui_application.h
 * This file is part of RTGUI in RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2009, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-01-13     Grissiom     first version
 */
#ifndef __RTGUI_APPLICATION_H__
#define __RTGUI_APPLICATION_H__

#include <rtthread.h>
#include <rtgui/rtgui.h>
#include <rtgui/rtgui_system.h>

DECLARE_CLASS_TYPE(application);

/** Gets the type of a application */
#define RTGUI_APPLICATION_TYPE       (RTGUI_TYPE(application))
/** Casts the object to an rtgui_workbench */
#define RTGUI_APPLICATION(obj)       (RTGUI_OBJECT_CAST((obj), RTGUI_APPLICATION_TYPE, struct rtgui_application))
/** Checks if the object is an rtgui_workbench */
#define RTGUI_IS_APPLICATION(obj)    (RTGUI_OBJECT_CHECK_TYPE((obj), RTGUI_APPLICATION_TYPE))

enum rtgui_application_flag
{
	/* use this flag to exit from the event loop. It's different from the
	 * EXITED flag in that when it marked as closed, it's not guarantee that
	 * the event loop is exited already. But EXITED do.*/
	RTGUI_APPLICATION_FLAG_CLOSED  = 0x02,
	RTGUI_APPLICATION_FLAG_EXITED  = 0x04,

	RTGUI_APPLICATION_FLAG_SHOWN   = 0x08
};

typedef void (*rtgui_idle_func)(struct rtgui_object* obj, struct rtgui_event *event);

struct rtgui_application
{
	struct rtgui_object parent;

	/* application name */
	unsigned char *name;

	enum rtgui_application_flag state_flag;

	rt_base_t exit_code;

	/* the thread id */
	rt_thread_t tid;
    rt_thread_t server;

	/* the message queue of thread */
	rt_mq_t mq;
	/* event buffer */
	rt_uint8_t event_buffer[RTGUI_EVENT_BUFFER_SIZE];

	/* the object I sent event to */
	struct rtgui_object *root_object;
	/* if not RT_NULL, the application is modaled by modal_object. If is
	 * RT_NULL, nothing modals. */
	struct rtgui_object *modal_object;
	/* the widget that will receive the KBD event by default */
	struct rtgui_widget *focused_widget;

	/* on idle event handler */
	rtgui_idle_func on_idle;
};

/**
 * create an application named @myname on thread @param tid
 */
struct rtgui_application* rtgui_application_create(
        rt_thread_t tid,
        const char *myname);
void rtgui_application_destroy(struct rtgui_application *app);
rt_err_t rtgui_application_show(struct rtgui_application *app);
rt_err_t rtgui_application_hide(struct rtgui_application *app);

rt_base_t rtgui_application_run(struct rtgui_application *app);
void rtgui_application_exit(struct rtgui_application *app, rt_base_t code);

void rtgui_application_set_onidle(rtgui_idle_func onidle);
rtgui_idle_func rtgui_application_get_onidle(void);
struct rtgui_application* rtgui_application_self(void);

rt_thread_t rtgui_application_get_server(void);

void rtgui_application_set_root_object(struct rtgui_object* object);
struct rtgui_object* rtgui_application_get_root_object(void);

struct rtgui_event;
rt_err_t rtgui_application_send(rt_thread_t tid, struct rtgui_event* event, rt_size_t event_size);
rt_err_t rtgui_application_send_urgent(rt_thread_t tid, struct rtgui_event* event, rt_size_t event_size);
rt_err_t rtgui_application_send_sync(rt_thread_t tid, struct rtgui_event* event, rt_size_t event_size);
rt_err_t rtgui_application_ack(struct rtgui_event* event, rt_int32_t status);
rt_err_t rtgui_application_recv(struct rtgui_event* event, rt_size_t event_size);
rt_err_t rtgui_application_recv_nosuspend(struct rtgui_event* event, rt_size_t event_size);
rt_err_t rtgui_application_recv_filter(rt_uint32_t type, struct rtgui_event* event, rt_size_t event_size);

#endif /* end of include guard: RTGUI_APPLICATION_H */
