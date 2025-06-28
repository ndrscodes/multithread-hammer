
#include "HammeringPattern.hpp"
#include <random>
class SimplePatternBuilder {
private:
  std::mt19937 engine;
public:
  void generate_pattern(HammeringPattern &pattern, size_t min_sides, size_t max_sides);
  SimplePatternBuilder(size_t seed);
  SimplePatternBuilder();
};
