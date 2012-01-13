#include <rtthread.h>

#include <rtgui/rtgui.h>
#include <rtgui/rtgui_server.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/driver.h>

#include "test_cases.h"

void rt_init_thread_entry(void* parameter)
{
    extern void rtgui_startup();
    extern void rt_hw_lcd_init();
    extern void rtgui_touch_hw_init(void);

    rt_device_t lcd;

    /* init lcd */
    rt_hw_lcd_init();

    /* init touch panel */
    /*rtgui_touch_hw_init();*/

    /* re-init device driver */
    rt_device_init_all();

    /* find lcd device */
    lcd = rt_device_find("sdl");

    /* set lcd device as rtgui graphic driver */
    rtgui_graphic_set_device(lcd);

    rtgui_system_server_init();

    become_rtgui_thread();

    show_win1();

    show_win2();

    window_focus();

    end_become_rtgui_thread();
}

int rt_application_init()
{
	rt_thread_t init_thread;

	rt_err_t result;

#if (RT_THREAD_PRIORITY_MAX == 32)
	init_thread = rt_thread_create("init",
								rt_init_thread_entry, RT_NULL,
								2048, 8, 20);
#else
	init_thread = rt_thread_create("init",
								rt_init_thread_entry, RT_NULL,
								2048, 80, 20);
#endif

	if (init_thread != RT_NULL)
		rt_thread_startup(init_thread);

	return 0;
}
