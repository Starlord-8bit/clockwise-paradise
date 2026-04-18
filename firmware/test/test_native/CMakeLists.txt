cmake_minimum_required(VERSION 3.16)
project(test_native C CXX)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  unity
  GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
  GIT_TAG        v2.5.2
)
FetchContent_MakeAvailable(unity)

add_executable(test_native SimpleTests.cpp)
target_include_directories(test_native PRIVATE ../../lib/cw-logic/core)
target_link_libraries(test_native PRIVATE unity)
