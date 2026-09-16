# Task Distribution: Game Controller Firmware

Architect-level document for the agent team implementing `intent.md`. Each
firmware component below has its own `specs/*.spec.md` ticket. This file
covers what the specs don't: the shared contract, integration order, and
the hardware-access setup every agent depends on.

## Roles

| Role | Owns | Spec |
|---|---|---|
| Architect (this session) | shared interface contract, hw-mutex tooling, integration order, review/merge | — |
| Sensor driver | I2C2 devicetree + LSM6DSO16IS Zephyr sensor glue | [specs/sensor-driver.spec.md](../specs/sensor-driver.spec.md) |
| Board I/O | PWM/buzzer + button devicetree and C API | [specs/board-io.spec.md](../specs/board-io.spec.md) |
| Fusion/gesture | raw IMU samples → gestures + tilt axes | [specs/fusion-gesture.spec.md](../specs/fusion-gesture.spec.md) |
| Communication | ASCII serial protocol to host, RTT logging | [specs/communication.spec.md](../specs/communication.spec.md) |
| Supervisor | wires threads into `main.c`, `prj.conf`, end-to-end acceptance | [specs/supervisor.spec.md](../specs/supervisor.spec.md) |

Sensor driver and board I/O are split out from a single "sensor
driver/integration" role (as sketched in the sibling repo's
`docu/vibecoding_challenge.md`) so the buzzer/button path isn't blocked on
IMU bring-up, and so fusion only depends on one narrow contract.

## Shared interface contract

Frozen by the architect before any other agent starts, in
`app/include/controller_ipc.h`:

```c
#pragma once
#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/* sensor driver -> fusion */
struct imu_sample {
    int64_t t_ms;
    float ax, ay, az;   /* g */
    float gx, gy, gz;   /* deg/s */
};

/* board I/O -> communication (buttons are already discrete/debounced) */
struct button_event {
    uint8_t id;
    bool pressed;
    int64_t t_ms;
};

/* fusion -> communication */
enum controller_evt_type { EVT_GESTURE, EVT_AXIS };
enum gesture_id {
    GESTURE_SWING_UP, GESTURE_SWING_DOWN,
    GESTURE_SWING_LEFT, GESTURE_SWING_RIGHT,
    GESTURE_SHAKE, GESTURE_PUNCH,
};

struct controller_event {
    enum controller_evt_type type;
    int64_t t_ms;
    union {
        struct { enum gesture_id id; float confidence; } gesture;
        struct { const char *name; float value; } axis; /* e.g. "tilt_x" */
    };
};

/* communication -> board I/O (host BUZZ command) */
void buzzer_request(uint32_t freq_hz, uint32_t duration_ms);

extern struct k_msgq imu_sample_q;      /* sensor driver produces, fusion consumes */
extern struct k_msgq button_evt_q;      /* board I/O produces, communication consumes */
extern struct k_msgq controller_evt_q;  /* fusion produces, communication consumes */
```

The three `K_MSGQ_DEFINE`s backing these `extern`s live in
`app/src/controller_ipc.c`, added by the architect in the same commit as
the header. No firmware agent edits this file; if a type needs to change,
that goes through the architect so all consumers stay in sync.

## Devicetree ownership

Both additions land in `boards/demo/demo.dts`, in disjoint subtrees so the
two agents don't conflict:

- **Sensor driver**: `&i2c2` node (pinctrl PB13/PB14, `I2C_BITRATE_FAST`),
  child `lsm6dso16is@6a`, `aliases { accelerometer = &lsm6dso16is; };`.
- **Board I/O**: `&timers3`/`pwm1` node (pinctrl TIM3_CH2/PC7), `buzzer`
  alias; `gpio-keys` node with a `sw0`-aliased button. The exact onboard
  user-button pin is unconfirmed (see that spec) — resolving it is part of
  the board I/O agent's work, not assumed here.

## Serial protocol vs. logging

Decision: move Zephyr logging to RTT (`CONFIG_LOG_BACKEND_RTT=y`, UART log
backend off) so USART1 carries only the `HELLO`/`BTN`/`GESTURE`/`AXIS` /
`BUZZ`/`PONG`/`RESET` protocol from `intent.md`, with no interleaved
`printk` output to filter around. `external/modules/debug/segger` (RTT) is
already in the west manifest, unused until now.

## Hardware-access mutex (architect setup, before agents start)

`.claude/skills/debug-on-target/scripts/{build,flash,debug_on_target}.sh`
and `watch_serial.py` have no serialization today — concurrent agents in
separate worktrees will race the single ST-Link/serial bridge. Fix before
handoff:

- `.claude/skills/debug-on-target/scripts/with-hw-lock.sh` — thin `flock`
  wrapper (lock file e.g. `/tmp/zephyr-hw.lock`) around the three scripts.
- A `PreToolUse` hook in `.claude/settings.json` routing Bash calls to
  those scripts through the wrapper, so agents queue instead of colliding.

This is a same-session architect task, not a separate spec.

## Integration order

1. Architect merges `controller_ipc.h`/`.c` and the hw-mutex hook. Everyone
   else branches from that commit.
2. Sensor driver, board I/O, and communication agents proceed **in
   parallel** — each only needs the shared header. Communication can stub
   its inputs with a fake producer thread (`CONFIG_CONTROLLER_FAKE_INPUT`)
   if it finishes before the real producers exist.
3. Fusion starts in parallel too, as soon as `imu_sample`/`controller_event`
   are frozen (step 1) — it validates against synthetic sample sequences
   on `native_sim`, independent of real hardware.
4. Supervisor integrates last: pulls all four branches, assembles
   `prj.conf`, runs the end-to-end hardware acceptance pass from
   `intent.md`'s definition of done.

## Testing strategy summary

| Component | Native sim | Hardware-in-the-loop |
|---|---|---|
| Sensor driver | optional (fake sensor backend) | primary |
| Board I/O | no (needs real PWM/GPIO) | primary |
| Fusion/gesture | primary (ztest, synthetic samples) | spot-check |
| Communication | codec only (encode/decode split from transport) | transport |
| Supervisor | no | primary (acceptance scenario) |
