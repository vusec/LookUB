#pragma once

#include "UnsafeStrategy.h"
#include "scc/program/Program.h"
#include "scc/utils/Error.h"

/// Program generator/mutate class that works on 'unsafe' (=buggy) programs.
class UnsafeGenerator {
public:
  typedef UnsafeStrategy Strategy;

  /// Returns a new simple program.
  std::unique_ptr<Program> generate(RngSource source,
                                    LangOpts opts = LangOpts());

  /// Mutates the given program with decisions decided by the given strategy.
  ///
  /// scaleMul can be an integer > 0 that decides if the mutation process
  /// should be repeated multiple times.
  std::vector<Strategy::Frag> mutate(Program &p, RngSource source,
                                     const Strategy &strat, unsigned scaleMul);

  /// Performs a single reduction step on the given program.
  std::vector<Strategy::Frag> reduce(Program &p, RngSource source,
                                     const Strategy &strat);

  /// Returns a string that is prepended to the printed program code.
  static std::string getProgramPrefix(const Program &p);
  /// Returns a string that is appended to the printed program code.
  static std::string getProgramSuffix(const Program &p);

  /// Hook that allows modifying the oracle command passed from the user.
  /// @param exePath The path to the current fuzzer binary.
  /// @param cmd The command line specified by the user.
  /// @param seed The seed that should be used to seed Rngs.
  static std::string expandEvalCommand(std::string exePath, std::string cmd,
                                       RngSource source) {
    return cmd;
  }

  /// Returns a random program from the provided entrophy.
  std::unique_ptr<Program> generateFromEntrophy(EntrophyVec entrophy,
                                                UnsafeStrategy strat,
                                                LangOpts opts = LangOpts());

  /// Handle custom command line arguments.
  OptError handleArgs(std::vector<std::string> arg) { return {}; }
};
