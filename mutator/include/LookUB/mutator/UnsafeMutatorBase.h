#ifndef UNSAFEMUTATORBASE_H
#define UNSAFEMUTATORBASE_H

#include "StatementContext.h"
#include "UnsafeStrategy.h"
#include "scc/mutator-utils/MutatorBase.h"
#include "scc/mutator-utils/Rng.h"
#include "scc/mutator-utils/StrategyInstance.h"
#include "scc/program/Statement.h"

typedef StrategyInstance<UnsafeStrategy> UnsafeInstance;

/// Base class for different parts of the 'unsafe' way to mutate programs.
struct UnsafeMutatorBase : MutatorBase<UnsafeStrategy> {
  UnsafeMutatorBase(MutatorData &i) : MutatorBase(i) {}

  /// Returns true if the given var decl is references anywhere in the given
  /// parent statement.
  bool isVarUsed(const Statement &parent, const Statement &varDecl) {
    return parent.forAllChildren([&varDecl](const Statement &other) {
      return other == Stmt::Kind::LocalVarRef &&
             other.getReferencedVarID() == varDecl.getDeclaredVarID();
    });
  }

  /// Returns true if we can directly modify the given statement.
  bool canMutate(const StatementContext &context, const Statement &parent,
                 const Statement &s) {
    // Don't remove catch statements. They need to be
    // removed via removing/changing the try otherwise we end up
    // with weird code like this:
    // try {}
    // /*removed catch=*/;
    // catch (...) {}
    if (parent.getKind() == Statement::Kind::Try &&
        s.getKind() == Statement::Kind::Catch)
      return false;

    if (s.getKind() == Statement::Kind::GotoLabel) {
      // For GotoLabels, check all goto's to make sure it is not leaving
      // them without a valid target.
      if (!context.getFuncBody().forAllChildren([&context](const Statement &s) {
            if (s.getKind() != Statement::Kind::GotoLabel)
              return true;
            return !context.getFuncBody().usesID(s.getJumpTarget());
          }))
        return false;
    }

    // Array constants have weird language rules where they can happen, so
    // avoid messing with them directly
    if (s.getKind() == Statement::Kind::ArrayConstant)
      return false;

    return true;
  }

  /// Potentially assigns an expression to a variable.
  ///
  /// This is so that we make it more likely for the result to not just be
  /// optimized away because it might be used by code later on.
  Statement wrapExprInStmt(Statement c) {
    SCCAssert(c.isExpr(), "Already a statement?");
    if (c.getEvalType() != Void() && decision(Frag::AssignExprToVar))
      return Statement::VarDef(c.getEvalType(), newID(), c);
    return Statement::StmtExpr(c);
  }
};

#endif // UNSAFEMUTATORBASE_H
