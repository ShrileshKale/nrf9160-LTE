#include "data_aquistion.h"
#include "cloud_connection.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#define STACK_SIZE 2048

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

K_THREAD_DEFINE(con_thread, STACK_SIZE, cloud_connection_thread,
				NULL, NULL, NULL, 0, 0, 0);

K_THREAD_DEFINE(application_thread, STACK_SIZE, main_application_thread_fn,
				NULL, NULL, NULL, 0, 0, 0);

int main(void)
{
	return 0;
}
