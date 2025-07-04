#pragma once

#include "FuzzingParameterSet.hpp"
#include "HammeringPattern.hpp"
struct MappedPattern {
  HammeringPattern pattern;
  PatternAddressMapper mapper;
  FuzzingParameterSet params;
};
