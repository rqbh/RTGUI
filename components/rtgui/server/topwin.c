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
#define get_topwin_from_list(list_entry) \
	(rt_list_entry((list_entry), struct rtgui_topwin, list))

static struct rt_semaphore _rtgui_topwin_lock;

static void rtgui_topwin_update_clip(void);
static void rtgui_topwin_redraw(struct rtgui_rect* rect);
static void _rtgui_topwin_activate_next(void);

void rtgui_topwin_init(void)
{
	/* init window list */
	rt_list_init(&_rtgui_topwin_show_list);

	rt_sem_init(&_rtgui_topwin_lock,
		"topwin", 1, RT_IPC_FLAG_FIFO);
}

static struct rtgui_topwin* rtgui_topwin_search_in_list(struct rtgui_win* window,
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
		if (topwin->wid == window)
		{
			return topwin;
		}

		topwin = rtgui_topwin_search_in_list(window, &topwin->child_list);
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

	topwin->wid    = event->wid;
#ifdef RTGUI_USING_SMALL_SIZE
	topwin->extent = RTGUI_WIDGET(event->wid)->extent;
#else
	topwin->extent = event->extent;
#endif
	topwin->tid    = event->parent.sender;

	if (event->parent_window == RT_NULL)
	{
		topwin->parent = RT_NULL;
		rt_list_insert_before(&_rtgui_topwin_show_list, &topwin->list);
	}
	else
	{
		topwin->parent  = rtgui_topwin_search_in_list(event->parent_window, &_rtgui_topwin_show_list);
		rt_list_insert_before(&topwin->parent->child_list, &topwin->list);
		if (topwin->parent == RT_NULL)
		{
			/* parent does not exist. Orphan window? */
			rtgui_free(topwin);
			return -RT_ERROR;
		}
	}

	rt_list_init(&topwin->child_list);

	topwin->flag = 0;
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
	else
		topwin->title = RT_NULL;

	rtgui_list_init(&topwin->monitor_list);

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

		/* remove node from list */
		rt_list_remove(&(topwin->list));

		if (topwin->flag & WINTITLE_SHOWN)
		{
			rtgui_topwin_update_clip();

			/* redraw the old rect */
			rtgui_topwin_redraw(&(topwin->extent));

			if (old_focus_topwin == topwin)
			{
				_rtgui_topwin_activate_next();
			}
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

	return -RT_ERROR;
}

/* neither deactivate the old focus nor change _rtgui_topwin_show_list.
 * Suitable to be called when the first item is the window to be activated
 * already. */
static void _rtgui_topwin_only_activate(struct rtgui_topwin *topwin)
{
	struct rtgui_event_win event;

	RT_ASSERT(topwin != RT_NULL);
	RT_ASSERT(topwin->flag & WINTITLE_SHOWN);

	topwin->flag |= WINTITLE_ACTIVATE;

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

static void _rtgui_topwin_activate_next(void)
{
	if (!rt_list_isempty(&_rtgui_topwin_show_list))
	{
		struct rtgui_event_win wevent;
		struct rtgui_topwin *topwin;
		/* get the topwin */
		topwin = rt_list_entry(_rtgui_topwin_show_list.next,
				struct rtgui_topwin, list);
		if (!(topwin->flag & WINTITLE_SHOWN))
			return;

		_rtgui_topwin_only_activate(topwin);
	}
}

/* activate a win
 * - deactivate the old focus win
 * - activate a win
 * - draw win title
 */
void rtgui_topwin_activate_win(struct rtgui_topwin* topwin)
{
	struct rtgui_event_win event;
	struct rtgui_topwin *old_focus_topwin;

	RT_ASSERT(topwin != RT_NULL);

	/* If there is only one shown window, don't deactivate the old one. */
	if ((_rtgui_topwin_show_list.next == &topwin->list &&
		_rtgui_topwin_show_list.prev == &topwin->list) ||
		(_rtgui_topwin_show_list.next == &topwin->list &&
		 !(get_topwin_from_list(topwin->list.next)->flag & WINTITLE_SHOWN))
	   )
	{
		_rtgui_topwin_only_activate(topwin);
		return;
	}

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

/* show a window */
void rtgui_topwin_show(struct rtgui_event_win* event)
{
	struct rtgui_topwin* topwin;
	struct rtgui_win* wid = event->wid;

	/* find in hide list */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_show_list);
	if (topwin == RT_NULL)
	{
		/* there is no such a window recorded */
		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_ERROR);
		return;
	}

	topwin->flag |= WINTITLE_SHOWN;
	rtgui_topwin_activate_win(topwin);

	rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);
}

/* hide a window */
void rtgui_topwin_hide(struct rtgui_event_win* event)
{
	struct rtgui_topwin* topwin;
	struct rtgui_topwin *old_focus_topwin = rtgui_topwin_get_focus();
	struct rtgui_win* wid = event->wid;

	/* find in show list */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_show_list);
	if (topwin == RT_NULL)
	{
		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_ERROR);
		return;
	}
	if (!(topwin->flag & WINTITLE_SHOWN))
	{
		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);
		return;
	}

	old_focus_topwin = rtgui_topwin_get_focus();

	topwin->flag &= ~WINTITLE_SHOWN;
	if (topwin->title != RT_NULL)
	{
		RTGUI_WIDGET_HIDE(RTGUI_WIDGET(topwin->title));
	}

	rt_list_remove(&topwin->list);
	rt_list_insert_before(&_rtgui_topwin_show_list, &topwin->list);

	/* update clip info */
	rtgui_topwin_update_clip();

	/* redraw the old rect */
	rtgui_topwin_redraw(&(topwin->extent));

	if (old_focus_topwin == topwin)
	{
		_rtgui_topwin_activate_next();
	}

	rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);
}

/* move top window */
void rtgui_topwin_move(struct rtgui_event_win_move* event)
{
	struct rtgui_topwin* topwin;
	int dx, dy;
	rtgui_rect_t old_rect; /* the old topwin coverage area */
	struct rtgui_list_node* node;

	/* find in show list */
	topwin = rtgui_topwin_search_in_list(event->wid, &_rtgui_topwin_show_list);
	if (topwin == RT_NULL ||
		!(topwin->flag & WINTITLE_SHOWN))
	{
		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_ERROR);
	}

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

/*
 * resize a top win
 * Note: currently, only support resize hidden window
 */
void rtgui_topwin_resize(struct rtgui_win* wid, rtgui_rect_t* rect)
{
	struct rtgui_topwin* topwin;
	struct rtgui_rect extent;
	struct rtgui_region region;

	/* find in show list */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_show_list);
	if (topwin == RT_NULL ||
		!(topwin->flag & WINTITLE_SHOWN))
		return;

	/* record the old rect */
	rtgui_region_init_with_extents(&region, &topwin->extent);
	/* union the new rect so this is the region we should redraw */
	rtgui_region_union_rect(&region, &region, rect);

	topwin->extent = *rect;

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
		if (!(topwin->flag & WINTITLE_NO))
			rect.y1 -= WINTITLE_HEIGHT;

		RTGUI_WIDGET(topwin->title)->extent = rect;
	}

	/* update windows clip info */
	rtgui_topwin_update_clip();

	/* update old window coverage area */
	rtgui_topwin_redraw(rtgui_region_extents(&region));
}

struct rtgui_topwin* rtgui_topwin_get_focus(void)
{
	if (rt_list_isempty(&_rtgui_topwin_show_list) ||
		!(get_topwin_from_list(_rtgui_topwin_show_list.next)->flag & WINTITLE_SHOWN))
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
		if (!(topwin->flag & WINTITLE_SHOWN))
			break;

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

static void rtgui_topwin_update_clip(void)
{
	rt_int32_t count = 0;
	struct rtgui_event_clip_info eclip;
	struct rt_list_node* node;

	RTGUI_EVENT_CLIP_INFO_INIT(&eclip);

	rt_list_foreach(node, &_rtgui_topwin_show_list, next)
	{
		struct rtgui_topwin* topwin;

		topwin = rt_list_entry(node, struct rtgui_topwin, list);

		if (!(topwin->flag & WINTITLE_SHOWN))
			break;

		rtgui_topwin_do_clip(topwin);

		eclip.num_rect = count;
		eclip.wid = topwin->wid;

		count ++;

		/* send to destination window */
		rtgui_application_send(topwin->tid, &(eclip.parent), sizeof(struct rtgui_event_clip_info));

		/* update clip in win title */
		if (topwin->title != RT_NULL)
		{
			/* reset clip info */
			rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(topwin->title));
			rtgui_region_subtract_rect(&(RTGUI_WIDGET(topwin->title)->clip),
				&(RTGUI_WIDGET(topwin->title)->clip),
				&(topwin->extent));
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

		// TODO: we can optimize a little by skipping the hidden window firstly.
		if (topwin->flag & WINTITLE_SHOWN &&
			rtgui_rect_is_intersect(rect, &(topwin->extent)) == RT_EOK)
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
		return;

	/* remove rect from top window monitor rect list */
	rtgui_mouse_monitor_remove(&(win->monitor_list), rect);
}

static void _rtgui_topwin_clip_screen(struct rtgui_widget *widget)
{
	struct rtgui_rect screen_rect;

	RT_ASSERT(widget != RT_NULL);

	/* reset toplevel widget clip to extent */
	rtgui_region_reset(&(widget->clip), &(widget->extent));

	/* subtract the screen rect */
	screen_rect.x1 = screen_rect.y1 = 0;
	screen_rect.x2 = rtgui_graphic_driver_get_default()->width;
	screen_rect.y2 = rtgui_graphic_driver_get_default()->height;
	rtgui_region_intersect_rect(&(widget->clip), &(widget->clip),
			&screen_rect);
}

/**
 * do clip for topwin list
 * topwin, the clip topwin to be done clip
 */
void rtgui_topwin_do_clip(struct rtgui_topwin* target_topwin)
{
	struct rtgui_rect   * rect;
	struct rt_list_node * node;

	RT_ASSERT(target_topwin != RT_NULL);
	RT_ASSERT(target_topwin->wid != RT_NULL);

	if (target_topwin->title != RT_NULL)
	{
		_rtgui_topwin_clip_screen(RTGUI_WIDGET(target_topwin->title));
	}

	_rtgui_topwin_clip_screen(RTGUI_WIDGET(target_topwin->wid));

	rt_sem_take(&_rtgui_topwin_lock, RT_WAITING_FOREVER);
	/* get all of topwin rect list */
	rt_list_foreach(node, &target_topwin->list, prev)
	{
		struct rtgui_topwin *topwin;
		if (node == &_rtgui_topwin_show_list)
			break;

		topwin = get_topwin_from_list(node);

		/* get extent */
		if (topwin->title != RT_NULL)
			rect = &(RTGUI_WIDGET(topwin->title)->extent);
		else
			rect = &(topwin->extent);

		/* subtract the topwin rect */
		if (topwin->title != RT_NULL)
			rtgui_region_subtract_rect(&(RTGUI_WIDGET(target_topwin->title)->clip),
									   &(RTGUI_WIDGET(target_topwin->title)->clip), rect);
		rtgui_region_subtract_rect(&(RTGUI_WIDGET(target_topwin->wid)->clip),
				&(RTGUI_WIDGET(target_topwin->wid)->clip), rect);
	}
	rt_sem_release(&_rtgui_topwin_lock);
}

