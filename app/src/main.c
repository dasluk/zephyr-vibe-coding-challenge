/*
 * Supervisor: wires up the one component that doesn't self-start.
 *
 * The sensor driver, fusion, and communication threads all start
 * automatically via K_THREAD_DEFINE. Board I/O's buzzer/button primitives
 * are the exception (board_io.h) and must be explicitly initialized once
 * at boot before anything calls buzzer_request() or expects button events.
 * See specs/supervisor.spec.md.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "board_io.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

int main(void)
{
	int ret;

	printk("Game controller firmware booting (%s)\n", CONFIG_BOARD_TARGET);

	ret = board_io_buzzer_init();
	if (ret < 0) {
		LOG_ERR("board_io_buzzer_init failed: %d (buzzer will not work)", ret);
	}

	ret = board_io_button_init();
	if (ret < 0) {
		LOG_ERR("board_io_button_init failed: %d (button events will not work)", ret);
	}

	LOG_INF("Startup complete (sensor driver / fusion / communication threads "
		"are already running via K_THREAD_DEFINE)");

	return 0;
}
