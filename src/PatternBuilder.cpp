#include "PatternBuilder.hpp"
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include "RandomData.hpp"
#include <cassert>
#include <cstring>
#include <random>
#include <set>
#include <vector>

const size_t MIN_NUM_BANKS = 1;
const size_t VICTIM_ROWS = 5;

PatternBuilder::PatternBuilder(Allocation &allocation) : allocation(allocation) {
  engine = std::mt19937();
  row_offset_dist = std::uniform_int_distribution<>(MIN_ROW_OFFSET, MAX_ROW_OFFSET);
  bank_offset_dist = std::uniform_int_distribution<>(0, DRAMConfig::get().banks() - 1);
  agg_count_dist = std::uniform_int_distribution<>(MIN_NUM_AGGRESSORS, MAX_NUM_AGGRESSORS);
}

bool PatternBuilder::address_valid(void *address) {
  return allocation.is_valid(address);
}

DRAMAddr PatternBuilder::get_random_address() {
  return get_random_address(0);
}

DRAMAddr PatternBuilder::get_random_address(size_t bank) {
  DRAMAddr addr;
  uint16_t retries = 0;
  do {
    addr = DRAMAddr(bank, row_offset_dist(engine), 0);
    assert(++retries < 512);
  } while(!address_valid(addr.to_virt()));
  return addr;
}

std::vector<DRAMAddr> PatternBuilder::create() {
  std::uniform_int_distribution<> bank_num_dist(MIN_NUM_BANKS, DRAMConfig::get().banks());
  size_t single_aggressors = agg_count_dist(engine);
  size_t double_aggressors = agg_count_dist(engine);
  PatternConfig config = {
    .num_single_aggressors_per_bank = single_aggressors,
    .num_banks = (size_t)bank_num_dist(engine),
    .num_double_aggressors_per_bank = double_aggressors,
    .allow_duplicates = get_bool(),
    .root_bank = (size_t)bank_offset_dist(engine)
  };
  return create(config);
}

std::vector<DRAMAddr> PatternBuilder::create(size_t bank) {
  return create(bank, agg_count_dist(engine));
}

std::vector<DRAMAddr> PatternBuilder::create(size_t bank, size_t num_aggressors_per_bank) {
  PatternConfig config = {
    .num_single_aggressors_per_bank = num_aggressors_per_bank,
    .num_banks = 1,
    .num_double_aggressors_per_bank = 0,
    .allow_duplicates = get_bool(),
    .root_bank = bank,
  };
  return create(config);
}

std::vector<DRAMAddr> PatternBuilder::create(PatternConfig config) {
  std::set<size_t> used_banks;
  std::vector<DRAMAddr> addresses;
  used_banks.insert(config.root_bank);
  do {
    used_banks.insert((config.root_bank + bank_offset_dist(engine)) % DRAMConfig::get().banks());
  } while(used_banks.size() < config.num_banks);

  std::set<void *> checked_addrs;
  DRAMAddr start_dram_addr((void *)allocation.get_start_address());
  for(auto bank : used_banks) {
    int i = 0;
    while(i < config.num_single_aggressors_per_bank) {
      DRAMAddr address = get_random_address(bank);
      void *address_virt = address.to_virt();

      if(!address_valid(address_virt)) {
        continue;
      }

      if(!config.allow_duplicates) {
        if(checked_addrs.contains(address_virt)) {
          continue;
        }
        checked_addrs.insert(address_virt);
      }

      addresses.push_back(address);
      i++;
    }

    i = 0;
    while(i < config.num_double_aggressors_per_bank) {
      DRAMAddr upper_address = get_random_address(bank);
      void *upper_virt = upper_address.to_virt();
      DRAMAddr lower_address = upper_address.add(0, 2, 0);
      void *lower_virt = lower_address.to_virt();
      
      //check if we wrapped around to the other "side"
      if(upper_address.actual_row() > lower_address.actual_row()) {
        continue;
      }

      //check if both aggressors are valid at all
      if(!address_valid(upper_virt) || !address_valid(lower_virt)) {
        continue;
      }

      if(!config.allow_duplicates) {
        if(checked_addrs.contains(lower_virt) || checked_addrs.contains(upper_virt)) {
          continue;
        }
        checked_addrs.insert(lower_virt);
        checked_addrs.insert(upper_virt);
      }

      addresses.push_back(lower_address);
      addresses.push_back(upper_address);
      i++;
    }
  }

  return addresses;
}

size_t PatternBuilder::check(std::vector<DRAMAddr> aggressors) {
  size_t s = DRAMConfig::get().row_to_row_offset();
  void* compare_data = malloc(sizeof(char) * s);
  init_pattern pattern = allocation.get_init_pattern();
  char expected_value = allocation.get_fill_value(pattern); 

  size_t flips = 0;

  Allocation::initialize(pattern, compare_data, sizeof(char) * s);

  std::set<void *> checked_addrs;

  for(auto aggressor : aggressors) {
    for(int i = -(int)VICTIM_ROWS; i < VICTIM_ROWS; i++) {
      if(i == 0) {
        continue;
      }
      void *victim_addr = DRAMAddr(aggressor.bank, aggressor.row - i, 0).to_virt();
      if(!allocation.is_valid(victim_addr) || checked_addrs.contains(victim_addr)) {
        continue;
      }
      
      checked_addrs.insert(victim_addr);

      //if the entire row matches, there is no need to do further analysis for this victim.
      if(memcmp(allocation.get_start_address(), compare_data, s) != 0) {
        for(size_t j = 0; j < s; j++) {
          char v = *(((char *)victim_addr) + j);
          if(v != expected_value) {
            flips++;
          }
        }
      }
    }
  }

  free(compare_data);

  return flips;
}

