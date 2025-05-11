#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include "FuzzReport.hpp"
#include "HammerSuite.hpp"

struct Args {
  uint64_t runtime_limit = 3600;
  uint64_t locations = 3;
};

Args parse_args(int argc, char* argv[]) {
  Args args;
  for(int i = 1; i < argc; i++) {
    if((strncmp("-r", argv[i], 2) || strncmp("--runtime", argv[i], 9)) && i + 1 < argc) {
      args.runtime_limit = atoi(argv[i + 1]);
      i++;
    } else if((strncmp("-l", argv[i], 2) || strncmp("--locations", argv[i], 11)) && i + 1 < argc) {
      args.locations = atoi(argv[i + 1]);
      i++;
    }
  }

  return args;
}

int main(int argc, char* argv[]) {
  DRAMConfig::select_config(Microarchitecture::AMD_ZEN_3, 1, 4, 4, false);

  Args args = parse_args(argc, argv);

  Allocation alloc;
  printf("creating allocation...\n");
  alloc.allocate();
  printf("allocated %lu bytes of memory.\n", alloc.get_size());
  alloc.initialize(init_pattern::FENCE);
  printf("initialized memory with strategy FENCE.\n");
  DRAMAddr::initialize_mapping(0, (volatile char *)alloc.get_start_address());

  PatternBuilder builder(alloc);

  HammerSuite suite(builder);
  printf("initialized runtime parameter to %lu.\n", args.runtime_limit);
  printf("initialized location parameter to %lu.\n", args.locations);
  printf("starting hammering run!\n");
  std::vector<FuzzReport> reports = suite.auto_fuzz(args.locations, args.runtime_limit);
}
