/*
 * Board I/O: buzzer (PWM) driver.
 *
 * Implements buzzer_request() from controller_ipc.h — the shared entry
 * point the communication component calls on an incoming host `BUZZ`
 * line. See specs/board-io.spec.md.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

#include "board_io.h"

LOG_MODULE_REGISTER(board_io_buzzer, LOG_LEVEL_INF);

/* aliases { buzzer = &pwm1; } in boards/demo/demo.dts; pwm1 is the child
 * "pwm" node of &timers3 (TIM3), pinctrl'd to tim3_ch2_pc7 (Arduino D8).
 */
#define BUZZER_PWM_NODE DT_ALIAS(buzzer)
#define BUZZER_PWM_CHANNEL 2U /* TIM3 CH2 */

static const struct device *buzzer_pwm_dev = DEVICE_DT_GET(BUZZER_PWM_NODE);

static struct k_timer buzzer_off_timer;
static struct k_work buzzer_off_work;

/* Runs on the system workqueue thread (not the timer-expiry ISR context),
 * so it is always safe to call into the PWM driver here.
 */
static void buzzer_off_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	(void)pwm_set(buzzer_pwm_dev, BUZZER_PWM_CHANNEL, 0, 0, PWM_POLARITY_NORMAL);
}

/* k_timer expiry function: runs in ISR/system-timeout context. Keep this
 * minimal and non-blocking — just hand off to the workqueue.
 */
static void buzzer_off_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	k_work_submit(&buzzer_off_work);
}

int board_io_buzzer_init(void)
{
	if (!device_is_ready(buzzer_pwm_dev)) {
		LOG_ERR("buzzer PWM device not ready");
		return -ENODEV;
	}

	k_work_init(&buzzer_off_work, buzzer_off_work_handler);
	k_timer_init(&buzzer_off_timer, buzzer_off_timer_handler, NULL);

	return 0;
}

/*
 * void buzzer_request(uint32_t freq_hz, uint32_t duration_ms) — declared in
 * controller_ipc.h (frozen shared contract), implemented here.
 *
 * Non-blocking: sets the PWM period/pulse for freq_hz at ~50% duty, arms a
 * k_timer for duration_ms, and returns immediately. Safe to call from any
 * thread context (e.g. the communication thread on an incoming BUZZ line).
 */
void buzzer_request(uint32_t freq_hz, uint32_t duration_ms)
{
	uint32_t period_ns;
	uint32_t pulse_ns;
	int ret;

	if (!device_is_ready(buzzer_pwm_dev)) {
		return;
	}

	if (freq_hz == 0U || duration_ms == 0U) {
		/* Treat as "stop" rather than guessing a frequency. */
		k_timer_stop(&buzzer_off_timer);
		k_work_submit(&buzzer_off_work);
		return;
	}

	period_ns = (uint32_t)(NSEC_PER_SEC / freq_hz);
	pulse_ns = period_ns / 2U; /* ~50% duty */

	ret = pwm_set(buzzer_pwm_dev, BUZZER_PWM_CHANNEL, period_ns, pulse_ns,
		      PWM_POLARITY_NORMAL);
	if (ret < 0) {
		LOG_ERR("pwm_set failed: %d", ret);
		return;
	}

	/* Restarting an already-running timer is fine — it just reschedules
	 * the off-transition, so back-to-back BUZZ requests behave sanely.
	 */
	k_timer_start(&buzzer_off_timer, K_MSEC(duration_ms), K_NO_WAIT);
}
