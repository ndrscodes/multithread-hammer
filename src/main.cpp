#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include "GlobalDefines.hpp"
#include "HammerSuite.hpp"
#include "Memory.hpp"
#include <sys/resource.h>

struct Args {
  uint64_t runtime_limit = 3600;
  uint64_t locations = 3;
};

Args parse_args(int argc, char* argv[]) {
  Args args;
  for(int i = 1; i < argc; i++) {
    if((strncmp("-r", argv[i], 2) == 0 || strncmp("--runtime", argv[i], 9) == 0) && i + 1 < argc) {
      args.runtime_limit = atoi(argv[i + 1]);
      i++;
    } else if((strncmp("-l", argv[i], 2) == 0 || strncmp("--locations", argv[i], 11) == 0) && i + 1 < argc) {
      args.locations = atoi(argv[i + 1]);
      i++;
    }
  }

  return args;
}

int main(int argc, char* argv[]) {
  // give this process the highest CPU priority so it can hammer with less interruptions
  int ret = setpriority(PRIO_PROCESS, 0, -20);
  if (ret!=0) printf("Instruction setpriority failed.\n");
  
  DRAMConfig::select_config(Microarchitecture::AMD_ZEN_3, 1, 4, 4, false);

  Args args = parse_args(argc, argv);

  Memory alloc = Memory(true);
  printf("creating allocation...\n");
  alloc.allocate_memory(DRAMConfig::get().memory_size());
  printf("allocated %lu bytes of memory.\n", alloc.get_allocation_size());
  DRAMAddr::initialize_mapping(0, alloc.get_starting_address());

  HammerSuite suite(alloc);
  printf("initialized runtime parameter to %lu.\n", args.runtime_limit);
  printf("initialized location parameter to %lu.\n", args.locations);
  printf("starting hammering run!\n");
  std::vector<FuzzReport> reports = suite.auto_fuzz(args.locations, args.runtime_limit);
  size_t full_check = alloc.check_memory(alloc.get_starting_address(), alloc.get_starting_address() + alloc.get_allocation_size());
  printf("full check found %lu flips.\n", full_check);
}
