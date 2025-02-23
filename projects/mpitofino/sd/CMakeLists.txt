add_executable(mpitofino-sd
	main.cc
	state_repository.cc
	packet_processor.cc
	asic_driver.cc
	../common/epoll.cc
	../common/signalfd.cc
	../common/utils.cc
	../common/simple_types.cc
)

target_include_directories(mpitofino-sd PRIVATE
	${BFRT_INCLUDE_DIRS}
)

target_link_directories(mpitofino-sd PRIVATE
	${BFRT_LIBRARY_DIRS}
)

target_link_libraries(mpitofino-sd PRIVATE
	-ldriver
)
