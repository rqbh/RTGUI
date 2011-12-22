#include <rtthread.h>

#include <SDL/sdl.h>
#include <rtgui/driver.h>

#define SDL_SCREEN_WIDTH	240
#define SDL_SCREEN_HEIGHT	320

struct sdlfb_device
{
	struct rt_device parent;

	SDL_Surface *screen;
	rt_uint16_t width;
	rt_uint16_t height;
};
struct sdlfb_device _device;

/* common device interface */
static rt_err_t  sdlfb_init(rt_device_t dev)
{
	return RT_EOK;
}
static rt_err_t  sdlfb_open(rt_device_t dev, rt_uint16_t oflag)
{
	return RT_EOK;
}
static rt_err_t  sdlfb_close(rt_device_t dev)
{
	return RT_EOK;
}
static rt_err_t  sdlfb_control(rt_device_t dev, rt_uint8_t cmd, void *args)
{
	struct sdlfb_device *device;

	device = (struct sdlfb_device*)dev;
	RT_ASSERT(device != RT_NULL);
	RT_ASSERT(device->screen != RT_NULL);

	switch (cmd)
	{
	case RTGRAPHIC_CTRL_GET_INFO:
		{
		struct rt_device_graphic_info *info;

		info = (struct rt_device_rect_info*) args;
		info->bits_per_pixel = 16;
		info->pixel_format = RTGRAPHIC_PIXEL_FORMAT_RGB565P;
		info->framebuffer = device->screen->pixels;
		info->width = device->screen->w;
		info->height = device->screen->h;
		}
		break;
	case RTGRAPHIC_CTRL_RECT_UPDATE:
		{
		struct rt_device_rect_info *rect;
		rect = (struct rt_device_rect_info*)args;

		/* SDL_UpdateRect(_device.screen, rect->x, rect->y, rect->x + rect->w, rect->y + rect->h); */
		SDL_UpdateRect(_device.screen, 0, 0, device->width, device->height);
		}
		break;
	case RTGRAPHIC_CTRL_SET_MODE:
		{		
		struct rt_device_rect_info* rect;

		rect = (struct rt_device_rect_info*)args;
		if ((_device.width == rect->width) && (_device.height == rect->height)) return;
		
		_device.width = rect->width;
		_device.height = rect->height;
		
		if (_device.screen != RT_NULL)
		{
			SDL_FreeSurface(_device.screen);
		
			/* re-create screen surface */
			_device.screen = SDL_SetVideoMode(_device.width, _device.height, 16, SDL_SWSURFACE | SDL_DOUBLEBUF);
			if ( _device.screen == NULL )
			{
				fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
				exit(1);
			}

			SDL_WM_SetCaption ("RT-Thread/GUI Simulator", NULL);
		}
		}
		break;
	}

	return RT_EOK;
}

void sdlfb_hw_init()
{
	/* set video driver for VC++ debug */
	_putenv("SDL_VIDEODRIVER=windib");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
	{
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

	_device.parent.init = sdlfb_init;
	_device.parent.open = sdlfb_open;
	_device.parent.close = sdlfb_close;
	_device.parent.read = RT_NULL;
	_device.parent.write = RT_NULL;
	_device.parent.control = sdlfb_control;

	_device.width  = SDL_SCREEN_WIDTH;
	_device.height = SDL_SCREEN_HEIGHT;
	_device.screen = SDL_SetVideoMode(_device.width, _device.height, 16, SDL_SWSURFACE | SDL_DOUBLEBUF);
    if (_device.screen == NULL)
	{
        fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
        exit(1);
    }

	SDL_WM_SetCaption ("RT-Thread/GUI Simulator", NULL);
	rt_device_register(&_device, "sdl", RT_DEVICE_FLAG_RDWR);
}

