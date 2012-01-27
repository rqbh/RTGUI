/*
 * File      : window.c
 * This file is part of RTGUI in RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2009, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-10-04     Bernard      first version
 */
#include <rtgui/dc.h>
#include <rtgui/color.h>
#include <rtgui/image.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/rtgui_application.h>

#include <rtgui/widgets/window.h>
#include <rtgui/widgets/button.h>

static void _rtgui_win_constructor(rtgui_win_t *win)
{
	/* init window attribute */
	win->on_activate	= RT_NULL;
	win->on_deactivate	= RT_NULL;
	win->on_close		= RT_NULL;
	win->title			= RT_NULL;
	win->modal_code		= RTGUI_MODAL_OK;
	win->modal_widget	= RT_NULL;

	/* set window hide */
	RTGUI_WIDGET_HIDE(RTGUI_WIDGET(win));

	/* set window style */
	win->style = RTGUI_WIN_STYLE_DEFAULT;

	win->flag  = RTGUI_WIN_FLAG_INIT;

	rtgui_object_set_event_handler(RTGUI_OBJECT(win), rtgui_win_event_handler);

	/* init user data */
	win->user_data = 0;
}

static void _rtgui_win_destructor(rtgui_win_t* win)
{
	struct rtgui_event_win_destroy edestroy;

	if (RTGUI_TOPLEVEL(win)->server != RT_NULL)
	{
		/* destroy in server */
		RTGUI_EVENT_WIN_DESTROY_INIT(&edestroy);
		edestroy.wid = win;
		if (rtgui_application_send_sync(RTGUI_TOPLEVEL(win)->server, RTGUI_EVENT(&edestroy),
			sizeof(struct rtgui_event_win_destroy)) != RT_EOK)
		{
			/* destroy in server failed */
			return;
		}
	}

	/* release field */
	rt_free(win->title);
}

static rt_bool_t _rtgui_win_create_in_server(struct rtgui_win *win)
{
	if (RTGUI_TOPLEVEL(win)->server == RT_NULL)
	{
		rt_thread_t server;
		struct rtgui_event_win_create ecreate;
		RTGUI_EVENT_WIN_CREATE_INIT(&ecreate);

		/* get server thread id */
		server = rtgui_application_get_server();
		if (server == RT_NULL)
		{
			rt_kprintf("RTGUI server is not running...\n");
			return RT_FALSE;
		}

		/* send win create event to server */
		ecreate.wid         = win;
		ecreate.parent.user	= win->style;
#ifndef RTGUI_USING_SMALL_SIZE
		ecreate.extent      = RTGUI_WIDGET(win)->extent;
		rt_strncpy((char*)ecreate.title, (char*)win->title, RTGUI_NAME_MAX);
#endif

		if (rtgui_application_send_sync(server,
										RTGUI_EVENT(&ecreate),
										sizeof(struct rtgui_event_win_create)
				) != RT_EOK)
		{
			rt_kprintf("create win: %s failed\n", win->title);
			return RT_FALSE;
		}

		/* set server */
		RTGUI_TOPLEVEL(win)->server = server;
	}

	return RT_TRUE;
}

DEFINE_CLASS_TYPE(win, "win",
	RTGUI_TOPLEVEL_TYPE,
	_rtgui_win_constructor,
	_rtgui_win_destructor,
	sizeof(struct rtgui_win));

rtgui_win_t* rtgui_win_create(rtgui_toplevel_t* parent_toplevel,
		                      const char* title,
							  rtgui_rect_t *rect,
							  rt_uint8_t style)
{
	struct rtgui_win* win;

	/* allocate win memory */
	win = RTGUI_WIN(rtgui_widget_create(RTGUI_WIN_TYPE));
	if (win != RT_NULL)
	{
		/* set parent toplevel */
		win->parent_toplevel = parent_toplevel;

		/* set title, rect and style */
		if (title != RT_NULL)
			win->title = rt_strdup(title);
		else
			win->title = RT_NULL;

		rtgui_widget_set_rect(RTGUI_WIDGET(win), rect);
		win->style = style;

		if (_rtgui_win_create_in_server(win) == RT_FALSE)
		{
			rtgui_widget_destroy(RTGUI_WIDGET(win));
			return RT_NULL;
		}
	}

	return win;
}

void rtgui_win_destroy(struct rtgui_win* win)
{
	if (win->flag & RTGUI_WIN_FLAG_MODAL)
	{
		/* set the RTGUI_WIN_STYLE_DESTROY_ON_CLOSE flag so the window will be
		 * destroyed after the event_loop */
		win->style |= RTGUI_WIN_STYLE_DESTROY_ON_CLOSE;
		rtgui_win_end_modal(win, RTGUI_MODAL_CANCEL);
	}
	else
		rtgui_widget_destroy(RTGUI_WIDGET(win));
}

static rt_bool_t _rtgui_win_deal_close(struct rtgui_win *win,
									   struct rtgui_event *event)
{
	if (win->on_close != RT_NULL)
	{
		if (win->on_close(RTGUI_OBJECT(win), event) == RT_FALSE)
			return RT_FALSE;
	}

	rtgui_win_hiden(win);

	win->flag |= RTGUI_WIN_FLAG_CLOSED;

	if (win->flag & RTGUI_WIN_FLAG_MODAL)
	{
		rtgui_win_end_modal(win, RTGUI_MODAL_CANCEL);
	}
	else if (win->style & RTGUI_WIN_STYLE_DESTROY_ON_CLOSE)
	{
		rtgui_win_destroy(win);
	}

	return RT_TRUE;
}

/* send a close event to myself to get a consistent behavior */
rt_bool_t rtgui_win_close(struct rtgui_win* win)
{
	struct rtgui_event_win_close eclose;

	RTGUI_EVENT_WIN_CLOSE_INIT(&eclose);
	eclose.wid = win;
	return _rtgui_win_deal_close(win,
								 (struct rtgui_event*)&eclose);
}

// TODO: return modal code
void rtgui_win_show(struct rtgui_win* win, rt_bool_t is_modal)
{

	if (win == RT_NULL)
		return;

	/* if it does not register into server, create it in server */
	if (RTGUI_TOPLEVEL(win)->server == RT_NULL)
	{
		if (_rtgui_win_create_in_server(win) == RT_FALSE)
			return;
	}

	if (RTGUI_WIDGET_IS_HIDE(RTGUI_WIDGET(win)))
	{
		rt_thread_t stid;
		/* send show message to server */
		struct rtgui_event_win_show eshow;

		stid = rtgui_application_get_server();
		RT_ASSERT(stid != RT_NULL);

		RTGUI_EVENT_WIN_SHOW_INIT(&eshow);
		eshow.wid = win;

		if (rtgui_application_send_sync(stid,
					                    RTGUI_EVENT(&eshow),
										sizeof(struct rtgui_event_win_show)
				) != RT_EOK)
		{
			rt_kprintf("show win failed\n");
			return;
		}

		/* set window unhidden */
		RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(win));
	}
	else
		rtgui_widget_update(RTGUI_WIDGET(win));

    if (is_modal == RT_TRUE)
    {
		win->flag |= RTGUI_WIN_FLAG_MODAL;

        RTGUI_OBJECT(win)->flag &= ~RTGUI_OBJECT_FLAG_DISABLED;
        _rtgui_application_event_loop(rtgui_application_self(),
                                      RTGUI_OBJECT(win));
		win->flag &= ~RTGUI_WIN_FLAG_MODAL;

		if (win->style & RTGUI_WIN_STYLE_DESTROY_ON_CLOSE)
		{
			rtgui_win_destroy(win);
		}
    }

	return;
}

void rtgui_win_end_modal(struct rtgui_win* win, rtgui_modal_code_t modal_code)
{
    if (win == RT_NULL)
        return;

    RTGUI_OBJECT(win)->flag |= RTGUI_OBJECT_FLAG_DISABLED;

	/* remove modal mode */
	win->flag &= ~RTGUI_WIN_FLAG_MODAL;
}

void rtgui_win_hiden(struct rtgui_win* win)
{
	RT_ASSERT(win != RT_NULL);

	if (!RTGUI_WIDGET_IS_HIDE(RTGUI_WIDGET(win)) &&
		RTGUI_TOPLEVEL(win)->server != RT_NULL)
	{
		/* send hidden message to server */
		struct rtgui_event_win_hide ehide;
		RTGUI_EVENT_WIN_HIDE_INIT(&ehide);
		ehide.wid = win;

		if (rtgui_application_send_sync(RTGUI_TOPLEVEL(win)->server, RTGUI_EVENT(&ehide),
			sizeof(struct rtgui_event_win_hide)) != RT_EOK)
		{
			rt_kprintf("hide win: %s failed\n", win->title);
			return;
		}

		/* set window hide and deactivated */
		RTGUI_WIDGET_HIDE(RTGUI_WIDGET(win));
		win->flag &= ~RTGUI_WIN_FLAG_ACTIVATE;
	}
}

rt_bool_t rtgui_win_is_activated(struct rtgui_win* win)
{
	RT_ASSERT(win != RT_NULL);

	if (win->flag & RTGUI_WIN_FLAG_ACTIVATE) return RT_TRUE;

	return RT_FALSE;
}

void rtgui_win_move(struct rtgui_win* win, int x, int y)
{
	struct rtgui_event_win_move emove;
	RTGUI_EVENT_WIN_MOVE_INIT(&emove);

	if (win == RT_NULL) return;

	if (RTGUI_TOPLEVEL(win)->server != RT_NULL)
	{
		/* set win hide firstly */
		RTGUI_WIDGET_HIDE(RTGUI_WIDGET(win));

		emove.wid 	= win;
		emove.x		= x;
		emove.y		= y;
		if (rtgui_application_send_sync(RTGUI_TOPLEVEL(win)->server, RTGUI_EVENT(&emove),
			sizeof(struct rtgui_event_win_move)) != RT_EOK)
		{
			return;
		}
	}

	/* move window to logic position */
	rtgui_widget_move_to_logic(RTGUI_WIDGET(win),
		x - RTGUI_WIDGET(win)->extent.x1,
		y - RTGUI_WIDGET(win)->extent.y1);

	/* set window visible */
	RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(win));
	return;
}

static rt_bool_t rtgui_win_ondraw(struct rtgui_win* win)
{
	struct rtgui_dc* dc;
	struct rtgui_rect rect;
	struct rtgui_event_paint event;

	/* begin drawing */
	dc = rtgui_dc_begin_drawing(RTGUI_WIDGET(win));
	if (dc == RT_NULL)
		return RT_FALSE;

	/* get window rect */
	rtgui_widget_get_rect(RTGUI_WIDGET(win), &rect);
	/* fill area */
	rtgui_dc_fill_rect(dc, &rect);

	/* paint each widget */
	RTGUI_EVENT_PAINT_INIT(&event);
	event.wid = RT_NULL;
	rtgui_container_dispatch_event(RTGUI_CONTAINER(win),
								   (rtgui_event_t*)&event);

	rtgui_dc_end_drawing(dc);

	return RT_FALSE;
}

rt_bool_t rtgui_win_event_handler(struct rtgui_object* object, struct rtgui_event* event)
{
	struct rtgui_win* win;

	RT_ASSERT(object != RT_NULL);
	RT_ASSERT(event != RT_NULL);

	win = RTGUI_WIN(object);

	switch (event->type)
	{
	case RTGUI_EVENT_WIN_SHOW:
		rtgui_win_show(win, RT_FALSE);
		break;

	case RTGUI_EVENT_WIN_HIDE:
		rtgui_win_hiden(win);
		break;

	case RTGUI_EVENT_WIN_CLOSE:
		_rtgui_win_deal_close(win, event);
		/* don't broadcast WIN_CLOSE event to others */
		return RT_TRUE;

	case RTGUI_EVENT_WIN_MOVE:
	{
		struct rtgui_event_win_move* emove = (struct rtgui_event_win_move*)event;

		/* move window */
		rtgui_win_move(win, emove->x, emove->y);
	}
	break;

	case RTGUI_EVENT_WIN_ACTIVATE:
		if (RTGUI_WIDGET_IS_HIDE(RTGUI_WIDGET(win)))
		{
			/* activate a hide window */
			return RT_TRUE;
		}

		win->flag |= RTGUI_WIN_FLAG_ACTIVATE;
#ifndef RTGUI_USING_SMALL_SIZE
		if (RTGUI_WIDGET(object)->on_draw != RT_NULL)
			RTGUI_WIDGET(object)->on_draw(object, event);
		else
#endif
		rtgui_widget_update(RTGUI_WIDGET(win));

		if (win->on_activate != RT_NULL)
		{
			win->on_activate(RTGUI_OBJECT(object), event);
		}
		break;

	case RTGUI_EVENT_WIN_DEACTIVATE:
		if (win->flag & RTGUI_WIN_FLAG_MODAL)
		{
			/* do not deactivate a modal win, re-send win-show event */
			struct rtgui_event_win_show eshow;
			RTGUI_EVENT_WIN_SHOW_INIT(&eshow);
			eshow.wid = win;

			rtgui_application_send(RTGUI_TOPLEVEL(win)->server, RTGUI_EVENT(&eshow),
				sizeof(struct rtgui_event_win_show));
		}
		else
		{
			win->flag &= ~RTGUI_WIN_FLAG_ACTIVATE;
#ifndef RTGUI_USING_SMALL_SIZE
			if (RTGUI_WIDGET(object)->on_draw != RT_NULL)
				RTGUI_WIDGET(object)->on_draw(object, event);
			else
#endif
				rtgui_win_ondraw(win);

			if (win->on_deactivate != RT_NULL)
			{
				win->on_deactivate(RTGUI_OBJECT(object), event);
			}
		}
		break;

	case RTGUI_EVENT_PAINT:
#ifndef RTGUI_USING_SMALL_SIZE
		if (RTGUI_WIDGET(object)->on_draw != RT_NULL)
			RTGUI_WIDGET(object)->on_draw(object, event);
		else
#endif
			rtgui_win_ondraw(win);
		break;

	case RTGUI_EVENT_PANEL_SHOW:
		RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(win));
		rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(win));
		break;

	case RTGUI_EVENT_MOUSE_BUTTON:
		/* check whether has widget which handled mouse event before */
		if (RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(win) != RT_NULL)
		{
			struct rtgui_event_mouse* emouse;

			emouse = (struct rtgui_event_mouse*)event;

			if (rtgui_rect_contains_point(
						&(RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(win)->extent),
						emouse->x, emouse->y) == RT_EOK)
			{
				RTGUI_OBJECT(RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(win)
						)->event_handler(
							RTGUI_OBJECT(RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(win)),
							event);

				/* clean last mouse event handled widget */
				RTGUI_TOPLEVEL_LAST_MEVENT_WIDGET(win) = RT_NULL;
			}
		}

		if (win->flag & RTGUI_WIN_FLAG_UNDER_MODAL)
		{
			if (win->modal_widget != RT_NULL)
				return RTGUI_OBJECT(win->modal_widget)->event_handler(
						RTGUI_OBJECT(win->modal_widget),
						event);
		}
		else if (rtgui_container_dispatch_mouse_event(RTGUI_CONTAINER(win),
			(struct rtgui_event_mouse*)event) == RT_FALSE)
		{
#ifndef RTGUI_USING_SMALL_SIZE
			if (RTGUI_WIDGET(object)->on_mouseclick != RT_NULL)
			{
				return RTGUI_WIDGET(object)->on_mouseclick(object, event);
			}
#endif
		}
		break;

	case RTGUI_EVENT_MOUSE_MOTION:
#if 0
		if (rtgui_widget_dispatch_mouse_event(widget,
			(struct rtgui_event_mouse*)event) == RT_FALSE)
		{
#ifndef RTGUI_USING_SMALL_SIZE
			/* handle event in current widget */
			if (widget->on_mousemotion != RT_NULL)
			{
				return widget->on_mousemotion(widget, event);
			}
#endif
		}
		else return RT_TRUE;
#endif
		break;

    case RTGUI_EVENT_KBD:
		if (win->flag & RTGUI_WIN_FLAG_UNDER_MODAL)
		{
			if (win->modal_widget != RT_NULL)
				return RTGUI_OBJECT(win->modal_widget
						)->event_handler(RTGUI_OBJECT(win->modal_widget),
										 event);
		}
		else if (RTGUI_OBJECT(RTGUI_CONTAINER(win)->focused) != object &&
				 RTGUI_CONTAINER(win)->focused != RT_NULL)
		{
			RTGUI_OBJECT(RTGUI_CONTAINER(win)->focused
					)->event_handler(
						RTGUI_OBJECT(RTGUI_CONTAINER(win)->focused),
						event);
		}
        break;

	default:
		/* call parent event handler */
		return rtgui_toplevel_event_handler(object, event);
	}

	return RT_FALSE;
}

void rtgui_win_set_rect(rtgui_win_t* win, rtgui_rect_t* rect)
{
	struct rtgui_event_win_resize event;

	if (win == RT_NULL || rect == RT_NULL) return;

	RTGUI_WIDGET(win)->extent = *rect;

	if (RTGUI_TOPLEVEL(win)->server != RT_NULL)
	{
		/* set window resize event to server */
		RTGUI_EVENT_WIN_RESIZE_INIT(&event);
		event.wid = win;
		event.rect = *rect;

		rtgui_application_send(RTGUI_TOPLEVEL(win)->server, &(event.parent), sizeof(struct rtgui_event_win_resize));
	}
}

#ifndef RTGUI_USING_SMALL_SIZE
void rtgui_win_set_box(rtgui_win_t* win, rtgui_box_t* box)
{
	if (win == RT_NULL || box == RT_NULL) return;

	rtgui_container_add_child(RTGUI_CONTAINER(win), RTGUI_WIDGET(box));
	rtgui_widget_set_rect(RTGUI_WIDGET(box), &(RTGUI_WIDGET(win)->extent));
}
#endif

void rtgui_win_set_onactivate(rtgui_win_t* win, rtgui_event_handler_ptr handler)
{
	if (win != RT_NULL)
	{
		win->on_activate = handler;
	}
}

void rtgui_win_set_ondeactivate(rtgui_win_t* win, rtgui_event_handler_ptr handler)
{
	if (win != RT_NULL)
	{
		win->on_deactivate = handler;
	}
}

void rtgui_win_set_onclose(rtgui_win_t* win, rtgui_event_handler_ptr handler)
{
	if (win != RT_NULL)
	{
		win->on_close = handler;
	}
}

void rtgui_win_set_title(rtgui_win_t* win, const char *title)
{
	/* send title to server */
	if (RTGUI_TOPLEVEL(win)->server != RT_NULL)
	{
	}

	/* modify in local side */
	if (win->title != RT_NULL)
	{
		rtgui_free(win->title);
		win->title = RT_NULL;
	}

	if (title != RT_NULL)
	{
		win->title = rt_strdup(title);
	}
}

char* rtgui_win_get_title(rtgui_win_t* win)
{
	RT_ASSERT(win != RT_NULL);

	return win->title;
}
