
#include "FuzzingParameterSet.hpp"
#include "HammeringPattern.hpp"
#include <random>
class SimplePatternBuilder {
private:
  std::mt19937 engine;
public:
  void generate_pattern(HammeringPattern &pattern, FuzzingParameterSet &parameters);
  SimplePatternBuilder(size_t seed);
  SimplePatternBuilder();
};
