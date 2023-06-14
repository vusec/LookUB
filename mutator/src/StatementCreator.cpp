#include "LookUB/mutator/StatementCreator.h"

StatementCreator::StatementCreator(MutatorData &input)
    : UnsafeMutatorBase(input), tc(input), fm(input), literalMaker(input),
      snippets(input) {}

std::vector<BuiltinFunctions::Kind> StatementCreator::callableBuiltins() const {
  return {
      BuiltinFunctions::Kind::Malloc, BuiltinFunctions::Kind::Free,
      BuiltinFunctions::Kind::Calloc, BuiltinFunctions::Kind::Realloc,
      BuiltinFunctions::Kind::Alloca, BuiltinFunctions::Kind::MemMove,
      BuiltinFunctions::Kind::MemCpy, BuiltinFunctions::Kind::MemChr,
      BuiltinFunctions::Kind::MemCmp, BuiltinFunctions::Kind::MemSet,
      BuiltinFunctions::Kind::StrCmp, BuiltinFunctions::Kind::StrNCmp,
      BuiltinFunctions::Kind::StrStr, BuiltinFunctions::Kind::StrCaseStr,
      BuiltinFunctions::Kind::StrCpy, BuiltinFunctions::Kind::StrNCpy,
      BuiltinFunctions::Kind::Strlen, BuiltinFunctions::Kind::StrNlen,
      BuiltinFunctions::Kind::Exit,   BuiltinFunctions::Kind::Abort,
      BuiltinFunctions::Kind::Printf,
  };
}
