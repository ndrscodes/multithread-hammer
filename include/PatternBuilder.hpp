#include "Allocation.hpp"
#include "DRAMAddr.hpp"
#include <random>
#include <vector>

const size_t MIN_ROW_OFFSET = 0;
const size_t MAX_ROW_OFFSET = 1024;
const size_t MIN_NUM_AGGRESSORS = 1;
const size_t MAX_NUM_AGGRESSORS = 100;

typedef struct {
  size_t num_single_aggressors_per_bank;
  size_t num_banks;
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
    PatternBuilder(Allocation allocation);
    std::vector<DRAMAddr> create();
    std::vector<DRAMAddr> create(size_t bank);
    std::vector<DRAMAddr> create(size_t bank, size_t num_aggressors_per_bank);
    std::vector<DRAMAddr> create(PatternConfig config);
    size_t check(std::vector<DRAMAddr> aggressors);
};
