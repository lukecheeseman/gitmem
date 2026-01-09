#pragma once

#include "execution_state.hh"
#include "graph.hh"
#include "graphviz.hh"
#include "lang.hh"
#include "progress_status.hh"
#include <trieste/trieste.h>
#include "sync_protocol.hh"
#include "termination_status.hh"

namespace gitmem {

using termination::TerminationStatus;

template <typename T>
using StepResult = std::variant<T, TerminationStatus>;

template <typename T>
bool is_terminated(const StepResult<T>& r) {
  return std::holds_alternative<TerminationStatus>(r);
}

template <typename T>
T& value(StepResult<T>& r) {
  return std::get<T>(r);
}

class Interpreter {
private:
  GlobalContext gctx;

public:
  Interpreter(GlobalContext gctx): gctx(std::move(gctx)) {}

  GlobalContext& context() { return gctx; }

  int run();

  StepResult<size_t> evaluate_expression(trieste::Node, Thread&);
  StepResult<int> run_statement(trieste::Node, Thread&);

  StepResult<ProgressStatus> progress_thread(Thread&);
  StepResult<ProgressStatus> run_single_thread_to_sync(Thread&);
  StepResult<ProgressStatus> run_threads_to_sync();

  void print_thread_traces();

  void build_and_print_revision_graph(const std::filesystem::path& output_path);

  graph::ExecutionGraph build_execution_graph_from_traces();

};

// Entry function
int interpret(const trieste::Node, const std::filesystem::path &output_file, SyncKind sync_kind);

} // namespace gitmem