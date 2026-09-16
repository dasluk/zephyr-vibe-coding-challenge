# Spec: Supervisor / Main App

Integrates the other four components into a bootable firmware image and
owns the end-to-end acceptance check. Part of [intent.md](../intent.md);
see [task-distribution.md](../docs/task-distribution.md) for the shared
contract and integration order. This is the last component to merge —
it depends on sensor driver, board I/O, fusion, and communication all
being in place.

## Goal

A single `app/src/main.c` and `app/prj.conf` that boot the board straight
into the full pipeline: IMU sampling → fusion → communication, buttons →
communication, host `BUZZ` → board I/O — with no manual wiring left for a
person to do after flashing.

## Scope

- `app/src/main.c`: since every component's thread is
  `K_THREAD_DEFINE`'d statically inside its own module (started at boot
  automatically), `main()` should end up close to empty — at most a
  startup log line and any one-time system-level init that doesn't belong
  to a single component (e.g. confirming all expected devices are
  `device_is_ready()` before anything starts producing events, so a wiring
  mistake fails loudly instead of silently dropping samples).
- `app/prj.conf`: merge the Kconfig needs from every spec (`CONFIG_I2C`,
  `CONFIG_SENSOR`, `CONFIG_PWM`, `CONFIG_GPIO` already on via
  `demo_defconfig`, `CONFIG_LOG_BACKEND_RTT`, plus stack-size tuning for
  the five threads if defaults prove too small).
- `app/CMakeLists.txt`: add every new component's `.c` file.
- Reconcile `boards/demo/demo.dts`: sensor driver's `i2c2` hunk and board
  I/O's `timers3`/`gpio-keys` hunk should merge cleanly (disjoint
  subtrees per `task-distribution.md`) — resolve if not.

## Interface

- Does not itself produce/consume `controller_ipc.h` types — it only
  wires up the other four components and provides the top-level
  `device_is_ready()` sanity gate before their threads start doing real
  work.

## Out of scope

- Any component-specific logic — if something belongs in sensor driver,
  board I/O, fusion, or communication, put it there and keep `main.c`
  thin.

## Definition of done (end-to-end acceptance, from intent.md)

On real hardware, in order, over the protocol UART (via
`debug-on-target`'s `watch_serial.py` or the reference client from
`communication.spec.md`):

1. Power on / reset → a `HELLO <controller_id> <fw_version>` line appears.
2. Tilt the board → `AXIS tilt_x <value> <t_ms>` lines appear and track
   the tilt direction.
3. Perform a swing/shake/punch motion → the matching `GESTURE` line
   appears, once per motion.
4. Press the button → `BTN <id> DOWN`/`UP` lines appear.
5. Send `BUZZ 440 200` from the host → the buzzer audibly sounds for
   ~200ms.
6. No stray `printk`/log text ever appears on the protocol UART.

Two boards running this image, each on its own serial port, are what a
two-player host game plugs into — this scenario passing on both boards is
the challenge's overall definition of done.

## Testing

Hardware-in-the-loop only — this is purely an integration/acceptance
check, run after the other four components' branches are merged.
