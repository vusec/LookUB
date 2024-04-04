#include "LookUB/mutator/UnsafeGenerator.h"
#include "scc/driver/ArgParser.h"
#include "scc/driver/Driver.h"
#include "scc/driver/DriverUtils.h"
#include "scc/mutator-utils/Scheduler.h"

#include <filesystem>
#include <iostream>

static void printUsage(std::string progamName) {
  std::cerr << "Usage: " << progamName
            << " [options...] -- oracle-bin oracle-arg1 oracle-arg2 ...\n";
  std::cerr << "Options:\n";
  std::cerr << " --lang-opts=PATH\n";
  std::cerr << " --tries=N         How many tries to give each program.\n";
  std::cerr << " --queue-size=N    How many programs to keep in the queue.\n";
  std::cerr
      << " --scale=N         How many mutations to apply each iteration.\n";
  std::cerr << " --step            Manual stepping mode.\n";
  std::cerr << " --stop-after-hit  Stop fuzzing after first finding.\n";
  std::cerr << " --stop-after=N    Stop fuzzing after N findings. \n";
  std::cerr << " --reducer-tries=N How many tries to reduce programs. \n";
  std::cerr << " --ui-update=N     UI update frequency (in ms)\n";
  std::cerr << " --splash          Whether to show a startup splash.\n";
}

template <typename Gen> int generatorMain(const ArgParser &args) {
  LangOpts opts;
  if (!args.optsFile.empty())
    if (auto error = opts.loadFromFile(args.optsFile)) {
      std::cerr << "Failed to parse option file: " + error->getMessage();
      return 1;
    }

  // Create a scheduler and pass all the parsed command line args.
  Scheduler<Gen> sched(args.seed, opts);
  std::cout << "Running with seed " << args.seed << "\n";
  sched.setMaxQueueSize(args.queueSize);
  sched.setMaxRunLimit(args.tries);
  sched.setMutatorScale(args.mutatorScale);
  sched.setStopAfter(args.stopAfter);
  sched.setStopAfterHit(args.stopAfterHits);
  sched.setReducerTries(args.reducerTries);

  // Let the scheduler/generator handle unknown args.
  if (auto err = sched.handleArgs(args.unknownArgs)) {
    printUsage(args.argv0);
    std::cerr << err->getMessage() << "\n";
    return 1;
  }

  // Create the command to run on every generated program.
  std::string evalCommand = args.getEvalCommand();
  evalCommand =
      Gen::expandEvalCommand(args.argv0, evalCommand, RngSource(args.seed));

  std::string saveDir = args.saveDir;
  if (!std::filesystem::create_directory(saveDir) &&
      !std::filesystem::exists(saveDir)) {
    std::cerr << "Failed to create output dir '" << saveDir << "'.\n";
    return 1;
  }

  // Create the driver that runs the scheduler and displays the UI.
  Driver driver(
      sched, evalCommand, [&sched]() { sched.step(); }, saveDir);
  driver.setSimpleUI(args.simpleUI);
  driver.setUpdateInterval(args.uiUpdateMs);
  driver.setManualStepping(args.manualStepping);
  driver.setPrefixFunc(
      [](const Program &p) { return Gen::getProgramPrefix(p); });
  driver.setSuffixFunc(
      [](const Program &p) { return Gen::getProgramSuffix(p); });

  // Run until we are done.
  driver.run(args.splash);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  for (int arg = 0; arg < argc; ++arg) {
    std::string argValue = argv[arg];
    if (argValue == "--")
      break;
    if (argValue != "--help")
      continue;
    printUsage(argv[0]);
    return 0;
  }

  DriverUtils::prependPythonPath(argv[0]);

  std::random_device d;
  ArgParser args;
  args.reducerTries = 0;
  // Use a simple UI if the output is not an interactive TTY.
  args.simpleUI = !DriverUtils::isStdoutAProperTerminal();
  args.seed = d();
  if (auto err = args.parse(argc, argv)) {
    std::cerr << "Failed to parse arguments: " << *err << "\n";
    printUsage(args.argv0);
    return 1;
  }

  return generatorMain<UnsafeGenerator>(args);
}
