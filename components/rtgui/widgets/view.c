/*
 * File      : view.c
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
 * 2010-09-24     Bernard      fix view destroy issue
 */
#include <rtgui/dc.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/widgets/view.h>
#include <rtgui/widgets/workbench.h>

static void _rtgui_view_constructor(rtgui_view_t *view)
{
	/* init view */
	rtgui_widget_set_event_handler(RTGUI_WIDGET(view),
		rtgui_view_event_handler);

	rtgui_list_init(&(view->children));

	/* no focus on start up */
	view->focused = RT_NULL;
	/* view is used to 'contain'(show) widgets and dispatch events to
	 * them, not interact with user. So no need to grab focus. If we did it,
	 * some widget inherited from view(e.g. notebook) will grab the focus
	 * annoyingly.
	 *
	 * For example, a focusable notebook N has a widget W. When the user press
	 * W, N will gain the focus and W will lose it at first. Then N will set
	 * focus to W because it is W that eventually interact with people. N will
	 * yield focus and W will gain the focus again. This loop will make W
	 * repaint twice every time user press it.
	 *
	 * Just eliminate it.
	 */
	RTGUI_WIDGET(view)->flag &= ~RTGUI_WIDGET_FLAG_FOCUSABLE;

	view->modal_show = RT_FALSE;
	view->title = RT_NULL;
}

static void _rtgui_view_destructor(rtgui_view_t *view)
{
	/* remove view from workbench */
	if (RTGUI_WIDGET(view)->parent != RT_NULL)
	{
		rtgui_workbench_t *workbench;

		if (view->modal_show == RT_TRUE)
			rtgui_view_end_modal(view, RTGUI_MODAL_CANCEL);

		workbench = RTGUI_WORKBENCH(RTGUI_WIDGET(view)->parent);
		rtgui_workbench_remove_view(workbench, view);
	}

	if (view->title != RT_NULL)
	{
		rt_free(view->title);
		view->title = RT_NULL;
	}

	rtgui_view_destroy_children(view);
}

static void _rtgui_view_update_toplevel(rtgui_view_t* view)
{
	struct rtgui_list_node* node;

	rtgui_list_foreach(node, &(view->children))
	{
		rtgui_widget_t* child = rtgui_list_entry(node, rtgui_widget_t, sibling);
		/* set child toplevel */
		child->toplevel = rtgui_widget_get_toplevel(RTGUI_WIDGET(view));

		if (RTGUI_IS_VIEW(child))
		{
			_rtgui_view_update_toplevel(RTGUI_VIEW(child));
		}
	}
}

DEFINE_CLASS_TYPE(view, "view",
	RTGUI_WIDGET_TYPE,
	_rtgui_view_constructor,
	_rtgui_view_destructor,
	sizeof(struct rtgui_view));

rt_bool_t rtgui_view_dispatch_event(rtgui_view_t *view, rtgui_event_t* event)
{
	/* handle in child widget */
	struct rtgui_list_node* node;

	rtgui_list_foreach(node, &(view->children))
	{
		struct rtgui_widget* w;
		w = rtgui_list_entry(node, struct rtgui_widget, sibling);

		if (w->event_handler(w, event) == RT_TRUE)
			return RT_TRUE;
	}

	return RT_FALSE;
}

rt_bool_t rtgui_view_dispatch_mouse_event(rtgui_view_t *view, struct rtgui_event_mouse* event)
{
	/* handle in child widget */
	struct rtgui_list_node* node;
	rtgui_widget_t *focus;

	/* get focus widget on toplevel */
	focus = RTGUI_VIEW(RTGUI_WIDGET(view)->toplevel)->focused;
	rtgui_list_foreach(node, &(view->children))
	{
		struct rtgui_widget* w;
		w = rtgui_list_entry(node, struct rtgui_widget, sibling);
		if (rtgui_rect_contains_point(&(w->extent), event->x, event->y) == RT_EOK)
		{
			if ((focus != w) && RTGUI_WIDGET_IS_FOCUSABLE(w))
				rtgui_widget_focus(w);
			if (w->event_handler(w, (rtgui_event_t*)event) == RT_TRUE) return RT_TRUE;
		}
	}

	return RT_FALSE;
}

rt_bool_t rtgui_view_event_handler(struct rtgui_widget* widget, struct rtgui_event* event)
{
	struct rtgui_view* view = RTGUI_VIEW(widget);
	RT_ASSERT(widget != RT_NULL);

	switch (event->type)
	{
	case RTGUI_EVENT_PAINT:
		{
			struct rtgui_dc* dc;
			struct rtgui_rect rect;

			dc = rtgui_dc_begin_drawing(widget);
			if (dc == RT_NULL)
				return RT_FALSE;
			rtgui_widget_get_rect(widget, &rect);

			/* fill view with background */
			rtgui_dc_fill_rect(dc, &rect);

			/* paint on each child */
			rtgui_view_dispatch_event(view, event);

			rtgui_dc_end_drawing(dc);
		}
		break;

	case RTGUI_EVENT_KBD:
		/* let parent to handle keyboard event */
		if (widget->parent != RT_NULL && widget->parent != widget->toplevel)
		{
			return widget->parent->event_handler(widget->parent, event);
		}
		break;

	case RTGUI_EVENT_MOUSE_BUTTON:
	case RTGUI_EVENT_MOUSE_MOTION:
		/* handle in child widget */
		return rtgui_view_dispatch_mouse_event(view,
			(struct rtgui_event_mouse*)event);

	case RTGUI_EVENT_COMMAND:
	case RTGUI_EVENT_RESIZE:
		rtgui_view_dispatch_event(view, event);
		break;

	default:
		/* call parent widget event handler */
		return rtgui_widget_event_handler(widget, event);
	}

	return RT_FALSE;
}

rtgui_view_t* rtgui_view_create(const char* title)
{
	struct rtgui_view* view;

	/* allocate view */
	view = (struct rtgui_view*) rtgui_widget_create (RTGUI_VIEW_TYPE);
	if (view != RT_NULL)
	{
		if (title != RT_NULL)
			view->title = rt_strdup(title);
	}

	return view;
}

void rtgui_view_destroy(rtgui_view_t* view)
{
	if (view->modal_show == RT_TRUE)
		rtgui_view_end_modal(view, RTGUI_MODAL_CANCEL);
	else
	{
		rtgui_view_hide(view);
		rtgui_widget_destroy(RTGUI_WIDGET(view));
	}
}

/*
 * This function will add a child to a view widget
 * Note: this function will not change the widget layout
 * the layout is the responsibility of layout widget, such as box.
 */
void rtgui_view_add_child(rtgui_view_t *view, rtgui_widget_t* child)
{
	RT_ASSERT(view != RT_NULL);
	RT_ASSERT(child != RT_NULL);

	/* set parent and toplevel widget */
	child->parent = RTGUI_WIDGET(view);
	/* put widget to parent's children list */
	rtgui_list_append(&(view->children), &(child->sibling));

	/* update children toplevel */
	if (RTGUI_WIDGET(view)->toplevel != RT_NULL &&
		RTGUI_IS_TOPLEVEL(RTGUI_WIDGET(view)->toplevel))
	{
		child->toplevel = rtgui_widget_get_toplevel(RTGUI_WIDGET(view));

		/* update all child toplevel */
		if (RTGUI_IS_VIEW(child))
		{
			_rtgui_view_update_toplevel(RTGUI_VIEW(child));
		}
	}
}

/* remove a child to widget */
void rtgui_view_remove_child(rtgui_view_t *view, rtgui_widget_t* child)
{
	RT_ASSERT(view != RT_NULL);
	RT_ASSERT(child != RT_NULL);

	if (child == view->focused)
	{
		/* set focused to itself */
		view->focused = RT_NULL;

		rtgui_widget_focus(RTGUI_WIDGET(view));
	}

	/* remove widget from parent's children list */
	rtgui_list_remove(&(view->children), &(child->sibling));

	/* set parent and toplevel widget */
	child->parent = RT_NULL;
	child->toplevel = RT_NULL;
}

/* destroy all children of view */
void rtgui_view_destroy_children(rtgui_view_t *view)
{
	struct rtgui_list_node* node;

	if (view == RT_NULL) return;

	node = view->children.next;
	while (node != RT_NULL)
	{
		rtgui_widget_t* child = rtgui_list_entry(node, rtgui_widget_t, sibling);

		if (RTGUI_IS_VIEW(child))
		{
			/* break parent firstly */
			child->parent = RT_NULL;

			/* destroy children of child */
			rtgui_view_destroy_children(RTGUI_VIEW(child));
		}

		/* remove widget from parent's children list */
		rtgui_list_remove(&(view->children), &(child->sibling));

		/* set parent and toplevel widget */
		child->parent = RT_NULL;

		/* destroy object and remove from parent */
		rtgui_object_destroy(RTGUI_OBJECT(child));

		node = view->children.next;
	}

	view->children.next = RT_NULL;
	view->focused = RT_NULL;
	if (RTGUI_WIDGET(view)->parent != RT_NULL)
		rtgui_widget_focus(RTGUI_WIDGET(view));

	/* update widget clip */
	rtgui_toplevel_update_clip(RTGUI_TOPLEVEL(RTGUI_WIDGET(view)->toplevel));
}

rtgui_widget_t* rtgui_view_get_first_child(rtgui_view_t* view)
{
	rtgui_widget_t* child = RT_NULL;

	if (view->children.next != RT_NULL)
	{
		child = rtgui_list_entry(view->children.next, rtgui_widget_t, sibling);
	}

	return child;
}

#ifndef RTGUI_USING_SMALL_SIZE
void rtgui_view_set_box(rtgui_view_t* view, rtgui_box_t* box)
{
	if (view == RT_NULL ||
		box  == RT_NULL) return;

	rtgui_view_add_child(RTGUI_VIEW(view), RTGUI_WIDGET(box));
	rtgui_widget_set_rect(RTGUI_WIDGET(box), &(RTGUI_WIDGET(view)->extent));
}
#endif

rtgui_modal_code_t rtgui_view_show(rtgui_view_t* view, rt_bool_t is_modal)
{
	rtgui_workbench_t* workbench;

	/* parameter check */
	if (view == RT_NULL) return RTGUI_MODAL_CANCEL;

	if (RTGUI_WIDGET(view)->parent == RT_NULL)
	{
		RTGUI_WIDGET_UNHIDE(RTGUI_WIDGET(view));
		return RTGUI_MODAL_CANCEL;
	}

	workbench = RTGUI_WORKBENCH(RTGUI_WIDGET(view)->parent);
	rtgui_workbench_show_view(workbench, view);
	if (RTGUI_VIEW(view)->focused != RT_NULL)
		rtgui_widget_focus(RTGUI_VIEW(view)->focused);
	else
	{
		if (RTGUI_WIDGET_IS_FOCUSABLE(RTGUI_WIDGET(view)))
			rtgui_widget_focus(RTGUI_WIDGET(view));
	}

	view->modal_show = is_modal;
	if (is_modal == RT_TRUE)
	{
		/* set modal mode */
		workbench->flag |= RTGUI_WORKBENCH_FLAG_MODAL_MODE;
		workbench->modal_widget = RTGUI_WIDGET(view);

		/* perform workbench event loop */
		rtgui_workbench_event_loop(workbench);

		workbench->modal_widget = RT_NULL;
		return workbench->modal_code;
	}

	/* no modal mode, always return modal_ok */
	return RTGUI_MODAL_OK;
}

void rtgui_view_end_modal(rtgui_view_t* view, rtgui_modal_code_t modal_code)
{
	rtgui_workbench_t* workbench;

	/* parameter check */
	if ((view == RT_NULL) || (RTGUI_WIDGET(view)->parent == RT_NULL))return ;

	workbench = RTGUI_WORKBENCH(RTGUI_WIDGET(view)->parent);
	workbench->modal_code = modal_code;
	workbench->flag &= ~RTGUI_WORKBENCH_FLAG_MODAL_MODE;

	/* remove modal mode */
	view->modal_show = RT_FALSE;
}

void rtgui_view_hide(rtgui_view_t* view)
{
	if (view == RT_NULL) return;

	if (RTGUI_WIDGET(view)->parent == RT_NULL)
	{
		RTGUI_WIDGET_HIDE(RTGUI_WIDGET(view));
		return;
	}

	rtgui_workbench_hide_view((rtgui_workbench_t*)(RTGUI_WIDGET(view)->parent), view);
}

char* rtgui_view_get_title(rtgui_view_t* view)
{
	RT_ASSERT(view != RT_NULL);

	return view->title;
}

void rtgui_view_set_title(rtgui_view_t* view, const char *title)
{
	RT_ASSERT(view != RT_NULL);

	if (view->title != RT_NULL)
	{
		rtgui_free(view->title);

		if (title != RT_NULL) view->title = rt_strdup(title);
		else view->title = RT_NULL;
	}
}
