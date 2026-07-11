cmake_minimum_required(VERSION 3.16)

project(CacheFlow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

add_library(cacheflow
    src/order_book.cpp
)

target_include_directories(cacheflow
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(cacheflow_bench
    benchmark/benchmark_main.cpp
)

target_include_directories(cacheflow_bench
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cacheflow_bench
    PRIVATE
        cacheflow
)

add_executable(cacheflow_cli
    cli/main.cpp
)

target_include_directories(cacheflow_cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cacheflow_cli
    PRIVATE
        cacheflow
)

include(CTest)

if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(cacheflow_tests
        tests/test_order_book.cpp
    )

    target_link_libraries(cacheflow_tests
        PRIVATE
            cacheflow
            GTest::gtest_main
    )

    add_test(NAME cacheflow_tests COMMAND cacheflow_tests)
endif()