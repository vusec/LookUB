#ifndef CANONICALIZER_H
#define CANONICALIZER_H

#include "scc/program/Statement.h"

#include <optional>

/// Transforms code into its 'canonical' form. This removes redundant parts of
/// code that don't add any meaning. E.g. `{{code}}` becomes `{code}` as the
/// extra compound statement doesn't add anything useful.
class Canonicalizer {
  static std::optional<Statement> canonicalize(const Statement &s);

public:
  /// Tries to simplify the given code without changing any semantics.
  /// Returns none if the code can't be simplified.
  static std::optional<Statement> canonicalizeStmt(const Statement &s) {
    auto res = canonicalize(s);
    // TODO: Add some sanity checks here?
    return res;
  }
};

#endif // CANONICALIZER_H
