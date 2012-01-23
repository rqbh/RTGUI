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
#include <rtgui/widgets/toplevel.h>

#ifdef RTGUI_USING_SMALL_SIZE
#define RTGUI_EVENT_SIZE	32
#else
#define RTGUI_EVENT_SIZE	256
#endif

rt_bool_t rtgui_application_event_handler(struct rtgui_object *object, struct rtgui_event *event);

static void _rtgui_application_constructor(struct rtgui_application *app)
{
	/* set event handler */
	rtgui_object_set_event_handler(RTGUI_OBJECT(app),
			                       rtgui_application_event_handler);

	/* set EXITED so we can destroy an application that just created */
	app->state_flag   = RTGUI_APPLICATION_FLAG_EXITED;
	app->exit_code    = 0;
	app->panel        = RT_NULL;
	app->name         = RT_NULL;
	app->current_view = RT_NULL;
	app->modal_code   = RTGUI_MODAL_OK;
	app->modal_widget = RT_NULL;
}

static rt_bool_t _rtgui_application_detach_panel(
		struct rtgui_application *app)
{
	struct rtgui_event_panel_detach edetach;

	if (RTGUI_TOPLEVEL(app)->server == RT_NULL)
		return RT_FALSE;

	RTGUI_EVENT_PANEL_DETACH_INIT(&edetach);

	/* detach from panel */
	edetach.panel = app->panel;

	/* send PANEL DETACH to server */
	if (rtgui_thread_send_sync(RTGUI_TOPLEVEL(app)->server,
				               RTGUI_EVENT(&edetach),
							   sizeof(struct rtgui_event_panel_detach)) != RT_EOK)
		return RT_FALSE;

	RTGUI_TOPLEVEL(app)->server = RT_NULL;

	return RT_TRUE;
}

static void _rtgui_application_destructor(struct rtgui_application *app)
{
	RT_ASSERT(app != RT_NULL);

	if (_rtgui_application_detach_panel(
				app) != RT_TRUE)
		return;
	/* release name */
	rt_free(app->name);
	app->name = RT_NULL;
}

DEFINE_CLASS_TYPE(application, "application",
	RTGUI_TOPLEVEL_TYPE,
	_rtgui_application_constructor,
	_rtgui_application_destructor,
	sizeof(struct rtgui_application));

static rt_bool_t _rtgui_application_attach_panel(
		struct rtgui_application *app,
		rt_thread_t server,
        const char *panel_name)
{
	/* the buffer uses to receive event */
	union
	{
		struct rtgui_event_panel_attach ecreate;
		struct rtgui_event_panel_info epanel;

		char buffer[256];	/* use to recv other information */
	} event;

	/* create application in server */
	RTGUI_EVENT_PANEL_ATTACH_INIT(&(event.ecreate));

	/* set the panel name and application */
	rt_strncpy(event.ecreate.panel_name, panel_name, RTGUI_NAME_MAX);
	event.ecreate.application = app;

	/* send PANEL ATTACH to server */
	if (rtgui_thread_send_sync(server,
				               &(event.ecreate.parent),
				               sizeof(struct rtgui_event_panel_attach))
			!= RTGUI_STATUS_OK)
	{
		return RT_FALSE;
	}

	/* get PANEL INFO */
	rtgui_thread_recv_filter(RTGUI_EVENT_PANEL_INFO, &(event.epanel.parent), sizeof(event));

	/* set panel */
	app->panel = (struct rtgui_panel*)event.epanel.panel;

	/* set extent of application */
	rtgui_widget_set_rect(RTGUI_WIDGET(app), &(event.epanel.extent));

	return RT_TRUE;
}

struct rtgui_application* rtgui_application_create(
        rt_thread_t tid,
        const char *panel_name,
        const char *myname)
{
	rt_thread_t server;
	struct rtgui_application *app;

	RT_ASSERT(tid != RT_NULL);
	RT_ASSERT(panel_name != RT_NULL);
	RT_ASSERT(myname != RT_NULL);

	/* the server thread id */
	server = rtgui_thread_get_server();
	if (server == RT_NULL)
	{
		rt_kprintf("can't find rtgui server\n");
		return RT_NULL;
	}

	/* create application */
	app = (struct rtgui_application*)rtgui_widget_create(RTGUI_APPLICATION_TYPE);
	if (app == RT_NULL)
		return RT_NULL;

	app->mq = rt_mq_create("rtgui", RTGUI_EVENT_SIZE, 32, RT_IPC_FLAG_FIFO);
	if (app->mq == RT_NULL)
		goto __mq_err;

	app->gui_thread = rtgui_thread_register(tid, app->mq);
	if (app->gui_thread == RT_NULL)
		goto __thread_err;

	if (_rtgui_application_attach_panel(
				app,
				server,
				panel_name) != RT_TRUE)
		goto __ap_err;

	/* connected */
	RTGUI_TOPLEVEL(app)->server = server;

	/* set application in thread */
	rtgui_thread_set_widget(RTGUI_WIDGET(app));

	/* set application title */
	app->name = (unsigned char*)rt_strdup((char*)myname);
	if (app->name != RT_NULL)
		return app;

	_rtgui_application_detach_panel(app);
__ap_err:
	rtgui_thread_deregister(app->gui_thread->tid);
__thread_err:
	rt_mq_delete(app->mq);
__mq_err:
	rtgui_widget_destroy(RTGUI_WIDGET(app));

	return RT_NULL;
}

rt_inline _rtgui_application_check(struct rtgui_application *app)
{
    RT_ASSERT(app != RT_NULL);
    RT_ASSERT(app->gui_thread != RT_NULL);
    RT_ASSERT(app->mq != RT_NULL);
}

void rtgui_application_destroy(struct rtgui_application *app)
{
    _rtgui_application_check(app);

	if (!(app->state_flag & RTGUI_APPLICATION_FLAG_EXITED))
	{
		rt_kprintf("cannot destroy a running application: %s.\n",
				   app->name);
		return;
	}

	rtgui_thread_deregister(app->gui_thread->tid);
	rt_mq_delete(app->mq);

	rtgui_widget_destroy(RTGUI_WIDGET(app));
}

rt_bool_t rtgui_application_event_handler(struct rtgui_object* object, struct rtgui_event* event)
{
	struct rtgui_application* app;

	RT_ASSERT(object != RT_NULL);
	RT_ASSERT(event != RT_NULL);

	app = RTGUI_APPLICATION(object);

	switch (event->type)
	{
	case RTGUI_EVENT_PANEL_DETACH:
		RTGUI_WIDGET_HIDE(RTGUI_WIDGET(app));
		RTGUI_TOPLEVEL(app)->server = RT_NULL;
		return RT_TRUE;

	case RTGUI_EVENT_PANEL_SHOW:
		/* show app in server */
		rtgui_application_show(app);
		break;

	case RTGUI_EVENT_PANEL_HIDE:
		/* hide widget */
		RTGUI_WIDGET_HIDE(RTGUI_WIDGET(app));
		break;

	case RTGUI_EVENT_MOUSE_MOTION:
		{
			struct rtgui_event_mouse* emouse = (struct rtgui_event_mouse*)event;
			struct rtgui_toplevel* top = RTGUI_TOPLEVEL(emouse->wid);

			/* check the destination window */
			if (top != RT_NULL && RTGUI_OBJECT(top)->event_handler != RT_NULL)
			{
				RTGUI_OBJECT(top)->event_handler(RTGUI_OBJECT(top), event);
			}
			else
			{
				/* let viewer to handle it */
				rtgui_container_t* view = app->current_view;
				if (view != RT_NULL &&
					RTGUI_OBJECT(view)->event_handler != RT_NULL)
				{
					RTGUI_OBJECT(view)->event_handler(RTGUI_OBJECT(view), event);
				}
			}
		}
		break;

	case RTGUI_EVENT_MOUSE_BUTTON:
		{
			struct rtgui_event_mouse* emouse = (struct rtgui_event_mouse*)event;
			struct rtgui_toplevel* top = RTGUI_TOPLEVEL(emouse->wid);

			/* check whether has widget which handled mouse event before */
			if (RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(app) != RT_NULL)
			{
				struct rtgui_event_mouse* emouse;

				emouse = (struct rtgui_event_mouse*)event;

				if (rtgui_rect_contains_point(
							&(RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(app)->extent),
							emouse->x, emouse->y) != RT_EOK)
				{
					RTGUI_OBJECT(RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(app)
							)->event_handler(
								RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(app),
								event);

					/* clean last mouse event handled widget */
					RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(app) = RT_NULL;
				}
			}

			/* check the destination window */
			if (top != RT_NULL && RTGUI_OBJECT(top)->event_handler != RT_NULL)
			{
				RTGUI_OBJECT(top)->event_handler(RTGUI_OBJECT(top), event);
			}
			else
			{
				if (RTGUI_APPLICATION_IS_MODALED(app))
				{
					/* let modal widget to handle it */
					if (app->modal_widget != RT_NULL &&
						RTGUI_OBJECT(app->modal_widget)->event_handler != RT_NULL)
					{
						RTGUI_OBJECT(app->modal_widget)->event_handler(
								RTGUI_OBJECT(app->modal_widget),
								event);
					}
				}
				else
				{
					/* let viewer to handle it */
					rtgui_container_t* view = app->current_view;
					if (view != RT_NULL &&
							RTGUI_OBJECT(view)->event_handler != RT_NULL)
					{
						RTGUI_OBJECT(view)->event_handler(
								RTGUI_OBJECT(view),
								event);
					}
				}
			}
		}
		break;

	case RTGUI_EVENT_KBD:
		{
			struct rtgui_event_kbd* kbd = (struct rtgui_event_kbd*)event;
			struct rtgui_toplevel* top = RTGUI_TOPLEVEL(kbd->wid);

			/* check the destination window */
			if (top != RT_NULL && RTGUI_OBJECT(top)->event_handler != RT_NULL)
			{
				RTGUI_OBJECT(top)->event_handler(RTGUI_OBJECT(top), event);
			}
			else
			{
				if (RTGUI_APPLICATION_IS_MODALED(app))
				{
					/* let modal widget to handle it */
					if (app->modal_widget != RT_NULL &&
						RTGUI_OBJECT(app->modal_widget)->event_handler != RT_NULL)
					{
						RTGUI_OBJECT(app->modal_widget
								)->event_handler(app->modal_widget, event);
					}
				}
				else
				{
					if (RTGUI_CONTAINER(object)->focused == RTGUI_WIDGET(object))
					{
						/* set focused widget to the current view */
						if (app->current_view != RT_NULL)
							rtgui_widget_focus(RTGUI_WIDGET(RTGUI_CONTAINER(app->current_view)->focused));
					}

					return rtgui_toplevel_event_handler(object, event);
				}
			}
		}
		break;

	case RTGUI_EVENT_PAINT:
		{
			struct rtgui_event_paint* epaint = (struct rtgui_event_paint*)event;
			struct rtgui_toplevel* top = RTGUI_TOPLEVEL(epaint->wid);

			/* check the destination window */
			if (top != RT_NULL && RTGUI_OBJECT(top)->event_handler != RT_NULL)
			{
				RTGUI_OBJECT(top)->event_handler(RTGUI_OBJECT(top), event);
			}
			else
			{
				rtgui_container_t* view;

				/* un-hide app */
				RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(app));

				/* paint a view */
				view = app->current_view;
				if (view != RT_NULL)
				{
					/* remake a paint event */
					RTGUI_EVENT_PAINT_INIT(epaint);
					epaint->wid = RT_NULL;

					/* send this event to the view */
					if (RTGUI_OBJECT(view)->event_handler != RT_NULL)
					{
						RTGUI_OBJECT(view)->event_handler(RTGUI_OBJECT(view), event);
					}
				}
				else
				{
					struct rtgui_dc* dc;
					struct rtgui_rect rect;

					dc = rtgui_dc_begin_drawing(app);
					rtgui_widget_get_rect(RTGUI_WIDGET(object), &rect);
					rtgui_dc_fill_rect(dc, &rect);
					rtgui_dc_end_drawing(dc);
				}
			}
		}
		break;

	case RTGUI_EVENT_CLIP_INFO:
		{
			struct rtgui_event_clip_info* eclip = (struct rtgui_event_clip_info*)event;
			struct rtgui_widget* dest_widget = RTGUI_WIDGET(eclip->wid);

			if (dest_widget != RT_NULL && RTGUI_OBJECT(dest_widget)->event_handler != RT_NULL)
			{
				RTGUI_OBJECT(dest_widget)->event_handler(
						RTGUI_OBJECT(dest_widget),
						event);
			}
			else
			{
				return rtgui_toplevel_event_handler(object, event);
			}
		}
		break;

	case RTGUI_EVENT_WIN_CLOSE:
	case RTGUI_EVENT_WIN_ACTIVATE:
	case RTGUI_EVENT_WIN_DEACTIVATE:
		{
			struct rtgui_event_win* wevent = (struct rtgui_event_win*)event;
			struct rtgui_widget* dest_widget = RTGUI_WIDGET(wevent->wid);
			if (dest_widget != RT_NULL &&
				RTGUI_OBJECT(dest_widget)->event_handler != RT_NULL)
			{
				RTGUI_OBJECT(dest_widget)->event_handler(
						RTGUI_OBJECT(dest_widget),
						event);
			}
		}
		break;

	case RTGUI_EVENT_WIN_MOVE:
		{
			struct rtgui_event_win_move* wevent = (struct rtgui_event_win_move*)event;
			struct rtgui_toplevel* top = RTGUI_TOPLEVEL(wevent->wid);
			if (top != RT_NULL && RTGUI_OBJECT(top)->event_handler != RT_NULL)
			{
				RTGUI_OBJECT(top)->event_handler(
						RTGUI_OBJECT(top),
						event);
			}
		}
		break;

	default:
		return rtgui_toplevel_event_handler(object, event);
	}

	return RT_TRUE;
}

/* Internal API. Should not called by user. This can be simplified by only
 * dispatch the events to root window. But this will require a more context
 * variable to determine whether we should exit */
rt_bool_t _rtgui_application_event_loop(struct rtgui_application *app)
{
	rt_err_t result;
	struct rtgui_event *event;

	_rtgui_application_check(app);

	/* point to event buffer */
	event = (struct rtgui_event*)app->gui_thread->event_buffer;

	if (app->state_flag & RTGUI_APPLICATION_FLAG_MODALED)
	{
		/* event loop for modal mode shown view */
		while (app->state_flag & RTGUI_APPLICATION_FLAG_MODALED)
		{
			if (app->gui_thread->on_idle != RT_NULL)
			{
				result = rtgui_thread_recv_nosuspend(event, RTGUI_EVENT_BUFFER_SIZE);
				if (result == RT_EOK)
					RTGUI_OBJECT(app)->event_handler(RTGUI_OBJECT(app), event);
				else if (result == -RT_ETIMEOUT)
					app->gui_thread->on_idle(RTGUI_OBJECT(app), RT_NULL);
			}
			else
			{
				result = rtgui_thread_recv(event, RTGUI_EVENT_BUFFER_SIZE);
				if (result == RT_EOK)
					RTGUI_OBJECT(app)->event_handler(RTGUI_OBJECT(app), event);
			}
		}
	}
	else
	{
		while (!(app->state_flag & RTGUI_APPLICATION_FLAG_CLOSED))
		{
			if (app->gui_thread->on_idle != RT_NULL)
			{
				result = rtgui_thread_recv_nosuspend(event, RTGUI_EVENT_BUFFER_SIZE);
				if (result == RT_EOK)
					RTGUI_OBJECT(app)->event_handler(RTGUI_OBJECT(app), event);
				else if (result == -RT_ETIMEOUT)
					app->gui_thread->on_idle(RTGUI_OBJECT(app), RT_NULL);
			}
			else
			{
				result = rtgui_thread_recv(event, RTGUI_EVENT_BUFFER_SIZE);
				if (result == RT_EOK)
					RTGUI_OBJECT(app)->event_handler(RTGUI_OBJECT(app), event);
			}
		}
	}

	return RT_TRUE;
}

// TODO: maybe it's better to put show/hide in window.c
rt_err_t rtgui_application_show(struct rtgui_application* app)
{
	struct rtgui_event_panel_show eraise;

	RT_ASSERT(app != RT_NULL);

	if (RTGUI_TOPLEVEL(app)->server == RT_NULL)
		return -RT_ERROR;

	RTGUI_EVENT_PANEL_SHOW_INIT(&eraise);
	eraise.application = app;

	eraise.panel = app->panel;
	if (rtgui_thread_send_sync(app->parent.server, RTGUI_EVENT(&eraise),
				sizeof(struct rtgui_event_panel_show)) != RT_EOK)
		return -RT_ERROR;

	RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(app));
	rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(app));

	app->state_flag |= RTGUI_APPLICATION_FLAG_SHOWN;

	return RT_EOK;
}

rt_err_t rtgui_application_hide(struct rtgui_application* app)
{
	struct rtgui_event_panel_hide ehide;

	RT_ASSERT(app != RT_NULL);

	if (RTGUI_TOPLEVEL(app)->server == RT_NULL)
		return RT_FALSE;

	RTGUI_EVENT_PANEL_HIDE_INIT(&ehide);

	ehide.panel = app->panel;
	if (rtgui_thread_send_sync(RTGUI_TOPLEVEL(app)->server, RTGUI_EVENT(&ehide),
				sizeof(struct rtgui_event_panel_hide)) != RT_EOK)
		return -RT_ERROR;

	RTGUI_WIDGET_HIDE(RTGUI_WIDGET(app));
	rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(app));

	app->state_flag &= ~RTGUI_APPLICATION_FLAG_SHOWN;

	return RT_EOK;
}

rt_base_t rtgui_application_exec(struct rtgui_application *app)
{
	_rtgui_application_check(app);

	if (!(app->state_flag & RTGUI_APPLICATION_FLAG_SHOWN))
		rtgui_application_show(app);

	/* cleanup */
	app->exit_code = 0;
	app->state_flag &= ~RTGUI_APPLICATION_FLAG_CLOSED;

	app->state_flag &= ~RTGUI_APPLICATION_FLAG_EXITED;
	_rtgui_application_event_loop(app);
	app->state_flag |= RTGUI_APPLICATION_FLAG_EXITED;

	rtgui_application_hide(app);

	return app->exit_code;
}

void rtgui_application_exit(struct rtgui_application* app, rt_base_t code)
{
	_rtgui_application_detach_panel(app);
	app->state_flag |= RTGUI_APPLICATION_FLAG_CLOSED;
	app->exit_code = code;
}

void rtgui_application_add_container(struct rtgui_application* app, rtgui_container_t* view)
{
	rtgui_container_add_child(RTGUI_CONTAINER(app), RTGUI_WIDGET(view));
	/* hide view in default */
	RTGUI_WIDGET_HIDE(RTGUI_WIDGET(view));

	/* reset view extent */
	rtgui_widget_set_rect(RTGUI_WIDGET(view), &(RTGUI_WIDGET(app)->extent));
}

void rtgui_application_remove_container(struct rtgui_application* app, rtgui_container_t* view)
{
	if (view == app->current_view)
		rtgui_application_hide_container(app, view);

	rtgui_container_remove_child(RTGUI_CONTAINER(app), RTGUI_WIDGET(view));
}

void rtgui_application_show_container(struct rtgui_application* app, rtgui_container_t* view)
{
	RT_ASSERT(app != RT_NULL);
	RT_ASSERT(view != RT_NULL);

	/* already shown */
	if (app->current_view == view) return;

	if (app->current_view != RT_NULL)
	{
		/* hide old view */
		RTGUI_WIDGET_HIDE(RTGUI_WIDGET(app->current_view));
	}

	/* show view */
	RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(view));
	app->current_view = view;

	/* update app clip */
	rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(app));

	if (!RTGUI_WIDGET_IS_HIDE(RTGUI_WIDGET(app)))
	{
		rtgui_widget_update(RTGUI_WIDGET(view));
	}
}

void rtgui_application_hide_container(struct rtgui_application* app, rtgui_container_t* view)
{
	RT_ASSERT(app != RT_NULL);
	RT_ASSERT(view != RT_NULL);

	/* hide view */
	RTGUI_WIDGET_HIDE(RTGUI_WIDGET(view));

	if (view == app->current_view)
	{
		rtgui_container_t *next_view;

		app->current_view = RT_NULL;

		next_view = RTGUI_CONTAINER(rtgui_widget_get_next_sibling(RTGUI_WIDGET(view)));
		if (next_view == RT_NULL)
			next_view = RTGUI_CONTAINER(rtgui_widget_get_prev_sibling(RTGUI_WIDGET(view)));

		if (next_view != RT_NULL)
		{
			rtgui_container_show(next_view, RT_FALSE);
		}
		else
		{
			/* update app clip */
			rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(app));
		}
	}
}
