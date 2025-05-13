cmake_minimum_required(VERSION 3.13)

project(playground C)

set(CMAKE_C_FLAGS "-Wall -std=c11 -O3")

# Find packages
find_package(PkgConfig)

pkg_check_modules(OMPI REQUIRED ompi)

add_subdirectory(src)
