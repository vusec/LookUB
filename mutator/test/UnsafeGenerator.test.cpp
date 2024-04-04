#include "LookUB/mutator/UnsafeGenerator.h"
#include "scc/mutator-utils/Scheduler.h"
#include "scc/utils/Counter.h"
#include "gtest/gtest.h"

#include <regex>

/// Generates two programs from the same entrophy inputs.
TEST(TestUnsafeGenerator, generateFromSameInputs) {
  std::string inputStr = "asdf";
  EntrophyVec input(inputStr);

  UnsafeStrategy strat;
  UnsafeGenerator gen;

  std::unique_ptr<Program> program1 = gen.generateFromEntrophy(input, strat);
  OutString out1;
  OptError err1 = program1->print(out1);
  ASSERT_FALSE(err1.hasError());

  input = EntrophyVec(inputStr);
  std::unique_ptr<Program> program2 = gen.generateFromEntrophy(input, strat);
  OutString out2;
  OptError err2 = program2->print(out2);
  ASSERT_FALSE(err2.hasError());

  // The same inputs should lead to the same programs.
  EXPECT_STREQ(out1.getStr().c_str(), out2.getStr().c_str());
}

/// Generates two programs from different entrophy inputs.
TEST(TestUnsafeGenerator, generateFromDifferentInput) {
  UnsafeStrategy strat;
  UnsafeGenerator gen;

  EntrophyVec input1("this is the first input entrophy");
  std::unique_ptr<Program> program1 = gen.generateFromEntrophy(input1, strat);
  OutString out1;
  OptError err1 = program1->print(out1);
  ASSERT_FALSE(err1.hasError());

  EntrophyVec input2("this is the second input entrophy");
  std::unique_ptr<Program> program2 = gen.generateFromEntrophy(input2, strat);
  OutString out2;
  OptError err2 = program2->print(out2);
  ASSERT_FALSE(err2.hasError());

  // The different inputs should lead to different programs.
  EXPECT_STRNE(out1.getStr().c_str(), out2.getStr().c_str());
}

/// Grows a program and then reduced it using our exploit generator.
TEST(TestUnsafeGenerator, generateAndReduce) {

  SchedulerBase::FeedbackFunc feedback = [](const Program &p) {
    SchedulerBase::Feedback result;
    result.score = p.countNodes();
    return result;
  };

  const std::size_t seed = 123;
  Scheduler<UnsafeGenerator> scheduler(feedback, seed);

  scheduler.steps(100);
  const std::size_t maxNodes = scheduler.getBestProg().countNodes();

  feedback = [](const Program &p) {
    SchedulerBase::Feedback result;
    result.score = p.countNodes();
    result.interesting = true;
    return result;
  };
  Reducer<UnsafeGenerator> reducer(feedback, seed, scheduler.getBestProg());

  // Expect that the reduced program is at least reduced by half.
  // This might fail in some really fringe case, so this is just a reasonable
  // expectation of how the reducer should work.
  const int reducedFactor = 2;

  for (auto counter : count::upTo(100)) {
    reducer.step();
    const std::size_t minNodes = reducer.getProgram().countNodes();
    // The reduced program should never grow.
    EXPECT_LE(minNodes, maxNodes);

    // Stop when we know the test will pass.
    if (minNodes * reducedFactor < maxNodes)
      break;
  }

  const std::size_t minNodes = reducer.getProgram().countNodes();
  EXPECT_LT(minNodes * reducedFactor, maxNodes);
}

/// How many iterations the test schedulers should make before giving up and
/// failing a test.
static const unsigned maxItersToFind = 20000;

/// Generate programs until the string is found in the source of a generated
/// program.
static void tryFind(std::string toFind) {
  SchedulerBase::FeedbackFunc feedback = [&](const Program &p) {
    SchedulerBase::Feedback result;
    OutString str;
    OptError err = p.print(str);
    // If we failed to print we just won't find our result.
    (void)err;
    result.interesting = (str.getStr().find(toFind) != std::string::npos);
    result.score = p.countNodes();
    return result;
  };

  const std::size_t seed = 123;
  Scheduler<UnsafeGenerator> scheduler(feedback, seed);
  EXPECT_TRUE(scheduler.stepUntilFinding(maxItersToFind));
}

/// Generate programs until a program matching the given regex in its source
/// is found.
/// The example just serves as a safety check that the regex makes sense.
static void tryFindRegex(std::string r, std::string example) {
  std::regex toFind(r, std::regex_constants::ECMAScript);
  EXPECT_TRUE(static_cast<bool>(std::regex_search(example, toFind)))
      << "regex doesn't match example?";

  SchedulerBase::FeedbackFunc feedback = [&](const Program &p) {
    SchedulerBase::Feedback result;
    OutString str;
    OptError err = p.print(str);
    // If we failed to print we just won't find our result.
    (void)err;
    result.interesting = std::regex_search(str.getStr(), toFind);
    result.score = p.countNodes();
    return result;
  };

  const std::size_t seed = 123;
  Scheduler<UnsafeGenerator> scheduler(feedback, seed);
  EXPECT_TRUE(scheduler.stepUntilFinding(maxItersToFind));
}

// The following tests just generate programs until a string is found.
// This ensures the generator can generate certain statements.

TEST(TestUnsafeGenerator, generatePrintf) { tryFind("printf("); }

TEST(TestUnsafeGenerator, generateMalloc) { tryFind("malloc("); }

TEST(TestUnsafeGenerator, generateCalloc) { tryFind("calloc("); }

TEST(TestUnsafeGenerator, generateRealloc) { tryFind("realloc("); }

TEST(TestUnsafeGenerator, generateFree) { tryFind("free("); }

TEST(TestUnsafeGenerator, generateAbort) { tryFind("abort("); }

TEST(TestUnsafeGenerator, generateExit) { tryFind("exit("); }

TEST(TestUnsafeGenerator, generateStrlen) { tryFind("strlen("); }

TEST(TestUnsafeGenerator, generateStrStr) { tryFind("strstr("); }

TEST(TestUnsafeGenerator, generateMemcmp) { tryFind("memcmp("); }

TEST(TestUnsafeGenerator, generateMemcpy) { tryFind("memcpy("); }

TEST(TestUnsafeGenerator, generateMemset) { tryFind("memset("); }

TEST(TestUnsafeGenerator, generateStrcpy) { tryFind("strcpy("); }

TEST(TestUnsafeGenerator, generatePureAttr) { tryFind("(pure)"); }

TEST(TestUnsafeGenerator, generateConstAttr) { tryFind("(const)"); }

TEST(TestUnsafeGenerator, generateAlwaysInlineAttr) {
  tryFind("(always_inline)");
}

TEST(TestUnsafeGenerator, generateNoBuiltinAttr) { tryFind("(no_builtin)"); }

TEST(TestUnsafeGenerator, generateEmptyStrLiteral) { tryFind("\"\""); }

TEST(TestUnsafeGenerator, generateStrLiteral) {
  tryFindRegex("\"[a-zA-Z0-9 ]+\"", R"str("abc")str");
}

TEST(TestUnsafeGenerator, generateSubscript) {
  tryFindRegex("[a-zA-Z0-9]\\[", R"str(array2[123])str");
}

TEST(TestUnsafeGenerator, arrayDef) {
  tryFindRegex("typedef [a-zA-Z0-9 *]+", "typedef int foo[2];");
}

TEST(TestUnsafeGenerator, funcPtr) { tryFind("funcPtr"); }

TEST(TestUnsafeGenerator, whileLoop) { tryFind(" while ("); }

TEST(TestUnsafeGenerator, ifCond) { tryFind(" if ("); }

TEST(TestUnsafeGenerator, argcUse) {
  // Check that argc is sometimes used.
  tryFind("(argc");
}

TEST(TestUnsafeGenerator, arrayInit) {
  // Check for array initializers.
  tryFind("= {");
}

TEST(TestUnsafeGenerator, assign) { tryFind(" = "); }

TEST(TestUnsafeGenerator, shiftLeft) { tryFind("<<"); }

TEST(TestUnsafeGenerator, shiftRight) { tryFind(">>"); }

TEST(TestUnsafeGenerator, addition) { tryFind("+"); }

TEST(TestUnsafeGenerator, multiplication) { tryFind(")*("); }

TEST(TestUnsafeGenerator, substraction) { tryFind("-"); }

TEST(TestUnsafeGenerator, division) { tryFind("-"); }

TEST(TestUnsafeGenerator, modulo) { tryFind("%"); }
