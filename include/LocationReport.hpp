#pragma once
#include "MappedPattern.hpp"
#include <vector>

typedef struct {
  MappedPattern pattern;
  size_t flips;
} PatternReport;

class LocationReport {
private:
  std::vector<PatternReport> reports;
public:
  std::vector<PatternReport> get_reports();
  size_t sum_flips();
  void add_report(PatternReport report);
};

