#include "HammerSuite.hpp"
#include <algorithm>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <emmintrin.h>
#include <iterator>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <vector>
#include <x86intrin.h>
#include "FuzzReport.hpp"
#include "LocationReport.hpp"
#include "PatternBuilder.hpp"
#include "Timer.hpp"
#define SYNC_TO_REF 1

static size_t ACTIVATIONS = 6000000;

HammerSuite::HammerSuite(PatternBuilder &builder) : builder(builder){}

LocationReport HammerSuite::fuzz_location(std::vector<Pattern> patterns) {
  std::vector<std::thread> threads(patterns.size());
  std::barrier barrier(patterns.size());

  size_t thread_id = 0;
  std::vector<uint64_t> timings(patterns.size());
  std::mutex mutex;
  Timer timer(builder);
  for(int i = 0; i < patterns.size(); i++) {
    printf("starting thread for pattern with %lu addresses on bank %lu...\n", patterns[i].size(), patterns[i][0].actual_bank());
    printf("pattern:");
    for(auto a : patterns[i]) {
      printf(" %lu", a.actual_row());
    }
    printf("\n");
    threads[i] = std::thread(
      &HammerSuite::hammer_fn, 
      this, 
      thread_id++, 
      std::ref(patterns[i]), 
      ACTIVATIONS, 
      std::ref(barrier), 
      std::ref(mutex),
      std::ref(timings[i]),
      std::ref(timer)
    );
  }
  
  for(auto& thread : threads) {
    thread.join();
  }
  
  //TODO this is super ugly and should be refactored as soon as possible.
  auto max_timing = std::max_element(timings.begin(), timings.end());
  printf("maximum timing during fuzzing run was %lu cycles. Updating iterations.\n", *max_timing);
  size_t full_ref_cycles = timer.get_cycles_per_refresh() * 8192 * 2;
  size_t index = std::distance(timings.begin(), max_timing);
  size_t pattern_size = patterns[index].size();
  size_t max_activations = full_ref_cycles / (*max_timing / pattern_size);
  if(std::abs((float_t)max_activations - ACTIVATIONS) > ACTIVATIONS * 0.1) {
    printf("updating iterations from %lu to %lu to match %lu cycles of hammering.\n", ACTIVATIONS, max_activations, full_ref_cycles);
    ACTIVATIONS = max_activations;
  }

  LocationReport locationReport;
  for(auto pattern : patterns) {
    PatternReport report {
      .pattern = pattern,
      .flips = builder.check(pattern)
    };
    locationReport.add_report(report);
  }

  return locationReport;
}

FuzzReport HammerSuite::fuzz(size_t locations, size_t patterns) {
  FuzzReport report;
  std::vector<Pattern> fuzz_patterns = builder.create_multiple_banks(patterns);
  bool first = true;
  printf("running %lu patterns over %lu locations...\n", patterns, locations);
  for(size_t i = 0; i < locations; i++) {
    if(!first) {
      for(size_t j = 0; j < patterns; j++) {
        //shift the pattern to the next bank with one row offset just to diversify...
        fuzz_patterns[j] = builder.translate(fuzz_patterns[j], 1, 1);
      }
    } else {
      first = false;
    }
    report.add_report(fuzz_location(fuzz_patterns));
    printf("executed fuzzing run on location %lu with %lu patterns, flipping %lu bits.\n", i, patterns, report.get_reports().back().sum_flips());
  }

  printf("managed to flip %lu bits over %lu locations.\n", report.sum_flips(), locations);
  return report;
}

std::vector<FuzzReport> HammerSuite::auto_fuzz(size_t locations_per_fuzz, size_t runtime_in_seconds) {
  std::mt19937 random;
  std::uniform_int_distribution<> location_dist(1, 3); // at most 3 will be used per pattern
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

void HammerSuite::hammer_fn(size_t id, Pattern &pattern, size_t iterations, std::barrier<> &start_barrier, std::shared_mutex &mutex, uint64_t &timing, Timer &timer) {
  //this is likely faster than an std::vector because we can skip the size checks and allocation going on...
  //the processor is likely to cache this array so the lookup will be more or less instant
  size_t s = pattern.size();
  volatile char **virt_addrs = (volatile char **)malloc(sizeof(volatile char *) * s);
  for(size_t i = 0; i < s; i++) {
    virt_addrs[i] = (volatile char *)pattern.at(i).to_virt();
  }

  //try to reset the sampler
  for(int i = 0; i < 100000; i++) {
    volatile char *random_addr = (volatile char *)builder.get_random_address().to_virt();
    *random_addr;
  }
  _mm_mfence();

  iterations /= s;

  std::unique_lock<std::shared_mutex> lock(mutex);
  lock.unlock();

  start_barrier.arrive_and_wait();
  printf("thread %lu is starting a hammering run for %lu addresses using %lu iterations.\n", id, s, iterations);
#if SYNC_TO_REF
  timer.wait_for_refresh(pattern[0].actual_bank());
#endif
  uint64_t start = timer.current_timestamp();
  for(size_t i = 0; i < iterations; i++) {
    for(size_t j = 0; j < s; j++) {
      _mm_clflushopt((void *)virt_addrs[j]);
    }
    for(size_t j = 0; j < s; j++) {
      _mm_mfence();
      *virt_addrs[j];
    }
  }
  uint64_t end = timer.current_timestamp();
  timing = end - start;
}
