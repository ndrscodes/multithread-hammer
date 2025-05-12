#pragma once
#include "DRAMAddr.hpp"
#include <cstddef>

class Allocation {
  private:
    size_t pages;
    size_t size;
    void* start;
    bool initialized = false;
  public:
    Allocation();
    void* allocate();
    void initialize();
    bool is_valid(DRAMAddr address);
    bool is_valid(void *address);
    const void* get_start_address();
    void* get_end_address();
    size_t get_size();
    size_t find_flips(void* start, void* end);
    size_t find_flips(void* addr);
    size_t find_flips(DRAMAddr addr);
    ~Allocation();
};
