#include <cstdio>
#include <cstdlib>
#include "HammerSuite.hpp"

int main(int argc, char* argv[]) {
  printf("sdfksdkfhsdkfs");
  Allocation alloc;
  printf("creating allocation...\n");
  alloc.allocate();
  printf("allocated %lu bytes of memory.\n", alloc.get_size());
  alloc.initialize(init_pattern::FENCE);
  printf("initialized memory with strategy FENCE.\n");
  PatternBuilder builder(alloc);
  PatternConfig config {
    .num_single_aggressors_per_bank = 0,
    .num_banks = 1,
    .num_double_aggressors_per_bank = 2,
    .allow_duplicates = true,
    .root_bank = 0,
  };

  Pattern pattern = builder.create(config);

  HammerSuite suite(builder);
  size_t flips = suite.hammer(50000);

  printf("flipped %lu bits.\n", flips);
}
