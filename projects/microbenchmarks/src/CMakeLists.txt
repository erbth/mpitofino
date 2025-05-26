add_executable(test-mpi
	test_mpi.c
)

target_include_directories(test-mpi PRIVATE ${OMPI_INCLUDE_DIRS})
target_link_directories(test-mpi PRIVATE ${OMPI_LIBRARY_DIRS})
target_link_libraries(test-mpi PRIVATE ${OMPI_LIBRARIES})


add_executable(mpi_coll_performance
	mpi_coll_performance.c
)

target_include_directories(mpi_coll_performance PRIVATE ${OMPI_INCLUDE_DIRS})
target_link_directories(mpi_coll_performance PRIVATE ${OMPI_LIBRARY_DIRS})
target_link_libraries(mpi_coll_performance PRIVATE ${OMPI_LIBRARIES})
