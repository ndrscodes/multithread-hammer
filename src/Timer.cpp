#include "Timer.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <sched.h>
#include <vector>
#include <math.h>
#define PEAK_DECISION_MULTIPLIER 1.02

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
  //however, we expect these differences to be so small that it doesn't really have an inpact on the result.
  return med[mid];
}

std::vector<uint64_t> Timer::get_measurements(size_t n) {
  std::vector<uint64_t> measurements(n);
  volatile char *addr = (volatile char *)pattern[0].to_virt();
  for(size_t i = 0; i < n; i++) {
    measurements[i] = measure(addr);
  }
  return measurements;
}

uint64_t Timer::get_refresh_threshold() {
  return refresh_threshold;
}

uint64_t Timer::wait_for_refresh() {
  uint64_t i = 0;
  uint64_t timing;
  volatile char *address = (volatile char *)pattern[0].to_virt();
  do {
    timing = measure(address);
    i++;
  } while(timing < refresh_threshold);

  return i;
}

uint64_t Timer::reanalyze() {
  //choose a new pattern to use for timing analysis
  pattern = builder.create(0, 1);

  uint64_t previous = 0;
  uint64_t threshold = 0;

  //keep measuring while both measurements are not within 10% of eachother. 
  uint16_t i = 0;
  do {
    previous = threshold;
    //warmup
    get_measurements(100000);
    sched_yield();

    std::vector<uint64_t> measurements = get_measurements(1000000);
    double_t avg = get_average(measurements);

    std::vector<uint64_t> peaks;
    for(auto measurement : measurements) {
      if(measurement > avg * PEAK_DECISION_MULTIPLIER) {
        peaks.push_back(measurement);
      }
    }

    uint64_t peak_median = get_median(peaks);
   
    //the median of the peaks should always be higher than the average across all samples
    //we select the middle between the average across all samples and the median of all peaks
    //as our threshold for refresh-induced peaks.
    threshold = avg + ((peak_median - avg) / 2);
    printf("measured threshold to be %lu.\n", threshold);
    assert(++i < 30);
  } while(abs((int64_t)threshold - (int64_t)previous) > previous / 10);

  return threshold;
}
