cmake_minimum_required(VERSION 3.31)
project(multithread_hammer CXX)

include(FetchContent)

add_executable(multithread_hammer src/main.cpp)
set(CMAKE_CXX_FLAGS "-g -O0 -march=native -msse4.2")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
target_include_directories(multithread_hammer PUBLIC include)
target_compile_features(
  multithread_hammer
  PUBLIC
  cxx_std_20
)

link_directories(src)
add_subdirectory("src")

add_subdirectory("3rdparty")

target_link_libraries(
  multithread_hammer
  PRIVATE
  asmjit::asmjit
  src
)
