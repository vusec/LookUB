#include "LookUB/mutator/UnsafeGenerator.h"
#include "LookUB/mutator/Canonicalizer.h"
#include "LookUB/mutator/FunctionMutator.h"
#include "LookUB/mutator/LiteralMaker.h"
#include "LookUB/mutator/Simplifier.h"
#include "LookUB/mutator/Snippets.h"
#include "LookUB/mutator/StatementContext.h"
#include "LookUB/mutator/StatementCreator.h"
#include "LookUB/mutator/StatementMutator.h"
#include "LookUB/mutator/TypeCreator.h"
#include "LookUB/mutator/UnsafeMutatorBase.h"
#include "scc/mutator-utils/GeneratorUtils.h"
#include "scc/mutator-utils/RecursionLimit.h"
#include "scc/mutator-utils/Rng.h"
#include "scc/mutator-utils/TypeGarbageCollector.h"
#include "scc/program/Function.h"
#include "scc/program/GlobalVar.h"
#include "scc/program/RecordDecl.h"
#include "scc/program/Statement.h"
#include "scc/program/Variable.h"

#include <iostream>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>

namespace {

struct GeneratorImpl : UnsafeMutatorBase {
  LiteralMaker literalMaker;
  FunctionMutator fm;
  StatementMutator sm;
  StatementCreator sc;

  explicit GeneratorImpl(MutatorData &input)
      : UnsafeMutatorBase(input), literalMaker(input), fm(input), sm(input),
        sc(input) {}

  bool couldBeSafeToRemove(Decl *d) {
    if (d->getKind() == Decl::Kind::GlobalVar) {
      NameID varId = static_cast<GlobalVar *>(d)->getNameID();
      return !p.isIDUsed(varId);
    } else if (d->getKind() == Decl::Kind::Function) {
      NameID funcName = static_cast<Function *>(d)->getNameID();
      return !p.isIDUsed(funcName);
    }
    return false;
  }

  Modified deleteType() {
    return Modified::No;
    auto verifyScope = p.queueVerify();
    for (Type &t : types) {
      // Never delete the builtin basic types.
      if (builtin.isBuiltin(t.getRef()))
        continue;
      if (p.isTypeUsed(t.getRef()))
        continue;
      assert(t.getKind() != Type::Kind::Basic);
      if (decision(Frag::DeleteTypes)) {
        t = Type();
        return Modified::Yes;
      }
    }
    return Modified::No;
  }

  Modified mutateType() {
    std::vector<Type *> option;
    for (Type &t : types) {
      if (builtin.isBuiltin(t.getRef()))
        continue;
      if (!t.isDerived())
        continue;
      option.push_back(&t);
    }
    if (option.empty())
      return Modified::No;

    TypeRef other = getRng().pickOneVec(option)->getRef();

    Type &t = *getRng().pickOneVec(option);
    if (t.isArray()) {
      if (other != t.getRef() && decision(Frag::MutateTypeBase)) {
        t.setBase(other);
        return Modified::Yes;
      }
      if (decision(Frag::MutateTypeArraySize)) {
        t.setArraySize(getRng().getBelow(16) + 1U);
        return Modified::Yes;
      }
    }

    return Modified::No;
  }

  bool isMain(Decl &d) {
    if (d.getKind() != Decl::Kind::Function)
      return false;
    return static_cast<Function &>(d).isMain(p);
  }

  Modified mutateFunction(Function &f) {
    if (decision(Frag::MutateFuncAttrs))
      fm.randomizeFuncAttrs(f);

    Statement newBody = f.getBody();
    sm.mutateFunctionBody(f, newBody);
    f.setBody(newBody);
    return Modified::Yes;
  }

  Modified mutateGlobalVar(GlobalVar &f) {
    if (decision(Frag::SwitchLinkageGlobalVar))
      f.is_static = !f.is_static;
    else {
      if (!types.get(f.getAsVar().getType()).isArray())
        f.setInit(sc.makeConstant(StatementContext::Global(),
                                  f.getAsVar().getType()));
    }
    return Modified::Yes;
  }

  Modified changeIdentifier() {
    NameID maxID = idents.getLastID();
    const unsigned tries = 100;
    const size_t maxLen = 64;
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz"
                              "0123456789_";
    for (unsigned i = 0; i < tries; ++i) {
      NameID id =
          NameID::fromInternalValue(getRng().getBelow(maxID.getInternalVal()));
      if (!idents.isValidID(id))
        continue;
      if (idents.isFixedID(id))
        continue;
      std::string n = idents.getName(id);
      const std::string beforeChange = n;
      for (unsigned modI = 0; modI < getRng().getBelow(10U) + 1; ++modI) {
        std::string orig = n;
        size_t pos = getRng().getBelow(n.size());
        if (getRng().flipCoin() && n.size() < maxLen)
          n.insert(n.begin() + pos, getRng().pickOneStr(chars));
        else if (n.size() > 1 && pos < n.size())
          n.erase(n.begin() + pos);
        if (orig == n)
          continue;
        if (!idents.isValidName(n)) {
          n = orig;
          continue;
        }
      }

      if (n == beforeChange)
        continue;

      if (!idents.isValidName(n))
        continue;

      if (idents.hasID(n))
        continue;

      if (!idents.tryChangeId(id, n)) {
        SCCError("Failed to change identifier '" + beforeChange + "' to '" + n +
                 "'");
        continue;
      }
      return Modified::Yes;
    }
    return Modified::No;
  }

  Modified mutateStep() {
    auto verifyScope = p.queueVerify();
    std::vector<Decl *> d = p.getDeclList();
    Decl &toMod = *getRng().pickOneVec(d);

    if (decision(Frag::MutateOverDelete) || isMain(toMod)) {
      switch (toMod.getKind()) {
      case Decl::Kind::Function:
        if (!decision(Frag::MutateFunction))
          return Modified::No;
        return mutateFunction(static_cast<Function &>(toMod));
      case Decl::Kind::GlobalVar:
        if (!decision(Frag::MutateGlobal))
          return Modified::No;
        return mutateGlobalVar(static_cast<GlobalVar &>(toMod));
      case Decl::Kind::Record:
        // TODO.
        break;
      }

      return Modified::No;
    }

    if (decision(Frag::ReorderOverDelete)) {
      Decl &original = *getRng().pickOneVec(d);
      if (original.isNamed()) {
        Decl &other = *original.clone();
        NamedDecl &named = static_cast<NamedDecl &>(other);
        p.removeDecl(&original);
        auto storages = p.getDeclStorages();
        DeclStorage &target = *getRng().pickOneVec(storages);
        target.store(&named, getRng().getBelow(target.size()));
        return Modified::Yes;
      }
    }

    if (decision(Frag::DeleteTypes))
      return deleteType();
    if (decision(Frag::MutateTypes))
      return mutateType();

    else if (couldBeSafeToRemove(&toMod)) {
      p.removeDecl(&toMod);
      return Modified::Yes;
    }
    return Modified::No;
  }

  void fixMainReturn() {
    std::vector<Decl *> decls = p.getDeclList();

    for (Decl *d : decls) {
      if (isMain(*d)) {
        Function &main = static_cast<Function &>(*d);
        bool hasReturn = false;

        main.getBody().foreachChild([&hasReturn](const Statement &child) {
          hasReturn |= (child == Stmt::Kind::Return);
          return LoopCtrl::Continue;
        });

        if (hasReturn)
          break;
        StatementContext context = sc.getContextForFunction(main);
        Stmt returnVal = sc.makeExpr(context, main.getReturnType());
        Stmt newBody =
            Stmt::CompoundStmt({main.getBody(), Stmt::Return(returnVal)});

        if (auto canonicalized = Canonicalizer::canonicalizeStmt(newBody))
          newBody = *canonicalized;
        main.setBody(newBody);
        break;
      }
    }
  }

  void mutate() {
    auto verifyScope = p.queueVerify();
    for (unsigned i = 0; i < 200; ++i)
      if (mutateStep() == Modified::Yes)
        break;
    if (decision(Frag::FixMainReturn))
      fixMainReturn();
    if (decision(Frag::GarbageCollectTypes)) {
      TypeGarbageCollector c(p);
      c.run();
      p.verifySelf();
    }
  }

  auto getTakenDecisions() const { return strategy.getTakenDecisions(); }
};
} // namespace

std::unique_ptr<Program> UnsafeGenerator::generate(RngSource source,
                                                   LangOpts opts) {
  std::unique_ptr<Program> res = std::make_unique<Program>();
  opts.setStandard(LangOpts::Standard::Cxx11);
  res->setLangOpts(opts);
  GeneratorUtils::addMain(*res);
  return res;
}

std::vector<UnsafeGenerator::Strategy::Frag>
UnsafeGenerator::mutate(Program &p, RngSource source, const Strategy &strat,
                        unsigned scaleMul) {
  assert(scaleMul && "Can't be 0 or the fuzzer would do nothing");
  StrategyInstance s(source, strat);
  UnsafeMutatorBase::MutatorData input(p, s, source);

  if (s.decision(UnsafeStrategy::Frag::RegenerateProgram))
    p = *generate(source, p.getLangOpts());

  std::vector<UnsafeGenerator::Strategy::Frag> decisions;
  for (unsigned i = 0; i < strat.scale * scaleMul; ++i) {
    GeneratorImpl impl(input);
    input.rng = input.rng.spawnChild();
    impl.mutate();
    auto n = impl.getTakenDecisions();
    decisions.insert(decisions.end(), n.begin(), n.end());
  }
  return decisions;
}

std::vector<UnsafeStrategy::Frag>
UnsafeGenerator::reduce(Program &p, RngSource source, const Strategy &strat) {
  StrategyInstance s(source, strat);
  UnsafeMutatorBase::MutatorData input(p, s, source);
  GeneratorImpl impl(input);
  impl.mutate();
  return impl.getTakenDecisions();
}

std::string UnsafeGenerator::getProgramPrefix(const Program &p) {
  return "#define main wrap_main\n";
}

std::string UnsafeGenerator::getProgramSuffix(const Program &p) {
  return "#undef main\n"
         "int main(int argc, char **argv) {\n"
         "  int res = wrap_main(argc, argv);\n"
         "  return argc == 0 ? res : 0;\n"
         "}\n";
}

std::unique_ptr<Program>
UnsafeGenerator::generateFromEntrophy(EntrophyVec entrophy,
                                      UnsafeStrategy strat, LangOpts opts) {
  RngSource rngSource(entrophy);

  UnsafeGenerator gen;
  std::unique_ptr<Program> program = gen.generate(rngSource);
  while (entrophy.hasData())
    gen.mutate(*program, rngSource, strat, 1);
  return program;
}
