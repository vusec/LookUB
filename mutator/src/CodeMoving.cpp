#include "LookUB/mutator/CodeMoving.h"
#include "scc/program/Function.h"

UnsafeMutatorBase::Modified
CodeMoving::outlineStatement(const StatementContext &ctx, Statement &s) {
  TypeRef returnT = Void();
  if (s.isExpr())
    returnT = s.getEvalType();
  auto f = std::make_unique<Function>(
      returnT, p.getIdents().makeNewID("outlined"), std::vector<Variable>{});

  Statement newBody = s;
  if (s.isExpr())
    newBody = Statement::Return(newBody);
  f->setBody(newBody);

  const bool isStmt = s.isStmt();
  s = Statement::Call(returnT, f->getNameID(), {});
  if (isStmt)
    s = Statement::StmtExpr(s);

  p.add(std::move(f));

  return Modified::Yes;
}

UnsafeMutatorBase::Modified CodeMoving::inlineCall(const StatementContext &ctx,
                                                   Statement &call) {
  if (call.getKind() != Statement::Kind::Call)
    return Modified::No;
  NameID calledFuncID = call.getCalledFuncID();
  Function *f = p.getFunctionWithID(calledFuncID);
  if (f == nullptr || f->isExternal())
    return Modified::No;
  // TODO: Handle variables.
  call = f->getBody();
  return Modified::Yes;
}
