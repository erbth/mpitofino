add_library(mpitofino SHARED
	mpitofino.cc
	"${CMAKE_CURRENT_BINARY_DIR}/proto_out/client_lib_nd.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/proto_out/common.pb.cc"
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/proto_out/")
add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/proto_out/client_lib_nd.pb.cc"
	COMMAND
		protoc --proto_path "${CMAKE_SOURCE_DIR}/proto/" client_lib_nd.proto
		--cpp_out "${CMAKE_CURRENT_BINARY_DIR}/proto_out/"
	DEPENDS "${CMAKE_SOURCE_DIR}/proto/client_lib_nd.proto")

add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/proto_out/common.pb.cc"
	COMMAND
		protoc --proto_path "${CMAKE_SOURCE_DIR}/proto/" common.proto
		--cpp_out "${CMAKE_CURRENT_BINARY_DIR}/proto_out/"
	DEPENDS "${CMAKE_SOURCE_DIR}/proto/common.proto")


target_compile_options(mpitofino PRIVATE -pthread)

# Warn if undefined symbols are referenced even though this is a shared library
target_link_options(mpitofino PRIVATE -Wl,--no-undefined)

target_include_directories(mpitofino PRIVATE
	${PROTOBUF_INCLUDE_DIRS}
	"${CMAKE_CURRENT_BINARY_DIR}/proto_out/"
)

target_link_directories(mpitofino PRIVATE
	${PROTOBUF_LIBRARY_DIRS}
)

target_link_libraries(mpitofino PRIVATE
	-pthread
	${PROTOBUF_LIBRARIES}
)

set_target_properties(mpitofino PROPERTIES
	VERSION "${MPITOFINO_VERSION_MAJOR}.${MPITOFINO_VERSION_MINOR}.${MPITOFINO_VERSION_PATCH}"
	SOVERSION "${MPITOFINO_VERSION_MAJOR}"
	PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/mpitofino.h")


# Example application
add_executable(mpitofino_example
	example.cc
)

target_link_libraries(mpitofino_example PRIVATE mpitofino)
