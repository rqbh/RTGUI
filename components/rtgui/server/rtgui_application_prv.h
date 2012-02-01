/*
 * File      : rtgui_application_prv.h
 * This file is part of RTGUI in RT-Thread RTOS
 * COPYRIGHT (C) 2012, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-02-01     Grissiom     first version
 */

/*
 * NOTE: All the APIs in this file is intent to use within RTGUI. No user
 * should use them.
 */

/* start a event loop on application @param app. the event loop will exit
 * either the @loop_guard became RT_FALSE or the application marked as closed.
 */
rt_base_t _rtgui_application_event_loop(struct rtgui_application *app,
										rt_bool_t *loop_guard);
