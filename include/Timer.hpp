#pragma once
#include "PatternBuilder.hpp"
#include <cstdint>
#include <emmintrin.h>
#include <x86intrin.h>

class Timer {
  private:
    PatternBuilder &builder;
    uint64_t refresh_threshold;
    double_t get_average(std::vector<uint64_t> &measurements);
    uint64_t get_median(std::vector<uint64_t> &measurements);
    std::vector<uint64_t> get_measurements(size_t n);
    uint64_t measure(volatile char *addr);
  public:
    Timer(PatternBuilder &builder);
    uint64_t get_refresh_threshold();
    uint64_t reanalyze();
    uint64_t wait_for_refresh(size_t bank);
};

inline uint64_t Timer::measure(volatile char *addr) {
  uint32_t tsc_aux;
  _mm_clflushopt((void *)addr);
  _mm_mfence();
  
  uint64_t start = __rdtscp(&tsc_aux);
  _mm_mfence();
  
  *addr;

  _mm_mfence();
  uint64_t end = __rdtscp(&tsc_aux);
  
  return end - start;
}
