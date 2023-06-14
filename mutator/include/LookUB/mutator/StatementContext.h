#ifndef STATEMENTCONTEXT_H
#define STATEMENTCONTEXT_H

#include "scc/program/Function.h"
#include "scc/program/Statement.h"
#include "scc/program/Variable.h"
#include <unordered_map>
#include <vector>

/// Provides information that helps with transforming/inspecting a statement.
///
/// The information here  is usually information that is not stored  in the
/// statement itself (for space/complexity reasons).
struct StatementContext {
  std::unordered_map<NameID, Variable> variables;
  std::optional<Variable> getVar(NameID id) {
    auto iter = variables.find(id);
    if (iter == variables.end())
      return {};
    return iter->second;
  }

  /// Whether the statement is within a loop.
  bool inLoop = false;
  /// The return type of the containing function.
  TypeRef returnType = Void();

  const Function *function = nullptr;

  /// Returns the function body of the containing
  /// function. If we're in the global scope it
  /// returns an empty statement to simplify
  /// usage.
  const Statement &getFuncBody() const {
    if (function)
      return function->getBody();
    return fakeBody;
  }

  TypeRef getRetType() const { return returnType; }

  StatementContext(const Function &f) : function(&f) {}

  /// Expands the context with the information added
  /// by the given statement. E.g., a variable declaration
  /// will add a mapping from name to variable to
  /// the context.
  void expandWithStmt(const Statement &s) {
    if (s.getKind() == Statement::Kind::VarDecl ||
        s.getKind() == Statement::Kind::VarDef) {
      Variable v(s.getVariableType(), s.getDeclaredVarID());
      variables[v.getName()] = v;
    }
  }

  /// Returns the StatementContext for a global
  /// variable initializer.
  static StatementContext Global() { return StatementContext(); }

private:
  StatementContext() = default;
  Statement fakeBody;
};
#endif // STATEMENTCONTEXT_H
