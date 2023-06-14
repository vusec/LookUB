#include "LookUB/mutator/Simplifier.h"

Simplifier::Simplifier(MutatorData &input)
    : UnsafeMutatorBase(input), literals(input) {}

bool Simplifier::simplifyStmt(StatementContext context, const Statement &parent,
                              Statement &s) {
  if (!canMutate(context, parent, s))
    return false;
  if (s.getKind() == StmtKind::Compound && decision(Frag::EmptyCompound)) {
    s = Statement::CompoundStmt({});
    p.verifySelf();
    return true;
  }

  if (simplifyCompound(s))
    return true;

  if (s.getEvalType() == Void()) {
    s = Statement::Empty();
    p.verifySelf();
    return true;
  }
  s = literals.makeConstant(context, s.getEvalType());
  return true;
}

bool Simplifier::simplifyCompound(Statement &s) {
  if (s.getKind() != StmtKind::Compound)
    return false;
  if (decision(Frag::CleanupCompound)) {
    std::vector<Statement> cleanChildren;
    for (const Statement &c : s) {
      if (!c.isEmpty())
        cleanChildren.push_back(c);
    }
    if (cleanChildren.size() == s.getNumChildren())
      return false;
    s = Statement::CompoundStmt(cleanChildren);
    p.verifySelf();
    return true;
  }
  if (!decision(Frag::DeleteCompoundStmts))
    return false;

  std::vector<Statement> cleanChildren;
  for (const Statement &c : s)
    if (!decision(Frag::DeleteStmtInCompound))
      cleanChildren.push_back(c);
  if (cleanChildren.empty())
    cleanChildren = {Statement::Empty()};

  if (cleanChildren.size() == s.getNumChildren())
    return false;
  s = Statement::CompoundStmt(cleanChildren);
  p.verifySelf();
  return true;
}
