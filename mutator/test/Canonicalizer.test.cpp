#include "LookUB/mutator/Canonicalizer.h"

#include "gtest/gtest.h"

TEST(TestCanonicalizer, SimplifyCompound) {
  // Try simplify nested compound statements.
  auto c =
      Statement::CompoundStmt({Statement::CompoundStmt({Statement::Break()})});
  std::optional<Statement> res = Canonicalizer::canonicalizeStmt(c);
  ASSERT_TRUE(res);

  ASSERT_EQ(res->getChildren().size(), 1U);
  ASSERT_EQ(res->getChildren().front().getKind(), Statement::Kind::Break);
}
