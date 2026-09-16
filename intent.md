# Intent: Gesture-Controlled Game Controller

## Problem statement

Participants have a Nucleo U385RG-Q board with an X-NUCLEO-IKS4A1 sensor
shield (IMU + environmental sensors), an onboard buzzer, and buttons — but
no firmware that turns it into an input device. We need firmware that
recognizes hand gestures and button presses on the board and reports them
to a host over a serial link, so that a game running on a laptop can be
controlled by moving the board around. At the end of the session, two
participants each hold one of these controllers and play against each other.

## Why

This is the "game controller" track of the Vibe Coding Challenge (see
[README.md](README.md#game-controller)). It's the vehicle for practicing the
full AI-native SDLC loop — intent → spec → plan → implement → debug on real
hardware — on a problem with an immediately visible, fun payoff: a
playable head-to-head game at the end of the two days.

## Goals (definition of done)

- Firmware on the Nucleo board samples the IMU continuously and turns raw
  motion into a small set of discrete, named gestures (e.g. swing, punch,
  tilt left/right/up/down, shake) plus raw/derived continuous axes (e.g.
  tilt angle) usable for analog-style steering.
- Onboard buttons are debounced and reported as discrete press/release
  events.
- All of the above is streamed off the board over the existing serial
  connection (UART1, already wired to the console) in a format a host-side
  game can parse in real time, without flooding or desyncing the link.
- The buzzer can be driven from the host side (e.g. as haptic/audio
  feedback for hits or events) — the link is not one-directional.
- Two boards can each drive one player in a two-player game running on a
  host machine (e.g. a laptop), well enough for a live, in-person match at
  the end of the challenge.
- A participant with no prior embedded experience can get from "board on
  the desk" to "gesture shows up on host serial monitor" by driving Claude,
  reusing the existing `debug-on-target` skill for build/flash/serial
  rather than reinventing that plumbing.

## Non-goals

- Not aiming for production-grade gesture recognition (ML models, training
  data, etc.) — simple thresholding/heuristics on accelerometer/gyro data
  is enough if it's fun and reliable enough for a live match.
- Not building the host-side game itself as part of this intent — the game
  is whatever a given team chooses to build (see Game Ideas in
  [README.md](README.md#game-controller)); this intent only covers the
  controller firmware and the interface it exposes.
- Not designing for more than two simultaneous controllers.
- No wireless (BLE/Wi-Fi) requirement — serial (USB/UART) is sufficient for
  the challenge.

## Constraints

- Target: Nucleo U385RG-Q (STM32U385RG, Cortex-M33), Zephyr RTOS, built via
  the existing `west`/`debug-on-target` toolchain in this repo.
- Sensors: X-NUCLEO-IKS4A1 shield (IMU is an LSM6DSO16IS-family part per
  the existing `feature/buzzer_control_with_LSM6DSO16IS` branch;
  confirm exact part/driver during spec).
- Buzzer: Arduino header D8 / PC7, TIM3 channel 2 (see
  [README.md](README.md#buzzer)).
- Only one board/ST-Link/serial port per participant machine — if firmware
  work is split across agents, they must not fight over the hardware (see
  the mutex note in `docu/toolchain_setup.md` in the sibling
  `camp-2026-ai-session` repo).
- Two-day hackathon timebox; participants write little to no code by hand.

## Proposed serial interface (illustrative — not binding)

This is a sketch of one workable shape for the link, meant to unblock
spec/design work, not a final protocol. The component spec(s) should
confirm or replace it.

- **Transport**: each board's own serial link (its console/USB-CDC port).
  For two-player play, the host just opens two ports — one per board —
  rather than multiplexing both players onto a single link.
- **Framing**: newline-terminated ASCII lines, so the link stays readable
  in a plain serial monitor while debugging (important given the "little
  to no hand-written code" constraint — participants and Claude both need
  to eyeball this).
- **Board → host** (controller events):
  ```
  HELLO <controller_id> <fw_version>
  BTN <id> <DOWN|UP> <t_ms>
  GESTURE <name> <confidence> <t_ms>
  AXIS <name> <value> <t_ms>
  PING <t_ms>
  ```
  Example:
  ```
  HELLO ctrl-A 0.1.0
  AXIS tilt_x 0.42 1234
  GESTURE SWING_RIGHT 0.87 1240
  BTN 1 DOWN 1241
  BTN 1 UP 1305
  ```
- **Host → board** (feedback / control):
  ```
  BUZZ <freq_hz> <duration_ms>
  PONG <t_ms>
  RESET
  ```
- **Handshake/liveness**: board sends `HELLO` on boot/reconnect; host may
  `PING`/expect `PONG` to detect a dead link (useful mid-match).
- Malformed or unrecognized lines are ignored by both sides rather than
  crashing the link — a live match shouldn't die on one bad frame.

## Open questions

- Exact IMU part/driver and which axes/derived signals give reliable
  gestures within the 2-day window — for the sensor-driver/fusion spec to
  answer.
- Whether the existing console UART should double as the game-data link,
  or whether logs (`printk`) need to move elsewhere to avoid corrupting
  frames the host is parsing.
- How much gesture smoothing/debounce is needed for gestures to feel
  responsive rather than laggy or falsely triggered during a live match.
- Final choice of game(s) — affects which gestures/axes actually matter.

## References

- [README.md](README.md) — hardware list, buzzer wiring, challenge ideas.
- `docu/vibecoding_challenge.md` and `docu/claude_basics.md` in the
  sibling `camp-2026-ai-session` repo — SDLC flow (intent → spec → plan),
  agent-team split (architect / sensor driver / fusion / communication /
  supervisor), and the hardware mutex note.
- `.claude/skills/debug-on-target` — existing build/flash/serial tooling to
  reuse rather than duplicate.
