#include "sync_protocol.hh"
#include "linear/sync_protocol.hh"
#include "branching/eager/sync_protocol.hh"
#include "branching/lazy/sync_protocol.hh"

namespace gitmem {

std::unique_ptr<SyncProtocol> make_protocol(SyncKind sync_kind) {
  switch (sync_kind) {
    case SyncKind::Linear:          return std::make_unique<linear::LinearSyncProtocol>();
    case SyncKind::BranchingEager:  return std::make_unique<branching::BranchingEagerSyncProtocol>();
    case SyncKind::BranchingLazy:   return std::make_unique<branching::BranchingLazySyncProtocol>();
  }
  std::unreachable();
}

} // namespace gitmem
