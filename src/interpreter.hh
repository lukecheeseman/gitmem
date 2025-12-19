#pragma once

#include "execution_state.hh"
#include "graph.hh"
#include "graphviz.hh"
#include "lang.hh"
#include <trieste/trieste.h>

namespace gitmem {

// Entry function
int interpret(const trieste::Node, const std::filesystem::path &output_file);

// Internal functions
int run_threads(GlobalContext &);

enum class ProgressStatus { progress, no_progress };
inline bool operator!(ProgressStatus p) {
  return p == ProgressStatus::no_progress;
}
inline ProgressStatus operator||(const ProgressStatus &p1,
                                 const ProgressStatus &p2) {
  return (p1 == ProgressStatus::progress || p2 == ProgressStatus::progress)
             ? ProgressStatus::progress
             : ProgressStatus::no_progress;
}
inline void operator|=(ProgressStatus &p1, const ProgressStatus &p2) {
  p1 = (p1 || p2);
}

std::variant<ProgressStatus, TerminationStatus>
progress_thread(GlobalContext &, const ThreadID, std::shared_ptr<Thread>);

} // namespace gitmem