cmake_minimum_required(VERSION 3.13)

project(mpitofino CXX)

set(CMAKE_CXX_FLAGS "-std=gnu++20 -Wall -O3")

# Find required packages
find_path(BFRT_INCLUDE_DIRS bf_rt/bf_rt.hpp REQUIRED
	HINTS "$ENV{SDE_INSTALL}/include")

find_path(BFRT_LIBRARY_DIRS libbfutils.so REQUIRED
	HINTS "$ENV{SDE_INSTALL}/lib")

# This allows for including files like `#include "common/..."'
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(sd)
