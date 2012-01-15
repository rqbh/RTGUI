#include <rtthread.h>

#include <rtgui/rtgui.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/widgets/window.h>
#include <rtgui/widgets/label.h>
#include <rtgui/widgets/button.h>

rt_bool_t picture_win_onpaint(struct rtgui_widget* widget, struct rtgui_event* event)
{
	if (event->type == RTGUI_EVENT_PAINT)
	{
		struct rtgui_dc* dc;
		struct rtgui_rect rect;
		struct rtgui_event_paint event;

		/* begin drawing */
		dc = rtgui_dc_begin_drawing(RTGUI_WIDGET(widget));
		if (dc == RT_NULL)
			return RT_FALSE;

		/* get window rect */
		rtgui_widget_get_rect(RTGUI_WIDGET(widget), &rect);

		RTGUI_DC_BC(dc) = white;
		rtgui_dc_fill_rect(dc, &rect);

		rtgui_dc_end_drawing(dc);

		return RT_FALSE;
	}
	else
	{
		return rtgui_win_event_handler(widget, event);
	}
}

rt_bool_t window_focus(void)
{
	rtgui_label_t *label;

	rtgui_button_t* start_btn;
	rtgui_button_t* stop_btn;
	rtgui_toplevel_t *parent;
    rtgui_win_t *win, *picture_win;
	rtgui_rect_t rect = {0, 20, 240, 320};

	/* 创建一个窗口 */
	win = rtgui_win_create(RT_NULL,
			"主窗口",
			&rect,
			RTGUI_WIN_STYLE_DEFAULT);
	/* 获得视图的位置信息 */
	rtgui_widget_get_rect(RTGUI_WIDGET(win), &rect);
	rtgui_widget_rect_to_device(RTGUI_WIDGET(win), &rect);
	rect.x1 += 10;
	rect.x2 -= 5;
	rect.y2 = rect.y1 + 20;

	/* 创建标题用的标签 */
	label = rtgui_label_create("主窗口");
	/* 设置标签位置信息 */
	rtgui_widget_set_rect(RTGUI_WIDGET(label), &rect);
	/* 添加标签到视图中 */
	rtgui_view_add_child(RTGUI_VIEW(win),
			RTGUI_WIDGET(label));
	/*添加放大按钮*/

	/* 获得视图的位置信息 */
	rtgui_widget_get_rect(RTGUI_WIDGET(win), &rect);
	rtgui_widget_rect_to_device(RTGUI_WIDGET(win), &rect);
	rect.x1 += 10;
	rect.y2 -= 10;
	rect.y1 = rect.y2 - 25;
	rect.x2 = rect.x1 + 50;

	/* 创建"启动"按钮 */
	start_btn = rtgui_button_create("按钮1");
	/* 设置按钮的位置信息 */
	rtgui_widget_set_rect(RTGUI_WIDGET(start_btn), &rect);
	/* 添加按钮到视图中 */
	rtgui_view_add_child(RTGUI_VIEW(win),
			RTGUI_WIDGET(start_btn));

	/* 添加停止按钮*/
	rtgui_widget_get_rect(RTGUI_WIDGET(win), &rect);
	rtgui_widget_rect_to_device(RTGUI_WIDGET(win), &rect);
	rect.x2 -= 10;
	rect.y2 -= 10;
	rect.x1 = rect.x2 - 50;
	rect.y1 = rect.y2 - 25;

	/* 创建"停止"按钮 */
	stop_btn = rtgui_button_create("按钮2");
	/* 设置按钮的位置信息 */
	rtgui_widget_set_rect(RTGUI_WIDGET(stop_btn), &rect);
	/* 添加按钮到视图中 */
	rtgui_view_add_child(RTGUI_VIEW(win),
			RTGUI_WIDGET(stop_btn));

	parent = RTGUI_TOPLEVEL(rtgui_widget_get_toplevel(RTGUI_WIDGET(start_btn)));
	/*创建一个绘图Windows控件*/
	rtgui_widget_get_rect(RTGUI_WIDGET(win), &rect);
	rtgui_widget_rect_to_device(RTGUI_WIDGET(win), &rect);
	rect.x1 += 10;
	rect.y1 += 20;
	rect.x2 = rect.x1 + 200;
	rect.y2 = rect.y1 + 150;
	picture_win = rtgui_win_create(parent,
			"绘图窗口", &rect,
            RTGUI_WIN_STYLE_NO_TITLE|RTGUI_WIN_STYLE_NO_BORDER|RTGUI_WIN_STYLE_NO_FOCUS);
    //创建窗口，没有标题栏，没有最小化窗口，也不能获取焦点
	/* 添加windows的事件*/
	rtgui_view_add_child(RTGUI_VIEW(win),
			RTGUI_WIDGET(picture_win));
	rtgui_widget_set_event_handler(RTGUI_WIDGET(picture_win),
			picture_win_onpaint);

	/* 非模态显示窗口 */
	rtgui_widget_focus(RTGUI_WIDGET(win));//设定主窗体获取焦点
	rtgui_win_show(win, RT_FALSE);
	rtgui_win_show(picture_win,RT_FALSE);

	/* 执行工作台事件循环 */
	rtgui_win_event_loop(win);
	return RT_TRUE;
}

