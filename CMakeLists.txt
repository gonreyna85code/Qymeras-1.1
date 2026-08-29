cmake_minimum_required(VERSION 3.16)

# Qymera-IDF native ESP-IDF project (qymera-IDF branch)
#
# Layout:
#   components/qymera  -> reusable library component (sources live in src/, see
#                         components/qymera/CMakeLists.txt)
#   main               -> example app: boots Qymera core + optional Matter bridge
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(qymera)