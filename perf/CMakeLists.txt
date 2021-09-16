set(CMAKE_CXX_STANDARD 11)

# For successful build of tests google benchmark library
# should be build with -DCMAKE_POSITION_INDEPENDENT_CODE=on
find_package(benchmark QUIET)
if (NOT ${benchmark_FOUND})
    message(AUTHOR_WARNING "Google Benchmark library was not found")
    return()
endif()

include_directories(${MSGPUCK_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/src/box)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

add_executable(tuple.perftest tuple.cc)
target_link_libraries(tuple.perftest core box tuple benchmark::benchmark)

add_library(txn SHARED txn.cc)
target_link_libraries(txn benchmark::benchmark)
set_target_properties(txn PROPERTIES PREFIX "")
