#include "LookUB/mutator/Canonicalizer.h"

typedef Statement::Kind StmtKind;

static bool hasVarDecls(const Statement &s) {
  SCCAssert(s.getKind() == StmtKind::Compound, "Can only scan compounds");
  for (const auto &c : s.getChildren()) {
    if (c.getKind() == StmtKind::VarDecl)
      return true;
    if (c.getKind() == StmtKind::VarDef)
      return true;
  }
  return false;
}

std::optional<Statement> Canonicalizer::canonicalize(const Statement &s) {
  const auto &children = s.getChildren();
  switch (s.getKind()) {
  case StmtKind::Compound: {

    bool hasChanges = false;

    std::vector<Statement> newChildren;
    for (const Statement &child : s) {
      if (child.getKind() == StmtKind::Empty)
        continue;

      auto newChild = child;
      if (auto canonChild = canonicalize(child)) {
        newChild = *canonChild;
        hasChanges = true;
      }

      if (newChild.getKind() == StmtKind::Compound && !hasVarDecls(s)) {
        for (const Statement &nestedChild : newChild)
          newChildren.push_back(nestedChild);
        hasChanges = true;
      } else
        newChildren.push_back(newChild);
    }

    if (!hasChanges)
      break;

    return Statement::CompoundStmt(newChildren);
  }
  case StmtKind::If: {
    auto newChild = canonicalize(children.at(1));
    if (newChild)
      return Statement::If(children.at(0), *newChild);
    break;
  }
  case StmtKind::While: {
    auto newChild = canonicalize(children.at(1));
    if (newChild)
      return Statement::While(children.at(0), *newChild);
    break;
  }
  case StmtKind::Try: {
    std::vector<Statement> newChildren;
    for (const Statement &child : s) {

      auto newChild = child;
      if (auto canonChild = canonicalize(child))
        newChild = *canonChild;

      newChildren.push_back(newChild);
    }
    Statement tryBody = newChildren.front();
    newChildren.erase(newChildren.begin());
    return Statement::Try(tryBody, newChildren);
  }
  case StmtKind::Catch: {
    auto newChild = canonicalize(children.at(0));
    if (newChild)
      return Statement::Catch(s.getVariableType(), s.getDeclaredVarID(),
                              *newChild);
    break;
  }
  default:
    break;
  }
  return {};
}
