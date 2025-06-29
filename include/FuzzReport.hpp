#pragma once
#include "FuzzingParameterSet.hpp"
#include "LocationReport.hpp"

class FuzzReport {
private:
  std::vector<LocationReport> reports;
public:
  std::vector<LocationReport> get_reports();
  size_t sum_flips();
  void add_report(LocationReport report);
};

