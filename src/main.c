#include "zephyr/devicetree.h"
#include "zephyr/drivers/gpio.h"
#include "zephyr/init.h"
#include "zephyr/sys/util.h"
#include "zephyr/sys/util_macro.h"
#include "zephyr/toolchain.h"
#include <stdint.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(hello_world);

#define FADE_STEPS 50

enum MODE {
	BLINK = 0,
	FADE
};

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
static const struct pwm_dt_spec led = PWM_DT_SPEC_GET(DT_ALIAS(led0));
static struct gpio_callback cb_data;

struct k_timer blink_timer;
struct k_timer fade_timer;

static const struct device *const console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

void blink(struct k_timer *timer_id)
{
	static bool led_status = 1;

	if (led_status) {
		LOG_DBG("Aceso!");
		pwm_set_pulse_dt(&led, led.period);
	} else {
		LOG_DBG("Apagado!");
		pwm_set_pulse_dt(&led, 0);
	}

	led_status = !led_status;
}

void fade(struct k_timer *timer_id)
{
	static int8_t dir = 1;
	static int32_t bright = 0;
	static int32_t actual_step = 0;

	bright += (led.period / FADE_STEPS) * dir;

	if (bright > led.period) {
		bright = led.period;
	}

	pwm_set_pulse_dt(&led, bright);

	bright = MIN(led.period, MAX(0, bright));

	LOG_DBG("PWM Bright: %d%%", (bright * 100) / led.period);

	if (actual_step >= FADE_STEPS) {
		actual_step = 0;
		dir *= -1;
	} else {
		actual_step++;
	}
}

void button_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	static enum MODE mode = 0;

	switch (mode) {
	case BLINK:
		k_timer_stop(&blink_timer);
		k_timer_start(&fade_timer, K_NO_WAIT, K_MSEC(20));

		LOG_INF("BLINK -> FADE\n");
		break;
	case FADE:
		k_timer_stop(&fade_timer);
		k_timer_start(&blink_timer, K_NO_WAIT, K_MSEC(CONFIG_BLINK_TIMER_INTERVAL));

		LOG_INF("FADE -> BLINK\n");
		break;
	}

	mode = !mode;
	k_msleep(20);
}

int main(void)
{
	int ret = 0;
	LOG_INF("Starting system....\n");

	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready!", button.port->name);

		return 0;
	}

	if (!pwm_is_ready_dt(&led)) {
		printk("Error: PWM device %s is not ready\n", led.dev->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n", ret, button.port->name,
		       button.pin);

		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to to configure intrreupt on %s pin %d!", button.port->name,
		       button.pin);

		return 0;
	}

	gpio_init_callback(&cb_data, button_cb, BIT(button.pin));
	gpio_add_callback(button.port, &cb_data);

	k_sleep(K_SECONDS(1));

	LOG_INF("Starting blink....\n");
	LOG_INF("Press button to toggle mode anytime!\n");

	k_timer_init(&fade_timer, fade, NULL);
	k_timer_init(&blink_timer, blink, NULL);
	k_timer_start(&blink_timer, K_NO_WAIT, K_MSEC(CONFIG_BLINK_TIMER_INTERVAL));

	char c;
	while (1) {
		if (!uart_poll_in(console_dev, &c) && (c == '\n' || c == '\r')) {
			LOG_DBG("Button Pressed!\n");
			button_cb(button.port, &cb_data, button.pin);
		}

		k_msleep(50);
	}

	return 0;
}
