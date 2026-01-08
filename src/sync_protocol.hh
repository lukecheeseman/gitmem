#pragma once

#include "conflict.hh"
#include "sync_kind.hh"
#include "sync_state.hh"
#include "execution_state.hh"
#include "read_result.hh"
#include <memory>
#include <optional>

namespace gitmem {

std::unique_ptr<SyncProtocol> make_protocol(SyncKind);

class SyncProtocol {
public:
  virtual ~SyncProtocol() = default;
  virtual SyncKind kind() const = 0;
  virtual std::unique_ptr<ThreadSyncState> make_thread_state(ThreadID tid) const = 0;
  virtual std::unique_ptr<LockSyncState> make_lock_state() const = 0;

  // Read a shared variable into the thread context
  virtual ReadResult read(ThreadContext &ctx, const std::string &var) = 0;

  // Write a shared variable (staged, not committed)
  virtual void write(ThreadContext &ctx, const std::string &var,
                     size_t value) = 0;

  virtual std::optional<std::shared_ptr<ConflictBase>>
  on_spawn(ThreadContext &parent, ThreadContext &child) = 0;

  virtual std::optional<std::shared_ptr<ConflictBase>>
  on_join(ThreadContext &joiner, ThreadContext &joinee) = 0;

  virtual std::optional<std::shared_ptr<ConflictBase>>
  on_start(ThreadContext &thread) = 0;

  virtual std::optional<std::shared_ptr<ConflictBase>>
  on_end(ThreadContext &thread) = 0;

  virtual std::optional<std::shared_ptr<ConflictBase>>
  on_lock(ThreadContext &thread, Lock &lock) = 0;

  virtual std::optional<std::shared_ptr<ConflictBase>>
  on_unlock(ThreadContext &thread, Lock &lock) = 0;


  virtual std::ostream &print(std::ostream &os) const = 0;
  friend std::ostream &operator<<(std::ostream &os,
                                  const SyncProtocol &protocol) {
    return protocol.print(os);
  }
};

} // namespace gitmem
