#ifndef CODEMOVING_H
#define CODEMOVING_H

#include "UnsafeMutatorBase.h"

/// Mutations that move code around the program.
///
/// E.g. outlining and inlining of functions.
class CodeMoving : public UnsafeMutatorBase {
public:
  CodeMoving(UnsafeMutatorBase::MutatorData &input)
      : UnsafeMutatorBase(input) {}
  // TODO: unused.
  Modified outlineStatement(const StatementContext &ctx, Statement &s);
  // TODO: unused.
  Modified inlineCall(const StatementContext &ctx, Statement &call);
};

#endif // CODEMOVING_H
