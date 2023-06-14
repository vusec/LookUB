#ifndef FUNCTIONMUTATOR_H
#define FUNCTIONMUTATOR_H

#include "UnsafeMutatorBase.h"
#include <string>

/// Mutates all data associated with a function except their bodies.
class FunctionMutator : public UnsafeMutatorBase {
  std::string getRandomCallingConvName() {
    // Random locations that Clang/GCC claim to support.
    return std::string(getRng().pickOne(
        {"stdcall", "regcall", "pascal", "ms_abi", "sysv_abi", "vectorcall"}));
  }

  /// Return a random calling convention.
  std::string getRandomCallingConv() {
    return "__attribute__((" + getRandomCallingConvName() + "))";
  }

  /// Returns a random unsigned integer string.
  std::string uintStr(unsigned limit) {
    return std::to_string(getRng().getBelow<unsigned>(limit));
  }

  /// Returns a random function attribute.
  std::string getRandomFuncAttr() {
    // Dump of random Clang attributes.
    return getRng().pickOneVec<std::string>(
        {"__attribute__((alloc_size(" + uintStr(4) + ")))",
         "__attribute__((alloc_size(" + uintStr(4) + ", " + uintStr(4) + ")))",
         "__attribute__((always_inline))",
         "__attribute__((assume_aligned (" + uintStr(4) + ")))",
         "__attribute__((const))", "__attribute__((disable_tail_calls))",
         "__attribute__((flatten))", "__attribute__((malloc))",
         "__attribute__((no_builtin))", "__attribute__((noinline))",
         "__attribute__((pure))",
         "__attribute__((no_caller_saved_registers, " +
             getRandomCallingConvName() + "))"});
  }

public:
  FunctionMutator(MutatorData &input) : UnsafeMutatorBase(input) {}

  /// Randomizes the function attributes of the given function.
  Modified randomizeFuncAttrs(Function &f) {
    if (decision(Frag::UseNonStdCallingConv)) {
      f.setCallingConv(getRandomCallingConv());
      return Modified::Yes;
    }

    if (decision(Frag::UseFunctionAttr)) {
      f.addAttr(getRandomFuncAttr());
      return Modified::Yes;
    }

    if (decision(Frag::UseSecondFunctionAttr)) {
      f.addAttr(getRandomFuncAttr());
      return Modified::Yes;
    }

    if (decision(Frag::DeleteFuncAttrs)) {
      auto attrs = f.getAllAttrs();
      if (attrs.empty())
        return Modified::No;
      f.removeAttr(getRng().pickOneVec(attrs));
      return Modified::Yes;
    }

    if (f.setWeight(
            getRng().pickOne({Function::Weight::None, Function::Weight::Hot,
                              Function::Weight::Cold})))
      return Modified::Yes;
    return Modified::No;
  }
};

#endif // FUNCTIONMUTATOR_H
