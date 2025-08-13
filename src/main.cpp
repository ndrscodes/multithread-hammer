#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include "Enums.hpp"
#include "FuzzingParameterSet.hpp"
#include "GlobalDefines.hpp"
#include "HammerSuite.hpp"
#include "Logger.hpp"
#include "Memory.hpp"
#include "PatternAddressMapper.hpp"
#include "PatternBuilder.hpp"
#include "SimplePatternBuilder.hpp"
#include <sys/resource.h>

FLUSHING_STRATEGY find_flushing_strategy(std::string strategy) {
  if("latest" == strategy) {
    return FLUSHING_STRATEGY::LATEST_POSSIBLE;
  }

  return FLUSHING_STRATEGY::EARLIEST_POSSIBLE;
}

FENCING_STRATEGY find_fencing_strategy(std::string strategy) {
  if("omit" == strategy) {
    return FENCING_STRATEGY::OMIT_FENCING;
  } else if("latest" == strategy) {
    return FENCING_STRATEGY::LATEST_POSSIBLE;
  }

  return FENCING_STRATEGY::EARLIEST_POSSIBLE;
}

bool string_to_bool(std::string str) {
  return "true" == str;
}

void print_help() {
  printf("%-40s: runtime in seconds.\n", "-r, --runtime <seconds>");
  printf("%-40s: number of locations.\n", "-l, --locations <locations>");
  printf("%-40s: number of threads/interleaved patterns.\n", "-t, --threads <threads>");
  printf("%-40s: check effective patterns after run.\n", "-f, --fuzz-effective");
  printf("%-40s: seed to use.\n", "-s, --seed <seed>");
  printf("%-40s: interleaved mode. Use pattern interleaving instead of multiple threads.\n", "-i, --interleaved");
  printf("%-40s: desired fence type (lfence, mfence, sfence).\n", "--fence-type <type>");
  printf("%-40s: scheduling type to use (pair, base, halfbase, repeat, default, none, full).\n", "--scheduling <type[,type]>");
  printf("%-40s: build simple patterns instead of complex zenhammer patterns.\n", "--simple <bool[,bool]>");
  printf("%-40s: when interleaving, controls the number of normal accesses between interleaved aggressors.\n", "--interleave-distance <dist>");
  printf("%-40s: interleave aggressors from one bank per pair instead of all of them.\n", "--interleave-single-pairs");
  printf("%-40s: randomize each pattern per thread instead of per fuzzing run.\n", "--randomize-each");
  printf("%-40s: the flushing strategy (earliest, latest).\n", "--flushing-strategy <type>");
  printf("%-40s: the fencing strategy (earliest, latest, omit).\n", "--fencing-strategy <type>");
}

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
    } else if(strcmp("-fr", argv[i]) == 0 || strcmp("--fuzz-random", argv[i]) == 0) {
      args.test_effective_patterns_random = true;
    } else if(strcmp("-fc", argv[i]) == 0 || strcmp("--fuzz-combined", argv[i]) == 0) {
      args.test_effective_patterns_combined = true;
    } else if((strcmp("-s", argv[i]) == 0 || strcmp("--seed", argv[i]) == 0) && i + 1 < argc) {
      args.seed = atoi(argv[i + 1]);
      i++;
    } else if(strcmp("-i", argv[i]) == 0 || strcmp("--interleaved", argv[i]) == 0) {
      args.interleaved = true;
    } else if(strcmp("--simple", argv[i]) == 0 && i + 1 < argc) {
      std::string str(argv[i + 1]);
      int idx = str.find(",");
      if(idx != std::string::npos) {
        args.simple_patterns_first_thread = string_to_bool(str.substr(0, idx));
        args.simple_patterns_other_threads = string_to_bool(str.substr(idx + 1));
      } else {
        bool simple = string_to_bool(str);
        args.simple_patterns_first_thread = simple;
        args.simple_patterns_other_threads = simple;
      }
      i++;
    } else if(strcmp("--interleave-distance", argv[i]) == 0 && i + 1 < argc) {
      args.interleaving_distance = atol(argv[i + 1]);
      i++;
    } else if(strcmp("--interleave-single-pairs", argv[i]) == 0) {
      args.interleave_single_pair_only = true;
    } else if(strcmp("--randomize-each", argv[i]) == 0) {
      args.randomize_each_pattern = true;
    } else if(strcmp("--fencing-strategy", argv[i]) == 0 && i + 1 < argc) {
      args.fencing_strategy = find_fencing_strategy(std::string(argv[i + 1]));
      i++;
    } else if(strcmp("--flushing-strategy", argv[i]) == 0 && i + 1 < argc) {
      args.flushing_strategy = find_flushing_strategy(std::string(argv[i + 1]));
      i++;
    } else if(strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0) {
      print_help();
      exit(0);
    } else if(strcmp("--thread", argv[i]) == 0 && i + 1 < argc) {
      args.thread_start_id = atol(argv[i + 1]);
      i++;
    } else {
      printf("unknown option: %s\n", argv[i]);
      print_help();
      exit(1);
    }
  }

  return args;
}

int main(int argc, char* argv[]) {
  Logger::initialize();
  // give this process the highest CPU priority so it can hammer with less interruptions
  int ret = setpriority(PRIO_PROCESS, 0, -20);
  if (ret!=0) printf("Instruction setpriority failed.\n");
  
  DRAMAddr::initialize_configs();

  Args args = parse_args(argc, argv);

  Memory alloc(true);
  if(args.seed > 0) {
    alloc.set_seed(args.seed);
    HammerSuite::set_seed(args.seed);
    SimplePatternBuilder::set_seed(args.seed);
    PatternBuilder::set_seed(args.seed);
    PatternAddressMapper::set_seed(args.seed);
  }
  printf("creating allocation...\n");
  alloc.allocate_memory(HUGEPAGE_SZ);
  DRAMAddr::initialize(alloc.get_starting_address(), 1, 8, 4, false);

  printf("initialized runtime parameter to %lu.\n", args.runtime_limit);
  printf("initialized location parameter to %hu.\n", args.locations);
  printf("initialized threads parameter to %hu\n", args.threads);
  printf("initialized simple pattern mode for first thread to %b\n", args.simple_patterns_first_thread);
  printf("initialized simple pattern mode for other threads to %b\n", args.simple_patterns_other_threads);
  HammerSuite *suite;
  if(args.interleaved) {
    printf("running in interleaved mode, just a single thread will be used.\n");
  }
  if(args.seed > 0) {
    printf("initialized seed to %lu\n", args.seed);
  }
  suite = new HammerSuite(alloc);
  
  if(args.test_effective_patterns_random) {
    printf("will test effective patterns in multiple fuzzing runs with random additional patterns after we are finished.\n");
  }
  if(args.test_effective_patterns_combined) {
    printf("will test effective patterns in multiple fuzzing runs using all effective patterns after we are finished.\n");
  }
  printf("starting hammering run!\n");
  std::vector<FuzzReport> reports = suite->auto_fuzz(args);
  alloc.check_memory_full();
  Logger::close();
  delete suite;
}
