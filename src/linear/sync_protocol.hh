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

  std::optional<size_t> read(ThreadContext &ctx,
                             const std::string &var) override;

  void write(ThreadContext &ctx, const std::string &var, size_t value) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_spawn(ThreadContext &parent, ThreadContext &child,
           GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_join(ThreadContext &joiner, ThreadContext &joinee,
          GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_start(ThreadContext &thread, GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_end(ThreadContext &thread, GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_lock(ThreadContext &thread, Lock &lock, GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_unlock(ThreadContext &thread, Lock &lock, GlobalContext &gctx) override;

  std::ostream &print(std::ostream &os) const override;
};

} // namespace linear

} // namespace gitmem