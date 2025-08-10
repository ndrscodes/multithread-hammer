#pragma once
#include "FuzzingParameterSet.hpp"
#include "MappedPattern.hpp"
#include <chrono>
#include <cmath>
#include <vector>

typedef struct {
  MappedPattern pattern;
  size_t flips;
  std::chrono::duration<float_t> duration;
} PatternReport;

class LocationReport {
private:
  std::vector<PatternReport> reports;
public:
  std::vector<PatternReport> get_reports();
  size_t sum_flips();
  void add_report(PatternReport report);
  std::chrono::duration<float> duration();
};

