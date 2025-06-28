include_directories(${multithread_hammer_SOURCE_DIR}/include)

add_library(
  src  
  DRAMAddr.cpp
  DRAMConfig.cpp
  PatternBuilder.cpp
  HammerSuite.cpp
  Allocation.cpp
  FuzzReport.cpp
  RefreshTimer.cpp
  LocationReport.cpp
  Logger.cpp
  Jitter.cpp
  HammeringPattern.cpp
  Aggressor.cpp
  AggressorAccessPattern.cpp
  PatternAddressMapper.cpp
  Memory.cpp
  PatternBuilder.cpp
  FuzzingParameterSet.cpp
  BitFlip.cpp
  CodeJitter.cpp
  Enums.cpp
  RandomPatternBuilder.cpp
  SimplePatternBuilder.cpp
)

target_include_directories(src PUBLIC
    "${CMAKE_SOURCE_DIR}/include" # This refers to the 'src' directory itself
)

target_link_libraries(src PUBLIC asmjit::asmjit)
