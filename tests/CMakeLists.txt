include(GoogleTest)

add_executable(unit_tests unit_tests.cpp)

target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/lib)

target_link_libraries(unit_tests
    PRIVATE
    ARBUZ
    GTest::gtest_main
    GTest::gtest
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(unit_tests PRIVATE --coverage -O0 -g)
    target_link_options(unit_tests PRIVATE --coverage)
endif()

gtest_discover_tests(unit_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
