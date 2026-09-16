# Spec: Communication

Implements the board↔host serial protocol proposed in
[intent.md](../intent.md) and resolves its open question about sharing the
UART with logging. See [task-distribution.md](../docs/task-distribution.md)
for the shared contract and integration order.

## Goal

Drain button and gesture/axis events off their queues and write them to
the host as ASCII lines; parse host commands and dispatch them (buzzer,
liveness, reset). USART1 carries only this protocol — logging moves to RTT.

## Scope

- Add to `app/prj.conf`: `CONFIG_LOG_BACKEND_RTT=y`, and disable the
  default UART log backend (`CONFIG_LOG_BACKEND_UART=n` or equivalent for
  this Zephyr revision) so nothing but protocol lines hits USART1. Keep
  `CONFIG_SERIAL=y`/console Kconfig for USART1 itself, but talk to it via
  the UART driver API directly (interrupt-driven TX/RX), not the console
  subsystem.
- **TX** (suggested `app/src/comms_tx.c`): thread that services
  `button_evt_q` and `controller_evt_q` (e.g. via `k_poll` on both, or
  round-robin with a short timeout) and writes lines:
  ```
  HELLO <controller_id> <fw_version>
  BTN <id> <DOWN|UP> <t_ms>
  GESTURE <name> <confidence> <t_ms>
  AXIS <name> <value> <t_ms>
  ```
  Send `HELLO` once at boot (`controller_id` from a `Kconfig` string so
  each flashed board can be told apart; `fw_version` from a build-time
  define). Throttle `AXIS` lines to a fixed rate (coordinate with fusion —
  it should already be throttling on its side per `fusion-gesture.spec.md`,
  but defend here too) so the link doesn't get flooded.
- **RX** (suggested `app/src/comms_rx.c`): thread/ISR parsing incoming
  lines:
  ```
  BUZZ <freq_hz> <duration_ms>   -> calls buzzer_request() from board-io
  PING <t_ms>                    -> reply PONG <t_ms>
  RESET                          -> sys_reboot(SYS_REBOOT_WARM)
  ```
  Malformed or unrecognized lines are logged (via RTT) and ignored — must
  never crash or wedge the link.
- Split the pure line-formatting/parsing ("codec") from the actual UART
  I/O ("transport") into separate functions/files so the codec is
  testable without hardware — see Testing below.

## Interface

- **Consumes**: `struct button_event` from `button_evt_q`,
  `struct controller_event` from `controller_evt_q`.
- **Calls**: `buzzer_request()` (from `board-io.spec.md`) on `BUZZ`.
- **Produces**: nothing onto shared queues; its output is the wire
  protocol itself.

## Out of scope

- Deciding the final protocol grammar beyond what's sketched in
  `intent.md` — refine it here if needed, but keep it a simple
  line-based ASCII format that's easy to eyeball in a serial monitor.
- The host-side game or any non-firmware tooling, beyond an optional small
  reference client (see below) to make this component testable stand-alone.

## Definition of done

- On real hardware: `HELLO` appears once at boot; pressing a button
  produces a `BTN` line; performing a gesture produces a `GESTURE` line;
  tilting produces throttled `AXIS` lines; sending `BUZZ 440 200` from a
  host terminal audibly sounds the buzzer for ~200ms; no `printk`/log
  output ever appears on the protocol UART.
- A short Python reference client (suggested:
  `scripts/host_serial_client.py`) that opens the board's serial port,
  prints parsed events, and can send `BUZZ`/`PING` — useful both for this
  component's own verification and as a starting point for whoever builds
  the actual host game.

## Testing

- Codec (line format/parse functions): unit-testable on `native_sim`/host,
  independent of the UART transport.
- Transport: hardware-in-the-loop, via the `debug-on-target` skill's
  `watch_serial.py` or the reference client above.
