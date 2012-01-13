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
#include <rtgui/widgets/workbench.h>

struct rtgui_application
{
    struct rtgui_workbench *workbench;
    rt_mq_t mq;
    rtgui_thread_t *gui_thread;
};

struct rtgui_application* rtgui_application_create(
        rt_thread_t tid,
        const unsigned char *panel_name,
        const unsigned char *myname);
void rtgui_application_delete(struct rtgui_application *app);
rt_bool_t rtgui_application_exec(struct rtgui_application *app);
#endif /* end of include guard: RTGUI_APPLICATION_H */
