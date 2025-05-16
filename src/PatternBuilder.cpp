#include "PatternBuilder.hpp"
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <set>
#include <vector>

const size_t VICTIM_ROWS = 7;
const size_t MAX_DIST = 1000;

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
  auto area = std::uniform_int_distribution<>(0, 4);
  size_t area_size = DRAMConfig::get().rows() / 5;
  auto row_offset = std::uniform_int_distribution<>(0, 100);
  uint16_t retries = 0;
  do {
    addr = DRAMAddr(bank, area(engine) * area_size + row_offset(engine), 0);
    assert(++retries < 512);
  } while(!address_valid(addr.to_virt()));
  return addr;
}

PatternConfig PatternBuilder::create_random_config(size_t bank) {
  size_t aggressors = agg_count_dist(engine);
  PatternConfig config = {
    .num_single_aggressors_per_bank = (size_t)(aggressors * 0.25),
    .num_double_aggressors_per_bank = (size_t)(aggressors * 0.75),
    .allow_duplicates = true,
    .root_bank = bank
  };
  return config;
}

size_t PatternBuilder::fill_abstract_pattern(std::vector<Aggressor> &aggressors) {
  std::uniform_int_distribution<> slot_dist(2, MAX_DIST);
  int slots = slot_dist(engine);
  size_t res = (size_t) slots;
  std::uniform_real_distribution<> distance_dist(1, slots / 2.);
  do {
    float_t distance = distance_dist(engine);
    slots -= slots / (distance + 1);
    aggressors.push_back({
      .distance = distance,
    });
  } while(slots > 0);
  return res;
}

Pattern PatternBuilder::create_advanced_pattern(size_t bank) {
  std::vector<Aggressor> abstract_pattern;
  size_t slots = fill_abstract_pattern(abstract_pattern);
  Pattern pattern(slots);
  std::vector<bool> occupations(slots);
  size_t start = 0;
  for(auto agg : abstract_pattern) {
    
    DRAMAddr aggressor;
    if(start % 2 == 1) {
      aggressor = pattern[start - 1];
      aggressor.add_inplace(0, 2, 0);
      if(!address_valid(aggressor.to_virt())) {
        aggressor = get_random_address(bank);
      }
    } else {
      aggressor = get_random_address(bank);
    }

    uint16_t distance_modifier = 0;
    for(float i = start; i < slots; i += agg.distance) {
      if(occupations[(int)i]) {
        //increases the frequency if we were not able to place the aggressor in the pattern in any iteration
        if(agg.distance - 1 > 1) {
          agg.distance--;
        }
        distance_modifier++;
        //this places us to the adjacent slot in the next iteration
        i = i - agg.distance + 1;
        continue;
      }
      //slowly restore the frequency if we were able to place
      if(distance_modifier > 0) {
        agg.distance++;
        distance_modifier--;
      }
      pattern[(int)i] = aggressor;
      occupations[(int)i] = true;
    }
    
    start++;
  }
  for(int i = 0; i < slots; i++) {
    if(occupations[i]) {
      continue;
    }
    pattern[i] = get_random_address(bank);
  }

  return pattern;
}

std::vector<DRAMAddr> PatternBuilder::create() {
  return create(create_random_config(bank_offset_dist(engine)));
}

std::vector<DRAMAddr> PatternBuilder::create(size_t bank) {
  return create(create_random_config(bank));
}

std::vector<DRAMAddr> PatternBuilder::create(size_t bank, size_t num_aggressors_per_bank) {
  PatternConfig config = create_random_config(bank);
  config.num_single_aggressors_per_bank = num_aggressors_per_bank * 0.25;
  config.num_double_aggressors_per_bank = num_aggressors_per_bank * 0.75;
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

  std::shuffle(addresses.begin(), addresses.end(), engine); 

  return addresses;
}

std::vector<Pattern> PatternBuilder::create_multiple_banks(size_t banks) {
  std::vector<Pattern> patterns(banks);
  size_t bank_start = bank_offset_dist(engine);
  for(size_t i = 0; i < banks; i++) {
    patterns[i] = create_advanced_pattern((bank_start + i) % DRAMConfig::get().banks());
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
  for(size_t i = 0; i < pattern.size(); i++) {
    DRAMAddr candidate = pattern[i].add(bank_offset, row_offset, 0);
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

size_t PatternBuilder::full_alloc_check() {
  return allocation.find_flips((void *)allocation.get_start_address(), (void *)allocation.get_end_address());
}

size_t PatternBuilder::check(std::vector<DRAMAddr> aggressors) {
  size_t flips = 0;

  std::set<void *> checked_addrs;

  for(auto aggressor : aggressors) {
    for(int i = -(int)VICTIM_ROWS; i < (int)VICTIM_ROWS; i++) {
      if(i == 0) {
        continue;
      }
      DRAMAddr victim = DRAMAddr(aggressor.bank, aggressor.row + i, 0);
      char *victim_addr = (char *)victim.to_virt();
      if(!allocation.is_valid(victim_addr)) {
        printf("warning: victim %s is invalid.\n", victim.to_string().c_str());
        continue;
      }
      if(checked_addrs.contains(victim_addr)) {
        continue;
      }

      flips += allocation.find_flips(victim_addr);
      checked_addrs.insert(victim_addr);
    }
  }

  return flips;
}

