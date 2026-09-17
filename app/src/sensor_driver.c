/*
 * Sensor driver: polls the onboard LSM6DSO16IS IMU (accelerometer + gyro)
 * at ~100 Hz and publishes each reading onto imu_sample_q for the fusion
 * component to consume. No gesture/threshold logic here (see
 * specs/sensor-driver.spec.md).
 */

#include "sensor_driver.h"
#include "controller_ipc.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_driver, LOG_LEVEL_INF);

#define IMU_NODE DT_ALIAS(accelerometer)

#if !DT_NODE_HAS_STATUS(IMU_NODE, okay)
#error "sensor_driver: 'accelerometer' alias missing/disabled in devicetree"
#endif

#define SENSOR_DRIVER_STACK_SIZE 1024
#define SENSOR_DRIVER_PRIORITY   5
#define SENSOR_DRIVER_PERIOD_MS  10 /* ~100 Hz */

/* TEMPORARY hardware bring-up aid, per specs/sensor-driver.spec.md's
 * Definition of Done ("moving the board visibly changes ax/ay/az/gx/gy/gz,
 * observable via a temporary debug log over RTT"). Off by default so it
 * has zero effect on normal builds; strip this whole block (and the
 * matching CONFIG_LOG/CONFIG_USE_SEGGER_RTT/CONFIG_LOG_BACKEND_RTT prj.conf
 * lines, which are NOT committed here since they belong to the
 * communication component's logging setup per docs/task-distribution.md)
 * once hardware bring-up is done, or replace with a permanent mechanism if
 * still wanted. To re-enable locally: flip this to 1 and add CONFIG_LOG=y,
 * CONFIG_USE_SEGGER_RTT=y, CONFIG_LOG_BACKEND_RTT=y to app/prj.conf.
 */
#define SENSOR_DRIVER_HW_DEBUG_LOG 0
#define SENSOR_DRIVER_HW_DEBUG_LOG_EVERY 10 /* ~every 100ms at 100Hz */

/* struct imu_sample stores accel in g and gyro in deg/s; the Zephyr sensor
 * API returns SI units (m/s^2, rad/s), so convert on the way out.
 */
#define STANDARD_GRAVITY_MS2 9.80665f
#define RAD_TO_DEG_FACTOR    57.29577951308232f /* 180/pi */

static void sensor_driver_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct device *imu = DEVICE_DT_GET(IMU_NODE);

	if (!device_is_ready(imu)) {
		LOG_ERR("IMU device not ready");
		return;
	}

	while (1) {
		int64_t cycle_start = k_uptime_get();
		struct sensor_value accel[3];
		struct sensor_value gyro[3];
		int rc;

		rc = sensor_sample_fetch(imu);
		if (rc != 0) {
			LOG_WRN("sensor_sample_fetch failed: %d", rc);
			goto sleep;
		}

		rc = sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
		if (rc != 0) {
			LOG_WRN("sensor_channel_get(ACCEL_XYZ) failed: %d", rc);
			goto sleep;
		}

		rc = sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro);
		if (rc != 0) {
			LOG_WRN("sensor_channel_get(GYRO_XYZ) failed: %d", rc);
			goto sleep;
		}

		struct imu_sample sample = {
			.t_ms = k_uptime_get(),
			.ax = sensor_value_to_float(&accel[0]) / STANDARD_GRAVITY_MS2,
			.ay = sensor_value_to_float(&accel[1]) / STANDARD_GRAVITY_MS2,
			.az = sensor_value_to_float(&accel[2]) / STANDARD_GRAVITY_MS2,
			.gx = sensor_value_to_float(&gyro[0]) * RAD_TO_DEG_FACTOR,
			.gy = sensor_value_to_float(&gyro[1]) * RAD_TO_DEG_FACTOR,
			.gz = sensor_value_to_float(&gyro[2]) * RAD_TO_DEG_FACTOR,
		};

		/* Non-blocking: drop the sample rather than stall sampling if
		 * the fusion consumer is behind and the queue is full.
		 */
		rc = k_msgq_put(&imu_sample_q, &sample, K_NO_WAIT);
		if (rc != 0) {
			LOG_DBG("imu_sample_q full, dropping sample");
		}

#if SENSOR_DRIVER_HW_DEBUG_LOG
		{
			static uint32_t dbg_count;

			if ((dbg_count++ % SENSOR_DRIVER_HW_DEBUG_LOG_EVERY) == 0) {
				/* Printed as milli-units (int) to avoid pulling in
				 * float printf support just for this debug aid.
				 */
				LOG_INF("ax=%d ay=%d az=%d mg  gx=%d gy=%d gz=%d mdeg/s",
					(int)(sample.ax * 1000), (int)(sample.ay * 1000),
					(int)(sample.az * 1000), (int)(sample.gx * 1000),
					(int)(sample.gy * 1000), (int)(sample.gz * 1000));
			}
		}
#endif

sleep:
		k_sleep(K_TIMEOUT_ABS_MS(cycle_start + SENSOR_DRIVER_PERIOD_MS));
	}
}

K_THREAD_DEFINE(sensor_driver_tid, SENSOR_DRIVER_STACK_SIZE,
		 sensor_driver_thread, NULL, NULL, NULL,
		 SENSOR_DRIVER_PRIORITY, 0, 0);
