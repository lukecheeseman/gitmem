#include "sync_protocol.hh"
#include "linear/sync_protocol.hh"
#include "branching/sync_protocol.hh"

namespace gitmem {

std::unique_ptr<SyncProtocol> make_protocol(SyncKind sync_kind) {
  switch (sync_kind) {
    case SyncKind::Linear:
      return std::make_unique<linear::LinearSyncProtocol>();
    case SyncKind::Branching:
      return std::make_unique<branching::BranchingSyncProtocol>();
  }
  std::unreachable();
}

} // namespace gitmem
