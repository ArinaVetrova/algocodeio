cmake_minimum_required(VERSION 3.1)
project(main)
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(main PRIVATE GTest::gtest GTest::gtest_main)
