# Agent Team Kickoff Prompt

Paste this into a `claude` session started from a system terminal (not the
VS Code integrated terminal — split-pane teammate view needs tmux/iTerm2)
in the root of `zephyr-vibe-coding-challenge`, once
`CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1` is set (already added to
`~/.claude/settings.json`).

```
Read intent.md, docs/task-distribution.md, and the five specs/*.spec.md
files. Spawn 4 teammates to implement them in parallel, one each for:
sensor-driver, board-io, fusion-gesture, communication. You (the lead)
handle the supervisor spec yourself after they're done, per the
integration order in docs/task-distribution.md.

Before spawning them, merge controller_ipc.h/.c (the shared struct/queue
contract from docs/task-distribution.md) into main yourself, so every
teammate branches from a commit that already has it.

Each teammate must:
- Create and work in its own git worktree and branch, e.g.
  `git worktree add ../wt-<component> -b feature/<component>`
  (this is NOT automatic — do it explicitly as your first step).
- Implement strictly against its own specs/<component>.spec.md and the
  frozen controller_ipc.h contract — do not touch another component's
  files or the shared header.
- NOT use any real hardware: the board isn't physically connected right
  now. Do not run anything from .claude/skills/debug-on-target (no
  build.sh/flash.sh/debug_on_target.sh/watch_serial.py). Verify only via
  `west build -b demo app -d app/build/demo` (must compile) and, where the
  spec's "Testing" section says it's feasible, native_sim/ztest. Any
  hardware-in-the-loop acceptance criteria in the spec should be reported
  as "not verifiable without hardware" instead of attempted.
- Report back its resulting file list and its produced/consumed
  controller_ipc.h types before you (the lead) integrate it.

Once all 4 are done: review each branch, resolve the two devicetree
hunks (i2c2 from sensor-driver, timers3/gpio-keys from board-io) into
boards/demo/demo.dts, implement the supervisor spec yourself (main.c,
prj.conf, CMakeLists.txt), and confirm the whole thing builds with
`west build -b demo app -d app/build/demo`.
```
