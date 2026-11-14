# マウス側（Seeed XIAO nRF52840）：PMW3610 + ボタン → ESB送信
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mouse_tx)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_sources(app PRIVATE src/main.c src/pmw3610_bb.c)
