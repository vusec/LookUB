#include "LookUB/mutator/UnsafeStrategy.h"

#include <map>

typedef UnsafeStrategy::Frag Frag;

static UnsafeStrategy getBaseUnsafeStrat(int scale) {
  UnsafeStrategy strat("mutate x " + std::to_string(scale));
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
  strat.scale = scale;
  return strat;
}

static UnsafeStrategy getBaseReductionStrat(int scale = 1) {
  UnsafeStrategy res("reduce x " + std::to_string(scale));
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
  res.scale = scale;
  return res;
}

std::vector<UnsafeStrategy> UnsafeStrategy::makeMutateStrategies() {
  auto builtinStrat = getBaseUnsafeStrat(1);
  builtinStrat.set(Frag::CallBuiltin, 0.99);
  builtinStrat.set(Frag::UseSnippet, 0.07);
  builtinStrat.set(Frag::MutateFuncAttrs, 0.01);
  builtinStrat.set(Frag::CleanupCompound, 0.01);
  builtinStrat.set(Frag::DeleteCompoundStmts, 0.01);
  builtinStrat.set(Frag::UseMutatedStmtAsChild, 0.4);
  builtinStrat.set(Frag::PreferModifyingStmtsOverExprs, 0.99);
  builtinStrat.set(Frag::PromoteChild, 0.01);
  builtinStrat.set(Frag::PromoteChildren, 0.01);
  builtinStrat.set(Frag::WrapInCompound, 0.01);
  builtinStrat.set(Frag::SimplifyStmt, 0.01);
  builtinStrat.set(Frag::MutateFoundStatement, 0.99);
  builtinStrat.set(Frag::MutateOverDelete, 0.99);
  builtinStrat.set(Frag::MutateCompound, 0.9);
  builtinStrat.set(Frag::ForceCallBuiltinStmt, 1.0f);
  builtinStrat.setName("call builtin function (stmt)");

  UnsafeStrategy builtinExprStrat = builtinStrat;
  builtinExprStrat.set(Frag::PreferModifyingStmtsOverExprs, 0.3);

  std::vector<UnsafeStrategy> result = {
      getBaseUnsafeStrat(1), getBaseUnsafeStrat(3),    getBaseUnsafeStrat(6),
      getBaseUnsafeStrat(9), getBaseReductionStrat(1), builtinStrat,
      builtinExprStrat};
  return result;
}

std::vector<UnsafeStrategy> UnsafeStrategy::makeReductionStrategies() {
  return {getBaseReductionStrat()};
}
