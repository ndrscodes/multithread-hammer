#include "HammerSuite.hpp"
#include <barrier>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <random>
#include <thread>
#include <x86intrin.h>
#include "DRAMConfig.hpp"
#include "FuzzReport.hpp"
#include "LocationReport.hpp"
#include "PatternBuilder.hpp"
#include "Timer.hpp"

const size_t ACTIVATIONS = 5000000;

HammerSuite::HammerSuite(PatternBuilder &builder) : builder(builder){}

LocationReport HammerSuite::fuzz_location(std::vector<Pattern> patterns) {
  std::vector<std::thread> threads(patterns.size());
  std::barrier barrier(patterns.size());

  Timer timer(builder);
  size_t thread_id = 0;
  for(int i = 0; i < patterns.size(); i++) {
    printf("starting thread for pattern with %lu addresses on bank %lu...\n", patterns[i].size(), patterns[i][0].actual_bank());
    threads[i] = std::thread(
      &HammerSuite::hammer_fn, 
      this, 
      thread_id++, 
      std::ref(patterns[i]), 
      ACTIVATIONS, 
      std::ref(barrier), 
      std::ref(timer)
    );
  }
  for(auto& thread : threads) {
    thread.join();
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

  printf("managed to flipt %lu bits over %lu locations.\n", report.sum_flips(), locations);
  return report;
}

std::vector<FuzzReport> HammerSuite::auto_fuzz(size_t locations_per_fuzz, size_t runtime_in_seconds) {
  std::mt19937 random;
  std::uniform_int_distribution<> location_dist(1, DRAMConfig::get().banks());
  std::vector<FuzzReport> reports;
  auto start = std::chrono::steady_clock::now();
  auto max_duration = std::chrono::seconds(runtime_in_seconds);
  while(std::chrono::steady_clock::now() - start < max_duration) {
    size_t locations = location_dist(random);
    reports.push_back(fuzz(3, locations));
    printf("managed to flip %lu bits over %lu reports.\n", reports.back().sum_flips(), reports.back().get_reports().size());
  }

  printf("stopping fuzzer since maximum duration of %lu seconds has passed. (%f)\n", 
         max_duration.count(),
         std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());

  return reports;
}

void HammerSuite::hammer_fn(size_t id, Pattern &pattern, size_t iterations, std::barrier<> &start_barrier, Timer &timer) {
  //this is likely faster than an std::vector because we can skip the size checks and allocation going on...
  //the processor is likely to cache this array so the lookup will be more or less instant
  size_t s = pattern.size();
  volatile char **virt_addrs = (volatile char **)malloc(sizeof(volatile char *) * s);
  for(size_t i = 0; i < s; i++) {
    virt_addrs[i] = (volatile char *)pattern.at(i).to_virt();
  }

  iterations /= s;

  start_barrier.arrive_and_wait();
  printf("starting pattern thread %lu.\n", id);
  timer.wait_for_refresh(pattern[0].actual_bank());
  for(size_t i = 0; i < iterations; i++) {
    for(size_t j = 0; j < s; j++) {
      _mm_clflushopt((void *)virt_addrs[j]);
    }
    for(size_t j = 0; j < s; j++) {
      *virt_addrs[j];
    }
  }
}
