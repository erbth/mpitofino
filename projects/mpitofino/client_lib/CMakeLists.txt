add_library(mpitofino SHARED
	mpitofino.cc
)

target_compile_options(mpitofino PRIVATE -pthread)

# Warn if undefined symbols are referenced even though this is a shared library
target_link_options(mpitofino PRIVATE -Wl,--no-undefined)
target_link_libraries(mpitofino PRIVATE -pthread)

set_target_properties(mpitofino PROPERTIES
	VERSION "${MPITOFINO_VERSION_MAJOR}.${MPITOFINO_VERSION_MINOR}.${MPITOFINO_VERSION_PATCH}"
	SOVERSION "${MPITOFINO_VERSION_MAJOR}"
	PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/mpitofino.h")


# Example application
add_executable(mpitofino_example
	example.cc
)

target_link_libraries(mpitofino_example PRIVATE mpitofino)
