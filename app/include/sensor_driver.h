#pragma once

/*
 * Sensor driver module: samples the onboard LSM6DSO16IS IMU (accel + gyro)
 * at ~100 Hz and publishes each reading as a `struct imu_sample` (see
 * controller_ipc.h) onto `imu_sample_q`.
 *
 * The sampling thread is started automatically via K_THREAD_DEFINE in
 * sensor_driver.c; there is nothing to call from main() or elsewhere.
 * This header exists as an extension point (e.g. exposing tunables) if
 * other components need to reference this module later.
 */
