# Spec: Sensor Fusion / Gesture Recognition

Turns raw IMU samples into the small set of discrete gestures and
continuous axes the game actually consumes. Part of
[intent.md](../intent.md); see
[task-distribution.md](../docs/task-distribution.md) for the shared
contract and integration order.

## Goal

Given a steady stream of `imu_sample`s, recognize:

- **Discrete gestures**: `GESTURE_SWING_UP/DOWN/LEFT/RIGHT`,
  `GESTURE_SHAKE`, `GESTURE_PUNCH` — one event per distinct motion, not a
  flood of repeats.
- **Continuous axis**: at least a `tilt_x` (roll) value, low-pass filtered,
  suitable for analog-style steering.

This does not need to be sophisticated — simple thresholding/peak
detection on accelerometer magnitude and dominant axis sign is enough, as
long as it's reliable enough for a live two-player match and doesn't
falsely trigger constantly.

## Scope

- New module (suggested: `app/src/fusion.c` + `app/include/fusion.h`): a
  `K_THREAD_DEFINE`'d thread that blocks on `k_msgq_get(&imu_sample_q, ...)`,
  maintains whatever running state it needs (e.g. a short moving-average
  for tilt, a simple state machine for "at rest → motion → classify →
  cooldown" for discrete gestures), and pushes `controller_event`s onto
  `controller_evt_q`.
- Keep tunable constants (thresholds, debounce/cooldown windows, filter
  coefficients) in one place (a `Kconfig` menu or a constants header) so
  they can be tuned without touching the detection logic — this is the
  part most likely to need live iteration against real motion data.
- Tilt axis: publish at a throttled rate (e.g. 20 Hz, not every sample) to
  avoid flooding the communication link — see `communication.spec.md`.

## Interface

- **Consumes**: `struct imu_sample` from `imu_sample_q`.
- **Produces**: `struct controller_event` onto `controller_evt_q`.
- No dependency on the real sensor driver, buzzer, or buttons — only the
  frozen `imu_sample`/`controller_event` shapes in `controller_ipc.h`.

## Out of scope

- Anything sensor-driver- or board-I/O-specific (I2C, PWM, GPIO).
- The serial protocol itself (communication component's job).

## Definition of done

- On `native_sim`, a ztest feeding synthetic `imu_sample` sequences (e.g.
  a recorded/hand-crafted "swing right" trace, a "shake" trace, a
  "resting" trace) produces the expected `controller_event`s and no
  spurious ones for the resting trace.
- On real hardware (once the sensor driver component's branch is merged),
  physically performing each gesture produces the matching event within
  roughly the motion's duration (no multi-second lag), and holding the
  board still produces no gesture events.

## Testing

Primary target for automated testing in this whole project — this is the
one component where `native_sim`/ztest with synthetic input sequences is
both feasible and worth the investment (see the "closing the loop"
approach in `docu/claude_basics.md` in the sibling repo). Spot-check on
real hardware once the sensor driver is available.
