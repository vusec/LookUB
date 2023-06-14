#include "LookUB/mutator/TypeCreator.h"

TypeCreator::TypeCreator(MutatorData &input) : UnsafeMutatorBase(input) {}

TypeRef TypeCreator::getPtrType() {
  auto verify = p.queueVerify();

  std::vector<TypeRef> options;
  for (const Type &t : types)
    if (t.getKind() == Type::Kind::Pointer)
      options.push_back(t.getRef());
  assert(!options.empty());
  return options.at(getRng().pickIndex(options.size()));
}

TypeRef TypeCreator::getAnyIntType(bool allowConst) {
  TypeRef result = getRng().pickOneVec(builtin.getIntTypes());
  if (!types.isConst(result) && decision(Frag::VolatileInt))
    result = types.getOrCreateDerived(idents, Type::Kind::Volatile, result);
  if (allowConst && !types.isVolatile(result) && decision(Frag::ConstInt))
    result = types.getOrCreateDerived(idents, Type::Kind::Const, result);
  return result;
}

TypeRef TypeCreator::makeNewPtrType() {
  auto verify = p.queueVerify();

  while (true) {
    TypeRef base = getExistingDefinedType();
    return types.getOrCreateDerived(idents, Type::Kind::Pointer, base);
  }
  return getPtrType();
  assert(false);
}

TypeRef TypeCreator::makeNewType() {
  auto verify = p.queueVerify();

  ++typesCreated;
  if (typesCreated > maxCreatedTypesPerRun)
    return getAnyIntType();

  auto recLimit = typeRecursionLimit.scope();
  if (recLimit.reached())
    return getAnyIntType();

  enum { Const, Volatile, Pointer, FuncPtr, Array, Record };
  // TODO: Re-add record generation.
  switch (getRng().pickOne({Const, Pointer, FuncPtr,
                            // Arrays are more interesting.
                            Array, Array, Array, Array})) {
  case Const:
    return types.getOrCreateDerived(idents, Type::Kind::Const,
                                    getExistingNonArrayDefinedType());
  case Volatile:
    return types.getOrCreateDerived(idents, Type::Kind::Volatile,
                                    getExistingNonArrayDefinedType());
  case Pointer:
    return makeNewPtrType();
  case FuncPtr:
    return makeNewFuncPtrType();
  case Array:
    return makeNewArrayType();
  case Record:
    return makeRecordType();
  }
  SCCError("Missing switch?");
}

TypeRef TypeCreator::getDefinedType() {
  auto verify = p.queueVerify();

  bool canCreateTypes = !typeRecursionLimit.scope().reached();
  if (canCreateTypes)
    if (decision(Frag::CreateNewType))
      return makeNewType();

  std::vector<TypeRef> options;
  for (const Type &t : types) {
    if (builtin.isVoid(t.getRef()))
      continue;
    options.push_back(t.getRef());
  }
  return options.at(getRng().pickIndex(options.size()));
}
