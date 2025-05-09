#include <cstdio>
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include "HammerSuite.hpp"

int main(int argc, char* argv[]) {
  DRAMConfig::select_config(Microarchitecture::AMD_ZEN_3, 1, 4, 4, false);

  Allocation alloc;
  printf("creating allocation...\n");
  alloc.allocate();
  printf("allocated %lu bytes of memory.\n", alloc.get_size());
  alloc.initialize(init_pattern::FENCE);
  printf("initialized memory with strategy FENCE.\n");
  DRAMAddr::initialize_mapping(0, (volatile char *)alloc.get_start_address());

  PatternBuilder builder(alloc);
  PatternConfig config {
    .num_single_aggressors_per_bank = 0,
    .num_double_aggressors_per_bank = 2,
    .allow_duplicates = true,
    .root_bank = 0,
  };

  Pattern pattern = builder.create(config);

  HammerSuite suite(builder);
  printf("starting hammering run!\n");
  suite.auto_fuzz(3, 3600);
}
