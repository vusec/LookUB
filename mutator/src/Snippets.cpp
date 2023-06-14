#include "LookUB/mutator/Snippets.h"

using Stmt = Statement;
using Kind = Stmt::Kind;

static Statement binOp(Program &p, Kind op, Statement l, Statement r) {
  return Stmt::BinaryOp(p, op, l, r);
}

Statement Snippets::createSnippetImpl(const StatementContext &ctx) {
  constexpr auto Compound = Stmt::CompoundStmt;
  constexpr auto VarDef = Stmt::VarDef;
  constexpr auto VarDecl = Stmt::VarDecl;
  constexpr auto While = Stmt::While;
  constexpr auto StmtExpr = Stmt::StmtExpr;
  constexpr auto Empty = Stmt::Empty;
  constexpr auto CommentStmt = Stmt::CommentStmt;
  constexpr auto Goto = Stmt::Goto;
  constexpr auto Break = Stmt::Break;
  constexpr auto GotoLabel = Stmt::GotoLabel;
  constexpr auto LocalVarRef = Stmt::LocalVarRef;
  constexpr auto Constant = Stmt::Constant;
  constexpr auto Subscript = Stmt::Subscript;

  using BI = BuiltinFunctions::Kind;
  auto getBuiltin = [this](BI b) {
    Function *f = builtinFuncs.get(p, b);
    SCCAssert(f, "Failed to create builtin?");
    return f;
  };

  enum Option {
    ForwardJump,
    BackwardsJump,
    MallocFree,
    CounterLoop,
    InfLoop,
    NoLoop,
    ArrayWithUse,
    UseAfterReturn,
  };

  std::vector<Option> options = {ForwardJump, BackwardsJump, MallocFree,
                                 InfLoop,     NoLoop,        CounterLoop,
                                 ArrayWithUse};

  // If the function returns a non-void ptr then we can
  // induce a use-after-return.
  const Type returnType = types.get(ctx.returnType);
  if (returnType.getKind() == Type::Kind::Pointer &&
      returnType.getBase() != p.getBuiltin().void_type &&
      !types.get(returnType.getBase()).expectsVarInitializer(types)) {
    options.push_back(UseAfterReturn);
  }

  switch (getRng().pickOneVec(options)) {
  case ForwardJump: {
    NameID l = newID("lbl");
    return Compound({Goto(l), Empty(), GotoLabel(l)});
  }
  case BackwardsJump: {
    NameID l = newID("lbl");
    return Compound({GotoLabel(l), Empty(), Goto(l)});
  }
  case MallocFree: {
    // Creates a malloc and respective free call.
    TypeRef t = tc.getPtrType();
    NameID l = newID("var");
    Stmt varRef = LocalVarRef(Variable(t, l));
    Stmt mallocCall =
        getBuiltin(BI::Malloc)->makeCall({Constant("128", builtin.getSizeT())});
    Stmt alloc = VarDef(t, l, Stmt::Cast(t, mallocCall));
    Stmt dealloc = StmtExpr(
        getBuiltin(BI::Free)->makeCall({Stmt::Cast(builtin.void_ptr, varRef)}));
    return Compound({alloc, dealloc});
  }
  case ArrayWithUse: {
    TypeRef arrayType = tc.makeNewArrayType();
    const unsigned arraySize = types.get(arrayType).getArraySize();
    TypeRef base = types.get(arrayType).getBase();

    // FIXME: This is inefficient. Select better types in the first place.
    if (types.isConst(base))
      return Stmt::Empty();
    if (types.get(base).isArray())
      return Stmt::Empty();

    NameID l = newID("localArray");
    Stmt varDef = VarDecl(arrayType, l);
    Stmt varRef = LocalVarRef(Variable(arrayType, l));

    auto makeSubscript = [&]() {
      std::string index = std::to_string(getRng().getBelow(arraySize));
      Stmt subscript =
          Subscript(base, varRef, Constant(index, builtin.unsigned_int));
      return subscript;
    };
    std::vector<Stmt> children = {varDef};
    for (unsigned i = 0; i < getRng().getBelow(10U); ++i) {
      Stmt assign = Stmt::StmtExpr(Stmt::BinaryOp(
          p, Stmt::Kind::Assign, makeSubscript(), makeSubscript()));
      children.push_back(assign);
    }
    return Compound(children);
  }
  case CounterLoop: {
    TypeRef t = tc.getAnyIntType();
    NameID l = newID("var");
    Statement varRef = LocalVarRef(Variable(t, l));
    return Compound(
        {VarDef(t, l, Constant("0", t)),
         While(
             binOp(p, Kind::Less, varRef, Constant("10", t)),
             Compound({
                 getRng().withSuccessChance(0.2) ? Empty() : Break(),
                 StmtExpr(binOp(p, Kind::Assign, varRef,
                                binOp(p, Kind::Add, varRef, Constant("1", t)))),
                 getRng().withSuccessChance(0.2) ? Empty() : Break(),
             }))});
  }
  case InfLoop: {
    return While(Constant("1", builtin.signed_int), Compound({Break()}));
  }
  case NoLoop: {
    return While(Constant("0", builtin.signed_int), Compound({Break()}));
  }
  case UseAfterReturn: {
    TypeRef underlying = returnType.getBase();
    SCCAssertNotEqual(underlying, p.getBuiltin().void_type,
                      "Can't allocate void type");
    NameID l = newID("var");
    Variable var(underlying, l);
    Stmt varRef = LocalVarRef(var);
    return Compound(
        {Stmt::VarDecl(underlying, l),
         Statement::Return(Statement::AddrOf(returnType.getRef(), varRef))});
  }
  }
  SCCError("Missing switch?");
}
