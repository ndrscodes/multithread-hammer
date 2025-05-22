cmake_minimum_required(VERSION 3.31)
project(multithread_hammer CXX)

include(FetchContent)

add_executable(multithread_hammer src/main.cpp)

add_subdirectory("3rdparty")
add_subdirectory("src")

# these changes are propagated to multithread_hammer due to the PUBLIC scope.
target_compile_features(
  src
  PUBLIC
  cxx_std_20
)

target_compile_options(src PUBLIC
    -g
    -O0
    -march=native
    -msse4.2
)

target_link_libraries(
  multithread_hammer
  PRIVATE
  src
)
