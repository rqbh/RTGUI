#include <rtthread.h>

int rt_application_init()
{
	panel_init();

	workbench_init();

	return 0;
}
