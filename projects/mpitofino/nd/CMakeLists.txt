add_executable(mpitofino-nd
	main.cc
	node_daemon.cc
	../common/epoll.cc
	../common/signalfd.cc
	../common/utils.cc
	"${CMAKE_CURRENT_BINARY_DIR}/proto_out/client_lib_nd.pb.cc"
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/proto_out/")
add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/proto_out/client_lib_nd.pb.cc"
	COMMAND
		protoc --proto_path "${CMAKE_SOURCE_DIR}/proto/" client_lib_nd.proto
		--cpp_out "${CMAKE_CURRENT_BINARY_DIR}/proto_out/"
	DEPENDS "${CMAKE_SOURCE_DIR}/proto/client_lib_nd.proto")

target_include_directories(mpitofino-nd PRIVATE
	${PROTOBUF_INCLUDE_DIRS}
	"${CMAKE_CURRENT_BINARY_DIR}/proto_out/"
)

target_link_directories(mpitofino-nd PRIVATE
	${PROTOBUF_LIBRARY_DIRS}
)

target_link_libraries(mpitofino-nd PRIVATE
	${PROTOBUF_LIBRARIES}
)
