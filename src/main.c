#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(threads_interact);

enum TYPE {
	TEMP = 0,
	HUMI
};

struct data {
	enum TYPE type;
	int8_t item;
};

K_MSGQ_DEFINE(consumer_queue, sizeof(struct data), 10, 4);
K_MSGQ_DEFINE(producer_queue, sizeof(struct data), 10, 4);

int8_t get_temp()
{
	return (k_cycle_get_32() % 31) + 10;
}

int8_t get_humi()
{
	return k_cycle_get_32() % 101;
}

void prod_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	enum TYPE type = (enum TYPE)(uintptr_t)arg1;

	struct data data;
	while (true) {
		uint32_t temp_value;
		char *s;

		switch (type) {
		case TEMP:
			temp_value = get_temp();
			s = "TEMP";

			break;
		case HUMI:
			temp_value = get_humi();
			s = "HUMI";

			break;
		default:
			return;
		}

		LOG_DBG("Put [%s:%d] on queue...\n", s, temp_value);

		data.item = temp_value;
		data.type = type;

		k_msgq_put(&producer_queue, &data, K_NO_WAIT);
		k_sleep(K_SECONDS(1));
	}

	return;
}

void filter_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	struct data data;
	while (true) {
		int err = k_msgq_get(&producer_queue, &data, K_FOREVER);
		if (err != 0) {
			continue;
		}

		char *s;

		switch (data.type) {
		case TEMP:
			s = "TEMP";

			if (data.item < 18 || data.item > 30) {
				LOG_ERR("Removed invalid data for [%s:%d] from the queue\n", s,
					data.item);
				continue;
			}

			break;
		case HUMI:
			s = "HUMI";

			if (data.item < 40 || data.item > 70) {
				LOG_ERR("Removed invalid data for [%s:%d] from the queue\n", s,
					data.item);
				continue;
			}

			break;
		default:
			continue;
		}

		LOG_DBG("Filtered [%s:%d] from queue\n", s, data.item);
		k_msgq_put(&consumer_queue, &data, K_NO_WAIT);
	}
}

void consu_thread(void *arg1, void *arg2, void *arg3)
{
	struct data data;
	while (true) {
		int err = k_msgq_get(&consumer_queue, &data, K_FOREVER);
		if (err != 0) {
			continue;
		}

		switch (data.type) {
		case TEMP:
			LOG_INF("Temperature: %dºC\n", data.item);

			break;
		case HUMI:
			LOG_INF("Humidity: %d%%\n", data.item);

			break;
		default:
			continue;
		}
	}
}

int main(void)
{
	return 0;
}

K_THREAD_DEFINE(producer1_thread, 1024, prod_thread, TEMP, NULL, NULL, 4, 0, 0);
K_THREAD_DEFINE(producer2_thread, 1024, prod_thread, HUMI, NULL, NULL, 4, 0, 0);
K_THREAD_DEFINE(filtering_thread, 1024, filter_thread, NULL, NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(consulmer_thread, 1024, consu_thread, NULL, NULL, NULL, 2, 0, 0);
