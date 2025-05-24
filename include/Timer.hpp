#pragma once
#include "PatternBuilder.hpp"
#include <cstdint>
#include <emmintrin.h>
#include <vector>
#include <x86intrin.h>

typedef struct {
  uint64_t timing;
  uint64_t timestamp;
} measurement;

class Timer {
  private:
    PatternBuilder &builder;
    uint64_t refresh_threshold;
    uint64_t cycles_per_refresh;
    double_t get_average(std::vector<uint64_t> &measurements);
    uint64_t get_median(std::vector<uint64_t> &measurements);
    std::vector<measurement> get_measurements(size_t n);
    measurement measure(volatile char *addr);
    std::vector<uint64_t> extract_timestamps(std::vector<measurement> &measurements);
    std::vector<uint64_t> extract_timings(std::vector<measurement> &measurements);
  public:
    Timer(PatternBuilder &builder);
    uint64_t get_refresh_threshold();
    uint64_t get_cycles_per_refresh();
    uint64_t reanalyze();
    uint64_t wait_for_refresh(size_t bank);
    static uint64_t current_timestamp();
};

inline measurement Timer::measure(volatile char *addr) {
  uint32_t tsc_aux;
  _mm_clflushopt((void *)addr);
  _mm_mfence();
  _mm_lfence();
  
  uint64_t start = __rdtscp(&tsc_aux);
  _mm_lfence();
  
  *addr;

  _mm_mfence();
  uint64_t end = __rdtscp(&tsc_aux);
  
  return { end - start, start };
}

inline uint64_t Timer::current_timestamp() {
  uint32_t tsc_aux;
  return __rdtscp(&tsc_aux);
}
