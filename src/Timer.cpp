#include "Timer.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <sched.h>
#include <vector>
#include <math.h>
#define PEAK_DECISION_MULTIPLIER 1.03

Timer::Timer(PatternBuilder &builder) : builder(builder) {
  refresh_threshold = reanalyze();
}

double_t Timer::get_average(std::vector<uint64_t> &measurements) {
  double_t sum = 0;
  for(auto measurement : measurements) {
    sum += measurement;
  }
  return sum / measurements.size();
}

uint64_t Timer::get_median(std::vector<uint64_t> &measurements) {
  std::vector<uint64_t> med(measurements);
  uint64_t mid = med.size() / 2;
  std::nth_element(med.begin(), med.begin() + measurements.size() / 2, med.end());
  //this median will not return correct results if measurements contains an even number of elements
  //however, we expect these differences to be so small that it doesn't really have an impact on the result.
  return med[mid];
}

std::vector<measurement> Timer::get_measurements(size_t n) {
  std::vector<measurement> measurements(n);
  volatile char *addr = (volatile char *)builder.get_random_address().to_virt();
  for(size_t i = 0; i < n; i++) {
    measurements[i] = measure(addr);
  }
  return measurements;
}

uint64_t Timer::get_refresh_threshold() {
  return refresh_threshold;
}

uint64_t Timer::get_cycles_per_refresh() {
  return cycles_per_refresh;
}

std::vector<uint64_t> Timer::extract_timings(std::vector<measurement> &measurements) {
  std::vector<uint64_t> timings(measurements.size());
  for(int i = 0; i < measurements.size(); i++) {
    timings[i] = measurements[i].timing;
  }
  return timings;
}

std::vector<uint64_t> Timer::extract_timestamps(std::vector<measurement> &measurements) {
  std::vector<uint64_t> timestamps(measurements.size());
  for(int i = 0; i < measurements.size(); i++) {
    timestamps[i] = measurements[i].timestamp;
  }
  return timestamps;
}

uint64_t Timer::wait_for_refresh(size_t bank) {
  uint64_t i = 0;
  uint64_t timing;
  volatile char *address = (volatile char *)builder.get_random_address(bank).to_virt();
  do {
    timing = measure(address).timing;
    if(timing > refresh_threshold * 3) {
      printf("[WARN] retry %lu: we probably just missed a refresh since we detected a timing spike (%lu vs threshold of %lu)\n", 
             i, 
             timing, 
             refresh_threshold);
    }
    i++;
  } while(timing < refresh_threshold || timing > refresh_threshold * 3);

  return i;
}

uint64_t Timer::reanalyze() {
  //choose a new pattern to use for timing analysis
  uint64_t previous = 0;
  uint64_t threshold = 0;

  //keep measuring while both measurements are not within 10% of eachother. 
  uint16_t i = 0;
  do {
    previous = threshold;
    //warmup
    get_measurements(100000);
    sched_yield();

    std::vector<measurement> measurements = get_measurements(400000);
    std::vector<uint64_t> timings = extract_timings(measurements);
    double_t avg = get_average(timings);

    std::vector<measurement> peaks;
    for(auto measurement : measurements) {
      if(measurement.timing > avg * PEAK_DECISION_MULTIPLIER) {
        peaks.push_back(measurement);
      }
    }
    
    std::vector<uint64_t> peak_timings(peaks.size());
    for(int j = 0; j < peaks.size(); j++) {
      peak_timings.push_back(peaks[j].timing);
    }
    uint64_t peak_median = get_median(peak_timings);

    std::vector<uint64_t> peak_diffs(peaks.size() - 1);
    for(int j = 1; j < peaks.size(); j++) {
      peak_diffs.push_back(peaks[j].timestamp - peaks[j - 1].timestamp);
    }

    cycles_per_refresh = get_median(peak_diffs);
   
    //the median of the peaks should always be higher than the average across all samples
    //we select the middle between the average across all samples and the median of all peaks
    //as our threshold for refresh-induced peaks.
    threshold = avg + ((peak_median - avg) / 2);
    printf("measured threshold to be %lu. %lu cycles per refresh based on %lu peaks\n", threshold, cycles_per_refresh, peaks.size());
    assert(++i < 30);
  } while(fmin(previous, threshold) / (float_t)fmax(previous, threshold) > 0.1);

  return threshold;
}
