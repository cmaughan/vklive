if(BUILD_TESTS)

find_package(Catch2 CONFIG REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

list(APPEND TEST_SOURCES
    tests/CMakeLists.txt
    tests/main.cpp)

file(GLOB_RECURSE FOUND_TEST_SOURCES "${ZEST_ROOT}/*.test.cpp")

exclude_files_from_dir_in_list("${FOUND_TEST_SOURCES}" "/m3rdparty/" FALSE)

enable_testing()

set (TEST_SOURCES
    ${FOUND_TEST_SOURCES}
    ${TEST_SOURCES}
    )

set(MODULE_NAME Zest_UnitTests)

add_executable(${MODULE_NAME} ${TEST_SOURCES})

target_include_directories(${MODULE_NAME} PRIVATE
    ${M3RDPARTY_DIR}
    ${CMAKE_BINARY_DIR}
    include
    )


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
find_package(Threads REQUIRED)
set(PLATFORM_LINKLIBS
    Threads::Threads
	)
endif()

target_link_libraries(${MODULE_NAME}
    PRIVATE
        Zest::Zest
        Catch2::Catch2
        Catch2::Catch2WithMain
        ${PLATFORM_LINKLIBS}
        ${CMAKE_THREAD_LIBS_INIT})

add_test(${MODULE_NAME} ${MODULE_NAME})

SOURCE_GROUP(tests REGULAR_EXPRESSION ".*.test.(cpp|h)+")
SOURCE_GROUP(tests FILES ${TEST_SOURCES})

endif()

