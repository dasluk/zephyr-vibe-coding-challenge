cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(workshop)

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE src/main.c src/controller_ipc.c)
target_sources(app PRIVATE
  src/comms_codec.c
  src/comms_transport.c
  src/comms_tx.c
  src/comms_rx.c
)

# fw_version reported in the boot-time HELLO line (communication.spec.md);
# controller_id is a Kconfig string instead (see app/Kconfig) so each
# flashed board can be told apart without a rebuild.
target_compile_definitions(app PRIVATE APP_FW_VERSION="0.1.0")
