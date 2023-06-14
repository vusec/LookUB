#include "LookUB/mutator/LiteralMaker.h"

#include "scc/utils/Counter.h"

void LiteralMaker::setupIntegers() {
  specialIntegers.push_back("0");

  unsigned long specialUInt = 1;
  for (int i = 0; i <= 64; ++i) {
    specialIntegers.push_back(std::to_string(specialUInt) + "ULL");
    specialIntegers.push_back(std::to_string(specialUInt + 1u) + "ULL");
    specialIntegers.push_back(std::to_string(specialUInt - 1U) + "ULL");
    specialUInt *= 2;
  }

  long specialInt = -1;
  for (int i = 0; i <= 62; ++i) {
    specialIntegers.push_back("(" + std::to_string(specialInt) + "LL)");
    specialIntegers.push_back("(" + std::to_string(specialInt + 1) + "LL)");
    specialIntegers.push_back("(" + std::to_string(specialInt - 1) + "LL)");
    specialInt *= 2;
  }
}

void LiteralMaker::setupFloats() {
  specialFloats.push_back("0.0");
  unsigned long specialUInt = 1;
  for (int i = 0; i <= 64; ++i) {
    specialFloats.push_back(std::to_string(specialUInt) + ".0");
    specialFloats.push_back(std::to_string(specialUInt + 1u) + ".0");
    specialFloats.push_back(std::to_string(specialUInt - 1U) + ".0");
    specialUInt *= 2;
  }
}

LiteralMaker::LiteralMaker(MutatorData &input) : UnsafeMutatorBase(input) {
  setupIntegers();
  setupFloats();
}

Statement LiteralMaker::makeConstant(const StatementContext &, TypeRef t) {
  auto verify = p.queueVerify();
  if (getType(t).getKind() == Type::Kind::Array) {
    auto verify = p.queueVerify();
    t = types.getOrCreateDerived(idents, Type::Kind::Pointer,
                                 getType(t).getBase());
  }
  const std::string str = makeConstantStr(t);
  return Statement::Cast(t, Statement::Constant(str, t));
}

std::string LiteralMaker::makeConstantStr(TypeRef tref) {
  tref = stripCV(tref);

  SCCAssertNotEqual(tref, builtin.void_type,
                    "Trying to make void str literal?");
  if (builtin.isIntType(tref))
    return getRng().pickOneVec(specialIntegers);
  if (builtin.isFloatType(tref))
    return getRng().pickOneVec(specialFloats);

  Type t = getType(tref);
  if (t.getKind() == Type::Kind::Pointer) {
    if (tref == builtin.const_char_ptr && decision(Frag::EmitStringLiteral)) {
      std::string res = "\"";
      if (!decision(Frag::EmitEmptyStringLiteral))
        for (unsigned int i = 0; i < getRng().getBelow<unsigned>(10); ++i)
          getRng().pickOneStr("abcdefghZSDF0123456789$&^()");
      res += "\"";
      return res;
    }

    return getRng().pickOne({"0", "-1", "1"});
  }

  if (t.getKind() == Type::Kind::FunctionPointer)
    return getRng().pickOne({"0", "-1"});

  if (t.getKind() == Type::Kind::Array) {
    return getRng().pickOne({"{0}", "{1, 2}", "{}", "{1}"});
  }

  if (t.getKind() == Type::Kind::Record)
    return getRng().pickOne({"{0}", "{}"});

  if (t.getKind() == Type::Kind::Invalid) {
    SCCError("Requesting constant string of invalid type?");
    return "INVALID_TYPE_REQUESTED";
  }

  SCCError(
      "Unimplemented type: " + std::to_string(static_cast<int>(t.getKind())) +
      " (id: " + std::to_string(static_cast<int>(tref.getInternalVal())) + ")");
  return "ERR";
}
