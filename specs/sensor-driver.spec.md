# Spec: Sensor Driver

Owner of the IMU devicetree wiring and the Zephyr sensor API glue that
turns it into `imu_sample` events. Part of [intent.md](../intent.md); see
[task-distribution.md](../docs/task-distribution.md) for the shared
contract and integration order.

## Goal

Sample the onboard LSM6DSO16IS accelerometer/gyro at a steady rate and
publish each reading as an `imu_sample` for the fusion component to
consume — no gesture logic here, just clean, timestamped raw data.

## Scope

- Add to `boards/demo/demo.dts`:
  - `&i2c2` with pinctrl `i2c2_scl_pb13`/`i2c2_sda_pb14` (Arduino D15/D14),
    `clock-frequency = <I2C_BITRATE_FAST>`, `status = "okay"`.
  - Child node `lsm6dso16is@6a` (`compatible = "st,lsm6dso16is"`,
    `reg = <0x6a>`), with a reasonable starting ODR/range (e.g. 104 Hz,
    ±4g / ±500dps — tune later if gestures need more headroom).
  - `aliases { accelerometer = &lsm6dso16is; };`
- Add to `app/prj.conf`: `CONFIG_I2C=y`, `CONFIG_SENSOR=y`.
- New module (suggested: `app/src/sensor_driver.c` +
  `app/include/sensor_driver.h`): a `K_THREAD_DEFINE`'d thread at ~100 Hz
  that does `sensor_sample_fetch()`/`sensor_channel_get()` on
  `DT_ALIAS(accelerometer)` for both `SENSOR_CHAN_ACCEL_XYZ` and
  `SENSOR_CHAN_GYRO_XYZ`, fills an `imu_sample`, and
  `k_msgq_put()`s it onto `imu_sample_q` (from `controller_ipc.h`,
  non-blocking — drop the sample if the queue is full rather than stalling
  sampling).
- Driver trigger mode (`SENSOR_TRIG_DATA_READY`) is a nice-to-have if the
  LSM6DSO16IS driver supports it cleanly in this Zephyr revision; polling
  in the thread is an acceptable fallback given the timebox.

## Interface

- **Produces**: `struct imu_sample` onto `imu_sample_q` (defined in
  `controller_ipc.h`/`.c`, owned by the architect — do not redefine).
- **Consumes**: nothing from other firmware components.

## Out of scope

- Any gesture/threshold logic (fusion component's job).
- The buzzer or buttons (board I/O component's job).
- Devicetree changes outside the `i2c2` subtree.

## Definition of done

- `west build -b demo app` succeeds with the new node and module.
- On real hardware (via the `debug-on-target` skill, through the
  hardware-mutex wrapper — see task-distribution.md), moving the board
  visibly changes `ax`/`ay`/`az`/`gx`/`gy`/`gz` values, observable via a
  temporary debug log over RTT (not the protocol UART).
- Sample rate is steady enough (~100 Hz, no long stalls) for the fusion
  component to build gesture detection on top of it.

## Testing

Hardware-in-the-loop only for this component (real I2C bus). A
`native_sim` fake-sensor-backend variant is optional and only worth doing
if time remains after the hardware path works.
