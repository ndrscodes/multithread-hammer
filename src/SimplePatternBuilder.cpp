#include "FuzzingParameterSet.hpp"
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

void SimplePatternBuilder::generate_pattern(HammeringPattern &pattern, FuzzingParameterSet &params) {
  size_t current_aggressor_id = 1;
  int target_length = params.get_total_acts_pattern();

  int current_length = 0;
  pattern.aggressors = std::vector<Aggressor>(target_length, Aggressor());
  while(current_length < target_length) {
    int inner_length = params.get_random_amplitude(target_length / 2);
    int num_aggs = params.get_random_N_sided();
    std::vector<Aggressor> aggressors(num_aggs);
    for(int i = 0; i < aggressors.size(); i++) {
      aggressors[i] = Aggressor(current_aggressor_id++);
    }
    AggressorAccessPattern next_pattern;
    next_pattern.aggressors = aggressors;
    next_pattern.start_offset = current_length;
    next_pattern.frequency = target_length;
    pattern.agg_access_patterns.push_back(next_pattern);

    while(inner_length > 0 && current_length < target_length) {
      pattern.aggressors[current_length++] = aggressors[inner_length-- % num_aggs];
    }
  }
}
