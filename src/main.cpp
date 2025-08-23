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

SCHEDULING_POLICY find_policy(std::string policy) {
  if("pair" == policy) {
    return SCHEDULING_POLICY::PAIR;
  } else if("full" == policy) {
    return SCHEDULING_POLICY::FULL;
  } else if("halfbase" == policy) {
    return SCHEDULING_POLICY::HALF_BASE_PERIOD;
  } else if("base" == policy) {
    return SCHEDULING_POLICY::BASE_PERIOD;
  } else if("repeat" == policy) {
    return SCHEDULING_POLICY::REPETITON;
  } else if("default" == policy) {
    return SCHEDULING_POLICY::DEFAULT;
  } else if("none" == policy) {
    return SCHEDULING_POLICY::NONE;
  }
  
  printf("unknown scheduling policy \"%s\"\n", policy.c_str());
  exit(EXIT_FAILURE);
}

FLUSHING_STRATEGY find_flushing_strategy(std::string strategy) {
  if("latest" == strategy) {
    return FLUSHING_STRATEGY::LATEST_POSSIBLE;
  }
  if("omit" == strategy) {
    return FLUSHING_STRATEGY::OMIT_FLUSHING;
  }

  return FLUSHING_STRATEGY::EARLIEST_POSSIBLE;
}

ColumnRandomizationStyle find_randomization_style(std::string randomization_style) {
  if("all" == randomization_style) {
    return ColumnRandomizationStyle::PER_ACCESS;
  }
  if("aggressor" == randomization_style) {
    return ColumnRandomizationStyle::PER_AGGRESSOR;
  }

  return ColumnRandomizationStyle::NONE;
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
  printf("%-40s: number of locations when fuzzing (using -fr).\n", "-fl, --fuzz-locations <locations>");
  printf("%-40s: number of threads/interleaved patterns.\n", "-t, --threads <threads>");
  printf("%-40s: check effective patterns after run (using all other found patterns).\n", "-fc, --fuzz-combined");
  printf("%-40s: check effective patterns after run.\n", "-fr, --fuzz-random");
  printf("%-40s: seed to use.\n", "-s, --seed <seed>");
  printf("%-40s: interleaved mode. Use pattern interleaving instead of multiple threads.\n", "-i, --interleaved");
  printf("%-40s: desired fence type (lfence, mfence, sfence).\n", "--fence-type <type>");
  printf("%-40s: scheduling type to use (pair, base, halfbase, repeat, default, none, full).\n", "--scheduling <type[,type]>");
  printf("%-40s: build simple patterns instead of complex zenhammer patterns.\n", "--simple <bool[,bool]>");
  printf("%-40s: when interleaving, controls the number of normal accesses between interleaved aggressors.\n", "-id, --interleave-distance <dist>");
  printf("%-40s: when interleaving, controls how many accesses of the current pattern are included per iteration.\n", "-ic, --interleaving-chunk-size <size>");
  printf("%-40s: when interleaving, controls how many patterns are used in each iteration.\n", "-ip, --interleaving-patterns");
  printf("%-40s: randomize each pattern per thread instead of per fuzzing run.\n", "--randomize-each");
  printf("%-40s: the flushing strategy (earliest, latest).\n", "--flushing-strategy <type>");
  printf("%-40s: the fencing strategy (earliest, latest, omit).\n", "--fencing-strategy <type>");
  printf("%-40s: column randomization style (all, aggressor, none).\n", "-rs, --randomization-style");
  printf("%-40s: compensate for the difference in access counts when interleaving.\n", "--compensate");
}

Args parse_args(int argc, char* argv[]) {
  Args args;
  for(int i = 1; i < argc; i++) {
    if((strcmp("-r", argv[i]) == 0 || strcmp("--runtime", argv[i]) == 0) && i + 1 < argc) {
      args.runtime_limit = atoi(argv[i + 1]);
      i++;
    } else if((strcmp("-l", argv[i]) == 0 || strcmp("--locations", argv[i]) == 0) && i + 1 < argc) {
      args.locations = atoi(argv[i + 1]);
      i++;
    } else if((strcmp("-fl", argv[i]) == 0 || strcmp("--fuzz-locations", argv[i]) == 0) && i + 1 < argc) {
      args.fuzz_locations = atoi(argv[i + 1]);
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
    } else if(strcmp("--compensate", argv[i]) == 0) {
      args.compensate_access_count = true;
    } else if(strcmp("--fence-type", argv[i]) == 0 && i + 1 < argc) {
      if(strcmp("lfence", argv[i + 1]) == 0) {
        args.fence_type = FENCE_TYPE::LFENCE;
      } else if(strcmp("sfence", argv[i + 1]) == 0) {
        args.fence_type = FENCE_TYPE::SFENCE;
      } else if(strcmp("mfence", argv[i + 1]) == 0) {
        args.fence_type = FENCE_TYPE::MFENCE;
      } else {
        printf("unknown fence type \"%s\"\n", argv[i + 1]);
        exit(EXIT_FAILURE);
      }
      i++;
    } else if(strcmp("--scheduling", argv[i]) == 0 && i + 1 < argc) {
      std::string str(argv[i + 1]);
      int idx = str.find(",");
      if(idx != std::string::npos) {
        args.scheduling_policy_first_thread = find_policy(str.substr(0, idx));
        args.scheduling_policy_other_threads = find_policy(str.substr(idx + 1));
      } else {
        SCHEDULING_POLICY policy = find_policy(str);
        args.scheduling_policy_first_thread = policy;
        args.scheduling_policy_other_threads = policy;
      }
      i++;
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
    } else if((strcmp("--interleaving-distance", argv[i]) == 0 || strcmp("-id", argv[i]) == 0) && i + 1 < argc) {
      args.interleaving_distance = atol(argv[i + 1]);
      i++;
    } else if((strcmp("--interleaving-chunk-size", argv[i]) == 0 || strcmp("-ic", argv[i]) == 0) && i + 1 < argc) {
      args.interleaving_chunk_size = atol(argv[i + 1]);
      i++;
    } else if((strcmp("--interleaving-patterns", argv[i]) == 0 || strcmp("-ip", argv[i]) == 0) && i + 1 < argc) {
      args.interleaving_patterns = atol(argv[i + 1]);
      i++;
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
    } else if(strcmp("-rs", argv[i]) == 0 || strcmp("--randomization-style", argv[i]) && i + 1 < argc) {
      args.randomization_style = find_randomization_style(std::string(argv[i + 1]));
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
  
  DRAMConfig::select_config(Microarchitecture::AMD_ZEN_3, 1, 4, 4, false);

  Args args = parse_args(argc, argv);

  Memory alloc(true);
  if(args.seed > 0) {
    alloc.set_seed(args.seed);
    HammerSuite::set_seed(args.seed);
    FuzzingParameterSet::set_seed(args.seed);
    SimplePatternBuilder::set_seed(args.seed);
    PatternBuilder::set_seed(args.seed);
    PatternAddressMapper::set_seed(args.seed);
  }
  printf("creating allocation...\n");
  alloc.allocate_memory(DRAMConfig::get().memory_size());
  printf("allocated %lu bytes of memory.\n", alloc.get_allocation_size());
  DRAMAddr::initialize_mapping(0, alloc.get_starting_address());

  printf("initialized runtime parameter to %lu.\n", args.runtime_limit);
  printf("initialized location parameter to %hu.\n", args.locations);
  printf("initialized threads parameter to %hu\n", args.threads);
  printf("initialized scheduling policy for first thread to %s\n", to_string(args.scheduling_policy_first_thread).c_str());
  printf("initialized scheduling policy for other threads to %s\n", to_string(args.scheduling_policy_other_threads).c_str());
  printf("initialized simple pattern mode for first thread to %b\n", args.simple_patterns_first_thread);
  printf("initialized simple pattern mode for other threads to %b\n", args.simple_patterns_other_threads);
  printf("initialized fencing strategy to %s\n", to_string(args.fence_type).c_str());
  if(args.randomization_style != ColumnRandomizationStyle::NONE) {
    printf("columns will be randomized with type: %s\n", 
           args.randomization_style == ColumnRandomizationStyle::PER_AGGRESSOR ? "PER_AGGRESSOR" : "PER_ACCESS");
  }
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
  size_t full_check = alloc.check_memory(alloc.get_starting_address(), alloc.get_starting_address() + alloc.get_allocation_size());
  printf("full check found %lu flips.\n", full_check);
  Logger::close();
  delete suite;
}
