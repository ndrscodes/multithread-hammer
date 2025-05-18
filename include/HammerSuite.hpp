#pragma once
#include <barrier>
#include <cstdint>
#include <map>
#include <vector>
#include "FuzzReport.hpp"
#include "PatternBuilder.hpp"
#include "LocationReport.hpp"
#include "Timer.hpp"

class HammerSuite {
private:
  size_t current_pattern_id;
  std::map<size_t, Pattern> patterns;
  void hammer_fn(size_t id, Pattern &pattern, size_t iterations, std::barrier<> &start_barrier, std::mutex &mutex, uint64_t &timing, Timer &timer);
  PatternBuilder &builder;
public:
  HammerSuite(PatternBuilder &builder);
  FuzzReport fuzz(size_t locations, size_t patterns);
  LocationReport fuzz_location(std::vector<Pattern> patterns);
  std::vector<FuzzReport> auto_fuzz(size_t locations_per_fuzz, size_t max_runtime_in_seconds);
};
