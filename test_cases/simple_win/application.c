#include <rtthread.h>

#include <rtgui/rtgui.h>
#include <rtgui/rtgui_server.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/rtgui_application.h>
#include <rtgui/widgets/window.h>
#include <rtgui/widgets/label.h>
#include <rtgui/driver.h>

/*#include "test_cases.h"*/

struct rtgui_application* app;
struct rtgui_win *win1;
struct rtgui_win *win2;

rt_bool_t on_window_close(struct rtgui_object* object, struct rtgui_event* event)
{
	rt_kprintf("win %s(%p) closing\n",
			rtgui_win_get_title(RTGUI_WIN(object)),
			object);
	return RT_TRUE;
}

void create_wins(void)
{
	rtgui_label_t *label;
	rtgui_rect_t rect = {40, 40, 200, 80};

	win1 = rtgui_win_create(RT_NULL,
		"test window", &rect, RTGUI_WIN_STYLE_DEFAULT);

	rtgui_win_set_onclose(win1, on_window_close);

	rtgui_win_show(win1, RT_TRUE);

	rt_kprintf("win1 terminated\n");
	rtgui_win_destroy(win1);

	win2 = rtgui_win_create(RT_NULL,
		"test window2", &rect, RTGUI_WIN_STYLE_DEFAULT);

	rtgui_win_show(win2, RT_FALSE);

	rect.x1 += 20;
	rect.x2 -= 5;
	rect.y1 += 5;
	rect.y2 = rect.y1 + 20;

	label = rtgui_label_create("test label in win");
	rtgui_widget_set_rect(RTGUI_WIDGET(label), &rect);
	rtgui_container_add_child(RTGUI_CONTAINER(win2), RTGUI_WIDGET(label));
}

void rt_init_thread_entry(void* parameter)
{
    extern void rtgui_startup();
    extern void rt_hw_lcd_init();
    extern void rtgui_touch_hw_init(void);

	app = rtgui_application_create(
			rt_thread_self(),
			RT_NULL,
			"guiapp");

	RT_ASSERT(app != RT_NULL);

	create_wins();

    /*window_focus();*/

	rtgui_application_exec(app);

	rtgui_application_destroy(app);
	rt_kprintf("app destroyed\n");
}

int rt_application_init()
{
	rt_thread_t init_thread;

	rt_err_t result;

#if (RT_THREAD_PRIORITY_MAX == 32)
	init_thread = rt_thread_create("init",
								rt_init_thread_entry, RT_NULL,
								2048, 20, 20);
#else
	init_thread = rt_thread_create("init",
								rt_init_thread_entry, RT_NULL,
								2048, 80, 20);
#endif

	if (init_thread != RT_NULL)
		rt_thread_startup(init_thread);

	return 0;
}
