#include <rtgui/rtgui.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/rtgui_application.h>

#include <rtgui/widgets/window.h>
#include <rtgui/widgets/notebook.h>

struct rtgui_notebook *the_notebook;

static rt_bool_t demo_application_event_handler(struct rtgui_object* object, struct rtgui_event* event)
{
	/* 我们目前只对按键事件感兴趣。*/
	if (event->type == RTGUI_EVENT_KBD)
	{
		struct rtgui_event_kbd* ekbd = (struct rtgui_event_kbd*)event;

		if (ekbd->type == RTGUI_KEYDOWN)
		{
			if (ekbd->key == RTGUIK_RIGHT)
			{
				demo_view_next(RT_NULL, RT_NULL);
				return RT_TRUE;
			}
			else if (ekbd->key == RTGUIK_LEFT)
			{
				demo_view_prev(RT_NULL, RT_NULL);
				return RT_TRUE;
			}
		}
	}

	return rtgui_application_event_handler(object, event);
}

static void application_entry(void* parameter)
{
	struct rtgui_application *app;
	struct rtgui_win *win;
	struct rtgui_rect rect;

	app = rtgui_application_create(rt_thread_self(), "gui_demo");
	if (app == RT_NULL)
		return;

	rtgui_object_set_event_handler(RTGUI_OBJECT(app), demo_application_event_handler);

	/* create a full screen window */
	rtgui_graphic_driver_get_rect(rtgui_graphic_driver_get_default(), &rect);

	win = rtgui_win_create(RT_NULL, "demo_win", &rect, RTGUI_WIN_STYLE_NO_BORDER);
	if (win == RT_NULL)
	{
		rtgui_application_destroy(app);
		return;
	}

	/* create a no title notebook that we can switch demo on it easily. */
	the_notebook = rtgui_notebook_create(&rect, RTGUI_NOTEBOOK_NOTAB);
	if (the_notebook == RT_NULL)
	{
		rtgui_win_destroy(win);
		rtgui_application_destroy(app);
		return;
	}

	rtgui_container_add_child(RTGUI_CONTAINER(win), RTGUI_WIDGET(the_notebook));

	/* 初始化各个例子的视图 */
	demo_view_benchmark();

	demo_view_dc();
#if 0
#ifdef RTGUI_USING_TTF
	demo_view_ttf(notebook);
#endif

#ifndef RTGUI_USING_SMALL_SIZE
	demo_view_dc_buffer(notebook);
#endif
	demo_view_animation(notebook);
#ifndef RTGUI_USING_SMALL_SIZE
	demo_view_buffer_animation(notebook);
	// demo_view_instrument_panel(notebook);
#endif
	demo_view_window(notebook);
	demo_view_label(notebook);
	demo_view_button(notebook);
	demo_view_checkbox(notebook);
	demo_view_progressbar(notebook);
	demo_view_scrollbar(notebook);
	demo_view_radiobox(notebook);
	demo_view_textbox(notebook);
	demo_view_listbox(notebook);
	demo_view_menu(notebook);
	demo_view_listctrl(notebook);
	demo_view_combobox(notebook);
	demo_view_slider(notebook);
	demo_view_notebook(notebook);
	demo_view_mywidget(notebook);
#if defined(RTGUI_USING_DFS_FILERW) || defined(RTGUI_USING_STDIO_FILERW)
	demo_view_image(notebook);
#endif
#ifdef RT_USING_MODULE	
#if defined(RTGUI_USING_DFS_FILERW) || defined(RTGUI_USING_STDIO_FILERW)
	demo_view_module(notebook);
#endif
#endif
	demo_listview_view(notebook);
	demo_listview_icon_view(notebook);
#if defined(RTGUI_USING_DFS_FILERW) || defined(RTGUI_USING_STDIO_FILERW)
	demo_fn_view(notebook);
#endif
#endif

	rtgui_win_show(win, RT_FALSE);

	/* 执行工作台事件循环 */
	rtgui_application_run(app);

	rtgui_application_destroy(app);
}

void application_init()
{
	static rt_bool_t inited = RT_FALSE;

	if (inited == RT_FALSE) /* 避免重复初始化而做的保护 */
	{
		rt_thread_t tid;

		tid = rt_thread_create("wb",
			application_entry, RT_NULL,
			2048 * 2, 25, 10);

		if (tid != RT_NULL)
			rt_thread_startup(tid);

		inited = RT_TRUE;
	}
}

#ifdef RT_USING_FINSH
#include <finsh.h>
void application()
{
	application_init();
}
/* finsh的命令输出，可以直接执行application()函数以执行上面的函数 */
FINSH_FUNCTION_EXPORT(application, application demo)
#endif
