#include "HammerSuite.hpp"
#include <cstddef>
#include <thread>
#include <x86intrin.h>

HammerSuite::HammerSuite(PatternBuilder &builder) : builder(builder){}

HammerSuite::HammerSuite(PatternBuilder &builder, std::initializer_list<Pattern> patterns) : builder(builder) {
  for(auto p : patterns) {
    this->patterns.insert({ current_pattern_id++, p });
  }
}

size_t HammerSuite::add_pattern(Pattern pattern) {
  patterns.insert({ current_pattern_id++ , pattern });
  return current_pattern_id;
}

size_t HammerSuite::remove_pattern(size_t id) {
  return patterns.erase(id);
}

size_t HammerSuite::hammer(size_t iterations) {
  std::vector<std::thread> threads;
  for(auto it = patterns.begin(); it != patterns.end(); it++) {
    size_t id = it->first;
    threads.push_back(std::thread(&HammerSuite::hammer_fn, this, id, std::ref(it->second), iterations));
  }
  for(auto &t : threads) {
    t.join();
  }

  size_t flips = 0;
  for(auto it = patterns.begin(); it != patterns.end(); it++) {
    flips += builder.check(it->second);
  }

  return flips;
}

void HammerSuite::hammer_fn(size_t id, Pattern &pattern, size_t iterations) {
  //this is likely faster than an std::vector because we can skip the size checks and allocation going on...
  //the processor is likely to cache this array so the lookup will be more or less instant
  size_t s = pattern.size();
  volatile char **virt_addrs = (volatile char **)malloc(sizeof(volatile char *) * s);
  for(size_t i = 0; i < s; i++) {
    virt_addrs[i] = (volatile char *)pattern.at(i).to_virt();
  }

  iterations /= s;

  for(size_t i = 0; i < iterations; i++) {
    for(size_t j = 0; j < s; j++) {
      _mm_clflushopt((void *)virt_addrs[j]);
    }
    for(size_t j = 0; j < s; j++) {
      *virt_addrs[j];
    }
  }
}
