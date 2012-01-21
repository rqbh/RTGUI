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
#include <rtgui/widgets/toplevel.h>

DECLARE_CLASS_TYPE(application);

/** Gets the type of a application */
#define RTGUI_APPLICATION_TYPE       (RTGUI_TYPE(application))
/** Casts the object to an rtgui_workbench */
#define RTGUI_APPLICATION(obj)       (RTGUI_OBJECT_CAST((obj), RTGUI_APPLICATION_TYPE, struct rtgui_application))
/** Checks if the object is an rtgui_workbench */
#define RTGUI_IS_APPLICATION(obj)    (RTGUI_OBJECT_CHECK_TYPE((obj), RTGUI_APPLICATION_TYPE))

enum rtgui_application_flag
{
	RTGUI_APPLICATION_FLAG_MODALED = 0x01,
	/* use this flag to exit from the event loop. It's different from the
	 * EXITED flag in that when it marked as closed, it's not guarantee that
	 * the event loop is exited already. But EXITED do.*/
	RTGUI_APPLICATION_FLAG_CLOSED  = 0x02,
	RTGUI_APPLICATION_FLAG_EXITED  = 0x04,

	RTGUI_APPLICATION_FLAG_SHOWN   = 0x08
};
#define RTGUI_APPLICATION_IS_MODALED(w) ((w)->state_flag & RTGUI_APPLICATION_FLAG_MODALED)

struct rtgui_application
{
	struct rtgui_toplevel parent;

	/* panel id */
	struct rtgui_panel *panel;

	/* application name */
	unsigned char *name;

	enum rtgui_application_flag state_flag;

	rt_base_t exit_code;

	// TODO: remove some unneeded variables
	rtgui_modal_code_t modal_code;
	rtgui_widget_t *modal_widget;
	rtgui_container_t* current_view;

    rt_mq_t mq;
    rtgui_thread_t *gui_thread;
};

struct rtgui_application* rtgui_application_create(
        rt_thread_t tid,
        const char *panel_name,
        const char *myname);
void rtgui_application_destroy(struct rtgui_application *app);
rt_err_t rtgui_application_show(struct rtgui_application *app);
rt_err_t rtgui_application_hide(struct rtgui_application *app);

rt_base_t rtgui_application_exec(struct rtgui_application *app);
void rtgui_application_exit(rt_base_t code);

void rtgui_application_add_container(struct rtgui_application *app, rtgui_container_t *view);
void rtgui_application_remove_container(struct rtgui_application *app, rtgui_container_t *view);
void rtgui_application_show_container(struct rtgui_application *app, rtgui_container_t *view);
void rtgui_application_hide_container(struct rtgui_application *app, rtgui_container_t *view);
#endif /* end of include guard: RTGUI_APPLICATION_H */
