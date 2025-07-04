#include "AggressorAccessPattern.hpp"
#include "HammeringPattern.hpp"
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
#include "RandomPatternBuilder.hpp"

const size_t VICTIM_ROWS = 7;
const size_t MAX_DIST = 1000;

RandomPatternBuilder::RandomPatternBuilder() {
  std::random_device rd;
  engine = std::mt19937(rd());
}

size_t RandomPatternBuilder::fill_abstract_pattern(std::vector<RandomAggressor> &aggressors, size_t size) {
  int slots = size;
  size_t res = 0;
  std::uniform_real_distribution<> distance_dist(1.5, slots / 10.);
  std::uniform_int_distribution<> offset_dist(0, size * 0.75);
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
    size_t offset = offset_dist(engine);
    aggressors.push_back({
      .distance = distance,
      .id = id,
      .offset = offset > (size / 3) ? offset : 0
    });
    res++;
  } while(slots > 0);
  return res;
}

HammeringPattern RandomPatternBuilder::create_advanced_pattern(size_t max_activations) {
  HammeringPattern hammering_pattern;
  if(max_activations < 20) {
    max_activations = 20;
  }
  std::uniform_int_distribution<> slot_dist(20, max_activations);
  int slots = slot_dist(engine);
  int iterations = max_activations / slots;
  std::vector<Aggressor> full_pattern;
  std::vector<AggressorAccessPattern> patterns;
  std::set<int> seen;

  for(int i = 0; i < iterations; i++) {
    int agg_id = 0;
    std::vector<RandomAggressor> abstract_pattern;
    fill_abstract_pattern(abstract_pattern, slots);
    std::vector<bool> occupations(slots);
    std::vector<Aggressor> pattern(slots);
    for(auto agg : abstract_pattern) {
      uint16_t distance_modifier = 0;
      seen.insert(agg_id);
      for(float j = agg.offset; j < slots; j += agg.distance) {
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
        pattern[(int)j] = Aggressor(agg_id);
        occupations[(int)j] = true;
      }
      agg_id++;
    }
    full_pattern.insert(full_pattern.end(), pattern.begin(), pattern.end());
  }

  for(int i = 0; i < full_pattern.size(); i++) {
    if(full_pattern[i].id >= 0) {
      continue;
    }
    full_pattern[i].id = rand() % (seen.size() - 1);
  }

  std::vector<int> seen_ids(seen.size());
  for(int id : seen) {
    seen_ids.push_back(id);
  }

  for(int i = 0; i < seen_ids.size(); i++) {
    std::vector<Aggressor> aggressors;
    aggressors.push_back(Aggressor(seen_ids[i]));
    if(i + 1 < seen_ids.size()) {
      i++;
      aggressors.push_back(Aggressor(seen_ids[i]));
    }
    patterns.push_back(AggressorAccessPattern(100, 100, aggressors, 100));
  }

  hammering_pattern.aggressors = full_pattern;
  hammering_pattern.agg_access_patterns = patterns;

  return hammering_pattern;
}
