cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(workshop)

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE src/main.c src/controller_ipc.c src/sensor_driver.c)
