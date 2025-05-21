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
)
