#include "HammerSuite.hpp"
#include <algorithm>
#include <barrier>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <emmintrin.h>
#include <functional>
#include <pthread.h>
#include <random>
#include <sched.h>
#include <set>
#include <string>
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
#include "RefreshTimer.hpp"
#include "Jitter.hpp"
#include "SimplePatternBuilder.hpp"
#include "CsvExporter.hpp"
#define SYNC_TO_REF 0

size_t ACTIVATIONS = 6000000;
int start_thread = 6;
const bool reproducibility_mode = false;

std::mt19937 HammerSuite::engine = std::mt19937(std::random_device()());

HammerSuite::HammerSuite(Memory &memory) : memory(memory) {
}

void HammerSuite::set_seed(uint64_t seed){
  engine = std::mt19937(seed);
}

LocationReport HammerSuite::fuzz_pattern(std::vector<MappedPattern> &patterns, Args &args) {
  std::vector<std::thread> threads(patterns.size());
  std::vector<LocationReport> report;
  std::uniform_int_distribution shift_dist(2, 64);
  std::mt19937 rand(args.seed == 0 ? std::random_device()() : args.seed);
  size_t thread_id = args.thread_start_id;
  RefreshTimer timer((volatile char *)DRAMAddr(0, 0, 0).to_virt());
  //store it in the DRAMConfig so it can be used by ZenHammers CodeJitter.
  DRAMConfig::get().set_sync_ref_threshold(timer.get_refresh_threshold());

  std::vector<std::vector<volatile char *>> exported_patterns;
  bool first = true;
  for(auto pattern : patterns) {
    std::vector<volatile char *> exported;
    pattern.mapper.export_pattern(
      pattern.pattern.aggressors,
      pattern.params.get_base_period(),
      exported
    );
    exported_patterns.push_back(exported);
    first = false;
  } 

  std::vector<std::chrono::time_point<std::chrono::steady_clock>> starts(patterns.size());
  std::vector<std::chrono::time_point<std::chrono::steady_clock>> ends(patterns.size());

  if(args.interleaved) {
    std::vector<volatile char *> final_pattern = PatternAddressMapper::interleave(
      exported_patterns, 
      args.interleave_single_pair_only, 
      args.interleaving_distance);

    DRAMAddr first_addr(0, 0, 0);
    for(auto ptr : exported_patterns[0]) {
      if(ptr == nullptr) {
        continue;
      }
      first_addr = DRAMAddr((void *)ptr);
    }
    printf("starting interleaved pattern on main bank %lu with starting address %s.\n",
           first_addr.get_bank(), 
           first_addr.to_string().c_str());

    std::vector<volatile char *> non_accessed_rows;
    for(auto& pattern : patterns) {
      auto any_aggressor_row = pattern.mapper.aggressor_to_addr[pattern.pattern.aggressors[0].id];
      std::vector<volatile char*> sync_rows;
      auto mr = pattern.mapper.max_row;
      auto da = DRAMAddr(any_aggressor_row);
      da.add_inplace(0, 1, 0, 0, 0);
      da.set_row(mr);
      // std::cout << "sync_rows:\n";
      for (size_t i = 1; i <= 32; ++i) {
        da.add_inplace(0, 0, 0, i, 0);
        sync_rows.push_back((volatile char*)da.to_virt());
        // std::cout << da.to_string() << "\n";
      }
      // std::cout << std::endl;
      non_accessed_rows.insert(non_accessed_rows.end(), sync_rows.begin(), sync_rows.end());
    }

    std::barrier fake_barrier(1);
    CodeJitter jitter;

    hammer_fn(
      thread_id, 
      final_pattern, 
      non_accessed_rows, 
      jitter,
      patterns[0].params, 
      fake_barrier,
      timer,
      starts[0],
      ends[0]
    );

  } else {

    std::vector<std::vector<volatile char *>> non_accessed_rows(patterns.size());
    std::barrier barrier(patterns.size());

    for(int i = 0; i < exported_patterns.size(); i++) {
      auto any_aggressor_row = patterns[i].mapper.aggressor_to_addr[patterns[i].pattern.aggressors[0].id];
      std::vector<volatile char*> sync_rows;
      auto mr = patterns[i].mapper.max_row;
      auto da = DRAMAddr(any_aggressor_row);
      da.add_inplace(0, 1, 0, 0, 0);
      da.set_row(mr);
      // std::cout << "sync_rows:\n";
      for (size_t i = 1; i <= 32; ++i) {
        da.add_inplace(0, 0, 0, i, 0);
        sync_rows.push_back((volatile char*)da.to_virt());
        // std::cout << da.to_string() << "\n";
      }
      // std::cout << std::endl;
      non_accessed_rows[i] = sync_rows;
      DRAMAddr first_addr(0, 0, 0);
      for(auto ptr : exported_patterns[i]) {
        if(ptr == nullptr) {
          continue;
        }
        first_addr = DRAMAddr((void *)ptr);
      }
      printf("starting thread on bank %lu with first address being %s.\n", 
             first_addr.get_bank(),
             first_addr.to_string().c_str());

      threads[i] = std::thread(
        &HammerSuite::hammer_fn, 
        this, 
        thread_id++, 
        std::ref(exported_patterns[i]),
        std::ref(non_accessed_rows[i]),
        std::ref(patterns[i].mapper.get_code_jitter()),
        std::ref(patterns[i].params),
        std::ref(barrier), 
        std::ref(timer),
        std::ref(starts[i]),
        std::ref(ends[i])
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
    size_t flips = memory.check_memory(patterns[i].mapper, reproducibility_mode, true);
    if(!reproducibility_mode) {
      flips = 0;
      if(!patterns[i].mapper.bit_flips.empty()) {
        for(auto flip : patterns[i].mapper.bit_flips.back()) {
          flips += flip.count_bit_corruptions();
        }
      }
    }

    PatternReport report {
      .pattern = patterns[i],
      .flips = flips,
      .duration = ends[i] - starts[i]
    };

    if(!reproducibility_mode) {
      report.bit_flips = patterns[i].mapper.bit_flips.back();
    }

    total_flips += report.flips;
    if(report.flips) {
      printf("SUCCESS: Managed to flip %lu bits on mapping %d. The bank on which this happened was %lu.\n", 
             report.flips, 
             i, 
             patterns[i].mapper.get_victim_rows().begin()->get_bank());
    }

    locationReport.add_report(report);
  }

  printf("took: %lu us\n", std::chrono::duration_cast<std::chrono::microseconds>(locationReport.duration()).count());

  if(total_flips) {
    printf("[SUCCESS] flipped %d bits on this pattern combination.\n", total_flips);
  }

  printf("\n###########################################################################################\n\n");

  return locationReport;
}

std::vector<LocationReport> HammerSuite::fuzz_location(std::vector<MappedPattern> &patterns, size_t locations, Args &args) {
  std::vector<LocationReport> location_reports(locations);
  std::uniform_int_distribution row_shift_dist(1, 64);

  if(locations == 0) {
    printf("location parameter must be non-zero.\n");
    exit(1);
  }

  location_reports[0] = fuzz_pattern(patterns, args);
  if(--locations == 0) {
    return location_reports;
  }

  for(int i = 0; i < locations; i++) {
    for(int j = 0; j < patterns.size(); j++) {
      patterns[j].mapper.shift_mapping(row_shift_dist(engine), {});
    }
    location_reports[i + 1] = fuzz_pattern(patterns, args);
  }

  return location_reports;
}

std::vector<LocationReport> HammerSuite::fuzz_location(std::vector<HammeringPattern> &patterns, size_t locations, Args &args) {
  std::vector<LocationReport> location_reports(locations);
  std::vector<MappedPattern> mapped_patterns(patterns.size());
  std::uniform_int_distribution row_shift_dist(1, 64);

  if(locations == 0) {
    printf("location parameter must be non-zero.\n");
    exit(1);
  }

  FuzzingParameterSet parameters;
  parameters.randomize_parameters();

  for(int i = 0; i < mapped_patterns.size(); i++) {
    mapped_patterns[i] = map_pattern(patterns[i], parameters, i >= 1 && args.simple_patterns_other_threads || i == 0 && args.simple_patterns_first_thread);
    if(args.randomize_each_pattern) {
      parameters = FuzzingParameterSet();
      parameters.randomize_parameters();
    }
  }

  return fuzz_location(mapped_patterns, locations, args);
}

FuzzReport HammerSuite::fuzz(Args &args) {
  FuzzingParameterSet parameters;
  parameters.set_interleaved(args.interleaved);
  parameters.randomize_parameters();
  std::vector<HammeringPattern> fuzz_patterns(args.threads);

  #define USE_RANDOM_PATTERN_GEN 0
  for(size_t i = 0; i < args.threads; i++) {
#if USE_RANDOM_PATTERN_GEN
    RandomPatternBuilder random_pattern_builder;
    pattern = random_pattern_builder.create_advanced_pattern(rand() % 2048);
#else
    fuzz_patterns[i] = generate_pattern(
      parameters, 
      args.simple_patterns_other_threads && i > 0 || args.simple_patterns_first_thread && i == 0);
#endif
  }

  FuzzReport report;
  printf("running %hu patterns over %hu locations...\n", args.threads, args.locations);
  for(auto location_report : fuzz_location(fuzz_patterns, args.locations, args)) {
    report.add_report(location_report);
  }
  printf("executed fuzzing run on %hu locations with %hu patterns, flipping %lu bits.\n", args.locations, args.threads, report.get_reports().back().sum_flips());

  printf("managed to flip %lu bits over %hu locations.\n", report.sum_flips(), args.locations);
  return report;
}

HammeringPattern HammerSuite::generate_pattern(FuzzingParameterSet &params, bool simple) {
  HammeringPattern pattern;
  if(simple) {
    SimplePatternBuilder builder = SimplePatternBuilder();
    builder.generate_pattern(pattern, params);
  } else {
    PatternBuilder builder = PatternBuilder(pattern);
    builder.generate_frequency_based_pattern(params);
  }
  return pattern;
}

MappedPattern HammerSuite::map_pattern(HammeringPattern &pattern, FuzzingParameterSet &params, bool simple) {
  return map_pattern(-1, pattern, params, simple);
}

MappedPattern HammerSuite::map_pattern(int bank, HammeringPattern &pattern, FuzzingParameterSet &params, bool simple) {
  if(bank != -1) {
    PatternAddressMapper::set_bank_counter(bank);
  }
  MappedPattern p = {
    .pattern = pattern,
  };
  p.mapper.randomize_addresses(params, pattern.agg_access_patterns, true);

  return p;
}

MappedPattern HammerSuite::build_mapped(int bank, FuzzingParameterSet &params, bool simple) {
  HammeringPattern pattern = generate_pattern(params, simple);
  return map_pattern(bank, pattern, params, simple);
}

MappedPattern HammerSuite::build_mapped(FuzzingParameterSet &params, bool simple) {
  HammeringPattern pattern = generate_pattern(params, simple);
  return map_pattern(pattern, params, simple);
}

std::vector<FuzzReport> HammerSuite::filter_and_analyze_flips(std::vector<FuzzReport> &patterns, std::string &filepath) {
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
  printf("we flipped %lu bits over %lu fuzzing runs. We found %lu runs with at least one flip.\n", sum_flips, patterns.size(), effective_reports.size());

  if(sum_flips == 0) {
    printf("we did not flip any bits. Since there are no effective patterns, we will not continue analyzing.\n");
    return effective_reports;
  }

  size_t thread_flips[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  size_t thread_pattern_lengths[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  size_t thread_aggressor_nums[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  size_t effective_pattern_counts[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  std::map<size_t, size_t> bank_effective_counts;
  std::map<size_t, size_t> bank_flip_counts;

  for(auto& report : effective_reports) {
    std::vector<LocationReport> final_reports = report.get_reports();
    int location = 0;
    for(auto final_report: final_reports) {
      location++;
      int threads = final_report.get_reports().size();
      if(final_report.sum_flips() > 0) {
        auto loc_reports = final_report.get_reports();
        for(int k = 0; k < loc_reports.size(); k++) {
          if(loc_reports[k].flips > 0) {
            effective_pattern_counts[threads - 1]++;
            thread_pattern_lengths[threads - 1] += loc_reports[k].pattern.mapper.aggressor_to_addr.size();
            thread_aggressor_nums[threads - 1] += loc_reports[k].pattern.pattern.aggressors.size();
            thread_flips[threads - 1] += loc_reports[k].flips;
            
            size_t bank_no = loc_reports[k].pattern.mapper.pattern_start_row.get_bank();
            if(!bank_flip_counts.contains(bank_no)) {
              bank_flip_counts[bank_no] = 0;
            }
            bank_flip_counts[bank_no] += loc_reports[k].flips;

            if(!bank_effective_counts.contains(bank_no)) {
              bank_effective_counts[bank_no] = 0;
            }
            bank_effective_counts[bank_no]++;
          }

          if(loc_reports[k].flips) {
            printf("[THREAD-ANALYSIS] thread %d produced %lu flips on pattern %s at location %d!\n",
                   k, 
                   loc_reports[k].flips,
                   loc_reports[k].pattern.pattern.instance_id.c_str(),
                   location);
          }
        }
        auto sum = final_report.sum_flips();
        if(sum) {
          printf("[ANALYSIS] hammering produced %lu flips over %d threads on location %d!\n",
                 sum,
                 threads,
                 location);
        }
      }
    }
  }

  printf("%-15s %-15s %-15s %-15s %-15s\n", "threads", "effective", "avg. length", "avg. aggs", "flips");

  for(int i = 0; i < 8; i++) {
    size_t effective = effective_pattern_counts[i];
    double_t avg_length = (double_t)thread_aggressor_nums[i] / effective;
    double_t avg_aggrs = (double_t)thread_pattern_lengths[i] / effective;
    size_t flips = thread_flips[i];
    char avg_length_str[10];    
    char avg_aggr_str[10];
    sprintf(avg_length_str, "%.2f", avg_length);
    sprintf(avg_aggr_str, "%.2f", avg_aggrs);
    printf("%-15d %-15lu %-15s %-15s %-15lu\n", i + 1, effective, avg_length_str, avg_aggr_str, flips);
  }

  printf("%-10s %-10s %-10s\n", "bank no.", "effective", "flips");
  for(auto pair : bank_effective_counts) {
    printf("%-10lu %-10lu %-10lu\n", pair.first, pair.second, bank_flip_counts[pair.first]);
  }

  if(!reproducibility_mode) {
    int effective_banks_per_num_patterns[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int num_available_reports_per_num_patterns[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int zero_to_one = 0;
    int one_to_zero = 0;
    int num_bitflips = 0;

    CsvExporter exporter(filepath);

    printf("we found bitflip information on at least one pattern. Running analysis...\n");
    for(int r = 0; r < effective_reports.size(); r++) {
      auto loc_reports = effective_reports[r].get_reports();
      for(int loc = 0; loc < loc_reports.size(); loc++) {
        bool effective = false;
        auto patterns = loc_reports[loc].get_reports();
        int threads = patterns.size();
        for(int p = 0; p < patterns.size(); p++) {
          auto pat = patterns[p];
          std::set<size_t> banks;
          for(auto& flip : pat.bit_flips) {
            exporter.export_flip(
              flip, 
              r, 
              loc, 
              p,
              threads, 
              pat.pattern.mapper.aggressor_to_addr.size(), 
              pat.pattern.pattern.aggressors.size(),
              pat.duration);
            banks.insert(flip.address.get_bank());
            int z = flip.count_o2z_corruptions();
            int o = flip.count_z2o_corruptions();
            zero_to_one += o;
            one_to_zero += z;
            num_bitflips += o + z;
            effective = true;
          }
          effective_banks_per_num_patterns[loc_reports[loc].get_reports().size() - 1] += banks.size();
        }
        if(effective) {
          num_available_reports_per_num_patterns[loc_reports[loc].get_reports().size() - 1]++;
        }
      }
    }

    printf("found %d bitflips of which %d (%f) were one-to-zero and %d (%f) were zero-to-one flips.\n",
           num_bitflips, one_to_zero, one_to_zero / (double_t)num_bitflips, zero_to_one, zero_to_one / (double_t)num_bitflips);
    printf("%-10s %-10s\n", "threads", "banks");
    for(int i = 0; i < 8; i++) {
      int effective = effective_banks_per_num_patterns[i];
      int tests = num_available_reports_per_num_patterns[i];
      double_t avg_banks = effective / (double_t)tests;
      char avg_banks_s[10];
      sprintf(avg_banks_s, "%.2f", avg_banks);
      printf("%-10d %-10s\n", i + 1, avg_banks_s);
    }
  }

  return effective_reports;
}

void HammerSuite::check_effective_patterns(std::vector<FuzzReport> &patterns, Args &args) {
  std::vector<FuzzReport> fuzz_reports;
  std::string path("bit_flips_search.csv");
  std::vector<FuzzReport> effective_reports = filter_and_analyze_flips(patterns, path);

  std::vector<MappedPattern> effective_patterns;
  for(auto& report : effective_reports) {
    for(auto location_report : report.get_reports()) {
      for(auto& p : location_report.get_reports()) {
        if(p.flips > 0) {
          effective_patterns.push_back(p.pattern);
        }
        p.pattern.mapper.bit_flips.clear();
      }
      if(!args.test_effective_patterns_random) {
        continue;
      }
      //check from 1 to 6 threads
      for(int threads = 1; threads <= 8; threads++) {
        FuzzReport fuzzing_run_report;
        std::vector<MappedPattern> patterns;
        std::set<size_t> banks;
        std::vector<PatternReport> pattern_reports = location_report.get_reports();
        for(int i = 0; i < pattern_reports.size() && i < threads; i++) {
          patterns.push_back(pattern_reports[i].pattern);
          banks.insert(pattern_reports[i].pattern.mapper.pattern_start_row.get_bank());
        }

        while(patterns.size() < threads) {
          printf("we are generating another pattern because we want to fuzz using %d threads, but we only had %lu patterns available in this report.\n", 
                 threads,
                 patterns.size());
          
          size_t first_bank = patterns[0].mapper.pattern_start_row.get_bank();
          while(banks.contains(first_bank)) {
            first_bank = first_bank + 1 % DRAMConfig::get().banks();
          }
          banks.insert(first_bank);

          bool simple = (args.simple_patterns_first_thread && patterns.size() == 0) 
            || (args.simple_patterns_other_threads && patterns.size() > 0);

          FuzzingParameterSet parameters = patterns[0].params;
          if(args.randomize_each_pattern) {
            parameters = FuzzingParameterSet();
            parameters.randomize_parameters();
          }
          patterns.push_back(build_mapped(first_bank, parameters, simple));
        }

        printf("created %lu patterns for analysis run.\n", patterns.size());

        std::vector<LocationReport> final_reports = fuzz_location(patterns, 3, args);
        
        for(int j = 0; j < final_reports.size(); j++) {
          fuzzing_run_report.add_report(final_reports[j]);
        }
        fuzz_reports.push_back(fuzzing_run_report);
      }
    }
  }
 
  if(!args.test_effective_patterns_random) {
    return;
  }
  path = std::string("bit_flips_random_analysis.csv");
  filter_and_analyze_flips(fuzz_reports, path);

  if(effective_patterns.size() == 0 || !args.test_effective_patterns_combined) {
    return;
  }

  fuzz_reports.clear();
  for(int i = 0; i < effective_patterns.size(); i++) {
    std::vector<MappedPattern> patterns_to_run;
    std::set<size_t> banks;
    int appended = 0;
    for(int patterns = 1; patterns <= 6; patterns++) {
      int current = 0;
      while(appended < patterns && current < effective_patterns.size()) {
        if(!banks.contains(effective_patterns[current].mapper.pattern_start_row.get_bank())) {
          patterns_to_run.push_back(effective_patterns[current]);
          banks.insert(effective_patterns[current].mapper.pattern_start_row.get_bank());
          effective_patterns[current].mapper.bit_flips.clear();
          appended++;
        }
        current++;
      }

      if(appended < patterns) {
        //restore the patterns we appended but were unable to execute to the original pattern list and skip the run.
        for(int j = patterns_to_run.size() - 1; j >= patterns; j--) {
          effective_patterns.push_back(patterns_to_run[j]);
        }
        printf("we were unable to execute a pattern since we could not find enough additional patterns to run. (needed %d patterns but only found %d on different banks.)\n",
               patterns, 
               appended);

        continue;
      }

      printf("starting an effective-pattern run for %d patterns.\n", appended);

      std::vector<LocationReport> final_reports = fuzz_location(patterns_to_run, 1, args);
      
      FuzzReport fuzzing_run_report;
      for(int j = 0; j < final_reports.size(); j++) {
        fuzzing_run_report.add_report(final_reports[j]);
      }
      fuzz_reports.push_back(fuzzing_run_report);
    }

    effective_patterns.erase(effective_patterns.begin());
    std::shuffle(effective_patterns.begin(), effective_patterns.end(), engine);
  } 
 
  path = std::string("bit_flips_combined_analysis.csv");
  filter_and_analyze_flips(fuzz_reports, path);
}

std::vector<FuzzReport> HammerSuite::auto_fuzz(Args args) {
  std::mt19937 random;
  std::vector<FuzzReport> reports;
  auto start = std::chrono::steady_clock::now();
  auto max_duration = std::chrono::seconds(args.runtime_limit);
  while(std::chrono::steady_clock::now() - start < max_duration) {
    reports.push_back(fuzz(args));
    printf("managed to flip %lu bits over %lu reports.\n", reports.back().sum_flips(), reports.back().get_reports().size());
  }

  printf("stopping fuzzer since maximum duration of %lu seconds has passed. (%f)\n", 
         max_duration.count(),
         std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());

  check_effective_patterns(reports, args);

  return reports;
}

void HammerSuite::hammer_fn(size_t id,
                            std::vector<volatile char *> &pattern,
                            std::vector<volatile char *> &non_accessed_rows,
                            CodeJitter &jitter,
                            FuzzingParameterSet &params,
                            std::barrier<> &start_barrier, 
                            RefreshTimer &timer,
                            std::chrono::time_point<std::chrono::steady_clock> &start,
                            std::chrono::time_point<std::chrono::steady_clock> &end) {
#define USE_ZEN_JITTER 1

#if USE_ZEN_JITTER
  jitter.jit_strict(params.flushing_strategy, 
                    params.fencing_strategy, 
                    params.get_hammering_total_num_activations(),
                    pattern, 
                    non_accessed_rows); 
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
  if(rc) {
    printf("unable to set affinity to %lu\n.", id % 16);
    exit(1);
  }
  sched_yield();

  printf("thread %lu is starting a hammering run for %lu addresses.\n", id, pattern.size());
  start_barrier.arrive_and_wait();
  start = std::chrono::steady_clock::now();
#if SYNC_TO_REF
  timer.wait_for_refresh(DRAMAddr((void *)pattern[0]).actual_bank());
#endif
#if USE_ZEN_JITTER
  jitter.hammer_pattern(params, true);
  printf("done\n");
  jitter.cleanup();
#else
  size_t timing = fn();
  printf("thread %lu took %lu cycles\n", id, timing);
#endif
  end = std::chrono::steady_clock::now();
}
