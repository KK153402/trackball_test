# ドングル側（Raytac MDBT50Q-CX-40）：ESB受信 → USB HIDマウス
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dongle_rx)

# USB初期化はZephyrのUSBサンプル共通コード（sample_usbd）を流用
include(${ZEPHYR_BASE}/samples/subsys/usb/common/common.cmake)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_sources(app PRIVATE src/main.c)
