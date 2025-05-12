#include "Allocation.hpp"
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#define GB(n) 1024*1024*1024*n


Allocation::Allocation() {
  initialized = false;
  size = 0;
  start = nullptr;
}

void Allocation::initialize() {
  size_t page_size = DRAMConfig::get().row_to_row_offset();
  size_t pages = ((char *)get_end_address() - (char *)get_start_address()) / page_size;
  page_size /= sizeof(int);
  for(size_t i = 0; i < pages; i++) {
    srand(i);
    for(size_t j = 0; j < page_size; j++) {
      int *target = (int *)start + i * page_size + j;
      if(target > (int *)start) {
        break;
      }
      *target = rand();
    }
  }
}

void* Allocation::allocate() {
  size = GB(1);
  start = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB | MAP_HUGE_1GB | MAP_POPULATE, -1, 0);
  assert(start != MAP_FAILED);
  initialized = true;
  return start;
}

size_t Allocation::get_size() {
  return size;
}

const void* Allocation::get_start_address() {
  return start;
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

size_t Allocation::find_flips(void *start, void *end) {
  int *start_i = (int *)start;
  int *end_i = (int *)end;
  size_t init_pagesize = DRAMConfig::get().row_to_row_offset();

  size_t pages = (end_i - start_i) / init_pagesize;
  size_t flips = 0;
  for(size_t p = 0; p < pages; p++) {
    if(start_i > (int *)get_end_address()) {
      return 0;
    }

    size_t page_size = init_pagesize;
    if(start_i + page_size > get_end_address()) {
      page_size = (int *)get_end_address() - start_i;
    }
    page_size -= page_size % sizeof(int);

    int *compare_page = (int *)malloc(page_size);
    
    size_t seed = (start_i - (int *)get_start_address()) / page_size;
    srand(seed);
    
    for(size_t i = 0; i < page_size / sizeof(int); i++) {
      compare_page[i] = rand();
    }

    srand(seed);

    if(memcmp(compare_page, start_i, page_size) != 0) {
      printf("Flip detected on page %lu. Analyzing.\n", seed);
      for(size_t i = 0; i < page_size / sizeof(int); i++) {
        int v = compare_page[i];
        int actual = *((int *)start_i);
        if(v == actual) {
          continue;
        }
        for(size_t j = 0; j < sizeof(int) * 8; j++) {
          if(((actual >> j) & 0x1) != ((v >> j) & 0x1)) {
            flips++;
            printf("[FLIP] address: %p, page: %lu, offset: %lu, int offset: %lu", 
                   &compare_page[i],
                   seed,
                   i * sizeof(int),
                   j);
          }
        }
      }
    }

    start_i += init_pagesize;
  }

  return flips;
}

size_t Allocation::find_flips(void *addr) {
  return find_flips(addr, (char *)addr + DRAMConfig::get().row_to_row_offset());
}

size_t Allocation::find_flips(DRAMAddr addr) {
  return find_flips(addr.to_virt());
}

Allocation::~Allocation() {
  munmap(start, size);
}
