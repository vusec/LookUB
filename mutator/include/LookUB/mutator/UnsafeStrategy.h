#ifndef UNSAFE_STRATEGY_H
#define UNSAFE_STRATEGY_H

#include "scc/mutator-utils/StrategyBase.h"
#include <vector>

enum class UnsafeFrag {
#define DECISION(ID) ID,
#include "UnsafeDecisions.def"
};

/// Describes how likely certain mutations should be done.
///
/// 'Unsafe' because the mutations of this strategy probably introduce bugs
/// into programs.
struct UnsafeStrategy : StrategyBase<UnsafeStrategy, UnsafeFrag> {
  typedef UnsafeFrag Frag;
  /// A fragment of our strategy. Essentially a single
  /// decision made by the mutator mapped to how likely
  /// it is to be taken.
  static constexpr float defaultVal = 0.5f;
  UnsafeStrategy(std::string name = "default") {
#define DECISION(ID) values.push_back(defaultVal);
#include "UnsafeDecisions.def"
    this->name = name;
  }

  static std::vector<Frag> getAllFragments() {
    return {
#define DECISION(ID) Frag::ID,
#include "UnsafeDecisions.def"
    };
  }

  /// Returns a list of mutation strategies.
  static std::vector<UnsafeStrategy> makeMutateStrategies();

  static std::vector<UnsafeStrategy> makeReductionStrategies();

  /// Returns the user-readable string for a decision.
  std::string getFragName(Frag f) const {
#define DECISION(ID)                                                           \
  if (f == Frag::ID)                                                           \
    return #ID;
#include "UnsafeDecisions.def"
    return "INVALID";
  }
};

#endif // UNSAFE_STRATEGY_H
