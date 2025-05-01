cmake_minimum_required(VERSION 3.13)

project(tap_loopback CXX)

set(CMAKE_CXX_FLAGS "-Wall -std=gnu++17 -O3 -gdwarf-2")

add_executable(tap_loopback tap_loopback.cc)
