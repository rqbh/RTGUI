/*
 * File      : topwin.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2009, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-10-16     Bernard      first version
 */
#include "topwin.h"
#include "mouse.h"

#include <rtgui/event.h>
#include <rtgui/image.h>
#include <rtgui/rtgui_theme.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/rtgui_application.h>
#include <rtgui/widgets/window.h>

/* the first item of show list is always the active window which will have the
 * focus. The order of this list is the order of the windows. Thus, the first
 * item is the top most window and the last item is the bottom window. Top
 * window can always clip the window beneath it when the two overlapping.
 *
 * If you do not want to handle the kbd event in a window, let the parent to
 * handle it.
 */
static struct rt_list_node _rtgui_topwin_show_list;
/* the hide list have no specific order yet. */
static struct rt_list_node _rtgui_topwin_hide_list;
#define get_topwin_from_list(list_entry) \
	(rt_list_entry((list_entry), struct rtgui_topwin, list))

static struct rt_semaphore _rtgui_topwin_lock;

static void rtgui_topwin_update_clip(void);
static void rtgui_topwin_redraw(struct rtgui_rect* rect);

void rtgui_topwin_init()
{
	/* init window list */
	rt_list_init(&_rtgui_topwin_show_list);
	rt_list_init(&_rtgui_topwin_hide_list);

	rt_sem_init(&_rtgui_topwin_lock, 
		"topwin", 1, RT_IPC_FLAG_FIFO);
}

static struct rtgui_topwin* rtgui_topwin_search_in_list(struct rtgui_win* wid,
														struct rt_list_node* list)
{
	/* TODO: use a cache to speed up the search. */
	struct rt_list_node* node;
	struct rtgui_topwin* topwin;

	/* the action is tend to operate on the top most window. So we search in a
	 * depth first order.
	 */
	rt_list_foreach(node, list, next)
	{
		topwin = rt_list_entry(node, struct rtgui_topwin, list);

		/* is this node? */
		if (topwin->wid == wid)
		{
			return topwin;
		}

		topwin = rtgui_topwin_search_in_list(wid, &topwin->child_list);
		if (topwin != RT_NULL)
			return topwin;
	}

	return RT_NULL;
}

/* add a window to window list[hide] */
rt_err_t rtgui_topwin_add(struct rtgui_event_win_create* event)
{
	struct rtgui_topwin* topwin;

	topwin = rtgui_malloc(sizeof(struct rtgui_topwin));
	if (topwin == RT_NULL)
		return -RT_ERROR;

	topwin->wid 	= event->wid;
#ifdef RTGUI_USING_SMALL_SIZE
	topwin->extent	= RTGUI_WIDGET(event->wid)->extent;
#else
	topwin->extent 	= event->extent;
#endif
	topwin->tid 	= event->parent.sender;

	rt_list_init(&topwin->list);
	rt_list_init(&topwin->child_list);

	topwin->flag 	= 0;
	if (event->parent.user & RTGUI_WIN_STYLE_NO_TITLE) topwin->flag |= WINTITLE_NO;
	if (event->parent.user & RTGUI_WIN_STYLE_CLOSEBOX) topwin->flag |= WINTITLE_CLOSEBOX;
	if (!(event->parent.user & RTGUI_WIN_STYLE_NO_BORDER)) topwin->flag |= WINTITLE_BORDER;
	if (event->parent.user & RTGUI_WIN_STYLE_NO_FOCUS) topwin->flag |= WINTITLE_NOFOCUS;

	if(!(topwin->flag & WINTITLE_NO) || (topwin->flag & WINTITLE_BORDER))
	{
		/* get win extent */
		rtgui_rect_t rect = topwin->extent;

		/* add border rect */
		if (topwin->flag & WINTITLE_BORDER)
		{
			rtgui_rect_inflate(&rect, WINTITLE_BORDER_SIZE);
		}

		/* add title rect */
		if (!(topwin->flag & WINTITLE_NO)) rect.y1 -= WINTITLE_HEIGHT;

#ifdef RTGUI_USING_SMALL_SIZE
		topwin->title = rtgui_wintitle_create(event->wid->title);
#else
		topwin->title = rtgui_wintitle_create((const char*)event->title);
#endif
		rtgui_widget_set_rect(RTGUI_WIDGET(topwin->title), &rect);

		/* update clip info */
		rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(topwin->title));
		rtgui_region_subtract_rect(&(RTGUI_WIDGET(topwin->title)->clip),
			&(RTGUI_WIDGET(topwin->title)->clip),
			&(topwin->extent));
	}
	else topwin->title = RT_NULL;

	rt_list_init(&topwin->list);
	rtgui_list_init(&topwin->monitor_list);

	/* add topwin node to the hidden window list */
	rt_list_insert_after(&(_rtgui_topwin_hide_list), &(topwin->list));

	return RT_EOK;
}

rt_err_t rtgui_topwin_remove(struct rtgui_win* wid)
{
	struct rtgui_topwin* topwin;

	/* find the topwin node */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_show_list);

	if (topwin)
	{
		struct rtgui_topwin *old_focus_topwin = rtgui_topwin_get_focus();

		RT_ASSERT(old_focus_topwin != RT_NULL);

		/* remove node from list */
		rt_list_remove(&(topwin->list));

		rtgui_topwin_update_clip();

		/* redraw the old rect */
		rtgui_topwin_redraw(&(topwin->extent));

		if (old_focus_topwin == topwin)
		{
			_rtgui_topwin_activate_next();
		}

		/* free the monitor rect list, topwin node and title */
		while (topwin->monitor_list.next != RT_NULL)
		{
			struct rtgui_mouse_monitor* monitor = rtgui_list_entry(topwin->monitor_list.next,
				struct rtgui_mouse_monitor, list);

			topwin->monitor_list.next = topwin->monitor_list.next->next;
			rtgui_free(monitor);
		}

		/* destroy win title */
		rtgui_wintitle_destroy(topwin->title);
		topwin->title = RT_NULL;

		rtgui_free(topwin);

		return RT_EOK;
	}

	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_hide_list);
	if (topwin)
	{
		/* remove node from list */
		rt_list_remove(&(topwin->list));

		/* free the topwin node and title */
		rtgui_wintitle_destroy(topwin->title);
		topwin->title = RT_NULL;

		rtgui_free(topwin);

		return RT_EOK;
	}

	return -RT_ERROR;
}

/* neither deactivate the old focus nor change _rtgui_topwin_show_list.
 * Suitable to be called when the first item is the window to be activated
 * already. */
static void _rtgui_topwin_only_activate(struct rtgui_topwin *topwin)
{
	struct rtgui_event_win event;

	RT_ASSERT(topwin != RT_NULL);

	/* update clip info */
	rtgui_topwin_update_clip();

	/* activate the raised window */
	RTGUI_EVENT_WIN_ACTIVATE_INIT(&event);
	event.wid = topwin->wid;
	rtgui_application_send(topwin->tid, &(event.parent), sizeof(struct rtgui_event_win));

	/* redraw title */
	if (topwin->title != RT_NULL)
	{
		RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(topwin->title));
		topwin->flag |= WINTITLE_ACTIVATE;
		rtgui_theme_draw_win(topwin);
	}
}

/* activate a win
 * - deactivate the old focus win
 * - activate a win
 * - draw win title
 *
 * NOTE:
 *   This function does not affect the _rtgui_topwin_show_list. You should make
 *   sure the topwin is inserted to _rtgui_topwin_show_list properly.
 */
static void rtgui_topwin_activate_win(struct rtgui_topwin* topwin)
{
	struct rtgui_event_win event;
	struct rtgui_topwin *old_focus_topwin;

	RT_ASSERT(topwin != RT_NULL);

	old_focus_topwin = rtgui_topwin_get_focus();

	if (old_focus_topwin == topwin)
		return;

	if (old_focus_topwin != RT_NULL)
	{
		/* deactive the old focus window firstly, otherwise it will make the new
		 * window invisible. */
		RTGUI_EVENT_WIN_DEACTIVATE_INIT(&event);
		event.wid = old_focus_topwin->wid;
		rtgui_application_send(old_focus_topwin->tid,
				&event.parent, sizeof(struct rtgui_event_win));

		/* redraw title */
		if (old_focus_topwin->title != RT_NULL)
		{
			old_focus_topwin->flag &= ~WINTITLE_ACTIVATE;
			rtgui_theme_draw_win(old_focus_topwin);
		}
	}

	/* remove node from hidden list */
	rt_list_remove(&(topwin->list));
	/* add node to show list */
	rt_list_insert_after(&_rtgui_topwin_show_list, &(topwin->list));

	_rtgui_topwin_only_activate(topwin);
}

#if 0
/* the only way to deactivate a window is to activate a new window. So not need
 * to deactivate a window explictly */
/*
 * deactivate a win
 * - deactivate the win
 * - redraw win title
 */
void rtgui_topwin_deactivate_win(struct rtgui_topwin* win)
{
	/* deactivate win */
	struct rtgui_event_win event;
	RTGUI_EVENT_WIN_DEACTIVATE_INIT(&event);
	event.wid = win->wid;
	rtgui_application_send(win->tid,
		&event.parent, sizeof(struct rtgui_event_win));

	win->flag &= ~WINTITLE_ACTIVATE;
	rtgui_theme_draw_win(win);

	if (rtgui_server_focus_topwin == win)
	{
		rtgui_server_focus_topwin = RT_NULL;
	}
}
#endif

/* raise window to front */
void rtgui_topwin_raise(struct rtgui_win* wid)
{
	struct rtgui_topwin* topwin;
	struct rt_list_node *node;
	struct rtgui_event_clip_info eclip;

	RTGUI_EVENT_CLIP_INFO_INIT(&eclip);
	eclip.wid = RT_NULL;

	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_show_list);
	/* the window is already placed in front */
	if (&(topwin->list) == _rtgui_topwin_show_list.next)
	{
	    return;
	}

	// rt_sem_take(&_rtgui_topwin_lock, RT_WAITING_FOREVER);
	/* find the topwin node */
	rt_list_foreach(node, &_rtgui_topwin_show_list, next)
	{
		topwin = rt_list_entry(node, struct rtgui_topwin, list);
		if (topwin->wid == wid)
		{
			struct rt_list_node *n, *barrier;

			barrier = node->next;

			/* active window */
			rtgui_topwin_activate_win(topwin);

			/* send clip update to each upper window */
			for (n = node->next; n != barrier; n = n->next)
			{
				struct rtgui_topwin* wnd = rt_list_entry(n, struct rtgui_topwin, list);
				eclip.wid = wnd->wid;

				/* send to destination window */
				rtgui_application_send(wnd->tid, &(eclip.parent), sizeof(struct rtgui_event_clip_info));

				/* reset clip info in title */
				if (wnd->title != RT_NULL)
				{
					rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(wnd->title));
					rtgui_region_subtract_rect(&(RTGUI_WIDGET(wnd->title)->clip),
						&(RTGUI_WIDGET(wnd->title)->clip),
						&(wnd->extent));
				}
			}

			break;
		}
	}
	// rt_sem_release(&_rtgui_topwin_lock);
}

/* show a window */
void rtgui_topwin_show(struct rtgui_event_win* event)
{
	struct rtgui_topwin* topwin;
	struct rtgui_win* wid = event->wid;

	/* find in hide list */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_hide_list);

	/* find it */
	if (topwin != RT_NULL)
	{
		/* activate this window */
		rtgui_topwin_activate_win(topwin);

		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);
	}
	else
	{
		/* the wnd is located in show list, raise wnd to front */
		topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_show_list);
		if (topwin != RT_NULL)
		{
			if (_rtgui_topwin_show_list.next != &(topwin->list))
			{
				/* not the front window, raise it */
				rtgui_topwin_raise(wid);
				rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);
			}
			else
			{
				rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);
			}
		}
		else
		{
			/* there is no wnd in wnd list */
			rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_ERROR);
		}
	}
}

static _rtgui_topwin_activate_next(void)
{
	if (!rt_list_isempty(&_rtgui_topwin_show_list))
	{
		struct rtgui_event_win wevent;
		struct rtgui_topwin *topwin;
		/* get the topwin */
		topwin = rt_list_entry(_rtgui_topwin_show_list.next,
				struct rtgui_topwin, list);

		RTGUI_EVENT_WIN_ACTIVATE_INIT(&wevent);
		wevent.wid = topwin->wid;
		rtgui_application_send(topwin->tid, &(wevent.parent), sizeof(struct rtgui_event_win));

		_rtgui_topwin_only_activate(topwin);
	}
}

/* hide a window */
void rtgui_topwin_hide(struct rtgui_event_win* event)
{
	struct rtgui_topwin* topwin;
	struct rtgui_win* wid = event->wid;

	/* find in show list */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_show_list);

	/* found it */
	if (topwin)
	{
		struct rtgui_topwin *old_focus_topwin = rtgui_topwin_get_focus();

		RT_ASSERT(old_focus_topwin != RT_NULL);

		/* remove node from show list */
		rt_list_remove(&(topwin->list));

		/* add node to hidden list */
		rt_list_insert_after(&_rtgui_topwin_hide_list, &(topwin->list));

		/* show window title */
		if (topwin->title != RT_NULL)
		{
			RTGUI_WIDGET_HIDE(RTGUI_WIDGET(topwin->title));
		}

		/* update clip info */
		rtgui_topwin_update_clip();

		/* redraw the old rect */
		rtgui_topwin_redraw(&(topwin->extent));

		if (old_focus_topwin == topwin)
		{
			_rtgui_topwin_activate_next();
		}
	}
	else
	{
		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_ERROR);
		return;
	}

	rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);
}

/* move top window */
void rtgui_topwin_move(struct rtgui_event_win_move* event)
{
	struct rtgui_topwin* topwin;

	/* find in show list */
	topwin = rtgui_topwin_search_in_list(event->wid, &_rtgui_topwin_show_list);
	if (topwin != RT_NULL)
	{
		int dx, dy;
		rtgui_rect_t old_rect; /* the old topwin coverage area */
		struct rtgui_list_node* node;

		/* send status ok */
		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);

		/* get the delta move x, y */
		dx = event->x - topwin->extent.x1;
		dy = event->y - topwin->extent.y1;

		old_rect = topwin->extent;
		/* move window rect */
		rtgui_rect_moveto(&(topwin->extent), dx, dy);

		/* move window title */
		if (topwin->title != RT_NULL)
		{
			old_rect = RTGUI_WIDGET(topwin->title)->extent;
			rtgui_widget_move_to_logic(RTGUI_WIDGET(topwin->title), dx, dy);
		}

		/* move the monitor rect list */
		rtgui_list_foreach(node, &(topwin->monitor_list))
		{
			struct rtgui_mouse_monitor* monitor = rtgui_list_entry(node,
				struct rtgui_mouse_monitor,
				list);
			rtgui_rect_moveto(&(monitor->rect), dx, dy);
		}

		/* update windows clip info */
		rtgui_topwin_update_clip();

		/* update old window coverage area */
		rtgui_topwin_redraw(&old_rect);

		/* update top window title */
		if (topwin->title != RT_NULL)
			rtgui_theme_draw_win(topwin);
		if (rtgui_rect_is_intersect(&old_rect, &(topwin->extent)) != RT_EOK)
		{
			/*
			 * the old rect is not intersect with moved rect,
			 * re-paint window
			 */
			struct rtgui_event_paint epaint;
			RTGUI_EVENT_PAINT_INIT(&epaint);
			epaint.wid = topwin->wid;
			rtgui_application_send(topwin->tid, &(epaint.parent), sizeof(epaint));
		}
	}
	else
	{
		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_ERROR);
	}
}

/*
 * resize a top win
 * Note: currently, only support resize hidden window
 */
void rtgui_topwin_resize(struct rtgui_win* wid, rtgui_rect_t* r)
{
	struct rtgui_topwin* topwin;

	/* find in show list */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_hide_list);
	if (topwin)
	{
		topwin->extent = *r;

		if (topwin->title != RT_NULL)
		{
			/* get win extent */
			rtgui_rect_t rect = topwin->extent;

			/* add border rect */
			if (topwin->flag & WINTITLE_BORDER)
			{
				rtgui_rect_inflate(&rect, WINTITLE_BORDER_SIZE);
			}

			/* add title rect */
			if (!(topwin->flag & WINTITLE_NO)) rect.y1 -= WINTITLE_HEIGHT;

			RTGUI_WIDGET(topwin->title)->extent = rect;

			/* update title & border clip info */
			rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(topwin->title));
			rtgui_region_subtract_rect(&(RTGUI_WIDGET(topwin->title)->clip),
				&(RTGUI_WIDGET(topwin->title)->clip),
				&(topwin->extent));
		}
	}
}

struct rtgui_topwin* rtgui_topwin_get_focus(void)
{
	if (rt_list_isempty(&_rtgui_topwin_show_list))
		return RT_NULL;
	else
		return get_topwin_from_list(_rtgui_topwin_show_list.next);
}

struct rtgui_topwin* rtgui_topwin_get_wnd(int x, int y)
{
	struct rt_list_node* node;
	struct rtgui_topwin* topwin;

	/* search in list */
	rt_list_foreach(node, &(_rtgui_topwin_show_list), next)
	{
		topwin = rt_list_entry(node, struct rtgui_topwin, list);

		/* is this window? */
		if ((topwin->title != RT_NULL) &&
			rtgui_rect_contains_point(&(RTGUI_WIDGET(topwin->title)->extent), x, y) == RT_EOK)
		{
			return topwin;
		}
		else if (rtgui_rect_contains_point(&(topwin->extent), x, y) == RT_EOK)
		{
			return topwin;
		}
	}

	return RT_NULL;
}

static void rtgui_topwin_update_clip()
{
	rt_int32_t count = 0;
	struct rtgui_event_clip_info eclip;
	struct rt_list_node* node = _rtgui_topwin_show_list.next;

	RTGUI_EVENT_CLIP_INFO_INIT(&eclip);

	rt_list_foreach(node, &_rtgui_topwin_show_list, next)
	{
		struct rtgui_topwin* wnd;
		wnd = rt_list_entry(node, struct rtgui_topwin, list);

		eclip.num_rect = count;
		eclip.wid = wnd->wid;

		count ++;

		/* send to destination window */
		rtgui_application_send(wnd->tid, &(eclip.parent), sizeof(struct rtgui_event_clip_info));

		/* update clip in win title */
		if (wnd->title != RT_NULL)
		{
			/* reset clip info */
			rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(wnd->title));
			rtgui_region_subtract_rect(&(RTGUI_WIDGET(wnd->title)->clip),
				&(RTGUI_WIDGET(wnd->title)->clip),
				&(wnd->extent));
		}
	}
}

static void rtgui_topwin_redraw(struct rtgui_rect* rect)
{
	struct rt_list_node* node;
	struct rtgui_event_paint epaint;
	RTGUI_EVENT_PAINT_INIT(&epaint);
	epaint.wid = RT_NULL;

	/* redraw the windows from bottom to top */
	rt_list_foreach(node, &_rtgui_topwin_show_list, prev)
	{
		struct rtgui_topwin *topwin;

		topwin = rt_list_entry(node, struct rtgui_topwin, list);
		if (rtgui_rect_is_intersect(rect, &(topwin->extent)) == RT_EOK)
		{
			epaint.wid = topwin->wid;
			rtgui_application_send(topwin->tid, &(epaint.parent), sizeof(epaint));

			/* draw title */
			if (topwin->title != RT_NULL)
			{
				rtgui_theme_draw_win(topwin);
			}
		}
	}
}

void rtgui_topwin_title_onmouse(struct rtgui_topwin* win, struct rtgui_event_mouse* event)
{
	rtgui_rect_t rect;

	/* let window to process this mouse event */
	if (rtgui_rect_contains_point(&win->extent, event->x, event->y) == RT_EOK)
	{
		/* send mouse event to thread */
		rtgui_application_send(win->tid, &(event->parent), sizeof(struct rtgui_event_mouse));
		return;
	}

	/* get close button rect (device value) */
	rect.x1 = RTGUI_WIDGET(win->title)->extent.x2 - WINTITLE_BORDER_SIZE - WINTITLE_CB_WIDTH - 3;
	rect.y1 = RTGUI_WIDGET(win->title)->extent.y1 + WINTITLE_BORDER_SIZE + 3;
	rect.x2 = rect.x1 + WINTITLE_CB_WIDTH;
	rect.y2 = rect.y1 + WINTITLE_CB_HEIGHT;

	if (event->button & RTGUI_MOUSE_BUTTON_LEFT)
	{
		if (event->button & RTGUI_MOUSE_BUTTON_DOWN)
		{
			if (rtgui_rect_contains_point(&rect, event->x, event->y) == RT_EOK)
			{
				win->flag |= WINTITLE_CB_PRESSED;
				rtgui_theme_draw_win(win);
			}
#ifdef RTGUI_USING_WINMOVE
			else
			{
				/* maybe move window */
				rtgui_winrect_set(win);
			}
#endif
		}
		else if (win->flag & WINTITLE_CB_PRESSED && event->button & RTGUI_MOUSE_BUTTON_UP)
		{
			if (rtgui_rect_contains_point(&rect, event->x, event->y) == RT_EOK)
			{
				struct rtgui_event_win event;

				win->flag &= ~WINTITLE_CB_PRESSED;
				rtgui_theme_draw_win(win);

				/* send close event to window */
				RTGUI_EVENT_WIN_CLOSE_INIT(&event);
				event.wid = win->wid;
				rtgui_application_send(win->tid, &(event.parent), sizeof(struct rtgui_event_win));
			}
		}
	}
}

void rtgui_topwin_append_monitor_rect(struct rtgui_win* wid, rtgui_rect_t* rect)
{
	struct rtgui_topwin* win;

	/* parameters check */
	if (wid == RT_NULL || rect == RT_NULL) return;

	/* find topwin */
	win = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_show_list);
	if (win == RT_NULL)
		win = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_hide_list);

	if (win == RT_NULL)
		return;

	/* append rect to top window monitor rect list */
	rtgui_mouse_monitor_append(&(win->monitor_list), rect);
}

void rtgui_topwin_remove_monitor_rect(struct rtgui_win* wid, rtgui_rect_t* rect)
{
	struct rtgui_topwin* win;

	/* parameters check */
	if (wid == RT_NULL || rect == RT_NULL)
		return;

	/* find topwin */
	win = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_show_list);
	if (win == RT_NULL)
		win = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_hide_list);

	if (win == RT_NULL)
		return;

	/* remove rect from top window monitor rect list */
	rtgui_mouse_monitor_remove(&(win->monitor_list), rect);
}

/**
 * do clip for topwin list
 * widget, the clip widget to be done clip
 */
void rtgui_topwin_do_clip(rtgui_widget_t* widget)
{
	struct rtgui_win       * wid;
	struct rtgui_rect      * rect;
	struct rtgui_topwin    * topwin;
	struct rt_list_node * node;

	/* get toplevel wid */
	wid = widget->toplevel;

	rt_sem_take(&_rtgui_topwin_lock, RT_WAITING_FOREVER);
	/* get all of topwin rect list */
	rt_list_foreach(node, &_rtgui_topwin_show_list, next)
	{
		topwin = rt_list_entry(node, struct rtgui_topwin, list);

		if (topwin->wid == wid ||
			RTGUI_WIDGET(topwin->title) == widget) break; /* it's self window, break */

		/* get extent */
		if (topwin->title != RT_NULL)
			rect = &(RTGUI_WIDGET(topwin->title)->extent);
		else
			rect = &(topwin->extent);

		/* subtract the topwin rect */
		rtgui_region_subtract_rect(&(widget->clip), &(widget->clip), rect);
	}
	rt_sem_release(&_rtgui_topwin_lock);
}

