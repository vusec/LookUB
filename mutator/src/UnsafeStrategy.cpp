#include "LookUB/mutator/UnsafeStrategy.h"

#include <map>

typedef UnsafeStrategy::Frag Frag;

static UnsafeStrategy getBaseUnsafeStrat(std::string name) {
  UnsafeStrategy strat(name);
  strat.set(Frag::CallBuiltin, 0.2f);
  strat.set(Frag::CatchAll, 0.2f);
  strat.set(Frag::CleanupCompound, 0.2f);
  strat.set(Frag::CreateFuncPtrType, 0.05f);
  strat.set(Frag::CreateNewType, 0.5f);
  strat.set(Frag::DeleteFuncAttrs, 0.4f);
  strat.set(Frag::DeleteStmtInCompound, 0.3f);
  strat.set(Frag::DeleteTypes, 0.2f);
  strat.set(Frag::DontFillArrayConstant, 0.5f);
  strat.set(Frag::EmitStringLiteral, 0.5f);
  strat.set(Frag::EmptyCompound, 0.02f);
  strat.set(Frag::EnsureReturnInFunc, 0.96f);
  strat.set(Frag::MutateCompound, 0.5f);
  strat.set(Frag::PromoteChild, 0.1f);
  strat.set(Frag::PromoteChildren, 0.1f);
  strat.set(Frag::WrapInCompound, 0.1f);
  strat.set(Frag::MutateFuncAttrs, 0.005f);
  strat.set(Frag::UseNonStdCallingConv, 0.4f);
  strat.set(Frag::InitWithFuncAttrs, 0.01f);
  strat.set(Frag::DeleteCompoundStmts, 0.01f);
  strat.set(Frag::SimplifyStmt, 0.02f);
  strat.set(Frag::PickPtrOverInt, 0.8f);
  strat.set(Frag::UseSnippet, 0.03f);
  strat.set(Frag::AssignExprToVar, 0.9f);
  strat.set(Frag::InitGlobal, 0.8f);
  strat.set(Frag::MutateFunction, 1);
  strat.set(Frag::MutateGlobal, 0.05f);
  strat.set(Frag::ChangeIdentifier, 0.001f);
  strat.set(Frag::RegenerateProgram, 0.02f);
  strat.set(Frag::FixMainReturn, 0.9f);
  return strat;
}

static UnsafeStrategy getBaseReductionStrat(int scale = 1) {
  UnsafeStrategy res("reduce");
  for (float &v : res.getValueVecRef())
    v = 0.05f;
  res.set(Frag::MutateOverDelete, 0.8f);
  res.set(Frag::MutateFuncAttrs, 0.003f);
  res.set(Frag::DeleteFuncAttrs, 0.3f);
  for (auto f :
       {Frag::CleanupCompound, Frag::DeleteStmtInCompound, Frag::DeleteTypes,
        Frag::SimplifyStmt, Frag::EmptyCompound, Frag::DeleteCompoundStmts}) {
    res.set(f, 0.2f);
  }
  return res;
}

std::vector<UnsafeStrategy> UnsafeStrategy::makeMutateStrategies() {
  std::vector<UnsafeStrategy> result = {
      getBaseUnsafeStrat("generic mutate"),
      getBaseReductionStrat()
  };

  const float nearlyAlways = 0.96f;
  const float never = 0.01f;

  {
    UnsafeStrategy strat = getBaseUnsafeStrat("mutate function attributes");
    strat.set(Frag::MutateFunction, nearlyAlways);
    strat.set(Frag::MutateGlobal, never);
    strat.set(Frag::MutateFuncAttrs, nearlyAlways);
    result.push_back(strat);
  }

  {
    UnsafeStrategy strat = getBaseUnsafeStrat("mutate global variable");
    strat.set(Frag::MutateGlobal, nearlyAlways);
    strat.set(Frag::MutateFunction, never);
    result.push_back(strat);
  }

  {
    UnsafeStrategy strat = getBaseUnsafeStrat("mutate stmt");
    strat.set(Frag::MutateFunction, nearlyAlways);
    strat.set(Frag::MutateGlobal, never);
    strat.set(Frag::MutateFuncAttrs, never);
    strat.set(Frag::PreferModifyingStmtsOverExprs, nearlyAlways);
    result.push_back(strat);
  }

  {
    UnsafeStrategy strat = getBaseUnsafeStrat("mutate expr");
    strat.set(Frag::MutateFunction, nearlyAlways);
    strat.set(Frag::MutateGlobal, never);
    strat.set(Frag::MutateFuncAttrs, never);
    strat.set(Frag::PreferModifyingStmtsOverExprs, never);
    result.push_back(strat);
  }

  {
    UnsafeStrategy strat = getBaseUnsafeStrat("mutate types");
    strat.set(Frag::MutateOverDelete, never);
    strat.set(Frag::ReorderOverDelete, never);
    strat.set(Frag::MutateTypes, nearlyAlways);
    result.push_back(strat);
  }

  {
    UnsafeStrategy strat = getBaseUnsafeStrat("reorder types");
    strat.set(Frag::ReorderOverDelete, nearlyAlways);
    result.push_back(strat);
  }
  return result;
}

std::vector<UnsafeStrategy> UnsafeStrategy::makeReductionStrategies() {
  return {getBaseReductionStrat()};
}
