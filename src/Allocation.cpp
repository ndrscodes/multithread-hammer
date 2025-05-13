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
  for(size_t i = 0; i < pages; i++) {
    srand(i);
    for(size_t j = 0; j < page_size; j++) {
      char *target = (char *)start + i * page_size + j;
      if(target > (char *)get_end_address()) {
        break;
      }
      *target = rand() % 256;
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

size_t Allocation::find_flips(void *start_addr, void *end_addr) {
  size_t init_pagesize = DRAMConfig::get().row_to_row_offset();
  char *start_c = (char *)((uint64_t)start_addr / init_pagesize * init_pagesize);
  char *end_c = (char *)((uint64_t)end_addr / init_pagesize * init_pagesize);

  size_t pages = (end_c - start_c) / init_pagesize;
  size_t flips = 0;
  for(size_t p = 0; p < pages; p++) {
    if(start_c > (char *)get_end_address()) {
      return 0;
    }

    size_t page_size = init_pagesize;
    if(start_c + page_size > get_end_address()) {
      page_size = (char *)get_end_address() - start_c;
    }

    char *compare_page = (char *)malloc(page_size);
   
    size_t seed = (start_c - (char *)start) / init_pagesize;
    srand(seed);
    
    for(size_t i = 0; i < page_size; i++) {
      compare_page[i] = rand() % 256;
    }

    if(memcmp(compare_page, start_c, page_size) != 0) {
      printf("Flip detected on page %lu. Analyzing.\n", p);
      for(size_t i = 0; i < page_size; i++) {
        char v = compare_page[i];
        char actual = *((char *)start_c + i);
        if(v == actual) {
          continue;
        }
        for(size_t j = 0; j < 8; j++) {
          if(((actual >> j) & 0x1) != ((v >> j) & 0x1)) {
            flips++;
            printf("[FLIP] address: %p, page: %lu, seed: %lu, offset: %lu, int offset: %lu, from: %x, to: %x, bit from %b to %b\n", 
                   start_c + i,
                   p,
                   seed,
                   i,
                   j,
                   v,
                   actual,
                   (v >> j) & 1,
                   (actual >> j) & 1);
          }
        }
      }
    }

    start_c += init_pagesize;
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
