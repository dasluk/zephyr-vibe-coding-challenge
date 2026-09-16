# Spec: Board I/O (Buzzer + Buttons)

Owner of the buzzer and button devicetree wiring and their C API. Part of
[intent.md](../intent.md); see
[task-distribution.md](../docs/task-distribution.md) for the shared
contract and integration order.

## Goal

Give the rest of the firmware two small, independent capabilities: a
buzzer the communication component can drive on a host `BUZZ` command, and
debounced button events pushed straight to communication (no fusion
needed — buttons are already discrete).

## Scope

### Buzzer

- Add to `boards/demo/demo.dts`: `&timers3` with `st,prescaler` set for a
  usable PWM frequency range, child `pwm1` node pinctrl'd to
  `tim3_ch2_pc7` (Arduino D8), `status = "okay"`; `aliases { buzzer = &pwm1; };`
  (or a dedicated `buzzer` compatible node wrapping it — either is fine,
  keep it simple).
- Add to `app/prj.conf`: `CONFIG_PWM=y`.
- New module (suggested: `app/src/board_io_buzzer.c` +
  `app/include/board_io.h`): implement
  `void buzzer_request(uint32_t freq_hz, uint32_t duration_ms)` from
  `controller_ipc.h` — sets the PWM period/pulse for `freq_hz` at ~50% duty,
  arms a `k_timer` for `duration_ms`, then calls `pwm_set()` to zero duty
  on expiry. Must be safe to call from the communication thread's context
  (non-blocking, no long critical sections).

### Buttons

- Add a `gpio-keys` node with at least one button aliased `sw0`.
  **Open item — resolve before wiring**: the Nucleo U385RG-Q's onboard
  user-button (B1) pin is not yet confirmed anywhere in this repo. PC13 is
  the common Nucleo-64 convention but must be checked against this board's
  actual schematic/user manual (ST UM for the U385RG-Q, or physical
  continuity check) before committing to it. If it can't be confirmed in
  the time available, wire an external push-button on a spare GPIO instead
  of guessing the onboard pin.
- New module (suggested: `app/src/board_io_button.c`): GPIO interrupt +
  debounce (e.g. a `k_work_delayable` re-check after ~30ms), pushing a
  `struct button_event` onto `button_evt_q` on each confirmed press/release
  transition.

## Interface

- **Produces**: `struct button_event` onto `button_evt_q`.
- **Exposes**: `buzzer_request(freq_hz, duration_ms)`, called by the
  communication component on an incoming `BUZZ` line.
- **Consumes**: nothing from other firmware components.

## Out of scope

- The IMU / `i2c2` subtree (sensor driver component's job).
- Deciding the game's use of the buzzer/buttons — just expose the
  primitives.

## Definition of done

- `west build -b demo app` succeeds with both nodes and both modules.
- On real hardware: pressing the button produces a `button_event`
  (observable via a temporary RTT debug log); calling `buzzer_request()`
  audibly sounds the buzzer for the requested duration and stops on time.

## Testing

Hardware-in-the-loop only — PWM output and GPIO input both need the real
board to be meaningful; `native_sim` isn't worth the setup cost here given
the timebox.
