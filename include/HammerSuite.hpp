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

struct Args {
  uint64_t runtime_limit = 3600;
  uint16_t locations = 3;
  uint16_t threads = 1;
  bool test_effective_patterns;
};

class HammerSuite {
private:
  size_t current_pattern_id;
  std::map<size_t, HammeringPattern> patterns;
  void hammer_fn(size_t id, std::vector<volatile char *> pattern, PatternAddressMapper &mapper, FuzzingParameterSet &params, std::barrier<> &start_barrier, RefreshTimer &timer, bool sync_each_ref);
  void check_effective_patterns(std::vector<FuzzReport> &patterns);
  Memory &memory;
public:
  HammerSuite(Memory &memory);
  FuzzReport fuzz(size_t locations, size_t patterns);
  std::vector<LocationReport> fuzz_location(std::vector<HammeringPattern> &patterns, FuzzingParameterSet &params, size_t locations);
  std::vector<FuzzReport> auto_fuzz(Args args);
};
