cmake_minimum_required(VERSION 3.10)
project(algocodeio)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

find_package(GTest REQUIRED)

add_executable(callbackScheduler callbackScheduler/main.cpp)
target_link_libraries(callbackScheduler  PRIVATE GTest::gtest GTest::gtest_main)

add_executable(LRUCache LRUCache/TLruCache.h LRUCache/testLruCache.cpp LRUCache/main.cpp)
target_link_libraries(LRUCache PRIVATE GTest::gtest GTest::gtest_main)

add_executable(LFUCache LRUCache/TLfuCache.h LRUCache/testLfuCache.cpp LRUCache/main.cpp)
target_link_libraries(LFUCache PRIVATE GTest::gtest GTest::gtest_main)

add_executable(LfuCacheThreadsafeLazyUpdate LRUCache/TLfuCacheThreadsafeLazyUpdate.h LRUCache/testLfuCacheThreadsafeLazyUpdate.cpp LRUCache/main.cpp)
target_link_libraries(LfuCacheThreadsafeLazyUpdate PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)

add_executable(LfuCacheLockStripping LRUCache/TLfuCacheLockStripping.h LRUCache/testLfuCacheLockStripping.cpp LRUCache/main.cpp)
target_link_libraries(LfuCacheLockStripping PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)

add_executable(xmlEncryptor xmlEncryptor/xmlEncryptor.h xmlEncryptor/xmlEncryptor.cpp xmlEncryptor/testXmlEncryptor.cpp xmlEncryptor/main.cpp)
target_link_libraries(xmlEncryptor PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)

add_executable(xmlEncryptorMt xmlEncryptor/xmlEncryptorMt.h xmlEncryptor/xmlEncryptorMt.cpp xmlEncryptor/testXmlEncryptorMt.cpp xmlEncryptor/main.cpp)
target_link_libraries(xmlEncryptorMt PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)

add_executable(workStealingPool xmlEncryptor/workStealingPool.h xmlEncryptor/workStealingPool.cpp xmlEncryptor/testWorkStealingPool.cpp xmlEncryptor/main.cpp)
target_link_libraries(workStealingPool PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)

add_executable(discreteSampler discreteSampler/discreteSampler.h discreteSampler/testDiscreteSampler.cpp discreteSampler/main.cpp)
target_link_libraries(discreteSampler PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)

add_executable(discreteSamplerMt discreteSampler/discreteSamplerMt.h discreteSampler/testDiscreteSamplerMt.cpp discreteSampler/main.cpp)
target_link_libraries(discreteSamplerMt PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)

target_compile_options(discreteSamplerMt PRIVATE -fsanitize=thread -g)
target_link_options(discreteSamplerMt PRIVATE -fsanitize=thread)
