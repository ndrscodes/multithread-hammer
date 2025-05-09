#include "Allocation.hpp"
#include "DRAMAddr.hpp"
#include <cassert>
#include <cstring>
#include <sys/mman.h>
#define GB(n) 1024*1024*1024*n
#define FENCE_EVEN 0xAA


Allocation::Allocation() {
  initialized = false;
  size = 0;
  start = nullptr;
}

char Allocation::get_fill_value(init_pattern pattern) {
  switch(pattern) {
    case FENCE:
      return FENCE_EVEN;
    case ONE:
      return 0xFF;
    case ZERO:
      return 0x0;
    default:
      return FENCE_EVEN;
  }
}

void Allocation::initialize(init_pattern pattern, void *page_start, size_t size) {
  memset(page_start, get_fill_value(pattern), size);
}

void* Allocation::allocate() {
  size = GB(1);
  start = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB | MAP_HUGE_1GB | MAP_POPULATE, -1, 0);
  assert(start != MAP_FAILED);
  initialized = true;
  return start;
}

void Allocation::initialize(init_pattern p) {
  pattern = p;
  initialize(pattern, start, size);
}

size_t Allocation::get_size() {
  return size;
}

const void* Allocation::get_start_address() {
  return start;
}

init_pattern Allocation::get_init_pattern() {
  return pattern;
}

void* Allocation::get_end_address() {
  return (char*)start + size;
}

bool Allocation::is_valid(DRAMAddr address) {
  return is_valid(address.to_virt());
}

bool Allocation::is_valid(void *address) {
  return address >= get_start_address() && address < get_end_address();
}

Allocation::~Allocation() {
  munmap(start, size);
}
