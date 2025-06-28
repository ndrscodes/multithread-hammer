#include "HammeringPattern.hpp"
#include "asmjit/core/codeholder.h"
#include <random>
#include "SimplePatternBuilder.hpp"

SimplePatternBuilder::SimplePatternBuilder() {
  engine = std::mt19937(std::random_device()());
}

SimplePatternBuilder::SimplePatternBuilder(size_t seed) {
  engine = std::mt19937(seed);
}

void SimplePatternBuilder::generate_pattern(HammeringPattern &pattern, size_t min_sides, size_t max_sides) {
  size_t current_aggressor_id = 1;
  std::uniform_int_distribution<> length_dist(min_sides + 1, 1024);
  int target_length = length_dist(engine);
  pattern.base_period = target_length / (min_sides < 16 ? min_sides : 16);
  std::uniform_int_distribution<> inner_length_dist(min_sides, target_length / 2);
  std::uniform_int_distribution<> n_sided_count_dist(min_sides, max_sides);

  int current_length = 0;
  pattern.aggressors = std::vector<Aggressor>(target_length, Aggressor());
  while(current_length < target_length) {
    int inner_length = length_dist(engine);
    int num_aggs = n_sided_count_dist(engine);
    std::vector<Aggressor> aggressors(num_aggs);
    for(int i = 0; i < aggressors.size(); i++) {
      aggressors[i] = Aggressor(current_aggressor_id++);
    }
    AggressorAccessPattern next_pattern;
    next_pattern.aggressors = aggressors;
    next_pattern.start_offset = current_length;
    next_pattern.frequency = 1;
    pattern.agg_access_patterns.push_back(next_pattern);

    while(inner_length > 0) {
      pattern.aggressors[current_length++] = aggressors[inner_length-- % num_aggs];
    }
  }
}
