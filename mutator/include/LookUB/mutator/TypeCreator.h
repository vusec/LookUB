#ifndef TYPECREATOR_H
#define TYPECREATOR_H

#include "UnsafeMutatorBase.h"
#include "scc/mutator-utils/RecursionLimit.h"
#include "scc/program/Program.h"
#include "scc/program/RecordDecl.h"

/// Creates new types in the program.
class TypeCreator : UnsafeMutatorBase {
  // Define some recursion limits that avoid crashes.

  /// How many nested types we are allowed to create in one go.
  RecursionLimit typeRecursionLimit = 3;
  /// How many nested records we are allowed to create in on ego.
  RecursionLimit recordRecursionLimit = 3;

  /// Limits the amount of types that are created by a single TypeCreator.
  ///
  /// This is necessary to avoid a potential explosion of types given a bad RNG
  /// sequence or a bad mutation strategy. The biggest thing we're trying to
  /// avoid is creating a large sequence of function pointer types (as each
  /// argume might be against a new function pointer type which creates N
  /// new types).
  static constexpr unsigned maxCreatedTypesPerRun = 3;
  /// Counts how many types have been created
  /// so far by this TypeCreator.
  unsigned typesCreated = 0;

public:
  TypeCreator(MutatorData &input);

  /// Returns a random valid type.
  TypeRef getAnyType() {
    if (decision(Frag::PickVoidForAny))
      return Void();
    return getDefinedType();
  }

  /// Returns the pointer type that points to memory of the given type.
  TypeRef getPtrTypeOf(TypeRef t) {
    return types.getOrCreateDerived(idents, Type::Kind::Pointer, t);
  }

  /// Returns a random pointer type.
  TypeRef getPtrType();

  /// Return a random floating point type.
  TypeRef getAnyFloatType() {
    return getRng().pickOneVec(builtin.getFloatTypes());
  }

  /// Returns a random integer type. Might be cv-qualified.
  /// \param allowConst Whether the type can be const-qualified.
  TypeRef getAnyIntType(bool allowConst = false);

  /// Returns a random integer or float type.
  TypeRef getAnyIntOrFloatType() {
    if (decision(Frag::PickFloatOverInt))
      return getAnyFloatType();
    return getAnyIntType();
  }

  /// Returns a random integer or pointer type.
  TypeRef getAnyIntOrPtrType() {
    if (decision(Frag::PickPtrOverInt))
      return getPtrType();
    return getAnyIntType();
  }

  /// Returns a random int/float/pointer type.
  TypeRef getAnyIntOrPtrOrFloatType() {
    if (decision(Frag::PickPtrOverInt))
      return getPtrType();
    if (decision(Frag::PickFloatOverInt))
      return getAnyFloatType();
    return getAnyIntType();
  }

  /// Returns an existing type that is defined (e.g., has a size).
  TypeRef getExistingDefinedType() {
    std::vector<TypeRef> options;
    for (const Type &t : types)
      if (!builtin.isVoid(t.getRef()))
        options.push_back(t.getRef());
    return options.at(getRng().pickIndex(options.size()));
  }

  /// Returns an existing type that is defined and not an array type.
  TypeRef getExistingNonArrayDefinedType() {
    std::vector<TypeRef> options;
    for (const Type &t : types)
      if (!builtin.isVoid(t.getRef()) && t.getKind() != Type::Kind::Array)
        options.push_back(t.getRef());
    return options.at(getRng().pickIndex(options.size()));
  }

  /// Creates a new Ptr type.
  TypeRef makeNewPtrType();

  /// Create a new function pointer type.
  TypeRef makeNewFuncPtrType() {
    return makeNewFuncPtrTypeWithResult(getReturnType());
  }

  /// Returns a new function pointer with the given return type.
  TypeRef makeNewFuncPtrTypeWithResult(TypeRef ret) {
    auto verify = p.queueVerify();
    std::vector<TypeRef> args;
    for (unsigned i = 0; i < getRng().getBelow(5U); ++i)
      args.push_back(getExistingDefinedType());
    return types.addType(
        Type::FunctionPointer(ret, args, idents.makeNewID("funcPtrT")));
  }

  /// Returns a new random array type.
  TypeRef makeNewArrayType() {
    while (true) {
      TypeRef ret = getDefinedType();
      // Avoid multi-dimensional arrays as they have weird semantics.
      if (types.get(ret).isArray())
        continue;
      unsigned size = 1U + getRng().getBelow(128U);
      return types.addType(Type::Array(ret, size, idents.makeNewID("arrayT")));
    }
  }

  /// Returns an arbtirary newly created type.
  TypeRef makeNewType();

  /// Returns a type that is valid for a Function return type.
  TypeRef getReturnType() {
    while (true) {
      TypeRef t = getAnyType();
      if (!isValidReturnType(t))
        continue;
      return t;
    }
  }

  /// Returns a type that is defined and not const.
  TypeRef getDefinedNonConstType() {
    while (true) {
      TypeRef t = getDefinedType();
      if (types.isConst(t))
        continue;
      return t;
    }
  }

  /// Returns a type that is complete/defined.
  TypeRef getDefinedType();

  /// Returns whatever type is used for boolean results in the current language.
  TypeRef getBoolType() { return builtin.signed_int; }

  /// Creates a random field of the given type.
  Record::Field makeField(TypeRef t = Void()) {
    if (t == Void())
      t = getDefinedNonConstType();
    unsigned bitsize = 0;
    // if (p.getBuiltin().isIntType(t))
    //   bitsize = getRng().getBelow(types.getBitSize(t));
    Record::Field field(newID("field"), t);
    return field;
  }

  /// Creates a new record that contains at least one member of the given type.
  Record *makeRecord(TypeRef expectedMember) {
    auto verify = p.queueVerify();

    if (expectedMember == Void())
      expectedMember = getAnyIntOrFloatType();

    auto recLimit = recordRecursionLimit.scope();
    unsigned fieldLimit = 10;
    if (recLimit.reached())
      fieldLimit = 0;

    auto r = Record::Struct(p, newID("record"), {});
    for (unsigned i = 0; i < getRng().getBelow(fieldLimit); ++i)
      r->addField(makeField());
    r->addField(makeField(expectedMember));
    for (unsigned i = 0; i < getRng().getBelow(fieldLimit); ++i)
      r->addField(makeField());
    return &p.add(std::move(r));
  }

  /// Returns a random record type.
  TypeRef makeRecordType() {
    Record *r = makeRecord(Void());
    return r->getType();
  }
};

#endif // TYPECREATOR_H
