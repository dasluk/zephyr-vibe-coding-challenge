---
name: debug-on-target
description: Build, flash, and debug the Zephyr "demo" board target (STM32U385) over the macOS remote-pyocd setup used in this devcontainer, and monitor its serial console. Use this whenever asked to build/flash the firmware, debug something on the real board with gdb, or watch the board's serial output.
arguments: [target]
argument-hint: "[file, function, or bug description to debug]"
---

Build, flash, and debug on the physical board, and/or watch its serial output. What to debug: $target (may be empty if only build/flash/serial is needed).

## Environment

Runs inside the project's devcontainer on **macOS + Docker Desktop only**
(the "mac-pyocd" runner profile in `.vscode/zephyr-ide.json`). Ignore
SETUP-macos.md (native, no container) and the Windows/WSL2 paths in
`.vscode/launch.json` - neither applies here.

- **Flash** goes through pyocd's remote-probe protocol to a pyocd server on
  the host, reached as `remote:host.docker.internal`.
- **Debug** connects gdb to a gdbserver already running persistently on the
  host at `host.docker.internal:3333`. These scripts never start that
  gdbserver - it's the host's job. If `debug_on_target.sh` can't attach after
  retrying, it's probably not running; say so rather than trying to start one.
- **Serial** is a socat-created PTY at `/dev/ttyBRIDGE0`.

## Hardware lock (multiple agents/worktrees)

There is exactly one physical board: one ST-Link probe, one gdbserver, one
serial bridge. If more than one agent/worktree may be touching hardware at
the same time (e.g. an agent-team run with several teammates), every
hardware-touching script **must** go through
`scripts/with-hw-lock.sh <script> [args...]` instead of being run
directly - it's a thin `flock` wrapper around a shared lock file under
`/tmp` (shared across worktrees), so concurrent agents queue for the
hardware instead of racing it. `build.sh` alone doesn't touch hardware but
route it through the lock too for consistency and because it shares the
`app/build/demo` output the flash step needs.

The lock is held for as long as the wrapped command runs. For
long-running commands (`watch_serial.py`, a live gdb session), that means
you are holding up every other agent waiting on hardware for as long as
you leave it running. Stop it (ctrl-C, or kill the background job) as
soon as you're done reading/debugging, before moving on to anything else
- don't leave a serial watcher or gdb session open "just in case".

## Scripts

All under `scripts/`, safe to re-run. Wrap each of these in
`with-hw-lock.sh` (see above) whenever hardware may be shared.

- `build.sh [west-build-args...]` - `west build`s the `demo` board into `app/build/demo`.
- `flash.sh [west-build-args...]` - builds, then `west flash`es via the remote
  pyocd runner. Retries a few times on failure.
- `debug_on_target.sh [--skip-flash] [west-build-args...]` - builds and
  flashes (unless `--skip-flash`), then confirms gdb can attach at
  `host.docker.internal:3333`, retrying several times before giving up.
  A failed attempt right after a flash isn't unusual - let it retry.
- `watch_serial.py [port] [baud]` - streams serial output (defaults:
  `/dev/ttyBRIDGE0`, 115200). Run it in the background (`ctrl+b`); stop it
  as soon as you're done (see "Hardware lock" above).
- `with-hw-lock.sh <command> [args...]` - flock-serializes the wrapped
  command against every other agent/worktree sharing this hardware.
- `gdb/debug_template.gdb` - starting point for a gdb command file.

## Steps to debug $target

1. `./scripts/with-hw-lock.sh ./scripts/debug_on_target.sh` - builds, flashes, confirms the target is attachable.
2. For live serial output, start `./scripts/with-hw-lock.sh ./scripts/watch_serial.py` in the background (`ctrl+b`). Stop it as soon as you're done with it (see "Hardware lock" above) so other agents aren't blocked.
3. Based on $target (or general plausibility checks if none given), write a
   `.gdb` file from `scripts/gdb/debug_template.gdb` with the breakpoints/
   expressions you need. Do not `load` the elf or reset/halt on connect - the
   firmware is already flashed and running; the goal is to observe it, not
   restart it.
4. Launch gdb with it, also through the hardware lock (a live gdb session
   holds the same shared gdbserver, so it needs to queue too):
   ```
   ./scripts/with-hw-lock.sh \
       /opt/toolchains/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb \
       app/build/demo/zephyr/zephyr.elf -x your_commands.gdb
   ```
   A failed attach can be retried the same way as step 1 - a few attempts a
   few seconds apart before treating it as a real problem. Exit gdb as soon
   as you're done with this round of checks so the lock is released.
5. Repeat with different `.gdb` files as needed, cross-checking against
   serial output. Finish with a short summary of findings.

## Notes

- No MCP server for this.
- New helper scripts go under `scripts/` in this skill directory.
