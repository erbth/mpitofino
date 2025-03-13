add_executable(allreduce_poc-ctrl
	main.cc
	asic_driver.cc
	../common/epoll.cc
	../common/signalfd.cc
	../common/utils.cc
	../common/simple_types.cc
)

target_include_directories(allreduce_poc-ctrl PRIVATE
	${BFRT_INCLUDE_DIRS}
)

target_link_directories(allreduce_poc-ctrl PRIVATE
	${BFRT_LIBRARY_DIRS}
)

target_link_libraries(allreduce_poc-ctrl PRIVATE
	-ldriver
)
