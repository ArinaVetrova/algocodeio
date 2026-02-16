cmake_minimum_required(VERSION 3.1)
project(algocodeio)
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)

add_executable(callbackScheduler callbackScheduler/callbackScheduler.h callbackScheduler/testCallbackScheduler.cpp callbackScheduler/main.cpp)
target_link_libraries(callbackScheduler PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)
