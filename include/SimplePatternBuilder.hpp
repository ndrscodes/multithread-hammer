
#include "FuzzingParameterSet.hpp"
#include "HammeringPattern.hpp"
#include <cstdint>
#include <random>
class SimplePatternBuilder {
private:
  static std::mt19937 engine;
public:
  void generate_pattern(HammeringPattern &pattern, FuzzingParameterSet &parameters);
  void generate_pattern(HammeringPattern &pattern, FuzzingParameterSet &params, int num_aggressors);
  SimplePatternBuilder();
  static void set_seed(uint64_t seed);
};
