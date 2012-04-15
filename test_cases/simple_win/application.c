#include <rtthread.h>

#include <rtgui/rtgui.h>
#include <rtgui/rtgui_server.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/rtgui_application.h>
#include <rtgui/widgets/window.h>
#include <rtgui/widgets/label.h>
#include <rtgui/driver.h>

/*#include "test_cases.h"*/

rt_bool_t on_window_close(struct rtgui_object* object, struct rtgui_event* event)
{
	rt_kprintf("win %s(%p) closing\n",
			rtgui_win_get_title(RTGUI_WIN(object)),
			object);
	return RT_TRUE;
}

void create_wins(struct rtgui_application *app, void *parameter)
{
	struct rtgui_win *win1, *win2, *win3, *win4;
	struct rtgui_label *label;
	struct rtgui_rect rect;

#ifdef RTGUI_USING_DESKTOP_WINDOW
	struct rtgui_win *dsk;

	if (parameter)
	{
		rtgui_graphic_driver_get_rect(rtgui_graphic_driver_get_default(), &rect);
		dsk = rtgui_win_create(RT_NULL, "desktop", &rect, RTGUI_WIN_STYLE_DESKTOP_DEFAULT);
		rtgui_win_show(dsk, RT_FALSE);
	}
#endif

	rect.x1 = 40, rect.y1 = 40, rect.x2 = 200, rect.y2 = 80;

	win1 = rtgui_win_create(RT_NULL,
		"test window", &rect, RTGUI_WIN_STYLE_DEFAULT);

	rtgui_win_set_onclose(win1, on_window_close);

	rect.x1 += 20;
	rect.x2 -= 5;
	rect.y1 += 5;
	rect.y2 = rect.y1 + 20;

	label = rtgui_label_create("window in modal mode");
	rtgui_widget_set_rect(RTGUI_WIDGET(label), &rect);
	rtgui_container_add_child(RTGUI_CONTAINER(win1), RTGUI_WIDGET(label));

	rtgui_win_show(win1, RT_TRUE);

	rt_kprintf("win1 terminated\n");
	/*rtgui_win_destroy(win1);*/

	rect.x1 = 20;
	rect.y1 = 80;
	rect.x2 = 180;
	rect.y2 = 90;
	win2 = rtgui_win_create(win1,
		"test window2", &rect, RTGUI_WIN_STYLE_DEFAULT);

	rtgui_win_set_onclose(win2, on_window_close);
	rtgui_win_show(win1, RT_FALSE);
	/*rtgui_win_show(win2, RT_TRUE);*/

	/* create second window tree */
	rect.y1 = 150;
	rect.y2 = rect.y1 + 50;
	win3 = rtgui_win_create(RT_NULL,
		"test tree2", &rect, RTGUI_WIN_STYLE_DEFAULT);

	rtgui_win_set_onclose(win3, on_window_close);

	rect.x1 += 20;
	rect.x2 -= 5;
	rect.y1 += 5;
	rect.y2 = rect.y1 + 20;

	label = rtgui_label_create("window in modal mode");
	rtgui_widget_set_rect(RTGUI_WIDGET(label), &rect);
	rtgui_container_add_child(RTGUI_CONTAINER(win3), RTGUI_WIDGET(label));

	rect.x1 = 20;
	rect.y1 = 180;
	rect.x2 = 180;
	rect.y2 = 190;
	win4 = rtgui_win_create(win3,
		"test tree2.1", &rect, RTGUI_WIN_STYLE_DEFAULT);

	rtgui_win_set_onclose(win4, on_window_close);
	rtgui_win_show(win3, RT_FALSE);
	rtgui_topwin_dump_tree();
	/*rtgui_win_show(win4, RT_TRUE);*/

	rtgui_application_run(app);

	rtgui_win_destroy(win1);
	rtgui_win_destroy(win2);
	rtgui_win_destroy(win3);
	rtgui_win_destroy(win4);
}

void rt_init_thread_entry(void* parameter)
{
    extern void rtgui_startup();
    extern void rt_hw_lcd_init();
    extern void rtgui_touch_hw_init(void);

	struct rtgui_application* app;


	app = rtgui_application_create(
			rt_thread_self(),
			"guiapp");

	RT_ASSERT(app != RT_NULL);

	create_wins(app, parameter);

	window_focus();

	rtgui_application_run(app);

	rtgui_application_destroy(app);
	rt_kprintf("app destroyed\n");
}

int rt_application_init()
{
	rt_thread_t init_thread, init_thread2;

	rt_err_t result;

#if (RT_THREAD_PRIORITY_MAX == 32)
	init_thread = rt_thread_create("init",
								rt_init_thread_entry, 1,
								2048, 20, 20);
	init_thread2 = rt_thread_create("init2",
								rt_init_thread_entry, 0,
								2048, 20, 20);
#else
	init_thread = rt_thread_create("init",
								rt_init_thread_entry, 1,
								2048, 80, 20);
	init_thread2 = rt_thread_create("init2",
								rt_init_thread_entry, 0,
								2048, 80, 20);
#endif

	if (init_thread != RT_NULL)
		rt_thread_startup(init_thread);
	if (init_thread2 != RT_NULL)
		rt_thread_startup(init_thread2);

	return 0;

}
