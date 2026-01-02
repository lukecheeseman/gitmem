#pragma once

#include "execution_state.hh"
#include "graph.hh"
#include "graphviz.hh"
#include "lang.hh"
#include "progress_status.hh"
#include <trieste/trieste.h>
#include "sync_protocol.hh"
#include "termination_status.hh"
#include "step_result.hh"

namespace gitmem {

class Interpreter {
private:
  GlobalContext gctx;

public:
  Interpreter(GlobalContext gctx): gctx(std::move(gctx)) {}

  GlobalContext& context() { return gctx; }

  // Internal functions
  int run();

  std::variant<size_t, TerminationStatus> evaluate_expression(trieste::Node, Thread&);
  std::variant<int, TerminationStatus> run_statement(trieste::Node, Thread&);

  std::variant<ProgressStatus, TerminationStatus> progress_thread(Thread&);
  std::variant<ProgressStatus, TerminationStatus> run_single_thread_to_sync(Thread&);
  std::variant<ProgressStatus, TerminationStatus> run_threads_to_sync();

};

// Entry function
int interpret(const trieste::Node, const std::filesystem::path &output_file, SyncKind sync_kind);

} // namespace gitmem