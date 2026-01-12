#include <CLI/CLI.hpp>

#include "debug.hh"
#include "interpreter.hh"
#include "model_checker.hh"
#include "debugger.hh"
#include "lang.hh"
#include "linear/sync_protocol.hh"
#include "branching/base_sync_protocol.hh"

int main(int argc, char **argv) {
  using namespace trieste;
  CLI::App app;

  std::filesystem::path input_path;
  app.add_option("input", input_path, "Path to the input file ")
      ->required()
      ->check(CLI::ExistingFile);

  std::filesystem::path output_path = "";
  app.add_option("-o,--output", output_path, "Path to the output file.");

  bool verbose = false;
  app.add_flag("-v,--verbose", verbose,
               "Enable verbose output from the interpreter.");

  bool include_empty_commits = false;
  app.add_flag("--include-empty-commits", include_empty_commits,
               "Include empty commits in branching protocol output.");

  bool interactive = false;
  app.add_flag("-i,--interactive", interactive,
               "Enable interactive scheduling mode (use command ? for help).");

  bool model_check = false;
  app.add_flag("-e,--explore", model_check,
               "Explore all possible execution paths.");

  std::string sync_protocol = "linear";
  app.add_option("--sync", sync_protocol, "Select a sync protocol for execution (default: linear)")
    ->check(CLI::IsMember({"linear", "branching-eager", "branching-lazy"}))
    ->type_name("SYNC_KIND");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  try {
    gitmem::verbose::out.enabled = verbose;

    gitmem::verbose::out << "Reading file " << input_path << std::endl;
    if (!std::filesystem::exists(input_path)) {
      std::cerr << "Input file does not exist: " << input_path << std::endl;
      return 1;
    }

    auto reader = gitmem::lang::reader().file(input_path);
    auto result = reader.read();

    if (!result.ok) {
      trieste::logging::Error err;
      result.print_errors(err);
      trieste::logging::Debug() << result.ast;
      return 1;
    }

    if (output_path.empty())
      output_path = input_path.stem().replace_extension(".dot");

    gitmem::verbose::out << "Output will be written to " << output_path << std::endl;

    // Build protocol based on command line options
    std::unique_ptr<gitmem::SyncProtocol> protocol;
    if (sync_protocol == "linear") {
      protocol = gitmem::linear::LinearSyncProtocolBuilder().build();
    } else if (sync_protocol == "branching-eager") {
      protocol = gitmem::branching::BranchingSyncProtocolBuilder()
        .eager()
        .with_verbose_commits(include_empty_commits)
        .build();
    } else if (sync_protocol == "branching-lazy") {
      protocol = gitmem::branching::BranchingSyncProtocolBuilder()
        .lazy()
        .with_verbose_commits(include_empty_commits)
        .build();
    }

    int exit_status;
    wf::push_back(gitmem::lang::wf);
    if (model_check) {
      exit_status = gitmem::model_check(result.ast, output_path, std::move(protocol));
    } else if (interactive) {
      exit_status = gitmem::interpret_interactive(result.ast, output_path, std::move(protocol));
    } else {
      exit_status = gitmem::interpret(result.ast, output_path, std::move(protocol));
    }
    wf::pop_front();

    gitmem::verbose::out << "Execution finished with exit status " << exit_status
                    << std::endl;
    return exit_status;
  } catch (const std::exception &e) {
    std::cerr << "Exception caught: " << e.what() << std::endl;
    return 1;
  }
}
