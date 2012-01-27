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

void init_panel(void)
{
    rtgui_rect_t rect;

    /* register main panel */
    rect.x1 = 0;
    rect.y1 = 0;
    rect.x2 = rtgui_graphic_driver_get_default()->width;
    rect.y2 = rtgui_graphic_driver_get_default()->height;
    rtgui_panel_register("main", &rect);
    rtgui_panel_set_default_focused("main");
}

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
		"test window", &rect, RTGUI_WIN_STYLE_DEFAULT | RTGUI_WIN_STYLE_DESTROY_ON_CLOSE);

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
}

void rt_init_thread_entry(void* parameter)
{
    extern void rtgui_startup();
    extern void rt_hw_lcd_init();
    extern void rtgui_touch_hw_init(void);

	init_panel();

	app = rtgui_application_create(
			rt_thread_self(),
			"main",
			"guiapp");

	RT_ASSERT(app != RT_NULL);

	create_wins();

	window_focus();

	rtgui_application_run(app);

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
