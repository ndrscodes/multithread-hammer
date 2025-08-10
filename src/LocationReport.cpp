#include "LocationReport.hpp"
#include <chrono>
#include <cstddef>
#include <vector>

std::vector<PatternReport> LocationReport::get_reports() {
  return reports;
}

size_t LocationReport::sum_flips() {
  size_t sum = 0;
  for(auto report : reports) {
    sum += report.flips;
  }
  return sum;
}

void LocationReport::add_report(PatternReport report) {
  reports.push_back(report);
}

std::chrono::duration<float> LocationReport::duration() {
  std::chrono::duration<float> max = std::chrono::duration<float>::min();
  for(auto& report : reports) {
    if(report.duration > max) {
      max = report.duration;
    }
  }

  return max;
}
