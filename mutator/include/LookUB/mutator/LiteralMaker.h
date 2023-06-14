#ifndef LITERALMAKER_H
#define LITERALMAKER_H

#include "UnsafeMutatorBase.h"

/// Creates literal strings for specific types.
struct LiteralMaker : UnsafeMutatorBase {

  LiteralMaker(MutatorData &input);

  /// Returns a random constant literal of the given type.
  Statement makeConstant(const StatementContext &, TypeRef t);

private:
  /// Returns a valid constant string for the given type.
  std::string makeConstantStr(TypeRef tref);

  /// Known 'special' integers.
  ///
  /// Mostly powers of two and neighbors.
  std::vector<std::string> specialIntegers;
  /// Known 'special' floats.
  ///
  /// Mostly powers of two and neighbors.
  std::vector<std::string> specialFloats;

  /// Sets up the internal integer value list.
  void setupIntegers();
  /// Sets up the internal float value list.
  void setupFloats();
};

#endif // LITERALMAKER_H
