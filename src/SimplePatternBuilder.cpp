#include "FuzzingParameterSet.hpp"
#include "HammeringPattern.hpp"
#include "asmjit/core/codeholder.h"
#include <random>
#include "SimplePatternBuilder.hpp"

std::mt19937 SimplePatternBuilder::engine = std::mt19937(std::random_device()());
SimplePatternBuilder::SimplePatternBuilder() {
}

void SimplePatternBuilder::set_seed(uint64_t seed) {
  engine = std::mt19937(seed);
}

void SimplePatternBuilder::generate_pattern(HammeringPattern &pattern, FuzzingParameterSet &params) {
  size_t current_aggressor_id = 1;
  int target_length = params.get_total_acts_pattern();

  std::uniform_int_distribution<> amplitude_dist(2, target_length / 2);

  int current_length = 0;
  pattern.aggressors = std::vector<Aggressor>(target_length, Aggressor());
  while(current_length < target_length) {
    int inner_length = amplitude_dist(engine);
    int num_aggs = params.get_random_N_sided();
    std::vector<Aggressor> aggressors(num_aggs);
    for(int i = 0; i < aggressors.size(); i++) {
      aggressors[i] = Aggressor(current_aggressor_id++);
    }
    AggressorAccessPattern next_pattern;
    next_pattern.aggressors = aggressors;
    next_pattern.start_offset = current_length;
    next_pattern.frequency = target_length;
    next_pattern.amplitude = inner_length / num_aggs;
    pattern.agg_access_patterns.push_back(next_pattern);

    while(inner_length > 0 && current_length < target_length) {
      pattern.aggressors[current_length++] = aggressors[inner_length-- % num_aggs];
    }
  }
}

void SimplePatternBuilder::generate_pattern(HammeringPattern &pattern, FuzzingParameterSet &params, int num_aggressors) {
  if(num_aggressors == -1) {
    return generate_pattern(pattern, params);
  }

  size_t current_aggressor_id = 1;
  int target_length = params.get_total_acts_pattern();

  int current_length = 0;
  pattern.aggressors = std::vector<Aggressor>(target_length);
  std::vector<Aggressor> aggressors(num_aggressors);
  for(int i = 0; i < aggressors.size(); i++) {
    aggressors[i] = Aggressor(current_aggressor_id++);
  }
  AggressorAccessPattern next_pattern;
  next_pattern.aggressors = aggressors;
  next_pattern.start_offset = 0;
  next_pattern.frequency = target_length;
  next_pattern.amplitude = target_length;
  pattern.agg_access_patterns.push_back(next_pattern);

  int agg_index = 0;

  while(current_length < target_length) {
    pattern.aggressors[current_length++] = aggressors[agg_index++ % num_aggressors];
  }
}
