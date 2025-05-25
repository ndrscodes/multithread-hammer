#include "PatternBuilder.hpp"
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <vector>

const size_t VICTIM_ROWS = 7;
const size_t MAX_DIST = 1000;

PatternBuilder::PatternBuilder(Allocation &allocation) : allocation(allocation) {
  std::random_device rd;
  engine = std::mt19937(rd());
  row_offset_dist = std::uniform_int_distribution<>(MIN_ROW_OFFSET, MAX_ROW_OFFSET);
  bank_offset_dist = std::uniform_int_distribution<>(0, DRAMConfig::get().banks());
  agg_count_dist = std::uniform_int_distribution<>(MIN_NUM_AGGRESSORS, MAX_NUM_AGGRESSORS);
}

bool PatternBuilder::address_valid(void *address) {
  return allocation.is_valid(address);
}

bool PatternBuilder::address_valid(DRAMAddr address) {
  return address_valid(address.to_virt());
}

DRAMAddr PatternBuilder::get_random_address() {
  return get_random_address(bank_offset_dist(engine));
}

DRAMAddr PatternBuilder::get_random_address(size_t bank) {
  DRAMAddr addr;
  auto row_offset = std::uniform_int_distribution<>(0, DRAMConfig::get().rows());
  uint16_t retries = 0;
  do {
    addr = DRAMAddr(bank, row_offset(engine), 0);
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

size_t PatternBuilder::get_max_pattern_length() {
  return max_pattern_length;
}

void PatternBuilder::set_max_pattern_length(size_t length) {
  max_pattern_length = length;
}

size_t PatternBuilder::fill_abstract_pattern(std::vector<Aggressor> &aggressors, size_t size) {
  int slots = size;
  size_t res = (size_t) slots;
  std::uniform_real_distribution<> distance_dist(1.5, slots / 10.);
  std::vector<int> ids(slots / 10);
  for(int i = 0; i < ids.size(); i++) {
    ids[i] = i;
  }
  std::shuffle(ids.begin(), ids.end(), engine);

  int i = 0;
  do {
    float_t distance = distance_dist(engine);
    slots -= slots / (distance);
    if(slots < 0 || i >= ids.size()) {
      break;
    }
    int id = ids[i++];
    aggressors.push_back({
      .distance = distance,
      .id = id
    });
  } while(slots > 0);
  return res;
}

PatternContainer PatternBuilder::map_to_aggrs(size_t bank, std::vector<int> &abstract_pattern) {
  Pattern pattern(abstract_pattern.size());
  std::map<int, DRAMAddr> id_to_addr_map;
  id_to_addr_map[-1] = DRAMAddr(0, 0, DRAMConfig::get().columns()); //this identifies an unused slot
  for(size_t i = 0; i < abstract_pattern.size(); i++) {
    int aggr_id = abstract_pattern[i];
    if(aggr_id % 4 == 1 && aggr_id != -1) {
      DRAMAddr aggressor = id_to_addr_map[aggr_id - 1].add(0, 2, 0);
      if(address_valid(aggressor)) {
        id_to_addr_map[aggr_id] = aggressor;
      }
    }
    if(id_to_addr_map.contains(aggr_id)) {
      pattern[i] = id_to_addr_map[aggr_id];
    } else {
      DRAMAddr addr = get_random_address(bank);
      id_to_addr_map[aggr_id] = addr;
      pattern[i] = addr;
    }
  }

  std::vector<DRAMAddr> addrs(id_to_addr_map.size());
  for(auto pair : id_to_addr_map) {
    addrs.push_back(pair.second);
  }

  return {
    .aggressors = addrs,
    .pattern = pattern
  };
}

PatternContainer PatternBuilder::create_advanced_pattern(size_t bank, size_t max_activations) {
  std::uniform_int_distribution<> slot_dist(20, max_slots);
  int slots = slot_dist(engine);
  int iterations = max_activations / slots;
  std::vector<int> full_pattern;
  for(int i = 0; i < iterations; i++) {
    size_t start = 0;
    std::vector<Aggressor> abstract_pattern;
    fill_abstract_pattern(abstract_pattern, slots);
    std::vector<int> pattern(slots, -1);
    std::vector<bool> occupations(slots);

    for(auto agg : abstract_pattern) {
      uint16_t distance_modifier = 0;
      for(float j = start; j < slots; j += agg.distance) {
        if(occupations[(int)j]) {
          //increases the frequency if we were not able to place the aggressor in the pattern in any iteration
          if(agg.distance - 1 > 1) {
            agg.distance--;
          }
          distance_modifier++;
          //this places us to the adjacent slot in the next iteration
          j = j - agg.distance + 1;
          continue;
        }
        //slowly restore the frequency if we were able to place
        if(distance_modifier > 0) {
          agg.distance++;
          distance_modifier--;
        }
        pattern[(int)j] = agg.id;
        occupations[(int)j] = true;
      }
      
      start++;
    }
    full_pattern.insert(full_pattern.end(), pattern.begin(), pattern.end());
  }

  PatternContainer mapped_pattern = map_to_aggrs(bank, full_pattern); 
  printf("\ncreated pattern with %lu slots and %lu aggressors.\n", max_activations, full_pattern.size());

  return mapped_pattern;
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

std::vector<PatternContainer> PatternBuilder::create_multiple_banks(size_t banks) {
  std::vector<PatternContainer> patterns(banks);
  size_t bank_start = bank_offset_dist(engine);
  for(size_t i = 0; i < banks; i++) {
    patterns[i] = create_advanced_pattern((bank_start + i) % DRAMConfig::get().banks(), max_pattern_length);
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

PatternContainer PatternBuilder::translate(PatternContainer pattern, size_t bank_offset, size_t row_offset) {
  Pattern offset_pattern(pattern.pattern.size());
  std::vector<DRAMAddr> aggressors(pattern.aggressors.size());
  for(size_t i = 0; i < pattern.pattern.size(); i++) {
    DRAMAddr candidate = pattern.pattern[i].add(bank_offset, row_offset, 0);
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

  for(size_t i = 0; i < pattern.aggressors.size(); i++) {
    DRAMAddr candidate = pattern.aggressors[i].add(bank_offset, row_offset, 0);
    if(!address_valid(candidate.to_virt())) {
      i--;
      //we expect at least some space to be reseved on each bank.
      //we increment the row offset until we find a valid space within the new bank.
      //since all rows are moved by this fixed offset, the pattern still remains the same.
      row_offset += 32;
      continue;
    }
    aggressors[i] = candidate;
  }

  return {
    .aggressors = aggressors,
    .pattern = offset_pattern
  };
}

size_t PatternBuilder::full_alloc_check() {
  return allocation.find_flips((void *)allocation.get_start_address(), (void *)allocation.get_end_address());
}

size_t PatternBuilder::check(std::vector<DRAMAddr> aggressors) {
  size_t flips = 0;

  std::set<size_t> checked_rows;

  for(auto aggressor : aggressors) {
    for(int i = -(int)VICTIM_ROWS; i < (int)VICTIM_ROWS; i++) {
      if(i == 0) {
        continue;
      }
      DRAMAddr victim = DRAMAddr(aggressor.bank, aggressor.row + i, 0);
      if(checked_rows.contains(victim.row)) {
        continue;
      }
      
      char *victim_addr = (char *)victim.to_virt();
      if(!allocation.is_valid(victim_addr)) {
        printf("warning: victim %s is invalid.\n", victim.to_string().c_str());
        continue;
      }

      flips += allocation.find_flips(victim_addr);
      checked_rows.insert(victim.row);
    }
  }

  printf("checked %lu distinct addresses for flips.\n", checked_rows.size());

  return flips;
}

