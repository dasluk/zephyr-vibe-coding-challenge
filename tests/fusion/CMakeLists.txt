cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fusion_test)

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../app)

target_include_directories(app PRIVATE ${APP_DIR}/include)
target_sources(app PRIVATE
    ${APP_DIR}/src/controller_ipc.c
    ${APP_DIR}/src/fusion.c
    src/test_fusion.c
)
