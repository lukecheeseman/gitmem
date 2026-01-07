#pragma once

#include "branching/base_version_store.hh"

namespace gitmem {

namespace branching {

class LazyLocalVersionStore : public LocalVersionStore {
public:
  ~LazyLocalVersionStore() = default;

  LazyLocalVersionStore(ThreadID tid) : LocalVersionStore(tid) {}

  std::optional<Conflict> merge_with_commit(const std::shared_ptr<const Commit>&) override;
  std::optional<Value> get_committed(ObjectNumber number) const override;

};

}

}