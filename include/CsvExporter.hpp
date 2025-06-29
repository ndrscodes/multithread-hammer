
#include "BitFlip.hpp"
class CsvExporter {
private:
  std::string path;
  FILE *logfile;
public:
  void export_flip(BitFlip &flip, int run, int location, int pattern);
  CsvExporter(std::string filepath);
  ~CsvExporter();
};
