#include <barrier>
#include <initializer_list>
#include <map>
#include <vector>
#include "DRAMAddr.hpp"
#include "PatternBuilder.hpp"
typedef std::vector<DRAMAddr> Pattern;
typedef std::pair<size_t, std::vector<DRAMAddr>> PatternPair;

class HammerSuite {
private:
  size_t current_pattern_id;
  std::map<size_t, Pattern> patterns;
  void hammer_fn(size_t id, Pattern &pattern, size_t iterations, std::barrier<> &start_barrier);
  PatternBuilder &builder;
public:
  HammerSuite(PatternBuilder &builder);
  HammerSuite(PatternBuilder &alloc, std::initializer_list<Pattern> patterns);
  size_t add_pattern(Pattern pattern);
  size_t remove_pattern(size_t id);
  size_t hammer(size_t iterations);
};
