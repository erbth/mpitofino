add_executable(mpitofino-sd
	main.cc
	state_repository.cc
	packet_processor.cc
	asic_driver.cc
	distributed_control_plane_agent.cc
	../common/epoll.cc
	../common/signalfd.cc
	../common/simple_types.cc
	../common/timerfd.cc
	../common/utils.cc
	"${CMAKE_CURRENT_BINARY_DIR}/proto_out/common.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/proto_out/ctrl_sd.pb.cc"
)


file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/proto_out/")
add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/proto_out/common.pb.cc"
	COMMAND
		protoc --proto_path "${CMAKE_SOURCE_DIR}/proto/" common.proto
		--cpp_out "${CMAKE_CURRENT_BINARY_DIR}/proto_out/"
	DEPENDS "${CMAKE_SOURCE_DIR}/proto/common.proto")

add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/proto_out/ctrl_sd.pb.cc"
	COMMAND
		protoc --proto_path "${CMAKE_SOURCE_DIR}/proto/" ctrl_sd.proto
		--cpp_out "${CMAKE_CURRENT_BINARY_DIR}/proto_out/"
	DEPENDS "${CMAKE_SOURCE_DIR}/proto/ctrl_sd.proto")


# Avoid warnings from old protobuf version required by bf-sde 9.9.1
target_compile_options(mpitofino-sd PRIVATE -Wno-deprecated-declarations)

target_include_directories(mpitofino-sd PRIVATE
	${BFRT_INCLUDE_DIRS}
	${PROTOBUF_INCLUDE_DIRS}
	"${CMAKE_CURRENT_BINARY_DIR}/proto_out/"
)

target_link_directories(mpitofino-sd PRIVATE
	${BFRT_LIBRARY_DIRS}
	${PROTOBUF_LIBRARY_DIRS}
)

target_link_libraries(mpitofino-sd PRIVATE
	-ldriver
	${PROTOBUF_LIBRARIES}
)
