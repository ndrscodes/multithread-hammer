#pragma once
#include "Allocation.hpp"
#include "DRAMAddr.hpp"
#include <random>
#include <vector>
typedef std::vector<DRAMAddr> Pattern;
typedef std::pair<size_t, std::vector<DRAMAddr>> PatternPair;

const size_t MIN_ROW_OFFSET = 0;
const size_t MAX_ROW_OFFSET = 1024;
const size_t MIN_NUM_AGGRESSORS = 1;
const size_t MAX_NUM_AGGRESSORS = 100;

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
    bool address_valid(void *address);
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
};
