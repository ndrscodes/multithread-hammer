#include "Enums.hpp"

#include <cassert>
#include <map>
#include <Range.hpp>

std::string to_string(FLUSHING_STRATEGY strategy) {
  std::map<FLUSHING_STRATEGY, std::string> map =
      {
          {FLUSHING_STRATEGY::EARLIEST_POSSIBLE, "EARLIEST_POSSIBLE"},
          {FLUSHING_STRATEGY::LATEST_POSSIBLE, "LATEST_POSSIBLE"}
      };
  return map.at(strategy);
}

void from_string(const std::string &strategy, FLUSHING_STRATEGY &dest) {
  std::map<std::string, FLUSHING_STRATEGY> map =
      {
          {"EARLIEST_POSSIBLE", FLUSHING_STRATEGY::EARLIEST_POSSIBLE},
          {"LATEST_POSSIBLE", FLUSHING_STRATEGY::LATEST_POSSIBLE}
      };
  dest = map.at(strategy);
}

std::string to_string(FENCING_STRATEGY strategy) {
  std::map<FENCING_STRATEGY, std::string> map =
      {
          {FENCING_STRATEGY::LATEST_POSSIBLE, "LATEST_POSSIBLE"},
          {FENCING_STRATEGY::EARLIEST_POSSIBLE, "EARLIEST_POSSIBLE"},
          {FENCING_STRATEGY::OMIT_FENCING, "OMIT_FENCING"}
      };
  return map.at(strategy);
}

void from_string(const std::string &strategy, FENCING_STRATEGY &dest) {
  std::map<std::string, FENCING_STRATEGY> map =
      {
          {"LATEST_POSSIBLE", FENCING_STRATEGY::LATEST_POSSIBLE},
          {"EARLIEST_POSSIBLE", FENCING_STRATEGY::EARLIEST_POSSIBLE},
          {"OMIT_FENCING", FENCING_STRATEGY::OMIT_FENCING}
      };
  dest = map.at(strategy);
}

[[maybe_unused]] std::pair<FLUSHING_STRATEGY, FENCING_STRATEGY> get_valid_strategy_pair() {
  auto valid_strategies = get_valid_strategies();
  auto num_strategies = valid_strategies.size();
  std::random_device rd;
  std::mt19937 gen(rd());
  auto strategy_idx = Range<size_t>(0, num_strategies - 1).get_random_number(gen);
  return valid_strategies.at(strategy_idx);
}

std::vector<std::pair<FLUSHING_STRATEGY, FENCING_STRATEGY>> get_valid_strategies() {
  return std::vector<std::pair<FLUSHING_STRATEGY, FENCING_STRATEGY>>({
      std::make_pair(FLUSHING_STRATEGY::EARLIEST_POSSIBLE, FENCING_STRATEGY::OMIT_FENCING),
      std::make_pair(FLUSHING_STRATEGY::EARLIEST_POSSIBLE, FENCING_STRATEGY::LATEST_POSSIBLE),
      std::make_pair(FLUSHING_STRATEGY::LATEST_POSSIBLE, FENCING_STRATEGY::LATEST_POSSIBLE),
  });
}

std::string to_string(SCHEDULING_POLICY policy) {
  switch (policy) {
    case SCHEDULING_POLICY::DEFAULT:
      return "DEFAULT";
    case SCHEDULING_POLICY::NONE:
      return "NONE";
    case SCHEDULING_POLICY::FULL:
      return "FULL";
    case SCHEDULING_POLICY::BASE_PERIOD:
      return "BASE_PERIOD";
    case SCHEDULING_POLICY::HALF_BASE_PERIOD:
      return "HALF_BASE_PERIOD";
    case SCHEDULING_POLICY::PAIR:
      return "PAIR";
    case SCHEDULING_POLICY::REPETITON:
      return "REPETITON";
    default:
      assert(false && "Unreachable.");
  }
}

std::string to_string(FENCE_TYPE type) {
  switch (type) {
    case FENCE_TYPE::NO_FENCE:
      return "NO_FENCE";
    case FENCE_TYPE::MFENCE:
      return "MFENCE";
    case FENCE_TYPE::LFENCE:
      return "LFENCE";
    case FENCE_TYPE::SFENCE:
      return "SFENCE";
    default:
      assert(false && "Unreachable.");
  }
}
