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

/* This list is divided into two parts. The first part is the shown list, in
 * which all the windows have the WINTITLE_SHOWN flag set. Second part is the
 * hidden list, in which all the windows don't have WINTITLE_SHOWN flag.
 *
 * The active window is the one that would receive kbd events. It should always
 * in the first tree. The order of this list is the order of the windows.
 * Thus, the first item is the top most window and the last item is the bottom
 * window. Top window can always clip the window beneath it when the two
 * overlapping. Child window can always clip it's parent. Slibing windows can
 * clip each other with the same rule as this list. Thus, each child list is
 * the same as _rtgui_topwin_list. This forms the hierarchy tree structure of
 * all windows.
 *
 * The hidden list have no specific order.
 */
static struct rt_list_node _rtgui_topwin_list;
#define get_topwin_from_list(list_entry) \
	(rt_list_entry((list_entry), struct rtgui_topwin, list))

static struct rt_semaphore _rtgui_topwin_lock;

static void rtgui_topwin_update_clip(void);
static void rtgui_topwin_redraw(struct rtgui_rect* rect);
static void _rtgui_topwin_activate_next(void);

void rtgui_topwin_init(void)
{
	/* init window list */
	rt_list_init(&_rtgui_topwin_list);

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
		rt_list_insert_before(&_rtgui_topwin_list, &topwin->list);
	}
	else
	{
		topwin->parent = rtgui_topwin_search_in_list(event->parent_window, &_rtgui_topwin_list);
		if (topwin->parent == RT_NULL)
		{
			/* parent does not exist. Orphan window? */
			rtgui_free(topwin);
			return -RT_ERROR;
		}
		rt_list_insert_before(&topwin->parent->child_list, &topwin->list);
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

static struct rtgui_topwin* _rtgui_topwin_get_top_parent(struct rtgui_topwin *topwin)
{
	struct rtgui_topwin *topparent;

	RT_ASSERT(topwin != RT_NULL);

	topparent = topwin;
	while (topparent->parent != RT_NULL)
		topparent = topparent->parent;
	return topparent;
}

static struct rtgui_topwin* _rtgui_topwin_get_topmost_child_shown(struct rtgui_topwin *topwin)
{
	RT_ASSERT(topwin != RT_NULL);

	while (!(rt_list_isempty(&topwin->child_list)) &&
		   get_topwin_from_list(topwin->child_list.next)->flag & WINTITLE_SHOWN)
		topwin = get_topwin_from_list(topwin->child_list.next);
	return topwin;
}

static struct rtgui_topwin* _rtgui_topwin_get_topmost_window_shown(void)
{
	if (!(get_topwin_from_list(_rtgui_topwin_list.next)->flag & WINTITLE_SHOWN))
		return RT_NULL;
	else
		return _rtgui_topwin_get_topmost_child_shown(get_topwin_from_list(_rtgui_topwin_list.next));
}

/* a hidden parent will hide it's children. Top level window can be shown at
 * any time. */
static rt_bool_t _rtgui_topwin_could_show(struct rtgui_topwin *topwin)
{
	struct rtgui_topwin *parent;

	RT_ASSERT(topwin != RT_NULL);

	for (parent = topwin->parent; parent != RT_NULL; parent = parent->parent)
	{
		if (!(parent->flag & WINTITLE_SHOWN))
			return RT_FALSE;
	}
	return RT_TRUE;
}

static void _rtgui_topwin_remove_tree(struct rtgui_topwin *topwin,
									  struct rtgui_region *region)
{
	struct rt_list_node *node;

	RT_ASSERT(topwin != RT_NULL);

	rt_list_foreach(node, &topwin->child_list, next)
		_rtgui_topwin_remove_tree(get_topwin_from_list(node), region);

	/* remove node from list */
	rt_list_remove(&(topwin->list));

	if (topwin->title != RT_NULL)
		rtgui_region_union_rect(region, region, &RTGUI_WIDGET(topwin->title)->extent);
	else
		rtgui_region_union_rect(region, region, &topwin->extent);

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
}

rt_err_t rtgui_topwin_remove(struct rtgui_win* wid)
{
	struct rtgui_topwin *topwin;
	struct rtgui_region region;

	/* find the topwin node */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_list);

	if (topwin == RT_NULL)
		return -RT_ERROR;

	rtgui_region_init(&region);

	_rtgui_topwin_remove_tree(topwin, &region);
	if (topwin->flag & WINTITLE_SHOWN)
		rtgui_topwin_update_clip();

	if (rtgui_topwin_get_focus() == topwin)
	{
		_rtgui_topwin_activate_next();
	}

	/* redraw the old rect */
	rtgui_topwin_redraw(rtgui_region_extents(&region));

	return RT_EOK;
}

/* neither deactivate the old focus nor change _rtgui_topwin_list.
 * Suitable to be called when the first item is the window to be activated
 * already. */
static void _rtgui_topwin_only_activate(struct rtgui_topwin *topwin)
{
	struct rtgui_event_win event;

	RT_ASSERT(topwin != RT_NULL);

	if (topwin->flag & WINTITLE_NOFOCUS)
		return;

	/* activate the raised window */
	RTGUI_EVENT_WIN_ACTIVATE_INIT(&event);

	topwin->flag |= WINTITLE_ACTIVATE;

	event.wid = topwin->wid;
	rtgui_application_send(topwin->tid, &(event.parent), sizeof(struct rtgui_event_win));

	/* redraw title */
	if (topwin->title != RT_NULL)
	{
		rtgui_theme_draw_win(topwin);
	}
}

static void _rtgui_topwin_activate_next(void)
{
	struct rtgui_topwin *topwin;

	topwin = _rtgui_topwin_get_topmost_window_shown();
	if (topwin == RT_NULL)
		return;

	_rtgui_topwin_only_activate(topwin);
}

/* this function does not update the clip(to avoid doubel clipping). So if the
 * tree has changed, make sure it has already updated outside. */
static void _rtgui_topwin_deactivate(struct rtgui_topwin *topwin)
{
	struct rtgui_event_win event;

	RT_ASSERT(topwin != RT_NULL);
	RT_ASSERT(topwin->tid != RT_NULL);

	RTGUI_EVENT_WIN_DEACTIVATE_INIT(&event);
	event.wid = topwin->wid;
	rtgui_application_send(topwin->tid,
			&event.parent, sizeof(struct rtgui_event_win));

	/* redraw title */
	if (topwin->title != RT_NULL)
	{
		topwin->flag &= ~WINTITLE_ACTIVATE;
		rtgui_theme_draw_win(topwin);
	}
}

static void _rtgui_topwin_move_whole_tree2top(struct rtgui_topwin *topwin)
{
	struct rtgui_topwin *topparent;

	RT_ASSERT(topwin != RT_NULL);

	/* move the whole tree */
	topparent = _rtgui_topwin_get_top_parent(topwin);

	/* remove node from hidden list */
	rt_list_remove(&(topparent->list));
	/* add node to show list */
	rt_list_insert_after(&_rtgui_topwin_list, &(topparent->list));
}

static void _rtgui_topwin_raise_topwin_in_tree(struct rtgui_topwin *topwin)
{
	struct rt_list_node *win_level;

	RT_ASSERT(topwin != RT_NULL);

	_rtgui_topwin_move_whole_tree2top(topwin);
	if (topwin->parent == RT_NULL)
		win_level = &_rtgui_topwin_list;
	else
		win_level = &topwin->parent->child_list;
	rt_list_remove(&topwin->list);
	rt_list_insert_after(win_level, &topwin->list);
}

/* activate a win means:
 * - deactivate the old focus win if any
 * - raise the window to the front of it's siblings
 * - activate a win
 */
void rtgui_topwin_activate_win(struct rtgui_topwin* topwin)
{
	struct rtgui_topwin *old_focus_topwin;

	RT_ASSERT(topwin != RT_NULL);

	if (topwin->flag & WINTITLE_NOFOCUS)
	{
		/* just raise it, not affect others. */
		_rtgui_topwin_raise_topwin_in_tree(topwin);

		/* update clip info */
		rtgui_topwin_update_clip();
		return;
	}

	old_focus_topwin = rtgui_topwin_get_focus();

	if (old_focus_topwin == topwin)
		return;

	_rtgui_topwin_raise_topwin_in_tree(topwin);
	/* update clip info */
	rtgui_topwin_update_clip();

	if (old_focus_topwin != RT_NULL)
	{
		/* deactivate the old focus window firstly, otherwise it will make the new
		 * window invisible. */
		_rtgui_topwin_deactivate(old_focus_topwin);
	}

	_rtgui_topwin_only_activate(topwin);
}

/* map func to the topwin tree in preorder.
 *
 * Remember that we are in a embedded system so write the @param func memory
 * efficiently.
 */
rt_inline void _rtgui_topwin_preorder_map(struct rtgui_topwin *topwin, void (*func)(struct rtgui_topwin*))
{
	struct rt_list_node *child;

	RT_ASSERT(topwin != RT_NULL);
	RT_ASSERT(func != RT_NULL);

	func(topwin);

	rt_list_foreach(child, &topwin->child_list, next)
		_rtgui_topwin_preorder_map(get_topwin_from_list(child), func);
}

rt_inline void _rtgui_topwin_mark_hidden(struct rtgui_topwin *topwin)
{
	topwin->flag &= ~WINTITLE_SHOWN;
	if (topwin->title != RT_NULL)
	{
		RTGUI_WIDGET_HIDE(RTGUI_WIDGET(topwin->title));
	}
	RTGUI_WIDGET_HIDE(RTGUI_WIDGET(topwin->wid));
}

rt_inline void _rtgui_topwin_mark_shown(struct rtgui_topwin *topwin)
{
	topwin->flag |= WINTITLE_SHOWN;
	if (topwin->title != RT_NULL)
	{
		RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(topwin->title));
	}
	RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(topwin->wid));
}

static void _rtgui_topwin_draw_tree(struct rtgui_topwin *topwin, struct rtgui_event_paint *epaint)
{
	struct rt_list_node *node;

	if (topwin->title != RT_NULL)
	{
		rtgui_theme_draw_win(topwin);
	}

	epaint->wid = topwin->wid;
	rtgui_application_send(topwin->tid, &(epaint->parent), sizeof(struct rtgui_event_paint));

	rt_list_foreach(node, &topwin->child_list, prev)
	{
		_rtgui_topwin_draw_tree(get_topwin_from_list(node), epaint);
	}
}

/* show the window tree. @param epaint can be initialized outside and reduce stack
 * usage. */
static void _rtgui_topwin_show_tree(struct rtgui_topwin *topwin, struct rtgui_event_paint *epaint)
{
	RT_ASSERT(topwin != RT_NULL);
	RT_ASSERT(epaint != RT_NULL);

	/* we have to mark the _all_ tree before update_clip because update_clip
	 * will stop as hidden windows */
	_rtgui_topwin_preorder_map(topwin, _rtgui_topwin_mark_shown);

	// TODO: if all the window is shown already, there is no need to
	// update_clip. But since we use peorder_map, it seems it's a bit difficult
	// to tell whether @param topwin and it's children are all shown.
	rtgui_topwin_update_clip();

	_rtgui_topwin_draw_tree(topwin, epaint);
}

/** show a window
 *
 * If any parent window in the hierarchy tree is hidden, this window won't be
 * shown. If this window could be shown, all the child windows will be shown as
 * well. The topmost child will be active.
 *
 * Top level window(parent == RT_NULL) can always be shown.
 */
void rtgui_topwin_show(struct rtgui_event_win* event)
{
	struct rtgui_topwin *topwin;
	struct rtgui_win* wid = event->wid;
	struct rtgui_event_paint epaint;

	RTGUI_EVENT_PAINT_INIT(&epaint);

	/* find in hide list */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_list);
	if (topwin == RT_NULL ||
		!_rtgui_topwin_could_show(topwin))
	{
		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_ERROR);
		return;
	}

	_rtgui_topwin_show_tree(topwin, &epaint);

	rtgui_topwin_activate_win(topwin);

	rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);
}

/* hide a window */
void rtgui_topwin_hide(struct rtgui_event_win* event)
{
	struct rtgui_topwin* topwin;
	struct rtgui_topwin *old_focus_topwin = rtgui_topwin_get_focus();
	struct rtgui_win* wid = event->wid;
	struct rt_list_node *containing_list;

	/* find in show list */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_list);
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

	_rtgui_topwin_preorder_map(topwin, _rtgui_topwin_mark_hidden);

	if (topwin->parent == RT_NULL)
		containing_list = &_rtgui_topwin_list;
	else
		containing_list = &topwin->parent->child_list;

	rt_list_remove(&topwin->list);
	rt_list_insert_before(containing_list, &topwin->list);

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
	topwin = rtgui_topwin_search_in_list(event->wid, &_rtgui_topwin_list);
	if (topwin == RT_NULL ||
		!(topwin->flag & WINTITLE_SHOWN))
	{
		rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_ERROR);
	}

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

	/* send status ok */
	rtgui_application_ack(RTGUI_EVENT(event), RTGUI_STATUS_OK);
}

/*
 * resize a top win
 * Note: currently, only support resize hidden window
 */
void rtgui_topwin_resize(struct rtgui_win* wid, rtgui_rect_t* rect)
{
	struct rtgui_topwin* topwin;
	struct rtgui_region region;

	/* find in show list */
	topwin = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_list);
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

static struct rtgui_topwin* _rtgui_topwin_get_focus_from_list(struct rt_list_node *list)
{
	struct rt_list_node *node;

	RT_ASSERT(list != RT_NULL);

	rt_list_foreach(node, list, next)
	{
		struct rtgui_topwin *child = get_topwin_from_list(node);
		if (child->flag & WINTITLE_ACTIVATE)
			return child;

		child = _rtgui_topwin_get_focus_from_list(&child->child_list);
		if (child != RT_NULL)
			return child;
	}

	return RT_NULL;
}

struct rtgui_topwin* rtgui_topwin_get_focus(void)
{
#if 1
	// debug code
	struct rtgui_topwin *topwin = _rtgui_topwin_get_focus_from_list(&_rtgui_topwin_list);

	if (topwin != RT_NULL)
		RT_ASSERT(get_topwin_from_list(_rtgui_topwin_list.next) ==
				  _rtgui_topwin_get_top_parent(topwin));

	return topwin;
#else
	return _rtgui_topwin_get_focus_from_list(&_rtgui_topwin_list);
#endif
}

static struct rtgui_topwin* _rtgui_topwin_get_wnd_from_tree(struct rt_list_node *list, int x, int y)
{
	struct rt_list_node *node;
	struct rtgui_topwin *topwin, *target;

	RT_ASSERT(list != RT_NULL);

	rt_list_foreach(node, list, next)
	{
		topwin = get_topwin_from_list(node);
		if (!(topwin->flag & WINTITLE_SHOWN))
			break;

		/* if higher window have this point, return it */
		target = _rtgui_topwin_get_wnd_from_tree(&topwin->child_list, x, y);
		if (target != RT_NULL)
			return target;

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

struct rtgui_topwin* rtgui_topwin_get_wnd(int x, int y)
{
	return _rtgui_topwin_get_wnd_from_tree(&_rtgui_topwin_list, x, y);
}

/* clip region from topwin, and the windows beneath it. */
rt_inline void _rtgui_topwin_clip_to_region(struct rtgui_region *region,
										    struct rtgui_topwin *topwin)
{
	RT_ASSERT(region != RT_NULL);
	RT_ASSERT(topwin != RT_NULL);

	if (topwin->title != RT_NULL)
	{
		rtgui_region_reset(&(RTGUI_WIDGET(topwin->title)->clip),
						   &(RTGUI_WIDGET(topwin->title)->extent));
		rtgui_region_intersect(&(RTGUI_WIDGET(topwin->title)->clip),
							   &(RTGUI_WIDGET(topwin->title)->clip),
							   region);
		rtgui_region_subtract_rect(&(RTGUI_WIDGET(topwin->title)->clip),
								   &(RTGUI_WIDGET(topwin->title)->clip),
								   &topwin->extent);
	}

	rtgui_region_reset(&RTGUI_WIDGET(topwin->wid)->clip,
					   &RTGUI_WIDGET(topwin->wid)->extent);
	rtgui_region_intersect(&RTGUI_WIDGET(topwin->wid)->clip,
						   &RTGUI_WIDGET(topwin->wid)->clip,
						   region);
}

static void rtgui_topwin_update_clip(void)
{
	struct rtgui_topwin *top;
	struct rtgui_event_clip_info eclip;
	/* Note that the region is a "female die", that means it's the region you
	 * can paint to, not the region covered by others.
	 */
	struct rtgui_region region_available;

	if (rt_list_isempty(&_rtgui_topwin_list) ||
		!(get_topwin_from_list(_rtgui_topwin_list.next)->flag & WINTITLE_SHOWN))
		return;

	RTGUI_EVENT_CLIP_INFO_INIT(&eclip);

	rtgui_region_init_rect(&region_available, 0, 0,
			rtgui_graphic_driver_get_default()->width,
			rtgui_graphic_driver_get_default()->height);

	/* from top to bottom. */
	top = _rtgui_topwin_get_topmost_window_shown();

	while (top != RT_NULL)
	{
		/* clip the topwin */
		_rtgui_topwin_clip_to_region(&region_available, top);

		/* update available region */
		if (top->title != RT_NULL)
			rtgui_region_subtract_rect(&region_available, &region_available, &RTGUI_WIDGET(top->title)->extent);
		else
			rtgui_region_subtract_rect(&region_available, &region_available, &top->extent);

		/* send clip event to destination window */
		eclip.wid = top->wid;
		rtgui_application_send(top->tid, &(eclip.parent), sizeof(struct rtgui_event_clip_info));

		/* iterate to next topwin */
		if (top->list.next != &top->parent->child_list &&
			get_topwin_from_list(top->list.next)->flag & WINTITLE_SHOWN)
			top = get_topwin_from_list(top->list.next);
		else
			/* level up */
			top = top->parent;
	}
}

static void _rtgui_topwin_redraw_tree(struct rt_list_node *list,
									  struct rtgui_rect* rect,
									  struct rtgui_event_paint *epaint)
{
	struct rt_list_node *node;

	RT_ASSERT(list != RT_NULL);
	RT_ASSERT(rect != RT_NULL);
	RT_ASSERT(epaint != RT_NULL);

	/* skip the hidden windows */
	rt_list_foreach(node, list, prev)
	{
		if (get_topwin_from_list(node)->flag & WINTITLE_SHOWN)
			break;
	}

	for (; node != list; node = node->prev)
	{
		struct rtgui_topwin *topwin;

		topwin = get_topwin_from_list(node);

		//FIXME: intersect with clip?
		if (rtgui_rect_is_intersect(rect, &(topwin->extent)) == RT_EOK)
		{
			epaint->wid = topwin->wid;
			rtgui_application_send(topwin->tid, &(epaint->parent), sizeof(*epaint));

			/* draw title */
			if (topwin->title != RT_NULL)
			{
				rtgui_theme_draw_win(topwin);
			}
		}

		_rtgui_topwin_redraw_tree(&topwin->child_list, rect, epaint);
	}
}

static void rtgui_topwin_redraw(struct rtgui_rect* rect)
{
	struct rtgui_event_paint epaint;
	RTGUI_EVENT_PAINT_INIT(&epaint);
	epaint.wid = RT_NULL;

	_rtgui_topwin_redraw_tree(&_rtgui_topwin_list, rect, &epaint);
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
	win = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_list);
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
	win = rtgui_topwin_search_in_list(wid, &_rtgui_topwin_list);
	if (win == RT_NULL)
		return;

	/* remove rect from top window monitor rect list */
	rtgui_mouse_monitor_remove(&(win->monitor_list), rect);
}

