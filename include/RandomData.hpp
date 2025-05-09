#pragma once
#include <random>

static std::mt19937 engine;

static bool get_bool(double_t propability) {
  std::uniform_real_distribution<> dist(0, 1);
  return dist(engine) > 1 - propability;
}

static bool get_bool() {
  return get_bool(0.5);
}
