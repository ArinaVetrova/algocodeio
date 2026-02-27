cmake_minimum_required(VERSION 3.1)
project(algocodeio)
set(CMAKE_CXX_STANDARD 20)

find_package(GTest REQUIRED)

add_executable(callbackScheduler callbackScheduler/main.cpp)
target_link_libraries(callbackScheduler  PRIVATE GTest::gtest GTest::gtest_main)

add_executable(LRUCache LRUCache/TLruCache.h LRUCache/testLruCache.cpp LRUCache/main.cpp)
target_link_libraries(LRUCache PRIVATE GTest::gtest GTest::gtest_main)

add_executable(LFUCache LRUCache/TLfuCache.h LRUCache/testLfuCache.cpp LRUCache/main.cpp)
target_link_libraries(LFUCache PRIVATE GTest::gtest GTest::gtest_main)

add_executable(LfuCacheThreadsafeLazyUpdate LRUCache/TLfuCacheThreadsafeLazyUpdate.h LRUCache/testLfuCacheThreadsafeLazyUpdate.cpp LRUCache/main.cpp)
target_link_libraries(LfuCacheThreadsafeLazyUpdate PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)
