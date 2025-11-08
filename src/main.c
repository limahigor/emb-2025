#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hello_world);

#define FADE_STEPS 50

enum MODE {
	BLINK = 0,
	FADE
};

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
static const struct pwm_dt_spec led = PWM_DT_SPEC_GET(DT_ALIAS(led0));
static struct gpio_callback cb_data;

static struct k_timer blink_timer;
static struct k_work blink_work;

static struct k_work_delayable fade_work;
static int32_t fade_bright;
static int8_t fade_dir;
static int32_t fade_step_ns;

static volatile bool led_status;
static volatile enum MODE current_mode;

K_MUTEX_DEFINE(pwm_lock);

static void blink_work_handler(struct k_work *work)
{
	if (current_mode != BLINK) {
		return;
	}

	k_mutex_lock(&pwm_lock, K_FOREVER);

	if (led_status) {
		LOG_DBG("Aceso!");
		pwm_set_pulse_dt(&led, led.period);
	} else {
		LOG_DBG("Apagado!");
		pwm_set_pulse_dt(&led, 0);
	}
	k_mutex_unlock(&pwm_lock);
}

static void blink_timer_isr(struct k_timer *timer_id)
{
	if (current_mode != BLINK) {
		return;
	}
	led_status = !led_status;
	k_work_submit(&blink_work);
}

static void fade_work_handler(struct k_work *work)
{
	if (current_mode != FADE) {
		return;
	}

	int32_t b = fade_bright + fade_dir * fade_step_ns;
	if (b < 0) {
		b = 0;
	}
	if (b > (int32_t)led.period) {
		b = (int32_t)led.period;
	}

	k_mutex_lock(&pwm_lock, K_FOREVER);
	pwm_set_pulse_dt(&led, (uint32_t)b);
	k_mutex_unlock(&pwm_lock);

	fade_bright = b;

	static int32_t step = 0;
	LOG_DBG("PWM Bright: %d%%", (fade_bright * 100) / led.period);

	if (++step >= FADE_STEPS) {
		step = 0;
		fade_dir = -fade_dir;
	}

	k_work_reschedule(&fade_work, K_MSEC(20));
}

static void fade_start(void)
{
	fade_bright = 0;
	fade_dir = 1;
	fade_step_ns = led.period / FADE_STEPS;
	k_work_init_delayable(&fade_work, fade_work_handler);
	k_work_schedule(&fade_work, K_NO_WAIT);
}

static void fade_stop(void)
{
	k_work_cancel_delayable(&fade_work);
}

static void button_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	static enum MODE mode;
	switch (mode) {
	case BLINK:
		k_timer_stop(&blink_timer);
		current_mode = FADE;
		fade_start();
		LOG_INF("BLINK -> FADE\n");
		break;
	case FADE:
		fade_stop();
		current_mode = BLINK;
		k_timer_start(&blink_timer, K_NO_WAIT, K_MSEC(CONFIG_BLINK_TIMER_INTERVAL));
		LOG_INF("FADE -> BLINK\n");
		break;
	}
	mode = !mode;
}

int main(void)
{
	int ret;

	LOG_INF("Starting system....\n");

	if (!gpio_is_ready_dt(&button)) {
		return 0;
	}
	if (!pwm_is_ready_dt(&led)) {
		return 0;
	}

	k_work_init(&blink_work, blink_work_handler);
	k_timer_init(&blink_timer, blink_timer_isr, NULL);

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret) {
		return 0;
	}

	gpio_init_callback(&cb_data, button_cb, BIT(button.pin));
	gpio_add_callback(button.port, &cb_data);

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret) {
		return 0;
	}

	current_mode = BLINK;
	led_status = false;
	k_timer_start(&blink_timer, K_SECONDS(1), K_MSEC(CONFIG_BLINK_TIMER_INTERVAL));

	LOG_INF("Starting blink....\n");
	LOG_INF("Press button to toggle mode anytime!\n");

	while (1) {
		k_sleep(K_MSEC(50));
	}
}
