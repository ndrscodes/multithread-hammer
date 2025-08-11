#include "CsvExporter.hpp"
#include "BitFlip.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>

CsvExporter::CsvExporter(std::string filepath) : path(filepath) {
  logfile = fopen(path.c_str(), "w");
  if(logfile == nullptr) {
    printf("unable to open log file \"%s\"\n", path.c_str());
  }
}

CsvExporter::~CsvExporter() {
  fflush(logfile);
  fclose(logfile);
}

void CsvExporter::export_flip(BitFlip &flip, int run, int location, int pattern, int n_patterns, int n_aggs, int n_accesses, std::chrono::duration<float_t> duration) {
  fprintf(logfile, "%d;%d;%d;%d;%d;%d;%lu;%lu;%lu;%lu;%lu;%lu\n",
          run,
          location,
          pattern,
          n_patterns,
          n_aggs,
          n_accesses,
          std::chrono::duration_cast<std::chrono::microseconds>(duration).count(),
          flip.address.get_bank(),
          flip.address.get_row(),
          flip.address.get_column(),
          flip.count_o2z_corruptions(),
          flip.count_z2o_corruptions());
}
