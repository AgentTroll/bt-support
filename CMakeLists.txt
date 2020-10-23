set(BOARD "adafruit_feather_nrf52840")

cmake_minimum_required(VERSION 3.16)
find_package(Zephyr)

project(bt-support)

target_sources(app
    PRIVATE src/main.cpp)
