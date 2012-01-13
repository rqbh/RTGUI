/*
 * File      : window.c
 * This file is part of RTGUI in RT-Thread RTOS
 * COPYRIGHT (C) 2012, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-01-13     Grissiom     first version(just a prototype of application API)
 */

#include <rtgui/rtgui_system.h>
#include <rtgui/rtgui_application.h>
#include <rtgui/widgets/workbench.h>

#ifdef RTGUI_USING_SMALL_SIZE
#define RTGUI_EVENT_SIZE	32
#else
#define RTGUI_EVENT_SIZE	256
#endif

struct rtgui_application* rtgui_application_create(
        rt_thread_t tid,
        const unsigned char *panel_name,
        const unsigned char *myname)
{
    struct rtgui_application *app;

    RT_ASSERT(tid != RT_NULL);
    RT_ASSERT(panel_name != RT_NULL);
    RT_ASSERT(myname != RT_NULL);

    app = rtgui_malloc(sizeof(struct rtgui_application));
    if (app == RT_NULL)
        return RT_NULL;

	app->mq = rt_mq_create("rtgui", RTGUI_EVENT_SIZE, 32, RT_IPC_FLAG_FIFO);
    if (app->mq == RT_NULL)
        goto __mq_err;

	app->gui_thread = rtgui_thread_register(tid, app->mq);
    if (app->gui_thread == RT_NULL)
        goto __thread_err;

    app->workbench = rtgui_workbench_create(panel_name, myname);
    if (app->workbench == RT_NULL)
        goto __wb_err;
    else
        return app;

__wb_err:
    rtgui_thread_deregister(app->gui_thread);
__thread_err:
    rt_mq_delete(app->mq);
__mq_err:
    rt_free(app);

    return RT_NULL;
}

static rt_inline _rtgui_application_check(struct rtgui_application *app)
{
    RT_ASSERT(app != RT_NULL);
    RT_ASSERT(app->gui_thread != RT_NULL);
    RT_ASSERT(app->mq != RT_NULL);
    RT_ASSERT(app->workbench != RT_NULL);
}

void rtgui_application_delete(struct rtgui_application *app)
{
    _rtgui_application_check(app);
	rtgui_thread_deregister(app->gui_thread);
	rt_mq_delete(app->mq);
    rtgui_workbench_destroy(app->workbench);
}

rt_bool_t rtgui_application_exec(struct rtgui_application *app)
{
    _rtgui_application_check(app);
	rtgui_workbench_event_loop(app->workbench);
}
