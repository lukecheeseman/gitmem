#pragma once

#include "branching/version_store.hh"
#include "execution_state.hh"
#include "linear/version_store.hh"
#include <memory>
#include <optional>

/* i want an on_start and on_end event i think too */

namespace gitmem {

struct ConflictBase {
  virtual ~ConflictBase() = default;
  virtual std::ostream &print(std::ostream &os) const = 0;
  friend std::ostream &operator<<(std::ostream &os,
                                  const ConflictBase &conflict) {
    return conflict.print(os);
  }
};

template <typename VersionID> struct Conflict : ConflictBase {
  std::string var;
  std::pair<VersionID, VersionID> versions;

  Conflict(std::string var, std::pair<VersionID, VersionID> versions)
      : var(std::move(var)), versions(std::move(versions)) {}

  std::ostream &print(std::ostream &os) const override;
};

using LinearConflict = Conflict<linear::Timestamp>;
using BranchingConflict = Conflict<branching::Commit>;

class SyncProtocol {
public:
  virtual ~SyncProtocol() = default;

  // Read a shared variable into the thread context
  virtual std::optional<size_t> read(ThreadContext &ctx,
                                     const std::string &var) = 0;

  // Write a shared variable (staged, not committed)
  virtual void write(ThreadContext &ctx, const std::string &var,
                     size_t value) = 0;

  virtual std::optional<std::unique_ptr<ConflictBase>>
  on_spawn(ThreadContext &parent, ThreadContext &child,
           GlobalContext &gctx) = 0;

  virtual std::optional<std::unique_ptr<ConflictBase>>
  on_join(ThreadContext &joiner, ThreadContext &joinee,
          GlobalContext &gctx) = 0;

  virtual std::optional<std::unique_ptr<ConflictBase>>
  on_start(ThreadContext &thread, GlobalContext &gctx) = 0;

  virtual std::optional<std::unique_ptr<ConflictBase>>
  on_end(ThreadContext &thread, GlobalContext &gctx) = 0;

  virtual std::optional<std::unique_ptr<ConflictBase>>
  on_lock(ThreadContext &thread, Lock &lock, GlobalContext &gctx) = 0;

  virtual std::optional<std::unique_ptr<ConflictBase>>
  on_unlock(ThreadContext &thread, Lock &lock, GlobalContext &gctx) = 0;
};

// ---------------------------------
// Concrete protocols
// ---------------------------------

class LinearSyncProtocol final : public SyncProtocol {
  linear::GlobalVersionStore _global_store;

  static linear::LocalVersionStore &store(ThreadContext &ctx) {
    if (!ctx.linear)
      ctx.linear.emplace();
    return ctx.linear->store;
  }

  std::optional<LinearConflict> push(linear::LocalVersionStore &local);
  std::optional<LinearConflict> pull(linear::LocalVersionStore &local);

public:
  ~LinearSyncProtocol() override;

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
};

class BranchingSyncProtocol final : public SyncProtocol {

  // std::unordered_map<Commit, std::shared_ptr<graph::Node>> commit_nodes;

public:
  ~BranchingSyncProtocol() override;

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
};

} // namespace gitmem
