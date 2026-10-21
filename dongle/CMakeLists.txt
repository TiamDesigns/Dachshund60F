# 2.4 GHz dongle firmware (second nRF52840), Zephyr application.
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(kb_dongle LANGUAGES C)

target_sources(app PRIVATE src/main.c)
