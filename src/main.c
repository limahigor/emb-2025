#include "zephyr/logging/log_core.h"
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(hello_world, LOG_LEVEL_DBG);

void helloworld_timer_handler(struct k_timer *dummy)
{
	static uint32_t timer_count = 0;

	printk("Hello world!\n");

	LOG_DBG("Said Hello World %d times\n", timer_count);

	if (timer_count == UINT32_MAX) {
		LOG_ERR("Timer count buffer overflow detected! Restarting count");

		timer_count = 0;
	} else {
		timer_count++;
	}
}

K_TIMER_DEFINE(hello_timer, helloworld_timer_handler, NULL);

int main(void)
{

	LOG_INF("Starting Hello Timer....\n");

	LOG_DBG("Hello timer first execution configured to: %d\n", CONFIG_HELLOTIMER_INIT);
	LOG_DBG("Hello Timer interval configured to: %d\n", CONFIG_HELLOTIMER_INTERVAL);

	k_timer_start(&hello_timer, K_MSEC(CONFIG_HELLOTIMER_INIT),
		      K_MSEC(CONFIG_HELLOTIMER_INTERVAL));

	return 0;
}
