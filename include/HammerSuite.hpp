#pragma once
#include <barrier>
#include <chrono>
#include <cstdint>
#include <map>
#include <random>
#include <vector>
#include "CodeJitter.hpp"
#include "Enums.hpp"
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
  bool test_effective_patterns_random = false;
  bool test_effective_patterns_combined = false;
  uint64_t seed = 0;
  bool interleaved = false;
  bool simple_patterns_first_thread = false;
  bool simple_patterns_other_threads = false;
  bool disable_fencing = false;
  bool interleave_single_pair_only = false;
  size_t interleaving_distance = 3;
  bool randomize_each_pattern = false;
  FENCING_STRATEGY fencing_strategy = FENCING_STRATEGY::EARLIEST_POSSIBLE;
  FLUSHING_STRATEGY flushing_strategy = FLUSHING_STRATEGY::EARLIEST_POSSIBLE;
  size_t thread_start_id = 0;
};

class HammerSuite {
private:
  size_t current_pattern_id;
  std::map<size_t, HammeringPattern> patterns;
  void hammer_fn(size_t id, 
                 std::vector<volatile char *> &pattern, 
                 std::vector<volatile char *> &non_accessed_rows, 
                 CodeJitter &jitter,
                 FuzzingParameterSet &params, 
                 std::barrier<> &start_barrier, 
                 RefreshTimer &timer,
                 std::chrono::time_point<std::chrono::steady_clock> &start,
                 std::chrono::time_point<std::chrono::steady_clock> &end);
  void check_effective_patterns(std::vector<FuzzReport> &patterns, Args &args);
  Memory &memory;
  static std::mt19937 engine;
public:
  HammerSuite(Memory &memory);
  static void set_seed(uint64_t seed);
  MappedPattern build_mapped(FuzzingParameterSet &params, bool simple);
  HammeringPattern generate_pattern(FuzzingParameterSet &params, bool simple);
  MappedPattern build_mapped(int bank, FuzzingParameterSet &params, bool simple);
  MappedPattern map_pattern(int bank, HammeringPattern &pattern, FuzzingParameterSet &params, bool simple);
  MappedPattern map_pattern(HammeringPattern &pattern, FuzzingParameterSet &params, bool simple);
  std::vector<FuzzReport> filter_and_analyze_flips(std::vector<FuzzReport> &patterns, std::string &filepath);
  FuzzReport fuzz(Args &args);
  LocationReport fuzz_pattern(std::vector<MappedPattern> &patterns, Args &args);
  std::vector<LocationReport> fuzz_location(std::vector<HammeringPattern> &patterns, size_t locations, Args &args);
  std::vector<LocationReport> fuzz_location(std::vector<MappedPattern> &patterns, size_t locations, Args &args);
  std::vector<FuzzReport> auto_fuzz(Args args);
};
