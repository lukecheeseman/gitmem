#pragma once

#include <vector>
#include <optional>
#include <unordered_map>
#include <trieste/trieste.h>

#include "graphviz.hh"
#include "lang.hh"
#include "linear/version_store.hh"
#include "branching/version_store.hh"

namespace gitmem {

  class SyncProtocol;

  enum class TerminationStatus {
    completed,
    datarace_exception,
    unlock_exception,
    assertion_failure_exception,
    unassigned_variable_read_exception,
  };

  struct ThreadContext {
    std::unordered_map<std::string, size_t> locals;
    std::shared_ptr<graph::Node> tail;

    struct LinearData { linear::LocalVersionStore store; };
    struct BranchingData { branching::LocalVersionStore store; };

    std::optional<LinearData> linear;
    std::optional<BranchingData> branching;
  };

  struct Thread {
    ThreadContext ctx;
    trieste::Node block;
    size_t pc = 0;
    std::optional<TerminationStatus> terminated = std::nullopt;

    bool operator==(const Thread &other) const;
  };

  using ThreadID = size_t;

  struct Lock {
    // Globals globals;
    // std::optional<ThreadID> owner = std::nullopt;
    // std::shared_ptr<graph::Node> last;
  };

  template<typename T, typename...Args>
  std::shared_ptr<T> thread_append_node(ThreadContext& ctx, Args&&...args);

  template<>
  std::shared_ptr<graph::Pending> thread_append_node<graph::Pending>(ThreadContext& ctx, std::string&& stmt);

  struct GlobalContext {
    // Execution state
    std::vector<std::shared_ptr<Thread>> threads;
    std::unordered_map<std::string, Lock> locks;

    // AST evaluation cache
    lang::NodeMap<size_t> cache;

    // Graph root
    std::shared_ptr<graph::Node> entry_node;

    // Synchronisation semantics (policy)
    std::unique_ptr<SyncProtocol> protocol;

    GlobalContext(const trieste::Node &ast, std::unique_ptr<SyncProtocol> protocol);
    ~GlobalContext();

    bool operator==(const GlobalContext &other) const;

    void print_execution_graph(const std::filesystem::path &output_path) const;
  };

} // namespace gitmem