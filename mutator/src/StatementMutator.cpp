#include "LookUB/mutator/StatementMutator.h"

StatementMutator::StatementMutator(MutatorData &input)
    : UnsafeMutatorBase(input), literalMaker(input), simplifier(input),
      sc(input), mover(input) {}
