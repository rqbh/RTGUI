#include <SDL/SDL.h>

#include <rtgui/event.h>
#include <rtgui/kbddef.h>
#include <rtgui/rtgui_server.h>
#include <rtgui/rtgui_system.h>

int done = 0;
int main(int argc, char *argv[])
{
	SDL_Event event;
	int button_state = 0;
	rt_device_t device;

	rt_kprintf("RTGUI simulator ....\n");

	/* init rtt system */
	rt_mq_system_init();
	rt_mb_system_init();
	rt_thread_system_init();

	// while(!done);

	/* init driver */
	sdlfb_hw_init();
	device = rt_device_find("sdl");
	rtgui_graphic_set_device(device);

	/* init gui system */
	rtgui_system_server_init();

	/* initial user application */
	rt_application_init();

	/* handle SDL event */
	while ( !done )
	{
		/* Check for events */
		while ( SDL_PollEvent(&event) )
		{
			switch (event.type)
			{
			case SDL_MOUSEMOTION:
				{
					struct rtgui_event_mouse emouse;
					emouse.parent.type = RTGUI_EVENT_MOUSE_MOTION;
					emouse.parent.sender = RT_NULL;
                    emouse.wid = RT_NULL;

					emouse.x = ((SDL_MouseMotionEvent*)&event)->x;
					emouse.y = ((SDL_MouseMotionEvent*)&event)->y;

					/* init mouse button */
					emouse.button = button_state;

					/* send event to server */
					rtgui_server_post_event(&emouse.parent, sizeof(struct rtgui_event_mouse));
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				{
					struct rtgui_event_mouse emouse;
					SDL_MouseButtonEvent* mb;

					emouse.parent.type = RTGUI_EVENT_MOUSE_BUTTON;
					emouse.parent.sender = RT_NULL;
                    emouse.wid = RT_NULL;

					mb = (SDL_MouseButtonEvent*)&event;

					emouse.x = mb->x;
					emouse.y = mb->y;

					/* init mouse button */
					emouse.button = 0;

					/* set emouse button */
					if (mb->button & (1 << (SDL_BUTTON_LEFT - 1)) )
					{
						emouse.button |= RTGUI_MOUSE_BUTTON_LEFT;
					}
					else if (mb->button & (1 << (SDL_BUTTON_RIGHT - 1)))
					{
						emouse.button |= RTGUI_MOUSE_BUTTON_RIGHT;
					}
					else if (mb->button & (1 << (SDL_BUTTON_MIDDLE - 1)))
					{
						emouse.button |= RTGUI_MOUSE_BUTTON_MIDDLE;
					}

					if (mb->type == SDL_MOUSEBUTTONDOWN)
					{
						emouse.button |= RTGUI_MOUSE_BUTTON_DOWN;
						button_state = emouse.button;
					}
					else
					{
						emouse.button |= RTGUI_MOUSE_BUTTON_UP;
						button_state = 0;
					}


					/* send event to server */
					rtgui_server_post_event(&emouse.parent, sizeof(struct rtgui_event_mouse));
				}
				break;

			case SDL_KEYUP:
				{
					struct rtgui_event_kbd ekbd;
					ekbd.parent.type	= RTGUI_EVENT_KBD;
					ekbd.parent.sender	= RT_NULL;
					ekbd.type = RTGUI_KEYUP;
					ekbd.wid = RT_NULL;
					ekbd.mod = event.key.keysym.mod;
					ekbd.key = event.key.keysym.sym;

					/* FIXME: unicode */
					ekbd.unicode = 0;

					/* send event to server */
					rtgui_server_post_event(&ekbd.parent, sizeof(struct rtgui_event_kbd));
				}
				break;

			case SDL_KEYDOWN:
				{
					struct rtgui_event_kbd ekbd;
					ekbd.parent.type	= RTGUI_EVENT_KBD;
					ekbd.parent.sender	= RT_NULL;
					ekbd.type = RTGUI_KEYDOWN;
					ekbd.wid = RT_NULL;
					ekbd.mod = event.key.keysym.mod;
					ekbd.key = event.key.keysym.sym;

					/* FIXME: unicode */
					ekbd.unicode = 0;

					/* send event to server */
					rtgui_server_post_event(&ekbd.parent, sizeof(struct rtgui_event_kbd));
				}
				break;

			case SDL_QUIT:
				done = 1;
				break;

			default:
				break;
			}

			SDL_Delay(20);
		}
	}

	SDL_Quit();

	return 0;
}
