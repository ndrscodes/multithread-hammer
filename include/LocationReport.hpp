#pragma once
#include <vector>
#include "HammeringPattern.hpp"

typedef struct {
  HammeringPattern pattern;
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

