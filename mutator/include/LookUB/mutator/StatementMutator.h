#ifndef STATEMENTMUTATOR_H
#define STATEMENTMUTATOR_H

#include "CodeMoving.h"
#include "LiteralMaker.h"
#include "Simplifier.h"
#include "StatementCreator.h"
#include "UnsafeMutatorBase.h"

/// Mutates an existing statement in various ways.
class StatementMutator : public UnsafeMutatorBase {
  LiteralMaker literalMaker;
  Simplifier simplifier;
  StatementCreator sc;
  CodeMoving mover;

public:
  StatementMutator(MutatorData &input);

  /// Mutates a compound statement.
  ///
  /// Returns true iff the given statement was modified.
  bool mutateCompound(StatementContext context, Statement &s) {
    // This only modifies compounds.
    if (s.getKind() != StmtKind::Compound)
      return false;

    // The new child statements.
    std::vector<Statement> children;

    // Choose a random index to insert a new statement.
    size_t index = 0;
    const size_t insertAfter = getRng().getBelow(s.getNumChildren());
    for (const Statement &c : s) {
      // Make sure the code we generate down below knows about variables etc.
      context.expandWithStmt(c);
      children.push_back(c);
      if (index == insertAfter)
        children.push_back(sc.makeStmt(context));
      ++index;
    }

    // Form a new compound statement with the generated children.
    s = Statement::CompoundStmt(children);
    return true;
  }

  /// Replaces the given statement with a compound statement just containing
  /// its direct children.
  bool promoteChildren(Statement &s) {
    std::vector<Statement> newChildren;
    for (auto &c : s.getChildren())
      newChildren.push_back(ensureStmt(c));
    s = Statement::CompoundStmt(newChildren);
    return true;
  }

  /// Replaces the given statement with one of its child statements.
  bool promoteChild(Statement &s) {
    const auto &children = s.getChildren();
    if (children.empty())
      return false;
    Statement child = getRng().pickOneVec(children);
    s = ensureStmt(s);
    return true;
  }

  /// Surrounds the statement with random code.
  bool wrapInCompound(StatementContext context, Statement &s) {
    s = Statement::CompoundStmt(
        {sc.makeStmt(context), s, sc.makeStmt(context)});
    return true;
  }

  /// Randomly mutates the given statement.
  /// @param context The StatementContext that can be used.
  /// @param parent The parent of the given statement
  /// @param s The statement to mutate.
  /// @return Whether the statement has been modified.
  Modified mutateStatement(StatementContext context, const Statement &parent,
                           Statement &s) {
    auto verifyScope = p.queueVerify();

    // Check whether we can modify this statement.
    if (!canMutate(context, parent, s))
      return Modified::No;

    // Use compound specific mutations.
    if (decision(Frag::MutateCompound) && mutateCompound(context, s))
      return Modified::Yes;

    // Check for variable declarations/definitions.
    const bool isVar = s == Stmt::Kind::VarDecl || s == Stmt::Kind::VarDef;

    if (s.isStmt()) {
      if (isVar) {
        bool canBeDecl =
            !types.get(s.getVariableType()).expectsVarInitializer(types);
        // Swap a variable definition with a declaration.
        if (canBeDecl && decision(Frag::SwapDefAndDecl)) {
          if (s == Stmt::Kind::VarDecl) {
            s = Stmt::VarDef(s.getVariableType(), s.getDeclaredVarID(),
                             sc.makeExpr(context, s.getVariableType()));
          } else {
            s = Stmt::VarDecl(s.getVariableType(), s.getDeclaredVarID());
          }
          return Modified::Yes;
        }
        // if the declared variable is used anywhere, then we can't modify
        // further on the off-chance that we might delete it and render
        // the references to the variable invalid.
        if (isVarUsed(parent, s))
          return Modified::No;
      }

      // Try several generic mutation rules.
      if (decision(Frag::PromoteChild) && promoteChild(s))
        return Modified::Yes;
      if (decision(Frag::PromoteChildren) && promoteChildren(s))
        return Modified::Yes;
      if (decision(Frag::WrapInCompound) && wrapInCompound(context, s))
        return Modified::Yes;

      // Regenerate the statement. Before that, push it on a stack so that
      // the mutator might recycle it.
      sc.pushStmtOnStack(s);
      s = sc.makeStmt(context);
      return Modified::Yes;
    }

    // Regenerate an expression and overwrite the original code.
    SCCAssert(s.isExpr(), "Not an expression?");
    SCCAssert(types.isValid(s.getEvalType()), "invalid eval type?");
    s = sc.makeExpr(context, s.getEvalType());
    return Modified::Yes;
  }

  /// Returns the reconstructed StatementContext (e.g., what variables are in
  /// scope for the given statement).
  std::optional<StatementContext>
  rebuildStatementContextFor(StatementContext ctx, Statement &base,
                             Statement &target) {
    if (base.getKind() == StmtKind::While)
      ctx.inLoop = true;

    for (Statement &s : base) {
      if (&s == &target)
        return ctx;
      ctx.expandWithStmt(s);
      if (auto nested = rebuildStatementContextFor(ctx, s, target))
        return nested;
    }
    return ctx;
  }

  /// Mutates a random child of the given statement.
  Modified mutateRandomChild(StatementContext context, Statement &s) {
    // For simplicity, place an empty child in compound statements.
    // Note: This might lie about modification, but the empty statement doesn't
    // change any semantics so that is fine.
    if (s == Stmt::Kind::Compound && s.getNumChildren() == 0)
      s = Stmt::CompoundStmt({Stmt::Empty()});

    auto verifyScope = p.queueVerify();

    // Try simplifying compounds.
    if (simplifier.simplifyCompound(s))
      return Modified::Yes;

    // Pick a random child of the given statement.
    std::vector<Statement::StmtAndParent> allChildren = s.getAllChildren();
    if (allChildren.empty())
      return Modified::No;
    Statement::StmtAndParent toModify = getRng().pickOneVec(allChildren);

    // If we have an expression and we prefer modifying statements, then
    // try picking another one.
    while (toModify.stmt->getEvalType() != Void()) {
      if (!decision(Frag::PreferModifyingStmtsOverExprs))
        break;
      toModify = getRng().pickOneVec(allChildren);
    }

    // Build a statement context so we know which variables are in scope.
    context = *rebuildStatementContextFor(context, s, *toModify.stmt);

    // Try to just simplify the code and see if this helps.
    if (decision(Frag::SimplifyStmt))
      return simplifier.simplifyStmt(context, *toModify.parent, *toModify.stmt)
                 ? Modified::Yes
                 : Modified::No;
    // Otherwise, try mutating the statement.
    if (decision(Frag::MutateFoundStatement))
      return mutateStatement(context, *toModify.parent, *toModify.stmt);
    return Modified::No;
  }

  /// Mutates the body of the given function.
  Modified mutateFunctionBody(Function &f, Statement &newBody) {
    StatementContext context = sc.getContextForFunction(f);
    // Maybe try to just make a new function body.
    if (decision(Frag::RegenerateFunctionBody))
      newBody = sc.makeCompoundStmt(context);
    // Mutate a random piece of code in the function.
    if (mutateRandomChild(context, newBody) == Modified::No)
      return Modified::No;
    // Clean up any weird artifacts from the mutation process.
    if (auto canonicalized = Canonicalizer::canonicalizeStmt(newBody))
      newBody = *canonicalized;
    return Modified::Yes;
  }
};

#endif // STATEMENTMUTATOR_H
