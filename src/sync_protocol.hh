#pragma once

#include <optional>
#include <memory>
#include "execution_state.hh"

namespace gitmem {

struct Conflict;

class SyncProtocol {
public:
  virtual ~SyncProtocol() = default;

  // Read a shared variable into the thread context
  virtual std::optional<size_t> read(ThreadContext& ctx, const std::string& var) = 0;

  // Write a shared variable (staged, not committed)
  virtual void write(ThreadContext& ctx, const std::string& var, size_t value) = 0;

  virtual std::optional<Conflict> on_spawn(
    ThreadContext& parent,
    ThreadContext& child,
    GlobalContext& gctx
  ) = 0;

  virtual std::optional<Conflict> on_join(
    ThreadContext& joiner,
    ThreadContext& joinee,
    GlobalContext& gctx
  ) = 0;

  virtual std::optional<Conflict> on_lock(
    ThreadContext& thread,
    Lock& lock,
    GlobalContext& gctx
  ) = 0;

  virtual std::optional<Conflict> on_unlock(
    ThreadContext& thread,
    Lock& lock,
    GlobalContext& gctx
  ) = 0;
};

// ---------------------------------
// Concrete protocols
// ---------------------------------

class LinearSyncProtocol final : public SyncProtocol {
  // std::unordered_map<Timestamp, std::shared_ptr<graph::Node>> ts_nodes;

public:
  std::optional<size_t> read(ThreadContext& ctx, const std::string& var) override;

  void write(ThreadContext& ctx, const std::string& var, size_t value) override;

  std::optional<Conflict> on_spawn(
    ThreadContext& parent,
    ThreadContext& child,
    GlobalContext& gctx
  ) override;

  std::optional<Conflict> on_join(
    ThreadContext& joiner,
    ThreadContext& joinee,
    GlobalContext& gctx
  ) override;

  std::optional<Conflict> on_lock(
    ThreadContext& thread,
    Lock& lock,
    GlobalContext& gctx
  ) override;

  std::optional<Conflict> on_unlock(
    ThreadContext& thread,
    Lock& lock,
    GlobalContext& gctx
  ) override;
};

class BranchingSyncProtocol final : public SyncProtocol {

  std::unordered_map<Commit, std::shared_ptr<graph::Node>> commit_nodes;

public:
  std::optional<size_t> read(ThreadContext& ctx, const std::string& var) override;

  void write(ThreadContext& ctx, const std::string& var, size_t value) override;

  std::optional<Conflict> on_spawn(
    ThreadContext& parent,
    ThreadContext& child,
    GlobalContext& gctx
  ) override;

  std::optional<Conflict> on_join(
    ThreadContext& joiner,
    ThreadContext& joinee,
    GlobalContext& gctx
  ) override;

  std::optional<Conflict> on_lock(
    ThreadContext& thread,
    Lock& lock,
    GlobalContext& gctx
  ) override;

  std::optional<Conflict> on_unlock(
    ThreadContext& thread,
    Lock& lock,
    GlobalContext& gctx
  ) override;
};

} // namespace gitmem
