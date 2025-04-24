cmake_minimum_required(VERSION 3.13)

project(mpitofino CXX)

set(MPITOFINO_VERSION_MAJOR 0)
set(MPITOFINO_VERSION_MINOR 0)
set(MPITOFINO_VERSION_PATCH 1)


set(CMAKE_CXX_FLAGS "-std=gnu++17 -Wall -O3 -gdwarf-2")


# Build options
option(WITH_SD         "Compile switch-daemon. Requires bfrt development files" OFF)
option(WITH_CLIENT_LIB "Compile client library" OFF)
option(WITH_ND         "Compile node-daemon" ${WITH_CLIENT_LIB})


# Find required packages
find_package(PkgConfig REQUIRED)

if (WITH_SD)
	find_path(BFRT_INCLUDE_DIRS bf_rt/bf_rt.hpp REQUIRED
		HINTS "$ENV{SDE_INSTALL}/include")

	find_path(BFRT_LIBRARY_DIRS libbfutils.so REQUIRED
		HINTS "$ENV{SDE_INSTALL}/lib")
endif()

pkg_check_modules(PROTOBUF REQUIRED protobuf)

# This allows for including files like `#include "common/..."'
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

# Create config file
configure_file(config.h.in config.h)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

if (WITH_SD)
	add_subdirectory(sd)
endif()

if (WITH_CLIENT_LIB)
	add_subdirectory(client_lib)
endif()

if (WITH_ND)
	add_subdirectory(nd)
endif()
