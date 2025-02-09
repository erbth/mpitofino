cmake_minimum_required(VERSION 3.13)

project(mpitofino CXX)

set(CMAKE_CXX_FLAGS "-std=gnu++20 -Wall -O3")

add_subdirectory(sd)
