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
  void hammer_fn(size_t id, Pattern &pattern, std::barrier<> &start_barrier, uint64_t &timing, Timer &timer, bool sync_each_ref);
  PatternBuilder &builder;
public:
  HammerSuite(PatternBuilder &builder);
  FuzzReport fuzz(size_t locations, size_t patterns);
  LocationReport fuzz_location(std::vector<PatternContainer> &patterns, bool allow_recalculation);
  std::vector<FuzzReport> auto_fuzz(size_t locations_per_fuzz, size_t max_runtime_in_seconds);
};
