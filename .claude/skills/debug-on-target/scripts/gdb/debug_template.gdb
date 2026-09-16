# Template gdb command file for debugging on the target over the
# host.docker.internal:3333 gdbserver. Copy this, rename it, and fill in the
# breakpoints/expressions for the specific issue being debugged.
#
# NOTE: this attaches to firmware that was already flashed separately
# (scripts/flash.sh). Do not "load" the elf here and do not reset/halt the
# core on connect - both would undo the flash and restart the program from
# scratch instead of observing it as it runs.
target remote host.docker.internal:3333

# Example: break on the heartbeat loop in app/src/main.c and inspect the
# counter across a couple of iterations.
b main.c:11

info breakpoints
continue
p heartbeat_count

continue
p heartbeat_count

quit
