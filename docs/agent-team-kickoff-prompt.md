# Agent Team Kickoff Prompt

Paste this into a `claude` session started from a system terminal (not the
VS Code integrated terminal — split-pane teammate view needs tmux/iTerm2)
in the root of `zephyr-vibe-coding-challenge`, once
`CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1` is set (already added to
`~/.claude/settings.json`).

## Worktree/build setup gotchas (learned running this once)

These bit the first real run and are baked into the prompt below, but are
called out here so they're understood rather than just copy-pasted:

- **Worktrees must live inside the repo, not as siblings of it.** The
  original `git worktree add ../wt-<component> ...` assumes the directory
  *above* the repo root is writable. In a sandboxed/devcontainer checkout
  that's often not true (`fatal: could not create leading directories of
  '../wt-.../.git': Permission denied`). Use a path *inside* the repo
  instead, e.g. `worktrees/wt-<component>` (gitignore it, or just don't
  commit it from any branch).
- **A failed `git worktree add` still creates the branch.** If the first
  attempt fails on directory creation (see above), the `-b
  feature/<component>` branch it was about to check out already exists,
  and retrying with a fixed path fails with `fatal: a branch named
  'feature/<component>' already exists`. `git branch -D feature/<component>`
  before retrying.
- **`external/` (the west-managed Zephyr SDK/module checkout) is
  gitignored**, so a fresh worktree checkout doesn't have it — `west
  build` from inside a new worktree fails much later with confusing
  Kconfig/module errors otherwise. Symlink the existing one in once per
  worktree: `ln -s /workdir/external worktrees/wt-<component>/external`
  (it's read-only shared reference, not owned by any teammate, so a
  symlink is fine and avoids copying the whole SDK per worktree).
- **`west build` from a worktree can't find the `demo` board on its own.**
  The west workspace topdir (`.west/`) lives at the main repo root, and
  `BOARD_ROOT` isn't inferred across a worktree boundary — you get "No
  board named 'demo' found" even though `boards/demo/` is right there
  (it's git-tracked, so every worktree has its own copy). Pass it
  explicitly: `west build -b demo app -d app/build/demo --
  -DBOARD_ROOT=<absolute-path-to-that-worktree>`. This applies to every
  `west build` invocation below, both the teammates' per-branch
  verification and the lead's final integration build from the main
  checkout (there it's just `-DBOARD_ROOT=/workdir`, i.e. the repo root).

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
- Create and work in its own git worktree and branch, rooted inside the
  repo (a sibling directory of the repo root may not be writable), e.g.
  `git worktree add worktrees/wt-<component> -b feature/<component>`
  (this is NOT automatic — do it explicitly as your first step). If this
  fails partway and you retry with a different path, check for
  (and `git branch -D`) a stray branch left over from the failed attempt
  first. Symlink the SDK checkout into the new worktree so builds work
  there: `ln -s /workdir/external worktrees/wt-<component>/external`.
- Implement strictly against its own specs/<component>.spec.md and the
  frozen controller_ipc.h contract — do not touch another component's
  files or the shared header.
- Verify via `west build -b demo app -d app/build/demo --
  -DBOARD_ROOT=<absolute-path-to-its-worktree>` (must compile — the
  explicit BOARD_ROOT is required from any worktree other than the main
  checkout) and, where the spec's "Testing" section says it's feasible,
  native_sim/ztest.
- The board is physically connected and reachable via
  .claude/skills/debug-on-target — use it for on-device verification
  (flashing, watching serial/RTT logging, gdb) once a teammate's own
  build is green and it has something worth checking on real hardware.
  Since only one physical board exists, every hardware-touching command
  MUST go through the flock-based mutex wrapper,
  `.claude/skills/debug-on-target/scripts/with-hw-lock.sh <command>
  [args...]`, instead of calling build.sh/flash.sh/debug_on_target.sh/
  watch_serial.py (or gdb) directly — see the skill's "Hardware lock"
  section. That lock is held for as long as the wrapped command runs, so
  gracefully shut down anything long-running (a `watch_serial.py`
  session, a gdb attach) as soon as you're done reading/debugging —
  ctrl-C it or kill the background job — before moving on, rather than
  leaving it open and blocking every other teammate queued on the same
  hardware.
- Report back its resulting file list and its produced/consumed
  controller_ipc.h types before you (the lead) integrate it.

Once all 4 are done: review each branch, resolve the two devicetree
hunks (i2c2 from sensor-driver, timers3/gpio-keys from board-io) into
boards/demo/demo.dts, implement the supervisor spec yourself (main.c,
prj.conf, CMakeLists.txt), and confirm the whole thing builds from the
main checkout with `west build -b demo app -d app/build/demo --
-DBOARD_ROOT=/workdir`.
```
