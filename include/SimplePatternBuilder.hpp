
#include "FuzzingParameterSet.hpp"
#include "HammeringPattern.hpp"
#include <cstdint>
#include <random>
class SimplePatternBuilder {
private:
  static std::mt19937 engine;
public:
  void generate_pattern(HammeringPattern &pattern, FuzzingParameterSet &parameters);
  SimplePatternBuilder();
  static void set_seed(uint64_t seed);
};
