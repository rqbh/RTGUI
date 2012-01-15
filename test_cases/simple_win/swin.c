#include <rtthread.h>

#include <rtgui/rtgui.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/widgets/window.h>
#include <rtgui/widgets/label.h>

rt_mq_t mq;

void become_rtgui_thread(void)
{
#ifdef RTGUI_USING_SMALL_SIZE
	mq = rt_mq_create("rtgui", 32, 32, RT_IPC_FLAG_FIFO);
#else
	mq = rt_mq_create("rtgui", 256, 32, RT_IPC_FLAG_FIFO);
#endif

	rtgui_thread_register(rt_thread_self(), mq);
}

void end_become_rtgui_thread(void)
{
	rtgui_thread_deregister(rt_thread_self());
	rt_mq_delete(mq);
}

rt_bool_t on_window_close(struct rtgui_widget* widget, struct rtgui_event* event)
{
	rt_kprintf("win %s(%p) closing\n",
			rtgui_win_get_title(RTGUI_WIN(widget)),
			widget);
	return RT_TRUE;
}

void show_win1(void)
{
	rtgui_win_t *win;
	rtgui_rect_t rect = {40, 40, 200, 80};

	/* 创建一个窗口 */
	win = rtgui_win_create(RT_NULL,
		"test window", &rect, RTGUI_WIN_STYLE_DEFAULT);

	rtgui_win_set_onclose(win, on_window_close);

	/* 模态显示窗口 */
	rtgui_win_show(win, RT_TRUE);
}

void show_win2(void)
{
	rtgui_win_t *win;
	rtgui_label_t *label;
	rtgui_rect_t rect = {40, 40, 200, 80};

	/* 创建一个窗口 */
	win = rtgui_win_create(RT_NULL,
		"test window2", &rect, RTGUI_WIN_STYLE_DEFAULT);

	rect.x1 += 20;
	rect.x2 -= 5;
	rect.y1 += 5;
	rect.y2 = rect.y1 + 20;

	label = rtgui_label_create("这是一个模式窗口");
	rtgui_widget_set_rect(RTGUI_WIDGET(label), &rect);
	rtgui_view_add_child(RTGUI_VIEW(win), RTGUI_WIDGET(label));

	rtgui_win_set_onclose(win, on_window_close);

	/* 模态显示窗口 */
	rtgui_win_show(win, RT_TRUE);
}
