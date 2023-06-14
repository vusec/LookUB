#ifndef SNIPPETS_H
#define SNIPPETS_H

#include "LiteralMaker.h"
#include "TypeCreator.h"
#include "UnsafeMutatorBase.h"

/// Creates random code from a predefined set of code snippets.
class Snippets : UnsafeMutatorBase {
  LiteralMaker literals;
  TypeCreator tc;
  Statement createSnippetImpl(const StatementContext &s);

public:
  Snippets(MutatorData &input)
      : UnsafeMutatorBase(input), literals(input), tc(input) {}

  /// Create a random predefined piece of code.
  Statement createSnippet(const StatementContext &s) {
    Statement res = createSnippetImpl(s);
    p.verifySelf();
    return res;
  }
};

#endif // SNIPPETS_H
