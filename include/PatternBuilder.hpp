#pragma once
#include "Allocation.hpp"
#include "DRAMAddr.hpp"
#include <random>
#include <vector>
typedef std::vector<DRAMAddr> Pattern;
typedef std::pair<size_t, std::vector<DRAMAddr>> PatternPair;
typedef struct {
  float_t distance;
  int id;
} Aggressor;

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

class PatternBuilder {
  private:
    Allocation allocation;
    std::mt19937 engine;
    std::uniform_int_distribution<> row_offset_dist;
    std::uniform_int_distribution<> bank_offset_dist;
    std::uniform_int_distribution<> agg_count_dist;
    size_t max_slots = 256;
    size_t max_pattern_length = 5000000;
    bool address_valid(void *address);
    size_t fill_abstract_pattern(std::vector<Aggressor> &pattern, size_t slots);
    Pattern map_to_aggrs(size_t bank, std::vector<int> &abstract_pattern);
  public:
    PatternBuilder(Allocation &allocation);
    Pattern create();
    Pattern create(size_t bank);
    Pattern create(size_t bank, size_t num_aggressors_per_bank);
    Pattern create(PatternConfig config);
    Pattern translate(Pattern pattern, size_t bank_offset, size_t row_offset);
    std::vector<Pattern> create_multiple_banks(size_t banks);
    std::vector<Pattern> create_multiple_banks(size_t banks, PatternConfig config);
    PatternConfig create_random_config(size_t bank);
    DRAMAddr get_random_address(size_t bank);
    DRAMAddr get_random_address();
    size_t check(std::vector<DRAMAddr> aggressors);
    size_t full_alloc_check();
    Pattern create_advanced_pattern(size_t bank, size_t max_activations);
    size_t get_max_pattern_length();
    void set_max_pattern_length(size_t length);
    bool address_valid(DRAMAddr address);
};
