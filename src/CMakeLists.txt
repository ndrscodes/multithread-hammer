include_directories(${multithread_hammer_SOURCE_DIR}/include)

add_library(
  src  
  DRAMAddr.cpp
  DRAMConfig.cpp
  PatternBuilder.cpp
  HammerSuite.cpp
  Allocation.cpp
  FuzzReport.cpp
  Timer.cpp
  LocationReport.cpp
  Logger.cpp
  Jitter.cpp
)

target_include_directories(src PUBLIC
    "${CMAKE_SOURCE_DIR}/include" # This refers to the 'src' directory itself
)

target_link_libraries(src PRIVATE asmjit::asmjit)
