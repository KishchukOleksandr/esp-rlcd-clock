cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Enforce strict compiler warnings across project components per Constitution Quality Gates
idf_build_set_property(COMPILE_OPTIONS "-Wall;-Wextra" APPEND)

project(esp_rlcd_clock)
