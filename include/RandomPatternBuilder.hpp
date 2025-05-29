#pragma once
#include "AggressorAccessPattern.hpp"
#include "Allocation.hpp"
#include "DRAMAddr.hpp"
#include "HammeringPattern.hpp"
#include <random>
#include <vector>
typedef std::vector<DRAMAddr> Pattern;
typedef std::pair<size_t, std::vector<DRAMAddr>> PatternPair;
typedef struct {
  float_t distance;
  int id;
  size_t offset;
} RandomAggressor;

const size_t MIN_ROW_OFFSET = 0;
const size_t MAX_ROW_OFFSET = 1024;
const size_t MIN_NUM_AGGRESSORS = 2;
const size_t MAX_NUM_AGGRESSORS = 50;

typedef struct {
  size_t num_single_aggressors_per_bank;
  size_t num_double_aggressors_per_bank;
  bool allow_duplicates;
  size_t root_bank;
} PatternConfig;

struct PatternContainer {
  std::vector<DRAMAddr> aggressors;
  Pattern pattern;
};

class RandomPatternBuilder {
  private:
    std::mt19937 engine;
    size_t max_slots = 1500;
    size_t fill_abstract_pattern(std::vector<RandomAggressor> &pattern, size_t slots);
  public:
    RandomPatternBuilder();
    HammeringPattern create_advanced_pattern(size_t max_activations);
};
