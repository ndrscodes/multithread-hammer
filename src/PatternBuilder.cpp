#include "PatternBuilder.hpp"
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include "RandomData.hpp"
#include <cassert>
#include <cstring>
#include <random>
#include <set>
#include <vector>

const size_t VICTIM_ROWS = 5;

PatternBuilder::PatternBuilder(Allocation &allocation) : allocation(allocation) {
  engine = std::mt19937();
  row_offset_dist = std::uniform_int_distribution<>(MIN_ROW_OFFSET, MAX_ROW_OFFSET);
  bank_offset_dist = std::uniform_int_distribution<>(0, DRAMConfig::get().banks());
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

PatternConfig PatternBuilder::create_random_config(size_t bank) {
  size_t aggressors = agg_count_dist(engine);
  PatternConfig config = {
    .num_single_aggressors_per_bank = (size_t)(aggressors * 0.5),
    .num_double_aggressors_per_bank = (size_t)(aggressors * 0.5),
    .allow_duplicates = get_bool(),
    .root_bank = bank
  };
  return config;
}

std::vector<DRAMAddr> PatternBuilder::create() {
  return create(create_random_config(bank_offset_dist(engine)));
}

std::vector<DRAMAddr> PatternBuilder::create(size_t bank) {
  return create(create_random_config(bank));
}

std::vector<DRAMAddr> PatternBuilder::create(size_t bank, size_t num_aggressors_per_bank) {
  PatternConfig config = create_random_config(bank);
  config.num_single_aggressors_per_bank = num_aggressors_per_bank * 0.75;
  config.num_double_aggressors_per_bank = num_aggressors_per_bank * 0.25;
  return create(config);
}

std::vector<DRAMAddr> PatternBuilder::create(PatternConfig config) {
  std::vector<DRAMAddr> addresses;

  std::set<void *> checked_addrs;
  DRAMAddr start_dram_addr((void *)allocation.get_start_address());
  int i = 0;
  while(i < config.num_single_aggressors_per_bank + config.num_double_aggressors_per_bank) {
    DRAMAddr address = get_random_address(config.root_bank);
    void *address_virt = address.to_virt();

    if(!config.allow_duplicates) {
      if(checked_addrs.contains(address_virt)) {
        continue;
      }
      checked_addrs.insert(address_virt);
    }

    if(i < config.num_double_aggressors_per_bank) {
      DRAMAddr lower_address = address.add(0, 2, 0);
      void *lower_virt = lower_address.to_virt();
    
      //check if we wrapped around to the other "side"
      if(address.actual_row() > lower_address.actual_row()) {
        continue;
      }

      //check if both aggressors are valid at all
      if(!address_valid(lower_virt)) {
        continue;
      }
    
      if(!config.allow_duplicates) {
        if(checked_addrs.contains(lower_virt)) {
          continue;
        }
        checked_addrs.insert(lower_virt);
      }

      addresses.push_back(lower_address);
    }

    addresses.push_back(address);
    i++;
  }

  return addresses;
}

std::vector<Pattern> PatternBuilder::create_multiple_banks(size_t banks) {
  std::vector<Pattern> patterns(banks);
  size_t bank_start = bank_offset_dist(engine);
  for(size_t i = 0; i < banks; i++) {
    patterns[i] = create((bank_start + i) % DRAMConfig::get().banks());
  }
  return patterns;
}

std::vector<Pattern> PatternBuilder::create_multiple_banks(size_t banks, PatternConfig config) {
  std::vector<Pattern> patterns(banks);
  for(size_t i = 0; i < banks; i++) {
    config.root_bank = (config.root_bank + i) % DRAMConfig::get().banks();
    patterns[i] = create(config);
  }
  return patterns;
}

Pattern PatternBuilder::translate(Pattern pattern, size_t bank_offset, size_t row_offset) {
  Pattern offset_pattern(pattern.size());
  for(size_t i = 0; i < offset_pattern.size(); i++) {
    DRAMAddr candidate = offset_pattern[i].add(bank_offset, row_offset, 0);
    if(!address_valid(candidate.to_virt())) {
      i--;
      //we expect at least some space to be reseved on each bank.
      //we increment the row offset until we find a valid space within the new bank.
      //since all rows are moved by this fixed offset, the pattern still remains the same.
      row_offset += 32;
      continue;
    }
    offset_pattern[i] = candidate;
  }

  return offset_pattern;
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
          for(int k = 0; k < 8; k++) {
            char mask = 1 << k;
            char v = *(((char *)victim_addr) + j);
            if((v & mask) != (expected_value & mask)) { 
              flips++;
            }
          }
        }
      }
    }
  }

  free(compare_data);

  return flips;
}

