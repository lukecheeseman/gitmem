#pragma once

#include "branching/base_version_store.hh"

namespace gitmem {

namespace branching {

class LazyLocalVersionStore : public LocalVersionStore {
private:
  bool raise_early_conflicts;

public:
  ~LazyLocalVersionStore() = default;

  LazyLocalVersionStore(ThreadID tid, bool verbose, bool raise_early_conflicts) : LocalVersionStore(tid, verbose), raise_early_conflicts(raise_early_conflicts) {}

  std::optional<Conflict> merge_with_commit(const std::shared_ptr<const Commit>&) override;
  BranchingReadResult get_committed(std::string var) const override;

};

}

}