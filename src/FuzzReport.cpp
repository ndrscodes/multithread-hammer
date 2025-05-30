#include "FuzzReport.hpp"
#include "FuzzingParameterSet.hpp"
#include "LocationReport.hpp"
#include <vector>

FuzzReport::FuzzReport(FuzzingParameterSet parameters) : parameters(parameters) {

}

std::vector<LocationReport> FuzzReport::get_reports() {
  return reports;
}

size_t FuzzReport::sum_flips() {
  size_t sum = 0;
  for(auto report : reports) {
    sum += report.sum_flips();
  }
  return sum;
}

void FuzzReport::add_report(LocationReport report) {
  reports.push_back(report);
}

FuzzingParameterSet FuzzReport::get_fuzzing_params() {
  return parameters;
}
