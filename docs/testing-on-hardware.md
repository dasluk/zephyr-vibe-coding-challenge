# Testing the Game Controller Firmware on Real Hardware

How to exercise the fully integrated firmware (sensor driver, board I/O,
fusion/gesture, communication) on the physical board, from `/workdir`
(branch `demo-agent-teams`, which has everything merged).

## 1. Flash the integrated firmware

Through the hw-lock wrapper, since the board is shared:

```
./.claude/skills/debug-on-target/scripts/with-hw-lock.sh \
    ./.claude/skills/debug-on-target/scripts/flash.sh -- -DBOARD_ROOT=/workdir
```

## 2. Watch the protocol traffic

Two options, both through the lock, both run in the background (`ctrl+b`):

- Raw:
  ```
  ./.claude/skills/debug-on-target/scripts/with-hw-lock.sh \
      ./.claude/skills/debug-on-target/scripts/watch_serial.py
  ```
- Parsed + interactive (recommended — nicer output, and lets you send
  commands):
  ```
  ./.claude/skills/debug-on-target/scripts/with-hw-lock.sh \
      python3 scripts/host_serial_client.py /dev/ttyBRIDGE0
  ```
  then type at its stdin prompt.

You should see `HELLO ctrl-A 0.1.0` the moment it boots/resets.

## 3. Exercise each behavior

| To test | Do this | Expect |
|---|---|---|
| Tilt sensing | Tilt the board left/right | `AXIS tilt_x <value> <t_ms>` lines, tracking direction |
| Gestures | Swing/shake/punch the board | one `GESTURE <name> <confidence> <t_ms>` per motion (not a flood) |
| Button | Press the onboard B1 button | `BTN 0 DOWN <t_ms>` / `BTN 0 UP <t_ms>` |
| Buzzer | In the `host_serial_client.py` prompt: `buzz 440 200` | buzzer audibly sounds ~200ms |
| Liveness | In the client: `ping` | `PONG <t_ms>` echoed back |
| Reset | In the client: `reset` | reboots, re-sends `HELLO` |

The client also accepts any other line typed at its prompt and sends it
verbatim to the board (advanced/debug use) — handy for trying raw
protocol lines by hand without waiting on a dedicated client command.

Remember to stop `watch_serial.py`/the client with ctrl-C when you're
done so you're not holding the hw-lock for no reason.
