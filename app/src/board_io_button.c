/*
 * Board I/O: button (gpio-keys / sw0) driver.
 *
 * GPIO interrupt + ~30ms re-check debounce, pushing a confirmed-transition
 * struct button_event onto button_evt_q (controller_ipc.h) for the
 * communication component to forward as `BTN <id> <DOWN|UP> <t_ms>`.
 * See specs/board-io.spec.md.
 *
 * Button pin: boards/demo/demo.dts wires sw0 to the onboard Nucleo B1
 * user button, gpioc pin 13, GPIO_ACTIVE_HIGH (confirmed against ST's
 * upstream nucleo_u385rg_q Zephyr board port + UM3062 — see demo.dts).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "board_io.h"

LOG_MODULE_REGISTER(board_io_button, LOG_LEVEL_INF);

#define BUTTON_ID 0U
#define BUTTON_DEBOUNCE_MS 30U

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static struct gpio_callback button_cb_data;
static struct k_work_delayable button_debounce_work;
static int button_last_state = -1; /* -1 = unknown, forces first report */

static void button_push_event(bool pressed)
{
	struct button_event evt = {
		.id = BUTTON_ID,
		.pressed = pressed,
		.t_ms = k_uptime_get(),
	};

	/* Never block the workqueue thread on a full queue — drop and log
	 * instead. The communication thread is expected to drain this
	 * promptly; a full queue means something downstream is stuck.
	 */
	if (k_msgq_put(&button_evt_q, &evt, K_NO_WAIT) != 0) {
		LOG_WRN("button_evt_q full, dropping event");
	}
}

static void button_debounce_work_handler(struct k_work *work)
{
	int state = gpio_pin_get_dt(&button);

	ARG_UNUSED(work);

	if (state < 0) {
		LOG_ERR("gpio_pin_get_dt failed: %d", state);
		return;
	}

	if (state != button_last_state) {
		button_last_state = state;
		button_push_event(state != 0);
	}
}

static void button_gpio_isr(const struct device *dev, struct gpio_callback *cb,
			     uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	/* Re-check the pin ~30ms after the edge rather than trusting it
	 * immediately — standard mechanical-switch debounce. Rescheduling
	 * on every edge naturally rides out bounce trains.
	 */
	k_work_reschedule(&button_debounce_work, K_MSEC(BUTTON_DEBOUNCE_MS));
}

int board_io_button_init(void)
{
	int ret;

	if (!device_is_ready(button.port)) {
		LOG_ERR("button GPIO port not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("failed to configure button pin: %d", ret);
		return ret;
	}

	k_work_init_delayable(&button_debounce_work, button_debounce_work_handler);

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret < 0) {
		LOG_ERR("failed to configure button interrupt: %d", ret);
		return ret;
	}

	gpio_init_callback(&button_cb_data, button_gpio_isr, BIT(button.pin));
	ret = gpio_add_callback(button.port, &button_cb_data);
	if (ret < 0) {
		LOG_ERR("failed to add button callback: %d", ret);
		return ret;
	}

	/* Prime last-known state so we don't spuriously report a transition
	 * for the initial level (and so debounce logic has a baseline).
	 */
	button_last_state = gpio_pin_get_dt(&button);

	return 0;
}
