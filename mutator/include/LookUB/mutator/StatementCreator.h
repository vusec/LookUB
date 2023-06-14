#ifndef STATEMENTCREATOR_H
#define STATEMENTCREATOR_H

#include "Canonicalizer.h"
#include "FunctionMutator.h"
#include "Snippets.h"
#include "TypeCreator.h"
#include "UnsafeMutatorBase.h"
#include "scc/program/GlobalVar.h"
#include "scc/utils/Counter.h"

/// Creates new random statements.
class StatementCreator : public UnsafeMutatorBase {
  TypeCreator tc;
  FunctionMutator fm;

  std::vector<Statement> stmtStack;
  /// How deeply we can nest expressions in one mutation step.
  RecursionLimit exprRecursionLimit = 2;
  /// How deeply we can nest statements in one mutation step.
  RecursionLimit stmtRecursionLimit = 3;
  /// How deeply we can neste functions in one mutation step.
  RecursionLimit funcRecursionLimit = 3;

  /// The snippet maker.
  Snippets snippets;
  /// Creates literals for types.
  LiteralMaker literalMaker;

  /// Verifies the code/program and returns it.
  template <typename T> T verify(T s) {
    s.verifySelf(p);
    return s;
  }

public:
  StatementCreator(MutatorData &input);

  /// Queues a statement to be recycled later.
  void pushStmtOnStack(Statement s) { stmtStack.push_back(s); }

  /// Returns an array variable initializer.
  Statement makeArrayInit(StatementContext &context, TypeRef tref) {
    auto verifyScope = p.queueVerify();
    tref = stripCV(tref);
    TypeRef base = getType(tref).getBase();
    SCCAssert(base != tref, "Cycle in array types?");

    std::vector<Statement> values;
    for (unsigned i = 0; i < types.get(tref).getArraySize(); ++i) {
      values.push_back(makeConstant(context, base));
      if (decision(Frag::DontFillArrayConstant))
        break;
    }
    return verify(Statement::ConstantArray(values, tref));
  }

  /// Returns the code that initializes a variable of type t.
  Statement makeVarInit(StatementContext &c, TypeRef t) {
    auto verifyScope = p.queueVerify();
    if (getType(t).getKind() == Type::Kind::Array)
      return makeArrayInit(c, t);
    return verify(makeConstant(c, t));
  }

  /// Given an incomplete function, adds it to the program and gives it
  /// a body if possible.
  Function *finishFunctionCreation(std::unique_ptr<Function> &&f) {
    auto verifyScope = p.queueVerify();
    if (decision(Frag::InitWithFuncAttrs))
      fm.randomizeFuncAttrs(*f);
    auto recLimit = funcRecursionLimit.scope();

    // Check if we reached the recursion limit for function scopes.
    // This avoids that a function-generation heavy strategy spirals out of
    // control.
    if (recLimit.reached()) {
      auto verifyScope = p.queueVerify();
      f->setBody(Statement::CompoundStmt({}));
    } else {
      auto verifyScope = p.queueVerify();
      f->setBody(makeFunctionBody(*f));
    }
    // Might make the function static or noexcept.
    f->isStatic = decision(Frag::FunctionIsStatic);
    if (p.getLangOpts().isCxx())
      f->isNoExcept = decision(Frag::FunctionIsNoExcept);
    return &p.add(std::move(f));
  }

  /// Creates a function with the given signature (encoded as a function ptr
  /// type).
  Function *createFunctionWithType(TypeRef tref) {
    auto verifyScope = p.queueVerify();
    Type t = getType(tref);
    std::vector<Variable> args;
    for (TypeRef arg : t.getArgs())
      args.push_back(Variable(arg, idents.makeNewID("arg")));
    // TODO: Make this variadic?

    auto f = std::make_unique<Function>(t.getFuncReturnType(),
                                        idents.makeNewID("func"), args);
    return finishFunctionCreation(std::move(f));
  }

  /// Creates a function with the given type as a return type.
  Function *createFunctionWithReturnType(TypeRef t) {
    auto verifyScope = p.queueVerify();
    std::vector<Variable> args;

    for (unsigned i = 0; i < getRng().getBelow<unsigned>(8); ++i) {
      auto verifyScope = p.queueVerify();
      args.push_back(
          Variable(tc.getExistingDefinedType(), idents.makeNewID("arg")));
    }

    auto f = std::make_unique<Function>(t, idents.makeNewID("func"), args);
    return finishFunctionCreation(std::move(f));
  }

  /// Returns any function.
  Function *getAnyFunction() {
    std::vector<Function *> options;
    for (Decl *d : p.getDeclList()) {
      if (d->getKind() == Decl::Kind::Function)
        options.push_back(static_cast<Function *>(d));
    }
    if (options.empty())
      return createFunctionWithReturnType(tc.getReturnType());
    return getRng().pickOneVec(options);
  }

  /// Create a goto.
  Statement makeGoto(StatementContext &context) {
    SCCAssert(context.function, "Can only do goto in function");
    if (!context.function)
      return makeStmt(context);

    auto labels = getAllLabels(*context.function);
    if (labels.empty())
      return makeStmt(context);

    return Statement::Goto(getRng().pickOneVec(labels).id);
  }

  /// Creates a random 'throw' statement.
  Statement makeThrow(StatementContext &context) {
    auto verifyScope = p.queueVerify();
    return Statement::Throw(makeExpr(context, tc.getDefinedType()));
  }

  /// Creates a random 'catch' statement.
  Statement makeCatch(StatementContext context) {
    return Statement::Catch(tc.getDefinedType(), idents.makeNewID("c"),
                            makeStmt(context));
  }

  /// Creates a catch all statement 'catch (...)'.
  Statement makeCatchAll(StatementContext context) {
    return Statement::CatchAll(makeStmt(context));
  }

  /// Creates a try statement with potentially several catch statements.
  Statement makeTry(StatementContext &context) {
    auto verifyScope = p.queueVerify();
    std::vector<Statement> catches;
    for (unsigned i = 0; i < getRng().getBelow(4U); ++i)
      catches.push_back(makeCatch(context));
    if (decision(Frag::CatchAll))
      catches.push_back(makeCatchAll(context));
    return Statement::Try(makeStmt(context), catches);
  }

  /// Creates a random inline assembly statement.
  Statement makeAsm(const StatementContext &context) {
    // TODO: We could do more here, but it's out of scope for sanitizers
    // to this probably just creates false positives.
    return Statement::Asm("nop");
  }

  /// Creates a call to function 'f' with random arguments.
  Statement makeCallToFunc(const StatementContext &context, Function *f) {
    auto verifyScope = p.queueVerify();
    std::vector<Statement> args;
    for (Variable arg : f->getArgs())
      args.push_back(makeExpr(context, arg.getType()));

    if (f->isVariadic())
      for (unsigned i = 0; i < getRng().getBelow(10); ++i)
        args.push_back(makeExpr(context, tc.getAnyIntOrPtrOrFloatType()));

    return Statement::Call(f->getReturnType(), f->getNameID(), args);
  }

  /// Create a call that results in a value of the given type.
  Statement makeCall(const StatementContext &context, TypeRef t) {
    auto verifyScope = p.queueVerify();
    // Maybe we can call a function pointer here?
    if (decision(Frag::CallFuncPtr)) {
      TypeRef funcPtrT = tc.makeNewFuncPtrTypeWithResult(t);
      Statement funcPtr = makeExpr(context, funcPtrT);
      std::vector<Statement> args;
      for (TypeRef arg : types.get(funcPtrT).getArgs())
        args.push_back(makeExpr(context, arg));
      return Statement::IndirectCall(t, funcPtr, args);
    }
    Function *f = createFunctionWithReturnType(t);
    std::vector<Statement> args;
    for (Variable arg : f->getArgs())
      args.push_back(makeExpr(context, arg.getType()));
    return Statement::Call(t, f->getNameID(), args);
  }

  /// Create a constant of the given type.
  Statement makeConstant(const StatementContext &c, TypeRef t) {
    auto verifyScope = p.queueVerify();
    return literalMaker.makeConstant(c, t);
  }

  /// Create a global variable of the given type.
  GlobalVar *makeGlobal(TypeRef t) {
    auto g = std::make_unique<GlobalVar>(t, newID("global"));
    g->is_static = decision(Frag::VarIsStatic);
    if (getType(t).expectsVarInitializer(types) || decision(Frag::InitGlobal)) {
      // No context with variables available for globals.
      StatementContext c = StatementContext::Global();
      if (getType(t).isArray())
        g->setInit(makeArrayInit(c, t));
      else
        g->setInit(makeConstant(c, t));
    }
    return &p.add(std::move(g));
  }

  /// Returns a global that might already exist.
  GlobalVar *makeOrFindGlobal(TypeRef t) {
    for (Decl *d : p.getDeclList()) {
      if (d->getKind() == Decl::Kind::GlobalVar) {
        GlobalVar *g = static_cast<GlobalVar *>(d);
        if (g->getAsVar().getType() == t) {
          if (decision(Frag::PickExistingGlobal))
            return g;
        }
      }
    }
    return makeGlobal(t);
  }

  /// Creates an lvalue expression of the given type.
  Statement makeLValue(const StatementContext &context, TypeRef t) {
    if (getType(t).getKind() == Type::Kind::Pointer &&
        decision(Frag::TryDerefVar)) {
      return Statement::Deref(t, makeLValue(context, tc.getPtrTypeOf(t)));
    }
    return makeVarRef(context, t);
  }

  /// Create a binary operator.
  Statement makeBinary(const StatementContext &context, TypeRef t) {
    Type type = getType(t);
    StmtKind kind;
    TypeRef lhsT = Void();
    TypeRef rhsT = Void();
    if (builtin.isIntType(t)) {
      kind = getRng().pickOneVec(Statement::getIntArithmeticOps());
      lhsT = tc.getAnyIntType(/*allowConst=*/kind != Stmt::Kind::Assign);
      rhsT = tc.getAnyIntType(/*allowConst=*/true);
    } else if (builtin.isFloatType(t)) {
      lhsT = tc.getAnyIntOrFloatType();
      rhsT = tc.getAnyIntOrFloatType();
      kind = getRng().pickOneVec(Statement::getFloatArithmeticOps());
    } else if (type.getKind() == Type::Kind::Pointer) {
      lhsT = t;
      rhsT = tc.getAnyIntType(/*allowConst=*/true);
      kind = getRng().pickOneVec(Statement::getPtrArithmeticOps());
    } else {
      SCCError("Neither ptr nor int binary operator requested?");
      return makeConstant(context, t);
    }

    if (kind == StmtKind::Assign)
      return Statement::BinaryOp(p, kind, makeLValue(context, lhsT),
                                 makeExpr(context, rhsT));
    return Statement::BinaryOp(p, kind, makeExpr(context, lhsT),
                               makeExpr(context, rhsT));
  }

  Statement makeCxxNewExpr(const StatementContext &context, TypeRef t) {
    return Statement::New(t, {});
  }

  Statement makeDelete(const StatementContext &context) {
    return Statement::Delete(makeExpr(context, tc.getPtrType()));
  }

  Statement makeDeref(const StatementContext &context, TypeRef t) {
    return Statement::Deref(
        t, makeExpr(context,
                    types.getOrCreateDerived(idents, Type::Kind::Pointer, t)));
  }

  /// Make a subscript statement (that is, a 'array[x]' statement).
  Statement makeSubscript(const StatementContext &context, TypeRef t) {
    Statement base = makeExpr(
        context, types.getOrCreateDerived(idents, Type::Kind::Pointer, t));
    Statement index = makeExpr(context, tc.getAnyIntType(/*allowConst=*/true));
    return Statement::Subscript(t, base, index);
  }

  /// Returns true if the 'from' type can be converted to the given 'to' type.
  bool canTypeConvertTo(TypeRef from, TypeRef to) {
    // Same type, so fine to convert.
    if (from == to)
      return true;
    const Type &fromT = types.get(from);
    const Type &toT = types.get(to);

    // Arrays can decay to pointers.
    if (fromT.getKind() == Type::Kind::Array &&
        toT.getKind() == Type::Kind::Pointer)
      return canTypeConvertTo(fromT.getBase(), toT.getBase());
    // CV-qualifiers can be added implicitly.
    if (toT.isCVQualified())
      return canTypeConvertTo(from, toT.getBase());
    return false;
  }

  /// Returns a variable reference.
  Statement makeVarRef(const StatementContext &context, TypeRef t) {
    // Check if we can use a variable that is in context.
    if (decision(Frag::PickLocalVar))
      for (const auto &v : context.variables)
        if (canTypeConvertTo(v.second.getType(), t))
          return Statement::LocalVarRef(v.second);
    return Statement::GlobalVarRef(makeOrFindGlobal(t)->getAsVar());
  }

  std::vector<BuiltinFunctions::Kind> callableBuiltins() const;

  /// Creates a random expression evaluating to the given type.
  Statement makeExprImpl(const StatementContext &context, TypeRef t) {
    SCCAssert(types.isValid(t), "Expression with invalid type?");

    auto verifyScope = p.queueVerify();
    auto recLimit = exprRecursionLimit.scope();
    if (recLimit.reached())
      return verify(makeConstant(context, t));

    t = stripCV(t);
    SCCAssert(types.isValid(t), "Type got invalid after stripping?");

    const bool isPointer = types.get(t).getKind() == Type::Kind::Pointer;

    if (decision(Frag::CallBuiltin)) {
      auto verifyScope = p.queueVerify();
      for (auto counter : count::upTo(10)) {
        BuiltinFunctions::Kind kind = getRng().pickOneVec(callableBuiltins());
        const TypeRef returnT = builtinFuncs.getReturnType(p, kind);
        if (returnT == t) {
          Function *f = builtinFuncs.get(p, kind);
          return verify(makeCallToFunc(context, f));
        }
        if (types.get(returnT).isPointer() && isPointer) {
          Function *f = builtinFuncs.get(p, kind);
          return Statement::Cast(t, verify(makeCallToFunc(context, f)));
        }
      }
    }

    // If we expect a void pointer, just pick any pointer/int and
    // cast it to a void pointer.
    if (builtin.isVoidPtr(t))
      return verify(
          Statement::Cast(t, makeExprImpl(context, tc.getAnyIntOrPtrType())));

    // For float/int/pointer we have several options.
    if (builtin.isFloatType(t) || builtin.isIntType(t) || isPointer) {
      auto verifyScope = p.queueVerify();
      enum Options {
        Constant,
        Bin,
        Call,
        Var,
        Cast,
        Deref,
        Subscript,
        AddrOf,
        New
      };
      std::vector<Options> options = {Constant, Subscript, Bin,  Call, Var,
                                      Var,      Var,       Cast, Deref};
      // If this is a pointer, we can also take the address of something.
      if (isPointer) {
        options.push_back(AddrOf);
        if (p.getLangOpts().isCxx())
          options.push_back(New);
      }
      // Pick one option.
      switch (getRng().pickOneVec(options)) {
      case Constant:
        return verify(makeConstant(context, t));
      case Bin:
        return verify(makeBinary(context, t));
      case Deref:
        return verify(makeDeref(context, t));
      case Subscript:
        return verify(makeSubscript(context, t));
      case New:
        return verify(makeCxxNewExpr(context, t));
      case Call:
        // Recurse and hope we pick another case.
        if (!isValidReturnType(t))
          return verify(makeExprImpl(context, t));
        return verify(makeCall(context, t));
      case Var:
        return verify(makeVarRef(context, t));
      case Cast: {
        if (isPointer)
          return verify(Statement::Cast(
              t, makeExprImpl(context, tc.getAnyIntOrPtrType())));
        return verify(Statement::Cast(
            t, makeExprImpl(context, tc.getAnyIntType(/*allowConst=*/true))));
      }
      case AddrOf:
        return verify(
            Statement::AddrOf(t, makeLValue(context, getType(t).getBase())));
      }
    }

    if (getType(t).getKind() == Type::Kind::Array) {
      enum { VarRef };
      switch (getRng().pickOne({VarRef})) {
      case VarRef:
        return verify(makeVarRef(context, t));
      }
    }

    if (getType(t).getKind() == Type::Kind::FunctionPointer) {
      enum { AddrOf, Constant };
      switch (getRng().pickOne({AddrOf, Constant})) {
      case Constant:
        return verify(makeConstant(context, t));
      case AddrOf:
        return verify(
            Statement::AddrOfFunc(t, createFunctionWithType(t)->getNameID()));
      }
    }

    if (getType(t).getKind() == Type::Kind::Record) {
      enum { Constant, GlobalVarRef };
      switch (getRng().pickOne({Constant, Constant})) {
      case Constant:
        return verify(makeConstant(context, t));
      case GlobalVarRef:
        return verify(makeVarRef(context, t));
      }
    }

    if (t == Void())
      return makeCall(context, t);

    // We should never reach this.
    assert(false);
    return verify(makeConstant(context, t));
  }

  Statement makeExpr(const StatementContext &context, TypeRef t) {
    Statement s = makeExprImpl(context, t);
    s.verifySelf(p);
    SCCAssert(s.isExpr(), "makeExpr returned a statement?");
    return s;
  }

  Statement makeReturn(const StatementContext &context) {
    auto verifyScope = p.queueVerify();
    if (context.getRetType() == Void())
      return Statement::VoidReturn();
    return Statement::Return(makeExpr(context, context.getRetType()));
  }

  Statement makeIf(const StatementContext &context) {
    auto verifyScope = p.queueVerify();
    Statement cond = makeExpr(context, tc.getBoolType());
    return Statement::If(cond, makeCompoundStmt(context));
  }

  Statement makeWhile(StatementContext context) {
    auto verifyScope = p.queueVerify();
    Statement cond = makeExpr(context, tc.getBoolType());
    context.inLoop = true;
    return Statement::While(cond, makeCompoundStmt(context));
  }

  /// Declare a random variable.
  Statement makeVarDecl(StatementContext &context, bool isDefinition) {
    auto verifyScope = p.queueVerify();
    TypeRef t = Void();
    for (unsigned i = 0; i < 1000; ++i) {
      if (isDefinition)
        t = tc.getDefinedType();
      else {
        t = tc.getDefinedNonConstType();
        assert(!types.isConst(t));
      }
      if (isDefinition)
        break;
      // If we ended up with a type that expects an initializer
      // but we have a declaration (not a definition) then just
      // retry.
      if (types.get(t).expectsVarInitializer(types))
        continue;
    }

    NameID id = newID("var");
    Variable info(t, id);

    if (isDefinition) {
      if (getType(t).getKind() == Type::Kind::Array) {
        Statement res = Statement::VarDef(t, id, makeArrayInit(context, t));
        context.variables[id] = info;
        return res;
      } else {
        Statement res = Statement::VarDef(t, id, makeExpr(context, t));
        context.variables[id] = info;
        return res;
      }
    }
    context.variables[id] = info;
    return Statement::VarDecl(t, id);
  }

  struct JumpLabel {
    NameID id = InvalidName;
    JumpLabel() = default;
    explicit JumpLabel(NameID id) : id(id) {}
  };

  /// Returns all goto labels in the given function.
  std::vector<JumpLabel> getAllLabels(const Function &f) {
    std::vector<JumpLabel> res;
    auto children = f.getBody().getAllChildren();
    for (const Statement::StmtAndParent &p : children) {
      if (p.stmt->getKind() == StmtKind::GotoLabel)
        res.emplace_back(p.stmt->getJumpTarget());
    }
    return res;
  }

  std::string makeStmtAttr() {
    return getRng().pickOne({"[[likely]]", "[[unlikely]]"});
  }

  Statement makeBuiltinCallStmt(StatementContext &context) {
    BuiltinFunctions::Kind kind = getRng().pickOneVec(callableBuiltins());
    Function *f = builtinFuncs.get(p, kind);
    return wrapExprInStmt(makeCallToFunc(context, f));
  }

  /// Creates a random statement.
  Statement makeStmtImpl(StatementContext &context, bool avoidDecl = false) {
    auto verifyScope = p.queueVerify();

    // Maybe we can just create a snippet?
    if (decision(Frag::UseSnippet))
      return snippets.createSnippet(context);

    // Maybe we can recycle an old piece of code we threw away before.
    if (decision(Frag::UseMutatedStmtAsChild) && !stmtStack.empty()) {
      Statement res = stmtStack.back();
      stmtStack.pop_back();
      return verify(res);
    }

    // Ask if we just want to call a builtin function.
    if (decision(Frag::ForceCallBuiltinStmt))
      return verify(makeBuiltinCallStmt(context));

    // If we reach this, then we could be in a recursion. Check the recursion
    // limit and return an empty statement as a fallback.
    auto recLimit = stmtRecursionLimit.scope();
    if (recLimit.reached())
      return verify(Statement::Empty());

    enum Opt {
      Return,
      Expr,
      If,
      While,
      VarDecl,
      VarDef,
      Break,
      Asm,
      Call,
      Try,
      Throw,
      Delete,
      Goto,
      Label,
      Compound
    };
    std::vector<Opt> toPick = {Return, Expr,     If,       While,   VarDecl,
                               Call,   VarDef,   Asm,      Break,   Goto,
                               Label,  Compound, Compound, Compound};
    // For C++, we can generate C++ generic statements.
    if (p.getLangOpts().isCxx()) {
      if (false) { // TODO: This frequently causes miscompiles.
        toPick.push_back(Try);
        toPick.push_back(Throw);
      }
      toPick.push_back(Delete);
    }

    // Pick one of the options and branch to one of the specific generation
    // functions.
    switch (getRng().pickOneVec(toPick)) {
    case Return:
      return verify(makeReturn(context));
    case Compound:
      return verify(makeCompoundStmt(context));
    case If:
      return verify(makeIf(context));
    case Break:
      if (context.inLoop)
        return verify(Statement::Break());
      return verify(makeStmt(context));
    case While:
      return verify(makeWhile(context));
    case Delete:
      return verify(makeDelete(context));
    case Asm:
      return verify(makeAsm(context));
    case Try:
      return verify(makeTry(context));
    case Throw:
      return verify(makeThrow(context));
    case Goto: {
      Statement s = verify(makeGoto(context));
      if (s.getKind() == Statement::Kind::Empty)
        return verify(makeStmt(context, avoidDecl));
      break;
    }
    case Label:
      return verify(Statement::GotoLabel(idents.makeNewID("rng_lbl")));
    case VarDecl:
      if (avoidDecl)
        return verify(makeStmt(context, avoidDecl));
      return verify(makeVarDecl(context, false));
    case VarDef:
      if (avoidDecl)
        return verify(makeStmt(context, avoidDecl));
      return verify(makeVarDecl(context, true));
    case Call:
      return wrapExprInStmt(makeCallToFunc(context, getAnyFunction()));
    case Expr:
      return wrapExprInStmt(makeExpr(context, tc.getDefinedType()));
    }

    return Statement::Empty();
  }

  /// Creates a random statement.
  Statement makeStmt(StatementContext &context, bool avoidDecl = false) {
    Statement s = makeStmtImpl(context, avoidDecl);
    s.verifySelf(p);
    SCCAssert(s.isStmt(), "makeStmt returned expression?");
    return s;
  }

  /// Creates a random compound statement (that is, code in curly brackets).
  Statement makeCompoundStmt(StatementContext context) {
    std::vector<Statement> children;
    for (unsigned i = 0; i < getRng().getBelow<unsigned>(16); ++i) {
      auto c = makeStmt(context);
      if (c.getKind() == StmtKind::Empty)
        break;
      context.expandWithStmt(c);
      children.push_back(c);
    }
    return Statement::CompoundStmt(children);
  }

  /// Recreates the statement context for f.
  StatementContext getContextForFunction(const Function &f) {
    StatementContext context(f);
    // Make function args available.
    for (const Variable &arg : f.getArgs()) {
      context.variables[arg.getName()] = arg;
      // Don't add argv which has variable contents and makes test cases
      // unstable.
      // FIXME: This actually helped in the past with making interesting test
      // cases, so we should find a way to re-add this properly. Not sure yet
      // what is the right thing to do here.
      if (f.isMain(p))
        break;
    }
    context.returnType = f.getReturnType();
    return context;
  }

  /// Creates a random function body for the given function.
  Statement makeFunctionBody(Function &f) {
    auto verifyScope = p.queueVerify();

    StatementContext context = getContextForFunction(f);
    std::vector<Statement> children;
    for (unsigned i = 0; i < 1 + getRng().getBelow<unsigned>(16); ++i)
      children.push_back(makeStmt(context));

    if (decision(Frag::EnsureReturnInFunc))
      children.push_back(makeReturn(context));

    Statement body = Statement::CompoundStmt(children);
    body.verifySelf(p);
    if (auto canonicalized = Canonicalizer::canonicalizeStmt(body))
      body = *canonicalized;

    return body;
  }
};

#endif // STATEMENTCREATOR_H
