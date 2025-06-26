#pragma once
#include <barrier>
#include <cstdint>
#include <map>
#include <random>
#include <vector>
#include "CodeJitter.hpp"
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
  bool test_effective_patterns = false;
  uint64_t seed = 0;
  bool interleaved = false;
};

class HammerSuite {
private:
  size_t current_pattern_id;
  std::map<size_t, HammeringPattern> patterns;
  void hammer_fn(size_t id, 
                 std::vector<volatile char *> pattern, 
                 std::vector<volatile char *> non_accessed_rows, 
                 CodeJitter &jitter,
                 FuzzingParameterSet &params, 
                 std::barrier<> &start_barrier, 
                 RefreshTimer &timer);
  void check_effective_patterns(std::vector<FuzzReport> &patterns, Args &args);
  Memory &memory;
  std::mt19937 engine;
public:
  HammerSuite(Memory &memory);
  HammerSuite(Memory &memory, uint64_t seed);
  MappedPattern build_mapped(FuzzingParameterSet &params, Args &args);
  HammeringPattern generate_pattern(FuzzingParameterSet &params, Args &args);
  MappedPattern build_mapped(int bank, FuzzingParameterSet &params, Args &args);
  MappedPattern map_pattern(int bank, HammeringPattern &pattern, FuzzingParameterSet &params, Args &args);
  MappedPattern map_pattern(HammeringPattern &pattern, FuzzingParameterSet &params, Args &args);
  std::vector<FuzzReport> filter_and_analyze_flips(std::vector<FuzzReport> &patterns);
  FuzzReport fuzz(Args &args);
  LocationReport fuzz_pattern(std::vector<MappedPattern> &patterns, FuzzingParameterSet &params);
  std::vector<LocationReport> fuzz_location(std::vector<HammeringPattern> &patterns, FuzzingParameterSet &params, size_t locations);
  std::vector<LocationReport> fuzz_location(std::vector<MappedPattern> &patterns, FuzzingParameterSet &params, size_t locations);
  std::vector<FuzzReport> auto_fuzz(Args args);
};
