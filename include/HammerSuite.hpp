#pragma once
#include <barrier>
#include <cstdint>
#include <map>
#include <vector>
#include "FuzzReport.hpp"
#include "FuzzingParameterSet.hpp"
#include "HammeringPattern.hpp"
#include "Memory.hpp"
#include "PatternAddressMapper.hpp"
#include "LocationReport.hpp"
#include "RefreshTimer.hpp"

class HammerSuite {
private:
  size_t current_pattern_id;
  std::map<size_t, HammeringPattern> patterns;
  void hammer_fn(size_t id, std::vector<volatile char *> pattern, PatternAddressMapper &mapper, FuzzingParameterSet &params, std::barrier<> &start_barrier, RefreshTimer &timer, bool sync_each_ref);
  Memory &memory;
public:
  HammerSuite(Memory &memory);
  FuzzReport fuzz(size_t locations, size_t patterns);
  std::vector<LocationReport> fuzz_location(std::vector<HammeringPattern> &patterns, FuzzingParameterSet &params, size_t locations);
  std::vector<FuzzReport> auto_fuzz(size_t locations_per_fuzz, size_t max_runtime_in_seconds);
};
