#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include "GlobalDefines.hpp"
#include "HammerSuite.hpp"
#include "Logger.hpp"
#include "Memory.hpp"
#include <sys/resource.h>

Args parse_args(int argc, char* argv[]) {
  Args args;
  for(int i = 1; i < argc; i++) {
    if((strncmp("-r", argv[i], 2) == 0 || strncmp("--runtime", argv[i], 9) == 0) && i + 1 < argc) {
      args.runtime_limit = atoi(argv[i + 1]);
      i++;
    } else if((strcmp("-l", argv[i]) == 0 || strcmp("--locations", argv[i]) == 0) && i + 1 < argc) {
      args.locations = atoi(argv[i + 1]);
      i++;
    } else if((strcmp("-t", argv[i]) == 0 || strcmp("--threads", argv[i]) == 0) && i + 1 < argc) {
      args.threads = atoi(argv[i + 1]);
      i++;
    } else if(strcmp("-f", argv[i]) || strcmp("--fuzz-effective", argv[i]) == 0) {
      args.test_effective_patterns = true;
    }
  }

  return args;
}

int main(int argc, char* argv[]) {
  Logger::initialize();
  // give this process the highest CPU priority so it can hammer with less interruptions
  int ret = setpriority(PRIO_PROCESS, 0, -20);
  if (ret!=0) printf("Instruction setpriority failed.\n");
  
  DRAMConfig::select_config(Microarchitecture::AMD_ZEN_3, 1, 4, 4, false);

  Args args = parse_args(argc, argv);

  Memory alloc(true);
  printf("creating allocation...\n");
  alloc.allocate_memory(DRAMConfig::get().memory_size());
  printf("allocated %lu bytes of memory.\n", alloc.get_allocation_size());
  DRAMAddr::initialize_mapping(0, alloc.get_starting_address());

  HammerSuite suite(alloc);
  printf("initialized runtime parameter to %lu.\n", args.runtime_limit);
  printf("initialized location parameter to %hu.\n", args.locations);
  printf("initialized threads parameter to %hu\n", args.threads);
  if(args.test_effective_patterns) {
    printf("will test effective patterns in multiple fuzzing runs after we are finished.\n");
  }
  printf("starting hammering run!\n");
  std::vector<FuzzReport> reports = suite.auto_fuzz(args.locations, args.runtime_limit);
  size_t full_check = alloc.check_memory(alloc.get_starting_address(), alloc.get_starting_address() + alloc.get_allocation_size());
  printf("full check found %lu flips.\n", full_check);
  Logger::close();
}
