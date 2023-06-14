#ifndef SIMPLIFIER_H
#define SIMPLIFIER_H

#include "LiteralMaker.h"
#include "StatementContext.h"
#include "UnsafeMutatorBase.h"

/// Performs simplification of code segments.
///
/// All modifiations here are expected to produce
/// less (== 'simpler') code.
class Simplifier : UnsafeMutatorBase {
  typedef Statement::Kind StmtKind;
  LiteralMaker literals;

public:
  Simplifier(MutatorData &input);

  /// Simplifies the given statement in-place.
  /// @return True if the code was simplified.
  bool simplifyStmt(StatementContext context, const Statement &parent,
                    Statement &s);

  /// Simplifies the given compound in-place.
  /// @return True if the code was simplified.
  bool simplifyCompound(Statement &s);
};

#endif // SIMPLIFIER_H
