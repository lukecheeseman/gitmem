#pragma once

#include "../sync_protocol.hh"
#include "base_version_store.hh"

namespace gitmem {

using BranchingConflict = Conflict<branching::Timestamp>;

namespace branching {

class BranchingSyncProtocolBase : public SyncProtocol {
protected:
  GlobalVersionStore _global_store;
  bool verbose_commits;

  explicit BranchingSyncProtocolBase(bool verbose_commits)
    : verbose_commits(verbose_commits) {}

public:
  ~BranchingSyncProtocolBase() override;

  std::unique_ptr<SyncProtocol> clone() const override = 0;

  ReadResult read(ThreadContext &ctx, const std::string &var) override;

  void write(ThreadContext &ctx, const std::string &var, size_t value) override;

  std::optional<std::shared_ptr<ConflictBase>>
  on_spawn(ThreadContext &parent, ThreadContext &child) override;

  std::optional<std::shared_ptr<ConflictBase>>
  on_join(ThreadContext &joiner, ThreadContext &joinee) override;

  std::optional<std::shared_ptr<ConflictBase>>
  on_start(ThreadContext &thread) override;

  std::optional<std::shared_ptr<ConflictBase>>
  on_end(ThreadContext &thread) override;

  std::optional<std::shared_ptr<ConflictBase>>
  on_lock(ThreadContext &thread, Lock &lock) override;

  std::optional<std::shared_ptr<ConflictBase>>
  on_unlock(ThreadContext &thread, Lock &lock) override;

  std::ostream &print(std::ostream &os) const override;

  std::string build_revision_graph_dot(const std::vector<const ThreadSyncState*>& thread_states) const override;

  std::unique_ptr<LockSyncState> make_lock_state() const override {
    return std::make_unique<LockState>();
  }
};

// Builder for creating branching sync protocols
class BranchingSyncProtocolBuilder {
private:
  bool eager_mode = false;
  bool verbose_commits = false;

public:
  BranchingSyncProtocolBuilder& eager() {
    eager_mode = true;
    return *this;
  }

  BranchingSyncProtocolBuilder& lazy() {
    eager_mode = false;
    return *this;
  }

  BranchingSyncProtocolBuilder& with_verbose_commits(bool v = true) {
    verbose_commits = v;
    return *this;
  }

  std::unique_ptr<SyncProtocol> build() const;
};

} // end branching

} // end gitmem