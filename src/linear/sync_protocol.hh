#pragma once

#include "../sync_protocol.hh"
#include "conflict.hh"
#include "sync_kind.hh"
#include "execution_state.hh"
#include "linear/version_store.hh"

namespace gitmem {

using LinearConflict = Conflict<linear::Timestamp>;

namespace linear {

class LinearSyncProtocol final : public SyncProtocol {
  GlobalVersionStore _global_store;

  std::optional<LinearConflict> push(LocalVersionStore &local);
  std::optional<LinearConflict> pull(LocalVersionStore &local);

public:
  ~LinearSyncProtocol() override;
  SyncKind kind() const override { return SyncKind::Linear; };

  ReadResult read(ThreadContext &ctx, const std::string &var) override;

  void write(ThreadContext &ctx, const std::string &var, size_t value) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_spawn(ThreadContext &parent, ThreadContext &child) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_join(ThreadContext &joiner, ThreadContext &joinee) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_start(ThreadContext &thread) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_end(ThreadContext &thread) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_lock(ThreadContext &thread, Lock &lock) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_unlock(ThreadContext &thread, Lock &lock) override;

  std::ostream &print(std::ostream &os) const override;

  std::unique_ptr<ThreadSyncState> make_thread_state(ThreadID tid) const override {
    return std::make_unique<LocalVersionStore>();
  }

  std::unique_ptr<LockSyncState> make_lock_state() const override {
    return nullptr;
  }
};

} // namespace linear

} // namespace gitmem