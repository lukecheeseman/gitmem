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

// Entry function
int interpret(const trieste::Node, const std::filesystem::path &output_file, SyncKind sync_kind);

std::variant<ProgressStatus, TerminationStatus>
progress_thread(GlobalContext &, const ThreadID, std::shared_ptr<Thread>);

} // namespace gitmem