#include "HammerSuite.hpp"
#include <algorithm>
#include <barrier>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <emmintrin.h>
#include <pthread.h>
#include <random>
#include <thread>
#include <vector>
#include <x86intrin.h>
#include "CodeJitter.hpp"
#include "DRAMConfig.hpp"
#include "Enums.hpp"
#include "FuzzReport.hpp"
#include "FuzzingParameterSet.hpp"
#include "HammeringPattern.hpp"
#include "LocationReport.hpp"
#include "MappedPattern.hpp"
#include "Memory.hpp"
#include "PatternAddressMapper.hpp"
#include "PatternBuilder.hpp"
#include "RandomPatternBuilder.hpp"
#include "RefreshTimer.hpp"
#include "Jitter.hpp"
#define SYNC_TO_REF 0

size_t ACTIVATIONS = 6000000;
int start_thread = 6;

HammerSuite::HammerSuite(Memory &memory) : memory(memory) {
  engine = std::mt19937(std::random_device()());
}

HammerSuite::HammerSuite(Memory &memory, uint64_t seed) : memory(memory) {
  engine = std::mt19937(seed);
}

LocationReport HammerSuite::fuzz_pattern(std::vector<MappedPattern> &patterns, FuzzingParameterSet &params) {
  std::vector<std::thread> threads(patterns.size());
  std::vector<LocationReport> report;
  std::uniform_int_distribution shift_dist(2, 64);
  std::mt19937 rand(params.get_seed() == 0 ? std::random_device()() : params.get_seed());
  size_t thread_id = start_thread;
  RefreshTimer timer((volatile char *)DRAMAddr(0, 0, 0).to_virt());
  //store it in the DRAMConfig so it can be used by ZenHammers CodeJitter.
  DRAMConfig::get().set_sync_ref_threshold(timer.get_refresh_threshold());

  std::vector<std::vector<volatile char *>> exported_patterns;
  for(auto pattern : patterns) {
    exported_patterns.push_back(pattern.mapper.export_pattern(pattern.pattern, SCHEDULING_POLICY::DEFAULT));
  } 

  if(params.is_interleaved()) {
    std::vector<volatile char *> final_pattern;

    DRAMAddr first_addr(0, 0, 0);
    for(auto ptr : exported_patterns[0]) {
      if(ptr == nullptr) {
        continue;
      }
      first_addr = DRAMAddr((void *)ptr);
    }
    printf("starting interleaved pattern on main bank %lu with starting address %s.\n",
           first_addr.actual_bank(), 
           first_addr.to_string().c_str());

    std::set<size_t> tuple_start_indices;
    for(auto idx : patterns[0].pattern.get_tuple_start_indices()) {
      tuple_start_indices.insert(idx);
    }      

    std::uniform_int_distribution tuple_dist(0, (int)exported_patterns.size() - 1);

    for(int i = 0; i < exported_patterns[0].size(); i++) {
      auto ptr = exported_patterns[0][i];
      if(ptr != nullptr) {
        final_pattern.push_back(ptr);
      }
      if(tuple_start_indices.contains(i)) {
        final_pattern.push_back(nullptr);
      }
      if(i % 3 == 0 && exported_patterns.size() > 1) {
        auto pattern = exported_patterns[tuple_dist(engine)];
        int count = 0;
        int idx = 0;
        for(int idx = 0; idx < pattern.size() && count < 2; idx++) {
          if(pattern[idx] == nullptr) {
            continue;
          }
          final_pattern.push_back(pattern[idx++]);
        }
      }
    }

    std::vector<volatile char *> non_accessed_rows;
    for(auto& pattern : patterns) {
      auto rows = pattern.mapper.get_random_nonaccessed_rows(DRAMConfig::get().rows());
      non_accessed_rows.insert(non_accessed_rows.end(), rows.begin(), rows.end());
    }

    std::barrier fake_barrier(1);
    CodeJitter jitter;

    hammer_fn(
      thread_id, 
      final_pattern, 
      non_accessed_rows, 
      jitter,
      params, 
      fake_barrier,
      timer 
    );

  } else {
    std::barrier barrier(patterns.size());
    for(int i = 0; i < exported_patterns.size(); i++) {
      DRAMAddr first_addr(0, 0, 0);
      for(auto ptr : exported_patterns[i]) {
        if(ptr == nullptr) {
          continue;
        }
        first_addr = DRAMAddr((void *)ptr);
      }
      printf("starting thread on bank %lu with first address being %s.\n", 
             first_addr.actual_bank(),
             first_addr.to_string().c_str());
      threads[i] = std::thread(
        &HammerSuite::hammer_fn, 
        this, 
        thread_id++, 
        exported_patterns[i],
        patterns[i].mapper.get_random_nonaccessed_rows(DRAMConfig::get().rows()),
        std::ref(patterns[i].mapper.get_code_jitter()),
        std::ref(params),
        std::ref(barrier), 
        std::ref(timer)
      );
    }
  
    for(auto& thread : threads) {
      thread.join();
    }
  }

  LocationReport locationReport;
  int total_flips = 0;
  for(int i = 0; i < patterns.size(); i++) {
    //this MUST be done SINGLE-THREADED as multiple threads would constantly overwrite the seed of srand().
    size_t flips = memory.check_memory(patterns[i].mapper, true, true);
    PatternReport report {
      .flips = flips
    };
    if(report.flips) {
      report.pattern = patterns[i]; //if the pattern was effective, we are storing it to fuzz it later.
    }
    total_flips += report.flips;
    if(report.flips) {
      printf("SUCCESS: Managed to flip %lu bits on mapping %d. The bank on which this happened was %lu.\n", 
             report.flips, 
             i, 
             DRAMAddr((void *)*patterns[i].mapper.get_victim_rows().begin()).actual_bank());
    }
    locationReport.add_report(report);
  }

  printf("\n###########################################################################################\n\n");

  return locationReport;
}

std::vector<LocationReport> HammerSuite::fuzz_location(std::vector<MappedPattern> &patterns, FuzzingParameterSet &params, size_t locations) {
  std::vector<LocationReport> location_reports(locations);
  std::uniform_int_distribution row_shift_dist(1, 64);

  if(locations == 0) {
    printf("location parameter must be non-zero.\n");
    exit(1);
  }

  location_reports[0] = fuzz_pattern(patterns, params);
  if(--locations == 0) {
    return location_reports;
  }

  for(int i = 0; i < locations; i++) {
    for(int j = 0; j < patterns.size(); j++) {
      patterns[j].mapper.shift_mapping(row_shift_dist(engine), {});
    }
    location_reports[i + 1] = fuzz_pattern(patterns, params);
  }

  return location_reports;
}

std::vector<LocationReport> HammerSuite::fuzz_location(std::vector<HammeringPattern> &patterns, FuzzingParameterSet &params, size_t locations) {
  std::vector<LocationReport> location_reports(locations);
  std::vector<MappedPattern> mapped_patterns(patterns.size());
  std::uniform_int_distribution row_shift_dist(1, 64);

  if(locations == 0) {
    printf("location parameter must be non-zero.\n");
    exit(1);
  }

  for(int i = 0; i < mapped_patterns.size(); i++) {
    mapped_patterns[i] = {
      .pattern = patterns[i],
      .mapper = PatternAddressMapper()
    };
    mapped_patterns[i].mapper.randomize_addresses(params, mapped_patterns[i].pattern.agg_access_patterns, true);
  }

  return fuzz_location(mapped_patterns, params, locations);
}

FuzzReport HammerSuite::fuzz(size_t locations, size_t patterns, bool interleaved) {
  FuzzingParameterSet parameters(engine());
  parameters.set_interleaved(interleaved);
  parameters.randomize_parameters();
  std::vector<HammeringPattern> fuzz_patterns(patterns);

  #define USE_RANDOM_PATTERN_GEN 0
  for(auto& pattern : fuzz_patterns) {
#if USE_RANDOM_PATTERN_GEN
    RandomPatternBuilder random_pattern_builder;
    pattern = random_pattern_builder.create_advanced_pattern(rand() % 2048);
#else
    PatternBuilder pattern_builder(pattern);
    pattern_builder.generate_frequency_based_pattern(parameters);
#endif
  }

  FuzzReport report(parameters);
  printf("running %lu patterns over %lu locations...\n", patterns, locations);
  for(auto location_report : fuzz_location(fuzz_patterns, parameters, locations)) {
    report.add_report(location_report);
  }
  printf("executed fuzzing run on %lu locations with %lu patterns, flipping %lu bits.\n", locations, patterns, report.get_reports().back().sum_flips());

  printf("managed to flip %lu bits over %lu locations.\n", report.sum_flips(), locations);
  return report;
}

void HammerSuite::check_effective_patterns(std::vector<FuzzReport> &patterns) {
  printf("\n##### BEGIN EFFECTIVE PATTERN ANALYSIS #####\n\n");

  std::vector<FuzzReport> effective_reports;
  size_t sum_flips = 0;
  for(auto& report : patterns) {
    size_t sum = report.sum_flips();
    sum_flips += sum;
    if(sum > 0) {
      effective_reports.push_back(report);
    }  
  }
  printf("we flipped %lu bits over %lu fuzzing runs.\n", sum_flips, patterns.size());

  if(sum_flips == 0) {
    printf("we did not flip any bits. Since there are no effective patterns, we will not continue analyzing.\n");
    return;
  }

  size_t thread_flips[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

  for(auto& report : effective_reports) {
    FuzzingParameterSet parameters = report.get_fuzzing_params();
    for(auto location_report : report.get_reports()) {
      for(auto pattern_report : location_report.get_reports()) {
        if(pattern_report.flips == 0) {
          continue;
        }

        std::vector<MappedPattern> patterns;
        //check from 1 to 6 threads
        for(int i = 0; i < 8; i++) {
          patterns.push_back(pattern_report.pattern);
          std::vector<LocationReport> final_reports = fuzz_location(patterns, parameters, 6);
          for(int j = 0; j < final_reports.size(); j++) {
            if(final_reports[j].sum_flips() > 0) {
              printf("[ANALYSIS] pattern %s produced %lu flips over %d threads on location %d!\n", 
                     pattern_report.pattern.pattern.instance_id.c_str(),
                     final_reports[j].sum_flips(),
                     i,
                     j);
              auto loc_reports = final_reports[j].get_reports();
              for(int k = 0; k < loc_reports.size(); k++) {
                printf("[THREAD-ANALYSIS] thread %d produced %lu flips on pattern %s at location %d!\n",
                       k, 
                       loc_reports[k].flips,
                       loc_reports[k].pattern.pattern.instance_id.c_str(),
                       j);
                thread_flips[k] += loc_reports[k].flips;
              }
            }
          }
        }
      }
    }
    
    for(int i = 0; i < 8; i++) {
      printf("fuzzing using %d threads yielded %lu flips.\n", i + 1, thread_flips[i]);
    }
  }
}

std::vector<FuzzReport> HammerSuite::auto_fuzz(Args args) {
  std::mt19937 random;
  std::vector<FuzzReport> reports;
  auto start = std::chrono::steady_clock::now();
  auto max_duration = std::chrono::seconds(args.runtime_limit);
  while(std::chrono::steady_clock::now() - start < max_duration) {
    reports.push_back(fuzz(args.locations, args.threads, args.interleaved));
    printf("managed to flip %lu bits over %lu reports.\n", reports.back().sum_flips(), reports.back().get_reports().size());
  }

  printf("stopping fuzzer since maximum duration of %lu seconds has passed. (%f)\n", 
         max_duration.count(),
         std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());

  if(args.test_effective_patterns) {
    check_effective_patterns(reports);
  }

  return reports;
}

void HammerSuite::hammer_fn(size_t id,
                            std::vector<volatile char *> pattern,
                            std::vector<volatile char *> non_accessed_rows,
                            CodeJitter &jitter,
                            FuzzingParameterSet &params,
                            std::barrier<> &start_barrier, 
                            RefreshTimer &timer) {
#define USE_ZEN_JITTER 1

#if USE_ZEN_JITTER
  jitter.jit_strict(params.flushing_strategy, 
                    params.fencing_strategy, 
                    pattern, 
                    FENCE_TYPE::MFENCE, 
                    params.get_hammering_total_num_activations());
#else 
  Jitter jitter(timer.get_refresh_threshold());
  HammerFunc fn = jitter.jit(pattern, non_accessed_rows, params.get_hammering_total_num_activations(), false);
#endif
  
  for(int i = 0; i < 10000; i++) {
    *non_accessed_rows[i % (non_accessed_rows.size() - 1)];
  }

  auto self = pthread_self();
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(id % 16, &cpuset);
  int rc = pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset); 

  start_barrier.arrive_and_wait();
  printf("thread %lu is starting a hammering run for %lu addresses.\n", id, pattern.size());
#if SYNC_TO_REF
  timer.wait_for_refresh(DRAMAddr((void *)pattern[0]).actual_bank());
#endif
#if USE_ZEN_JITTER
  jitter.hammer_pattern(params, true);
  jitter.cleanup();
#else
  size_t timing = fn();
  printf("thread %lu took %lu cycles\n", id, timing);
#endif
}
