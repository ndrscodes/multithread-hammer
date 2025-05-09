
#include "DRAMAddr.hpp"
#include <cstddef>
enum init_pattern {
  FENCE, ONE, ZERO
};

class Allocation {
  private:
    size_t pages;
    size_t size;
    void* start;
    bool initialized = false;
    init_pattern pattern = FENCE;
  public:
    Allocation();
    void* allocate();
    void initialize(init_pattern pattern);
    static void initialize(init_pattern pattern, void *page_start, size_t size);
    static char get_fill_value(init_pattern pattern);
    init_pattern get_init_pattern();
    bool is_valid(DRAMAddr address);
    bool is_valid(void *address);
    const void* get_start_address();
    void* get_end_address();
    size_t get_size();
    ~Allocation();
};
