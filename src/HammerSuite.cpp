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
#include <random>
#include <thread>
#include <vector>
#include <x86intrin.h>
#include "CodeJitter.hpp"
#include "DRAMConfig.hpp"
#include "Enums.hpp"
#include "FuzzReport.hpp"
#include "FuzzingParameterSet.hpp"
#include "LocationReport.hpp"
#include "Memory.hpp"
#include "PatternAddressMapper.hpp"
#include "PatternBuilder.hpp"
#include "RefreshTimer.hpp"
#include "Jitter.hpp"
#define SYNC_TO_REF 1

size_t ACTIVATIONS = 6000000;

HammerSuite::HammerSuite(Memory &memory) : memory(memory) {}

LocationReport HammerSuite::fuzz_location(std::vector<HammeringPattern> &patterns, FuzzingParameterSet &params) {
  std::random_device dev;
  std::mt19937 engine(dev());
  std::vector<std::thread> threads(patterns.size());
  std::barrier barrier(patterns.size());

  size_t thread_id = 0;
  bool sync_each = rand() % 2 == 0;
  std::vector<PatternAddressMapper> mappers(patterns.size());
 
  //this should be sufficient to determine the ref threshold.
  RefreshTimer timer((volatile char *)DRAMAddr(0, 0, 0).to_virt());
  //store it in the DRAMConfig so it can be used by ZenHammers CodeJitter.
  DRAMConfig::get().set_sync_ref_threshold(timer.get_refresh_threshold());

  for(int i = 0; i < patterns.size(); i++) {
    printf("mapping pattern with %lu aggressors to addresses...", patterns[i].aggressors.size());
   
    //randomize as described in FuzzyHammerer in ZenHammer.
    std::shuffle(patterns[i].agg_access_patterns.begin(), patterns[i].agg_access_patterns.end(), engine);

    //strange, but we will do the mapping here as PatternAddressMapper uses a static bank counter.
    //this could be a problem in multithreaded scenarios.
    //TODO: use an atomic bank counter instead.
    PatternAddressMapper mapper;
    mappers[i] = mapper;
    mappers[i].randomize_addresses(params, patterns[i].agg_access_patterns, true);
    std::vector<volatile char *> pattern = mappers[i].export_pattern(patterns[i], SCHEDULING_POLICY::DEFAULT);

    printf("starting thread for pattern with %lu aggressors...\n",
           patterns[i].aggressors.size());

    threads[i] = std::thread(
      &HammerSuite::hammer_fn, 
      this, 
      thread_id++, 
      pattern,
      std::ref(mappers[i]),
      std::ref(params),
      std::ref(barrier), 
      std::ref(timer),
      sync_each
    );
  }
  
  for(auto& thread : threads) {
    thread.join();
  }

  LocationReport locationReport;
  for(int i = 0; i < mappers.size(); i++) {
    //this MUST be done SINGLE-THREADED as multiple threads would constantly overwrite the seed of srand().
    memory.check_memory(mappers[i], true, true);
    PatternReport report {
      .flips = mappers[i].count_bitflips()
    };
    locationReport.add_report(report);
  }

  return locationReport;
}

FuzzReport HammerSuite::fuzz(size_t locations, size_t patterns) {
  FuzzReport report;
  FuzzingParameterSet parameters;
  parameters.randomize_parameters();
  std::vector<HammeringPattern> fuzz_patterns(patterns);
  for(auto& pattern : fuzz_patterns) {
    PatternBuilder pattern_builder(pattern);
    pattern_builder.generate_frequency_based_pattern(parameters);
  }

  printf("running %lu patterns over %lu locations...\n", patterns, locations);
  for(size_t i = 0; i < locations; i++) {
    report.add_report(fuzz_location(fuzz_patterns, parameters));
    printf("executed fuzzing run on location %lu with %lu patterns, flipping %lu bits.\n", i, patterns, report.get_reports().back().sum_flips());
  }

  printf("managed to flip %lu bits over %lu locations.\n", report.sum_flips(), locations);
  return report;
}

std::vector<FuzzReport> HammerSuite::auto_fuzz(size_t locations_per_fuzz, size_t runtime_in_seconds) {
  std::mt19937 random;
  std::uniform_int_distribution<> location_dist(1, 2); // at most 2 will be used per pattern
  std::vector<FuzzReport> reports;
  auto start = std::chrono::steady_clock::now();
  auto max_duration = std::chrono::seconds(runtime_in_seconds);
  while(std::chrono::steady_clock::now() - start < max_duration) {
    size_t locations = location_dist(random);
    reports.push_back(fuzz(locations_per_fuzz, locations));
    printf("managed to flip %lu bits over %lu reports.\n", reports.back().sum_flips(), reports.back().get_reports().size());
  }

  printf("stopping fuzzer since maximum duration of %lu seconds has passed. (%f)\n", 
         max_duration.count(),
         std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());

  return reports;
}

void HammerSuite::hammer_fn(size_t id,
                            std::vector<volatile char *> pattern,
                            PatternAddressMapper &mapper,
                            FuzzingParameterSet &params,
                            std::barrier<> &start_barrier, 
                            RefreshTimer &timer, 
                            bool sync_each_ref) {
  CodeJitter &jitter = mapper.get_code_jitter();
  jitter.jit_strict(params.flushing_strategy, 
                    params.fencing_strategy, 
                    pattern, 
                    FENCE_TYPE::MFENCE, 
                    params.get_hammering_total_num_activations());

  std::vector<volatile char *> non_accessed_rows = mapper.get_random_nonaccessed_rows(DRAMConfig::get().rows());
  for(int i = 0; i < 10000; i++) {
    *non_accessed_rows[i % (non_accessed_rows.size() - 1)];
  }

  _mm_mfence();

  start_barrier.arrive_and_wait();
  printf("thread %lu is starting a hammering run for %lu addresses.\n", id, pattern.size());
#if SYNC_TO_REF
  timer.wait_for_refresh(DRAMAddr((void *)pattern[0]).actual_bank());
#endif
  jitter.hammer_pattern(params, true);
  jitter.cleanup();
}
